#ifndef ADC_SUBSYSTEM_H
#define ADC_SUBSYSTEM_H

#include <Arduino.h>
#include <stdint.h>
#include "hardware_config.h"

// ─── ADC Channel Identifiers ──────────────────────────────────────────────────
typedef enum {
    ADC_CH_0 = 0,    // GP26 (External Pin, Header P2.11 / Pico Pin 31)
    ADC_CH_1 = 1,    // GP27 (External Pin, Header P2.12 / Pico Pin 32)
    ADC_CH_2 = 2,    // GP28 (External Pin, Header P2.14 / Pico Pin 34)
    ADC_CH_VSYS = 3, // GP29 (Internal VSYS Monitor, 1/3 Voltage Divider)
    ADC_CH_TEMP = 4  // Internal RP2040 Core Temperature Sensor
} AdcChannelId;

// ─── Measurement Modes ────────────────────────────────────────────────────────
typedef enum {
    ADC_MODE_VOLT = 0,
    ADC_MODE_RAW,
    ADC_MODE_RESISTANCE,
    ADC_MODE_CONTINUITY,
    ADC_MODE_WAVEFORM,
    ADC_MODE_CURRENT,
    ADC_MODE_SENSOR
} AdcMeasurementMode;

// ─── Continuity States ────────────────────────────────────────────────────────
typedef enum {
    CONT_STATE_OPEN = 0,
    CONT_STATE_CONNECTED,
    CONT_STATE_INTERMITTENT
} ContinuityState;

// ─── Trigger Slope ────────────────────────────────────────────────────────────
typedef enum {
    TRIG_SLOPE_NONE = 0,
    TRIG_SLOPE_RISING,
    TRIG_SLOPE_FALLING
} TriggerSlope;

// ─── Per-Channel Calibration & Front-End Scaling ──────────────────────────────
struct AdcChannelConfig {
    uint8_t  gpio;               // RP2040 GPIO (26, 27, 28, 29, or 0xFF for temp)
    uint8_t  adc_hw_index;       // Hardware mux index 0..4
    float    divider_ratio;      // Attenuation scaling factor (nominal 1.0f)
    float    cal_offset_volts;   // Additive offset correction
    float    cal_gain_scale;     // Multiplicative gain correction (nominal 1.0f)
    uint16_t averaging_samples;  // Default oversampling window
};

// ─── Continuity Tester Configuration ──────────────────────────────────────────
struct ContinuityConfig {
    float    threshold_ohms;     // Upper resistance threshold for continuous path (< 25 ohms)
    float    hysteresis_ohms;    // Release hysteresis offset (+ 10 ohms)
    float    r_known_ohms;       // Reference excitation resistor (e.g. 1000 ohms)
    float    probe_lead_ohms;    // Tare/zero offset for probe lead resistance
    bool     buzzer_enabled;     // Enable acoustic feedback
    uint16_t buzzer_freq_hz;     // Piezo resonant tone frequency
    uint16_t sample_interval_ms; // Background non-blocking poll period (e.g. 2 ms)
};

// ─── Continuity Live Telemetry Status ─────────────────────────────────────────
struct ContinuityStatus {
    ContinuityState state;
    float           measured_ohms;
    bool            is_connected;
    bool            buzzer_active;
    uint32_t        transition_count;
    uint32_t        last_state_change_ms;
};

// ─── Waveform Acquisition Parameters ──────────────────────────────────────────
struct AdcWaveformConfig {
    AdcChannelId channel;
    uint32_t     sample_rate_hz;     // 1000 Hz to 500,000 Hz
    uint16_t     sample_count;       // 64 to 2048 samples
    float        trigger_level_v;    // Analog trigger threshold in Volts
    TriggerSlope trigger_slope;      // Rising or Falling edge
    float        trigger_hysteresis_v;
    uint8_t      pre_trigger_pct;    // e.g. 70% pre-trigger
};

// ─── Waveform Computed Metrics ────────────────────────────────────────────────
struct AdcWaveformStats {
    float    v_min;
    float    v_max;
    float    v_pp;
    float    v_avg;
    float    v_rms;
    float    estimated_freq_hz;
    bool     triggered;
    uint16_t trigger_idx;
};

// ─── API Function Prototypes ──────────────────────────────────────────────────

// Subsystem Lifecycle
void adc_subsystem_init();

// Voltage & Raw Acquisition
uint16_t adc_read_raw_single(AdcChannelId ch);
float    adc_read_raw_averaged(AdcChannelId ch, uint16_t samples = 16);
float    adc_read_calibrated_voltage(AdcChannelId ch, uint16_t samples = 16);
float    adc_read_internal_temp_c();

// Calibration Management
void adc_set_calibration(AdcChannelId ch, float offset_v, float gain);
bool adc_calibrate_two_point(AdcChannelId ch, float v_ref_low, float v_meas_low, float v_ref_high, float v_meas_high);
void adc_reset_calibration(AdcChannelId ch);
void adc_get_calibration(AdcChannelId ch, float &offset_v, float &gain);
AdcChannelConfig& adc_get_channel_config(AdcChannelId ch);

// Resistance & Continuity Engine
float            adc_read_resistance(AdcChannelId ch, float r_known = CONTINUITY_R_REF_OHMS, float v_source = 3.3f);
void             adc_continuity_init(const ContinuityConfig &cfg);
ContinuityStatus adc_continuity_update(AdcChannelId ch = ADC_CH_0);
void             adc_continuity_set_threshold(float thresh_ohms, float hyst_ohms);
void             adc_continuity_set_buzzer_mute(bool mute);
ContinuityConfig& adc_continuity_get_config();
const ContinuityStatus& adc_continuity_get_last_status();

// Waveform DMA Engine
bool             adc_waveform_start(const AdcWaveformConfig &cfg);
bool             adc_waveform_is_busy();
bool             adc_waveform_wait_completion(uint32_t timeout_ms = 500);
const uint16_t*  adc_waveform_get_buffer();
uint16_t         adc_waveform_get_sample_count();
AdcWaveformStats adc_waveform_compute_stats(const AdcWaveformConfig &cfg);

// Sensor & Current Measurement
float adc_read_current_amps(AdcChannelId ch, float shunt_ohms, float v_zero = 0.0f);
float adc_read_linear_sensor(AdcChannelId ch, float v_min, float v_max, float out_min, float out_max);
float adc_read_ntc_temperature(AdcChannelId ch, float r_balance, float beta, float r0 = 10000.0f, float t0_c = 25.0f);

// Dedicated Buzzer Driver
void buzzer_init();
void buzzer_tone(uint16_t freq_hz, uint8_t duty_pct = 50);
void buzzer_stop();
void buzzer_beep_ms(uint16_t freq_hz, uint16_t duration_ms);

#endif // ADC_SUBSYSTEM_H
