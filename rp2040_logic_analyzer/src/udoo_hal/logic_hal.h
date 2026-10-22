#ifndef UDOO_LOGIC_HAL_H
#define UDOO_LOGIC_HAL_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>
#include "trigger_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─── UDOO KEY Logic Analyzer Physical Pinout ─────────────────────────────────
// Header P1 Pins (GP2..GP9):
// CH0 : GP2 (Header P1 Pin 4)
// CH1 : GP3 (Header P1 Pin 5)
// CH2 : GP4 (Header P1 Pin 6)
// CH3 : GP5 (Header P1 Pin 7)
// CH4 : GP6 (Header P1 Pin 8)
// CH5 : GP7 (Header P1 Pin 9)
// CH6 : GP8 (Header P1 Pin 10)
// CH7 : GP9 (Header P1 Pin 11)
// EXT_TRIG : GP10 (Dedicated External Trigger Input)

#define UDOO_LOGIC_CH_COUNT       8
#define UDOO_LOGIC_PIN_BASE       2
#define UDOO_LOGIC_DEFAULT_RATE   25000000 // 25 MSPS default
#define UDOO_LOGIC_MAX_RATE       100000000 // 100 MSPS maximum
#define UDOO_LOGIC_DEFAULT_DEPTH  4096     // 4,096 samples default
#define UDOO_LOGIC_MAX_DEPTH      65536    // 64 KiB buffer maximum

typedef struct {
    uint32_t       sample_rate_hz;    // Configured sample rate (10k to 100M)
    uint32_t       actual_rate_hz;    // Hardware clock rate from RP2040 PLL
    uint32_t       sample_count;      // Capture depth (1024 to 65536)
    uint8_t        enabled_channels;  // Bitmask of active channels (default 0xFF)
    AdvTriggerMode trigger_mode;      // Auto, Edge Rising/Falling/Any, Level, Pattern, Width
    uint8_t        trigger_channel;   // 0..7 or 10 (EXT_TRIG)
    uint32_t       trigger_param;     // Pulse duration (ns) / param
    uint8_t        pattern_mask;      // Mask for pattern match
    uint8_t        pattern_val;       // Value for pattern match
    uint8_t        pre_trigger_pct;   // Pre-trigger capture percent (default 70%)
    bool           active;            // Logic analyzer mode active
    bool           armed;             // Capture currently running in background
    bool           complete;          // Capture finished and frozen
    uint32_t       trigger_offset;    // Sample index where trigger event occurred
} LogicHalConfig;

typedef struct {
    uint32_t high_samples;            // Number of samples at logic HIGH
    uint32_t low_samples;             // Number of samples at logic LOW
    uint32_t transitions;             // Total number of 0->1 and 1->0 edges
    float    duty_pct;                // Duty cycle percentage (0.0 to 100.0)
    float    est_freq_hz;             // Estimated frequency in Hz (0 if <2 edges)
} LogicChannelStats;

/**
 * @brief Initialize Logic Analyzer HAL data structures.
 */
void logic_hal_init(void);

/**
 * @brief Acquire GP2..GP9 for PIO0 SM0 logic capture.
 */
bool logic_hal_acquire(void);

/**
 * @brief Release logic analyzer and restore GP2..GP9 to Safe High-Z.
 */
void logic_hal_release(void);

/**
 * @brief Get copy of active Logic Analyzer configuration.
 */
void logic_hal_get_config(LogicHalConfig* out_cfg);

/**
 * @brief Configure sample rate (10 kHz to 100 MHz).
 */
bool logic_hal_set_samplerate(uint32_t sample_rate_hz);

/**
 * @brief Configure capture depth in samples (1024, 2048, 4096, 8192, 16384, 32768, 65536).
 */
bool logic_hal_set_sample_count(uint32_t count);

/**
 * @brief Configure edge trigger on target channel (0..7 or 10 for EXT_TRIG).
 * @param edge_type TRIG_MODE_EDGE_RISING, TRIG_MODE_EDGE_FALLING, or TRIG_MODE_EDGE_ANY
 */
bool logic_hal_set_trigger_edge(uint8_t ch, AdvTriggerMode edge_type);

/**
 * @brief Configure level trigger on target channel (0..7).
 */
bool logic_hal_set_trigger_level(uint8_t ch, bool high);

/**
 * @brief Configure 8-bit bus pattern trigger.
 */
bool logic_hal_set_trigger_pattern(uint8_t mask, uint8_t val);

/**
 * @brief Configure pulse width / glitch / runt pulse trigger.
 * @param less_than true for pulse < dur_ns (glitch/runt), false for pulse > dur_ns
 */
bool logic_hal_set_trigger_width(uint8_t ch, bool less_than, uint32_t dur_ns);

/**
 * @brief Set trigger to Immediate / Auto free run.
 */
bool logic_hal_set_trigger_auto(void);

/**
 * @brief Set pre-trigger buffer percentage (0 to 100%, default 70%).
 */
bool logic_hal_set_pre_trigger_pct(uint8_t pct);

/**
 * @brief Arm PIO0 SM0 DMA acquisition engine.
 */
bool logic_hal_arm(void);

/**
 * @brief Service background capture state machine.
 * @return true if capture is complete
 */
bool logic_hal_poll(void);

/**
 * @brief Manually abort running acquisition.
 */
void logic_hal_abort(void);

/**
 * @brief Check if acquisition is currently armed.
 */
bool logic_hal_is_armed(void);

/**
 * @brief Check if trigger condition has fired.
 */
bool logic_hal_is_triggered(void);

/**
 * @brief Check if capture has finished and aligned buffer is ready.
 */
bool logic_hal_is_complete(void);

/**
 * @brief Get pointer to chronologically aligned capture buffer.
 * @param out_count Pointer to receive sample count
 * @param out_trigger_idx Pointer to receive trigger sample index
 */
const uint8_t* logic_hal_get_samples(uint32_t* out_count, uint32_t* out_trigger_idx);

/**
 * @brief Calculate digital signal metrics for single channel.
 */
void logic_hal_compute_channel_stats(uint8_t ch, LogicChannelStats* out_stats);

/**
 * @brief Print status and hardware configuration table to output stream.
 */
void logic_hal_print_status(Stream& out);

/**
 * @brief Render horizontal multi-channel ASCII timing diagram in terminal.
 * @param out Terminal stream
 * @param window_start Start sample index
 * @param window_len Number of sample columns to render (default 64)
 */
void logic_hal_render_ascii_diagram(Stream& out, uint32_t window_start, uint32_t window_len);

/**
 * @brief Run interactive acquisition: arms capture, polls until complete or aborted,
 * then prints statistics and renders the timing diagram.
 * @param out Terminal stream
 * @param timeout_ms Max wait timeout in ms (0 = indefinite until keypress)
 */
void logic_hal_run_capture_interactive(Stream& out, uint32_t timeout_ms);

/**
 * @brief Start hardware-timed PWM square wave signal generator on target pin.
 * @param pin RP2040 GPIO (e.g. GP10 EXT TRIG, GP14 PRESCALER, GP25 LED)
 * @param freq_hz Output frequency (10 Hz to 25 MHz)
 * @param duty_pct Duty cycle percentage (1 to 99, default 50)
 */
bool logic_hal_start_test_signal(uint8_t pin, uint32_t freq_hz, uint8_t duty_pct);

/**
 * @brief Stop hardware PWM signal generator and release pin to high-Z.
 */
void logic_hal_stop_test_signal(uint8_t pin);

/**
 * @brief Run automated self-test verification suite and output machine-readable report.
 */
void logic_hal_run_self_test(Stream& out);

/**
 * @brief Dump raw captured sample bytes in hexadecimal format with ASCII preview.
 * @param out Terminal stream
 * @param offset Starting sample index
 * @param count Number of sample bytes to display
 */
void logic_hal_dump_raw_samples(Stream& out, uint32_t offset, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif // UDOO_LOGIC_HAL_H
