#include "meter_hal.h"
#include "adc_subsystem.h"
#include "instrument_mode_manager.h"
#include <Arduino.h>

static MeterHalConfig s_meter_cfg = {
    0,                               // Active channel (CH0 / GP26)
    CONTINUITY_R_REF_OHMS,           // Reference excitation resistor (1000.0 ohms)
    CONTINUITY_DEFAULT_THRESH_OHMS,  // Threshold (25.0 ohms)
    CONTINUITY_DEFAULT_HYST_OHMS,    // Hysteresis (10.0 ohms)
    true,                            // Buzzer enabled
    false                            // Inactive initially
};

void meter_hal_init(void) {
    adc_subsystem_init();
    buzzer_init();
    s_meter_cfg.active_channel = 0;
    s_meter_cfg.r_ref_ohms = CONTINUITY_R_REF_OHMS;
    s_meter_cfg.cont_thresh_ohms = CONTINUITY_DEFAULT_THRESH_OHMS;
    s_meter_cfg.cont_hyst_ohms = CONTINUITY_DEFAULT_HYST_OHMS;
    s_meter_cfg.buzzer_enabled = true;
    s_meter_cfg.active = false;
}

bool meter_hal_acquire(void) {
    adc_subsystem_init();
    buzzer_init();
    s_meter_cfg.active = true;
    return true;
}

void meter_hal_release(void) {
    buzzer_stop();
    s_meter_cfg.active = false;
}

void meter_hal_get_config(MeterHalConfig* out_cfg) {
    if (out_cfg) {
        *out_cfg = s_meter_cfg;
    }
}

void meter_hal_read_all_voltages(uint16_t samples, MeterVoltageReport* out_rep) {
    if (!out_rep) return;
    if (samples == 0) samples = 16;
    if (samples > 256) samples = 256;

    out_rep->v0 = adc_read_calibrated_voltage(ADC_CH_0, samples);
    out_rep->v1 = adc_read_calibrated_voltage(ADC_CH_1, samples);
    out_rep->v2 = adc_read_calibrated_voltage(ADC_CH_2, samples);
    out_rep->vsys = adc_read_calibrated_voltage(ADC_CH_VSYS, samples);
    out_rep->core_temp_c = adc_read_internal_temp_c();

    out_rep->raw0 = adc_read_raw_single(ADC_CH_0);
    out_rep->raw1 = adc_read_raw_single(ADC_CH_1);
    out_rep->raw2 = adc_read_raw_single(ADC_CH_2);
}

float meter_hal_read_channel(uint8_t ch, uint16_t samples) {
    if (samples == 0) samples = 16;
    if (samples > 256) samples = 256;

    if (ch == 4) {
        return adc_read_internal_temp_c();
    }
    if (ch > 3) ch = 0;
    return adc_read_calibrated_voltage((AdcChannelId)ch, samples);
}

float meter_hal_read_resistance(uint8_t ch, float r_ref) {
    if (ch > 2) ch = 0;
    if (r_ref <= 0.0f) r_ref = CONTINUITY_R_REF_OHMS;
    return adc_read_resistance((AdcChannelId)ch, r_ref, ADC_VREF_VOLTS);
}

void meter_hal_run_continuity(Stream& host, uint8_t ch) {
    if (ch > 2) ch = 0;

    host.printf("\r\n=== Real-Time Continuity Monitor (CH%u / GP%u) ===\r\n",
        (unsigned int)ch, (unsigned int)(26 + ch));
    host.printf("  Threshold: < %.1f ohms (Beep) | Release: > %.1f ohms\r\n",
        s_meter_cfg.cont_thresh_ohms,
        s_meter_cfg.cont_thresh_ohms + s_meter_cfg.cont_hyst_ohms);
    host.printf("  Excitation: Pull-up via %.1f ohms to 3.3V\r\n", s_meter_cfg.r_ref_ohms);
    host.println("  Press ANY KEY to stop continuity monitor.\r\n");

    // Flush any pending input characters
    while (host.available()) host.read();

    ContinuityState prev_state = CONT_STATE_OPEN;
    uint32_t last_print = 0;

    while (!host.available()) {
        ContinuityStatus st = adc_continuity_update((AdcChannelId)ch);
        uint32_t now = millis();

        if (st.state != prev_state || (now - last_print > 500)) {
            prev_state = st.state;
            last_print = now;

            if (st.is_connected) {
                if (st.measured_ohms < 1.0f) {
                    host.printf("  [CONTINUOUS] R = %.2f ohms (SHORT) [BEEP]\r\n", st.measured_ohms);
                } else {
                    host.printf("  [CONTINUOUS] R = %.1f ohms [BEEP]\r\n", st.measured_ohms);
                }
            } else {
                if (st.measured_ohms >= 1e5f) {
                    host.println("  [OPEN CIRCUIT] R > 100 k-ohms");
                } else {
                    host.printf("  [OPEN CIRCUIT] R = %.1f ohms\r\n", st.measured_ohms);
                }
            }
        }
        delay(2);
    }

    // Clear keypress that triggered exit
    while (host.available()) host.read();
    buzzer_stop();
    host.println("\r\nContinuity monitor stopped.\r\n");
}

void meter_hal_render_scope(Stream& host, uint8_t ch, uint32_t sample_rate_hz) {
    if (ch > 2) ch = 0;
    if (sample_rate_hz < 1000) sample_rate_hz = 50000;
    if (sample_rate_hz > 500000) sample_rate_hz = 500000;

    AdcWaveformConfig wcfg;
    wcfg.channel = (AdcChannelId)ch;
    wcfg.sample_rate_hz = sample_rate_hz;
    wcfg.sample_count = 512;
    wcfg.trigger_level_v = 1.65f;
    wcfg.trigger_slope = TRIG_SLOPE_RISING;
    wcfg.trigger_hysteresis_v = 0.05f;
    wcfg.pre_trigger_pct = 15;

    host.printf("\r\n=== ADC Mini-Scope Capture (CH%u / GP%u @ %lu Hz) ===\r\n",
        (unsigned int)ch, (unsigned int)(26 + ch), (unsigned long)sample_rate_hz);

    if (!adc_waveform_start(wcfg)) {
        host.println("Error: Failed to initiate ADC DMA waveform acquisition.");
        return;
    }

    if (!adc_waveform_wait_completion(500)) {
        host.println("Warning: Waveform capture timed out (Trigger condition not met).");
    }

    AdcWaveformStats stats = adc_waveform_compute_stats(wcfg);
    const uint16_t* buf = adc_waveform_get_buffer();

    host.printf("  Vmin: %5.3f V | Vmax: %5.3f V | Vpp: %5.3f V | Vavg: %5.3f V | Vrms: %5.3f V\r\n",
        stats.v_min, stats.v_max, stats.v_pp, stats.v_avg, stats.v_rms);
    if (stats.estimated_freq_hz > 0.0f) {
        host.printf("  Est. Frequency: %.1f Hz\r\n", stats.estimated_freq_hz);
    }
    host.println("-------------------------------------------------------------------------");

    // 12-row by 64-column ASCII waveform plot
    const int ROWS = 12;
    const int COLS = 64;
    float v_span = stats.v_max - stats.v_min;
    if (v_span < 0.05f) v_span = 0.05f; // Prevent divide by zero

    for (int r = ROWS - 1; r >= 0; r--) {
        float row_v = stats.v_min + (v_span * (float)r / (float)(ROWS - 1));
        host.printf(" %5.2fV |", row_v);

        for (int c = 0; c < COLS; c++) {
            int s_idx = (c * (wcfg.sample_count - 1)) / (COLS - 1);
            float s_v = (((buf[s_idx] & 0x0FFF) * ADC_VREF_VOLTS) / ADC_MAX_RAW_VALUE);

            // Determine if sample belongs to this row
            int target_row = (int)(((s_v - stats.v_min) / v_span) * (float)(ROWS - 1) + 0.5f);
            if (target_row == r) {
                host.print("*");
            } else if (r == 0) {
                host.print("_");
            } else {
                host.print(" ");
            }
        }
        host.println("|");
    }

    host.println("        +----------------------------------------------------------------+");
    uint32_t total_time_us = (uint32_t)(((float)wcfg.sample_count / (float)sample_rate_hz) * 1e6f);
    host.printf("        0 us                                                           %lu us\r\n\r\n",
        (unsigned long)total_time_us);
}

void meter_hal_set_continuity_threshold(float thresh_ohms, float hyst_ohms) {
    if (thresh_ohms <= 0.0f) thresh_ohms = CONTINUITY_DEFAULT_THRESH_OHMS;
    if (hyst_ohms <= 0.0f) hyst_ohms = CONTINUITY_DEFAULT_HYST_OHMS;
    s_meter_cfg.cont_thresh_ohms = thresh_ohms;
    s_meter_cfg.cont_hyst_ohms = hyst_ohms;
    adc_continuity_set_threshold(thresh_ohms, hyst_ohms);
}

void meter_hal_set_buzzer(bool enable) {
    s_meter_cfg.buzzer_enabled = enable;
    adc_continuity_set_buzzer_mute(!enable);
}

void meter_hal_beep(uint16_t freq_hz, uint16_t duration_ms) {
    if (freq_hz == 0) freq_hz = CONTINUITY_BUZZER_FREQ_HZ;
    if (duration_ms == 0) duration_ms = 100;
    buzzer_beep_ms(freq_hz, duration_ms);
}
