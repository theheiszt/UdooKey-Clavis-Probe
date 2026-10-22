#ifndef UDOO_METER_HAL_H
#define UDOO_METER_HAL_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── UDOO KEY ADC & Meter Physical Pinout ────────────────────────────────────
// Header P2 Pins (40-pin Pico Header):
// ADC0 : GP26 (Header P2 Pin 31) -> Voltmeter Channel 0 / LDR
// ADC1 : GP27 (Header P2 Pin 32) -> Voltmeter Channel 1
// ADC2 : GP28 (Header P2 Pin 34) -> Voltmeter Channel 2
// VSYS : GP29 (Internal 1/3 divider) -> Power Rail Monitor
// TEMP : Sensor 4 (Internal Core Die Temperature)
// BUZZER : GP20 (Audio Feedback / Continuity)

#define UDOO_METER_ADC0_PIN      26
#define UDOO_METER_ADC1_PIN      27
#define UDOO_METER_ADC2_PIN      28
#define UDOO_METER_VSYS_PIN      29
#define UDOO_METER_BUZZER_PIN    20

typedef struct {
    float    v0;            // Voltage on ADC0 (GP26)
    float    v1;            // Voltage on ADC1 (GP27)
    float    v2;            // Voltage on ADC2 (GP28)
    float    vsys;          // Voltage on VSYS rail (GP29 * 3)
    float    core_temp_c;   // RP2040 Core Temperature (deg C)
    uint16_t raw0;          // Raw 12-bit SAR code for ADC0 (0..4095)
    uint16_t raw1;          // Raw 12-bit SAR code for ADC1 (0..4095)
    uint16_t raw2;          // Raw 12-bit SAR code for ADC2 (0..4095)
} MeterVoltageReport;

typedef struct {
    uint8_t  active_channel;    // 0, 1, or 2 (GP26, GP27, GP28)
    float    r_ref_ohms;        // Reference resistor (default 1000.0 ohms)
    float    cont_thresh_ohms;  // Continuity trigger threshold (default 25.0 ohms)
    float    cont_hyst_ohms;    // Continuity hysteresis release (default 10.0 ohms)
    bool     buzzer_enabled;    // Audible feedback enabled
    bool     active;            // Meter mode active
} MeterHalConfig;

/**
 * @brief Initialize Meter subsystem HAL.
 */
void meter_hal_init(void);

/**
 * @brief Acquire ADC and buzzer peripherals for Meter mode.
 */
bool meter_hal_acquire(void);

/**
 * @brief Release Meter mode and restore pins to safe idle.
 */
void meter_hal_release(void);

/**
 * @brief Get copy of active Meter configuration.
 */
void meter_hal_get_config(MeterHalConfig* out_cfg);

/**
 * @brief Read voltages across all channels with oversampling.
 * @param samples Number of oversampled measurements (1..256, default 16)
 * @param out_rep Pointer to receive voltage report
 */
void meter_hal_read_all_voltages(uint16_t samples, MeterVoltageReport* out_rep);

/**
 * @brief Read voltage from a single specific channel (0..4).
 * @param ch Channel: 0=ADC0, 1=ADC1, 2=ADC2, 3=VSYS, 4=TEMP
 * @param samples Oversampling count
 * @return Calibrated voltage in Volts (or deg C for TEMP)
 */
float meter_hal_read_channel(uint8_t ch, uint16_t samples);

/**
 * @brief Measure resistance on specified ADC channel.
 * @param ch ADC channel (0..2)
 * @param r_ref Reference excitation pull-up resistor in ohms (default 1000.0)
 * @return Measured resistance in ohms (or 1e6 for open circuit)
 */
float meter_hal_read_resistance(uint8_t ch, float r_ref);

/**
 * @brief Run interactive continuous continuity test loop with acoustic feedback.
 * Loops until character is received on host stream.
 * @param host Terminal stream for output and exit keypress detection
 * @param ch ADC channel (0..2, default 0)
 */
void meter_hal_run_continuity(Stream& host, uint8_t ch);

/**
 * @brief Capture high-speed analog waveform via ADC DMA and render ASCII scope trace.
 * @param host Terminal stream for plot output
 * @param ch ADC channel (0..2)
 * @param sample_rate_hz Sample rate in Hz (1000 to 500000)
 */
void meter_hal_render_scope(Stream& host, uint8_t ch, uint32_t sample_rate_hz);

/**
 * @brief Set continuity threshold and hysteresis.
 */
void meter_hal_set_continuity_threshold(float thresh_ohms, float hyst_ohms);

/**
 * @brief Enable or disable audible buzzer during continuity testing.
 */
void meter_hal_set_buzzer(bool enable);

/**
 * @brief Emit test tone on buzzer.
 */
void meter_hal_beep(uint16_t freq_hz, uint16_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif // UDOO_METER_HAL_H
