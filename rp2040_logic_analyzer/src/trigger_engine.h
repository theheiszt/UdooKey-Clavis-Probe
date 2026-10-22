#ifndef TRIGGER_ENGINE_H
#define TRIGGER_ENGINE_H

#include <Arduino.h>
#include "hardware_config.h"

enum AdvTriggerMode : uint8_t {
    TRIG_MODE_AUTO         = 0,  // Immediate / Free run
    TRIG_MODE_EDGE_RISING  = 1,  // Rising edge on target channel
    TRIG_MODE_EDGE_FALLING = 2,  // Falling edge on target channel
    TRIG_MODE_EDGE_ANY     = 3,  // Any edge (toggle) on target channel
    TRIG_MODE_LEVEL_HIGH   = 4,  // Level HIGH on target channel
    TRIG_MODE_LEVEL_LOW    = 5,  // Level LOW on target channel
    TRIG_MODE_LEVEL_QUAL   = 6,  // Edge on Ch A while Level on Ch B (or mask)
    TRIG_MODE_WIDTH_GT     = 7,  // Pulse width > param (ns)
    TRIG_MODE_WIDTH_LT     = 8,  // Pulse width < param (ns) [Runt pulse / Glitch]
    TRIG_MODE_PATTERN      = 9,  // Pattern match: (GPIO & mask) == value
    TRIG_MODE_EXT_TRIG     = 10, // Dedicated external trigger pin (GPIO 10)
    TRIG_MODE_N_EDGES      = 11, // Fire after N consecutive transitions
    TRIG_MODE_SEQUENCE     = 12, // Multi-stage sequence: Stage A -> B -> C
    TRIG_MODE_UART_BYTE    = 13, // Protocol: UART byte match
    TRIG_MODE_I2C_ADDR     = 14, // Protocol: I2C address match
    TRIG_MODE_SPI_WORD     = 15  // Protocol: SPI word match
};

enum TriggerState : uint8_t {
    TRIG_STATE_IDLE      = 0,
    TRIG_STATE_ARMED     = 1, // Watching circular SRAM stream
    TRIG_STATE_TRIGGERED = 2, // Trigger event tripped! Capturing post-trigger data
    TRIG_STATE_COMPLETE  = 3  // Capture finished, buffer aligned & frozen
};

struct TriggerStage {
    AdvTriggerMode mode;             // EDGE_RISING, EDGE_FALLING, LEVEL_HIGH, PATTERN, EXT_TRIG, etc.
    uint8_t        channel;          // 0..7 or PIN_EXT_TRIG (10)
    uint32_t       param;            // Pulse width in ns / byte / value
    uint8_t        pattern_mask;     // Bitmask for pattern or qual
    uint8_t        pattern_value;    // Expected value for pattern or qual
    uint8_t        qual_channel;     // Qualifier channel (e.g. while CH_B == HIGH)
    uint8_t        qual_level;       // Required level on qual_channel
    uint32_t       min_wait_samples; // Minimum samples to wait since last stage (e.g. 20)
    uint32_t       max_wait_samples; // Maximum samples to wait since last stage (e.g. 2000, 0=unlimited)
    uint32_t       timeout_us;       // Max microsecond timeout for stage (0 = indefinite)
};

#define MAX_TRIGGER_STAGES 4

struct TriggerEngineConfig {
    AdvTriggerMode mode;             // Primary mode or TRIG_MODE_SEQUENCE
    uint8_t        channel;          // Target channel 0..7 (or PIN_EXT_TRIG)
    uint32_t       param;            // Pulse width (ns) / N edges / Value
    uint8_t        pattern_mask;     // Bitmask for pattern match
    uint8_t        pattern_value;    // Expected value for pattern match
    uint8_t        qual_channel;     // Qualifier channel (for LEVEL_QUAL)
    uint8_t        qual_level;       // Required level on qual_channel (0=LOW, 1=HIGH)
    uint32_t       holdoff_samples;  // Hold-off delay in samples before arming trigger detector
    uint8_t        pre_trigger_pct;  // 0% - 100% (default 70%: 70% pre-trigger, 30% post-trigger)

    // Multi-stage composable sequence
    uint8_t        stage_count;
    TriggerStage   stages[MAX_TRIGGER_STAGES];
};

void trigger_engine_init();
void trigger_engine_configure(const TriggerEngineConfig& cfg);
const TriggerEngineConfig& trigger_engine_get_config();
TriggerState trigger_engine_get_state();

// Circular buffer & Acquisition Lifecycle
void     trigger_engine_arm(uint32_t output_samples, uint8_t pre_pct = CAPTURE_PRE_TRIGGER_DEFAULT_PCT);
void     trigger_engine_arm(uint32_t output_samples, uint8_t pre_pct, uint32_t ring_size);
void     trigger_engine_fire(uint32_t trigger_sample_idx);
void     trigger_engine_set_complete();
void     trigger_engine_reset();

uint32_t trigger_engine_get_trigger_pos();
uint64_t trigger_engine_get_fire_timestamp_us();
uint32_t trigger_engine_get_pre_samples();
uint32_t trigger_engine_get_post_samples();
bool     trigger_engine_is_complete();

// Sample Evaluation Core: evaluates single sample transition
bool trigger_engine_evaluate(uint8_t current_gpio, uint8_t prev_gpio, uint32_t pulse_duration_ns);

// Stream Evaluation: checks DMA circular buffer block from from_idx to to_idx
bool trigger_engine_evaluate_stream(const uint8_t* buffer, uint32_t from_idx, uint32_t to_idx,
                                   uint32_t sample_rate_hz, uint32_t buffer_size, uint32_t& match_idx);

// Re-aligns circular buffer into chronologically ordered [PRE-TRIGGER][TRIGGER][POST-TRIGGER] array
void trigger_engine_align_buffer(const uint8_t* src, uint8_t* dst, uint32_t output_samples, uint32_t ring_size = 0);

// Diagnostic and Help output
const char* trigger_mode_to_string(AdvTriggerMode m);
void        trigger_print_status(Stream& out);

#endif // TRIGGER_ENGINE_H
