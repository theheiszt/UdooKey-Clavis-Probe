#include "adc_subsystem.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include <math.h>

// ─── Module Private Storage ───────────────────────────────────────────────────

static AdcChannelConfig s_channels[5] = {
    // GPIO, HW_Index, Divider, Offset, Gain, AvgSamples
    { PIN_ADC0,                0, 1.0f, 0.0f, 1.0f, 16 }, // CH0 (GP26)
    { PIN_ADC1,                1, 1.0f, 0.0f, 1.0f, 16 }, // CH1 (GP27)
    { PIN_ADC2,                2, 1.0f, 0.0f, 1.0f, 16 }, // CH2 (GP28)
    { PIN_INTERNAL_VSYS_SENSE, 3, 3.0f, 0.0f, 1.0f, 16 }, // VSYS (GP29, 1/3 hardware divider)
    { 0xFF,                    4, 1.0f, 0.0f, 1.0f, 16 }  // Internal Core Temp Sensor
};

static ContinuityConfig s_cont_cfg = {
    CONTINUITY_DEFAULT_THRESH_OHMS, // 25.0 ohms
    CONTINUITY_DEFAULT_HYST_OHMS,   // 10.0 ohms
    CONTINUITY_R_REF_OHMS,          // 1000.0 ohms reference excitation
    0.0f,                           // 0.0 ohms probe lead tare
    true,                           // Buzzer enabled
    CONTINUITY_BUZZER_FREQ_HZ,      // 2700 Hz
    2                               // 2 ms poll period
};

static ContinuityStatus s_cont_status = {
    CONT_STATE_OPEN,
    1e6f,
    false,
    false,
    0,
    0
};

// Waveform DMA State & Buffer (4 KiB = 2048 16-bit samples)
static uint16_t s_dma_buffer[ADC_WAVEFORM_MAX_SAMPLES];
static int      s_dma_channel = -1;
static bool     s_dma_running = false;
static uint16_t s_dma_sample_count = 0;
static uint32_t s_dma_start_time_us = 0;

// Buzzer PWM State
static bool     s_buzzer_initialized = false;
static bool     s_buzzer_playing = false;

// ─── Buzzer Driver Implementation ─────────────────────────────────────────────

void buzzer_init() {
    if (s_buzzer_initialized) return;

    gpio_set_function(PIN_BUZZER, GPIO_FUNC_SIO);
    gpio_set_dir(PIN_BUZZER, GPIO_OUT);
    gpio_put(PIN_BUZZER, 0);

    // Also initialize onboard LED for synchronized optical continuity indicator
    gpio_init(PIN_ONBOARD_LED);
    gpio_set_dir(PIN_ONBOARD_LED, GPIO_OUT);
    gpio_put(PIN_ONBOARD_LED, 0);

    s_buzzer_initialized = true;
    s_buzzer_playing = false;
}

void buzzer_tone(uint16_t freq_hz, uint8_t duty_pct) {
    if (!s_buzzer_initialized) buzzer_init();
    if (freq_hz == 0) {
        buzzer_stop();
        return;
    }

    uint slice = pwm_gpio_to_slice_num(PIN_BUZZER);
    gpio_set_function(PIN_BUZZER, GPIO_FUNC_PWM);

    // RP2040 system clock is 133 MHz (or 125 MHz default)
    uint32_t sys_clk = clock_get_hz(clk_sys);
    uint32_t divider16 = (sys_clk / (freq_hz * 4096)) + 1;
    if (divider16 < 1) divider16 = 1;
    if (divider16 > 255) divider16 = 255;

    uint32_t wrap = (sys_clk / (divider16 * freq_hz)) - 1;
    if (wrap > 65535) wrap = 65535;

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, (float)divider16);
    pwm_config_set_wrap(&cfg, wrap);
    pwm_init(slice, &cfg, false);

    uint32_t level = (wrap * duty_pct) / 100;
    pwm_set_gpio_level(PIN_BUZZER, level);
    pwm_set_enabled(slice, true);

    // Light up onboard status LED
    gpio_put(PIN_ONBOARD_LED, 1);
    s_buzzer_playing = true;
}

void buzzer_stop() {
    if (!s_buzzer_initialized) return;

    uint slice = pwm_gpio_to_slice_num(PIN_BUZZER);
    pwm_set_enabled(slice, false);
    gpio_set_function(PIN_BUZZER, GPIO_FUNC_SIO);
    gpio_put(PIN_BUZZER, 0);

    // Extinguish onboard status LED
    gpio_put(PIN_ONBOARD_LED, 0);
    s_buzzer_playing = false;
}

void buzzer_beep_ms(uint16_t freq_hz, uint16_t duration_ms) {
    buzzer_tone(freq_hz, 50);
    delay(duration_ms);
    buzzer_stop();
}

// ─── Subsystem Lifecycle ──────────────────────────────────────────────────────

void adc_subsystem_init() {
    adc_init();

    // Configure external GPIO pins for analog mode
    adc_gpio_init(PIN_ADC0); // GP26
    adc_gpio_init(PIN_ADC1); // GP27
    adc_gpio_init(PIN_ADC2); // GP28
    adc_gpio_init(PIN_INTERNAL_VSYS_SENSE); // GP29

    // Enable internal silicon temperature sensor
    adc_set_temp_sensor_enabled(true);

    // Initialize audio driver
    buzzer_init();

    // Claim DMA channel for high-rate analog waveform captures
    if (s_dma_channel < 0) {
        s_dma_channel = dma_claim_unused_channel(false);
    }
}

// ─── Low-Level & DC Voltage Reading ───────────────────────────────────────────

uint16_t adc_read_raw_single(AdcChannelId ch) {
    if (ch > ADC_CH_TEMP) return 0;
    adc_select_input(s_channels[ch].adc_hw_index);
    return adc_read();
}

float adc_read_raw_averaged(AdcChannelId ch, uint16_t samples) {
    if (ch > ADC_CH_TEMP) return 0.0f;
    if (samples == 0) samples = 1;

    adc_select_input(s_channels[ch].adc_hw_index);

    uint32_t sum = 0;
    for (uint16_t i = 0; i < samples; i++) {
        sum += adc_read();
        busy_wait_us_32(2); // Short settling window
    }

    return (float)sum / (float)samples;
}

float adc_read_calibrated_voltage(AdcChannelId ch, uint16_t samples) {
    if (ch > ADC_CH_TEMP) return 0.0f;

    float raw_avg = adc_read_raw_averaged(ch, samples);
    if (raw_avg > ADC_MAX_RAW_VALUE) raw_avg = ADC_MAX_RAW_VALUE;

    // Direct uncalibrated voltage at ADC pin
    float v_pin = (raw_avg * ADC_VREF_VOLTS) / ADC_MAX_RAW_VALUE;

    // Apply two-point calibration correction
    const AdcChannelConfig &cfg = s_channels[ch];
    float v_corrected = (v_pin * cfg.cal_gain_scale) + cfg.cal_offset_volts;

    // Apply external front-end divider ratio
    return v_corrected * cfg.divider_ratio;
}

float adc_read_internal_temp_c() {
    // ADC Channel 4 measures RP2040 bandgap diode
    float v_temp = adc_read_calibrated_voltage(ADC_CH_TEMP, 32);
    // Standard RP2040 formula: T = 27 - (ADC_V - 0.706) / 0.001721
    float temp_c = 27.0f - ((v_temp - 0.706f) / 0.001721f);
    return temp_c;
}

// ─── Calibration Management ───────────────────────────────────────────────────

void adc_set_calibration(AdcChannelId ch, float offset_v, float gain) {
    if (ch <= ADC_CH_TEMP) {
        s_channels[ch].cal_offset_volts = offset_v;
        s_channels[ch].cal_gain_scale = gain;
    }
}

bool adc_calibrate_two_point(AdcChannelId ch, float v_ref_low, float v_meas_low, float v_ref_high, float v_meas_high) {
    if (ch > ADC_CH_TEMP) return false;
    float delta_meas = v_meas_high - v_meas_low;
    if (fabsf(delta_meas) < 0.001f) return false; // Prevent divide by zero

    float gain = (v_ref_high - v_ref_low) / delta_meas;
    float offset = v_ref_low - (gain * v_meas_low);

    s_channels[ch].cal_gain_scale = gain;
    s_channels[ch].cal_offset_volts = offset;
    return true;
}

void adc_reset_calibration(AdcChannelId ch) {
    if (ch <= ADC_CH_TEMP) {
        s_channels[ch].cal_offset_volts = 0.0f;
        s_channels[ch].cal_gain_scale = 1.0f;
    }
}

void adc_get_calibration(AdcChannelId ch, float &offset_v, float &gain) {
    if (ch <= ADC_CH_TEMP) {
        offset_v = s_channels[ch].cal_offset_volts;
        gain = s_channels[ch].cal_gain_scale;
    } else {
        offset_v = 0.0f;
        gain = 1.0f;
    }
}

AdcChannelConfig& adc_get_channel_config(AdcChannelId ch) {
    if (ch > ADC_CH_TEMP) return s_channels[0];
    return s_channels[ch];
}

// ─── Resistance & Continuity Engine ───────────────────────────────────────────

float adc_read_resistance(AdcChannelId ch, float r_known, float v_source) {
    // 16-sample oversampled voltage reading on divider node
    float v_meas = adc_read_calibrated_voltage(ch, 16);

    // Open circuit check: within 10mV of excitation voltage
    if (v_meas >= (v_source - 0.010f)) {
        return 1e7f; // 10 Megohms (effectively open circuit)
    }

    // Direct short check: under 0.5mV
    if (v_meas <= 0.0005f) {
        return 0.0f;
    }

    // Voltage divider calculation: Rx = Rknown * Vmeas / (Vsource - Vmeas)
    float rx = r_known * (v_meas / (v_source - v_meas));

    // Subtract probe lead zero offset
    if (rx > s_cont_cfg.probe_lead_ohms) {
        rx -= s_cont_cfg.probe_lead_ohms;
    } else {
        rx = 0.0f;
    }

    return rx;
}

void adc_continuity_init(const ContinuityConfig &cfg) {
    s_cont_cfg = cfg;
    s_cont_status.state = CONT_STATE_OPEN;
    s_cont_status.measured_ohms = 1e6f;
    s_cont_status.is_connected = false;
    s_cont_status.buzzer_active = false;
    s_cont_status.transition_count = 0;
    s_cont_status.last_state_change_ms = millis();
    buzzer_stop();
}

ContinuityStatus adc_continuity_update(AdcChannelId ch) {
    uint32_t now = millis();

    // Measure resistance on selected channel (defaults to CH0)
    float r = adc_read_resistance(ch, s_cont_cfg.r_known_ohms, ADC_VREF_VOLTS);
    s_cont_status.measured_ohms = r;

    bool prev_connected = s_cont_status.is_connected;

    // State machine with hysteresis:
    // Trip ON:  R < threshold
    // Trip OFF: R > (threshold + hysteresis)
    if (!prev_connected) {
        if (r < s_cont_cfg.threshold_ohms) {
            s_cont_status.is_connected = true;
            s_cont_status.state = CONT_STATE_CONNECTED;
            s_cont_status.transition_count++;
            s_cont_status.last_state_change_ms = now;

            if (s_cont_cfg.buzzer_enabled) {
                buzzer_tone(s_cont_cfg.buzzer_freq_hz, 50);
                s_cont_status.buzzer_active = true;
            }
        }
    } else {
        if (r > (s_cont_cfg.threshold_ohms + s_cont_cfg.hysteresis_ohms)) {
            s_cont_status.is_connected = false;
            s_cont_status.state = CONT_STATE_OPEN;
            s_cont_status.transition_count++;
            s_cont_status.last_state_change_ms = now;

            buzzer_stop();
            s_cont_status.buzzer_active = false;
        }
    }

    return s_cont_status;
}

void adc_continuity_set_threshold(float thresh_ohms, float hyst_ohms) {
    s_cont_cfg.threshold_ohms = thresh_ohms;
    s_cont_cfg.hysteresis_ohms = hyst_ohms;
}

void adc_continuity_set_buzzer_mute(bool mute) {
    s_cont_cfg.buzzer_enabled = !mute;
    if (mute && s_cont_status.buzzer_active) {
        buzzer_stop();
        s_cont_status.buzzer_active = false;
    }
}

ContinuityConfig& adc_continuity_get_config() {
    return s_cont_cfg;
}

const ContinuityStatus& adc_continuity_get_last_status() {
    return s_cont_status;
}

// ─── Waveform DMA Engine ──────────────────────────────────────────────────────

bool adc_waveform_start(const AdcWaveformConfig &cfg) {
    if (cfg.channel > ADC_CH_TEMP) return false;
    if (s_dma_running) return false;

    uint16_t count = cfg.sample_count;
    if (count == 0) count = 256;
    if (count > ADC_WAVEFORM_MAX_SAMPLES) count = ADC_WAVEFORM_MAX_SAMPLES;
    s_dma_sample_count = count;

    if (s_dma_channel < 0) {
        s_dma_channel = dma_claim_unused_channel(false);
        if (s_dma_channel < 0) return false;
    }

    // Configure ADC FIFO and clock divider
    adc_select_input(s_channels[cfg.channel].adc_hw_index);
    adc_fifo_setup(
        true,   // Write each conversion to FIFO
        true,   // Enable DMA data request (DREQ)
        1,      // DREQ asserted when FIFO contains at least 1 sample
        false,  // Omit error bit
        false   // Keep full 12-bit precision (halfword aligned)
    );

    // ADC Clock Rate: 48 MHz base / (1 + clkdiv) = sample rate
    uint32_t rate = cfg.sample_rate_hz;
    if (rate < 1000) rate = 1000;
    if (rate > 500000) rate = 500000; // 500 kSPS maximum for RP2040 SAR ADC

    float clkdiv = (48000000.0f / (float)rate) - 1.0f;
    if (clkdiv < 0.0f) clkdiv = 0.0f;
    adc_set_clkdiv(clkdiv);

    // Drain stale FIFO contents
    adc_fifo_drain();

    // Configure DMA channel
    dma_channel_config c = dma_channel_get_default_config(s_dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, false); // Read from fixed FIFO register
    channel_config_set_write_increment(&c, true);  // Increment target RAM buffer
    channel_config_set_dreq(&c, DREQ_ADC);

    dma_channel_configure(
        s_dma_channel,
        &c,
        s_dma_buffer,       // Target RAM buffer
        &adc_hw->fifo,      // Read from ADC FIFO
        s_dma_sample_count, // Total 16-bit transfers
        true                // Start immediately
    );

    s_dma_running = true;
    s_dma_start_time_us = micros();

    // Start free-running ADC hardware conversions
    adc_run(true);
    return true;
}

bool adc_waveform_is_busy() {
    if (!s_dma_running) return false;
    if (dma_channel_is_busy(s_dma_channel)) {
        return true;
    }

    // DMA completed
    adc_run(false);
    adc_fifo_drain();
    s_dma_running = false;
    return false;
}

bool adc_waveform_wait_completion(uint32_t timeout_ms) {
    uint32_t start_ms = millis();
    while (adc_waveform_is_busy()) {
        if ((millis() - start_ms) > timeout_ms) {
            dma_channel_abort(s_dma_channel);
            adc_run(false);
            adc_fifo_drain();
            s_dma_running = false;
            return false;
        }
        delayMicroseconds(10);
    }
    return true;
}

const uint16_t* adc_waveform_get_buffer() {
    return s_dma_buffer;
}

uint16_t adc_waveform_get_sample_count() {
    return s_dma_sample_count;
}

AdcWaveformStats adc_waveform_compute_stats(const AdcWaveformConfig &cfg) {
    AdcWaveformStats stats;
    stats.v_min = 1e6f;
    stats.v_max = -1e6f;
    stats.v_pp = 0.0f;
    stats.v_avg = 0.0f;
    stats.v_rms = 0.0f;
    stats.estimated_freq_hz = 0.0f;
    stats.triggered = false;
    stats.trigger_idx = 0;

    if (s_dma_sample_count == 0) return stats;

    const AdcChannelConfig &ch_cfg = s_channels[cfg.channel];
    float sum_v = 0.0f;
    float sum_v_sq = 0.0f;

    // Voltage array conversion & min/max detection
    for (uint16_t i = 0; i < s_dma_sample_count; i++) {
        uint16_t raw = s_dma_buffer[i] & 0x0FFF;
        float v_raw = (raw * ADC_VREF_VOLTS) / ADC_MAX_RAW_VALUE;
        float v = ((v_raw * ch_cfg.cal_gain_scale) + ch_cfg.cal_offset_volts) * ch_cfg.divider_ratio;

        if (v < stats.v_min) stats.v_min = v;
        if (v > stats.v_max) stats.v_max = v;

        sum_v += v;
        sum_v_sq += (v * v);
    }

    stats.v_pp = stats.v_max - stats.v_min;
    stats.v_avg = sum_v / (float)s_dma_sample_count;
    stats.v_rms = sqrtf(sum_v_sq / (float)s_dma_sample_count);

    // Analog Trigger Detection (Rising / Falling with Hysteresis)
    if (cfg.trigger_slope != TRIG_SLOPE_NONE) {
        float thresh = cfg.trigger_level_v;
        float hyst = cfg.trigger_hysteresis_v;
        if (hyst <= 0.0f) hyst = 0.05f; // 50 mV default hysteresis

        bool armed = false;
        for (uint16_t i = 1; i < s_dma_sample_count; i++) {
            float v_prev = (((s_dma_buffer[i-1] & 0x0FFF) * ADC_VREF_VOLTS) / ADC_MAX_RAW_VALUE) * ch_cfg.divider_ratio;
            float v_curr = (((s_dma_buffer[i] & 0x0FFF) * ADC_VREF_VOLTS) / ADC_MAX_RAW_VALUE) * ch_cfg.divider_ratio;

            if (cfg.trigger_slope == TRIG_SLOPE_RISING) {
                if (v_prev < (thresh - hyst)) armed = true;
                if (armed && v_prev <= thresh && v_curr >= thresh) {
                    stats.triggered = true;
                    stats.trigger_idx = i;
                    break;
                }
            } else if (cfg.trigger_slope == TRIG_SLOPE_FALLING) {
                if (v_prev > (thresh + hyst)) armed = true;
                if (armed && v_prev >= thresh && v_curr <= thresh) {
                    stats.triggered = true;
                    stats.trigger_idx = i;
                    break;
                }
            }
        }
    }

    // Frequency Estimation via Midpoint Zero-Crossing Interval
    float v_mid = (stats.v_min + stats.v_max) * 0.5f;
    uint16_t first_cross = 0xFFFF;
    uint16_t last_cross = 0xFFFF;
    uint16_t cross_count = 0;

    for (uint16_t i = 1; i < s_dma_sample_count; i++) {
        float v0 = (((s_dma_buffer[i-1] & 0x0FFF) * ADC_VREF_VOLTS) / ADC_MAX_RAW_VALUE) * ch_cfg.divider_ratio;
        float v1 = (((s_dma_buffer[i] & 0x0FFF) * ADC_VREF_VOLTS) / ADC_MAX_RAW_VALUE) * ch_cfg.divider_ratio;

        if (v0 <= v_mid && v1 > v_mid) { // Rising mid-crossing
            if (first_cross == 0xFFFF) first_cross = i;
            last_cross = i;
            cross_count++;
        }
    }

    if (cross_count > 1 && cfg.sample_rate_hz > 0) {
        float sample_period = 1.0f / (float)cfg.sample_rate_hz;
        float total_time = (last_cross - first_cross) * sample_period;
        float avg_period = total_time / (float)(cross_count - 1);
        if (avg_period > 0.0f) {
            stats.estimated_freq_hz = 1.0f / avg_period;
        }
    }

    return stats;
}

// ─── Current & Generic Sensor Conversion ──────────────────────────────────────

float adc_read_current_amps(AdcChannelId ch, float shunt_ohms, float v_zero) {
    if (shunt_ohms <= 0.0f) return 0.0f;
    float v = adc_read_calibrated_voltage(ch, 32);
    return (v - v_zero) / shunt_ohms;
}

float adc_read_linear_sensor(AdcChannelId ch, float v_min, float v_max, float out_min, float out_max) {
    float v = adc_read_calibrated_voltage(ch, 32);
    if (v < v_min) v = v_min;
    if (v > v_max) v = v_max;
    float span_v = v_max - v_min;
    if (span_v <= 0.0f) return out_min;
    float ratio = (v - v_min) / span_v;
    return out_min + (ratio * (out_max - out_min));
}

float adc_read_ntc_temperature(AdcChannelId ch, float r_balance, float beta, float r0, float t0_c) {
    float v = adc_read_calibrated_voltage(ch, 32);
    if (v <= 0.001f || v >= (ADC_VREF_VOLTS - 0.001f)) return -999.0f; // Sensor fault

    // NTC in lower leg: V = Vcc * R_ntc / (R_balance + R_ntc) => R_ntc = R_balance * V / (Vcc - V)
    float r_ntc = r_balance * (v / (ADC_VREF_VOLTS - v));

    // Steinhart-Hart / Beta equation: 1/T = 1/T0 + (1/B) * ln(R/R0)
    float t0_k = t0_c + 273.15f;
    float inv_t = (1.0f / t0_k) + ((1.0f / beta) * logf(r_ntc / r0));
    float t_c = (1.0f / inv_t) - 273.15f;
    return t_c;
}
