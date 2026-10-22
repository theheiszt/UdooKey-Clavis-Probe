#include "trigger_engine.h"
#include "event_streamer.h"
#include "pinmux_manager.h"
#include "pico/time.h"

static TriggerEngineConfig current_cfg = {
    .mode             = TRIG_MODE_AUTO,
    .channel          = 0,
    .param            = 0,
    .pattern_mask     = 0xFF,
    .pattern_value    = 0x00,
    .qual_channel     = 1,
    .qual_level       = 1,
    .pre_trigger_pct  = 50,
    .stage_count      = 0,
    .stages           = {}
};

static volatile TriggerState current_state = TRIG_STATE_IDLE;
static uint32_t trigger_fire_pos           = 0;
static uint64_t trigger_fire_timestamp_us  = 0;
static uint32_t configured_output_samples  = 8192;
static uint32_t configured_ring_size       = 8192;
static uint32_t pre_trigger_samples        = 4096;
static uint32_t post_trigger_samples       = 4096;

// Runtime state trackers
static uint32_t current_stage_idx      = 0;
static uint32_t stage_start_us         = 0;
static uint32_t edge_counter           = 0;
static uint32_t active_pulse_samples   = 0;
static uint8_t  last_sample_state      = 0;
static uint32_t uart_shift_reg         = 0;
static uint8_t  uart_bit_count         = 0;
static uint32_t i2c_shift_reg          = 0;
static uint8_t  i2c_bit_count          = 0;
static bool     i2c_in_frame           = false;
static bool     is_first_sample        = true;

void trigger_engine_init() {
    pinMode(PIN_EXT_TRIG, INPUT_PULLDOWN);
    current_state        = TRIG_STATE_IDLE;
    trigger_fire_pos     = 0;
    current_stage_idx    = 0;
    edge_counter         = 0;
    active_pulse_samples = 0;
    last_sample_state    = 0;
    uart_shift_reg       = 0;
    uart_bit_count       = 0;
    i2c_shift_reg        = 0;
    i2c_bit_count        = 0;
    i2c_in_frame         = false;
    is_first_sample      = true;
}

void trigger_engine_configure(const TriggerEngineConfig& cfg) {
    current_cfg = cfg;
    if (current_cfg.pre_trigger_pct > 100) current_cfg.pre_trigger_pct = CAPTURE_PRE_TRIGGER_DEFAULT_PCT;
    if (current_cfg.channel > 7 && current_cfg.channel != PIN_EXT_TRIG) current_cfg.channel = 0;
    current_state = TRIG_STATE_IDLE;
}

const TriggerEngineConfig& trigger_engine_get_config() {
    return current_cfg;
}

TriggerState trigger_engine_get_state() {
    return current_state;
}

void trigger_engine_arm(uint32_t output_samples, uint8_t pre_pct) {
    trigger_engine_arm(output_samples, pre_pct, output_samples);
}

void trigger_engine_arm(uint32_t output_samples, uint8_t pre_pct, uint32_t ring_size) {
    if (pre_pct > 100) pre_pct = current_cfg.pre_trigger_pct;
    current_cfg.pre_trigger_pct = pre_pct;

    if (ring_size == 0) ring_size = output_samples;

    configured_output_samples = output_samples;
    configured_ring_size      = ring_size;
    pre_trigger_samples       = (output_samples * (uint32_t)pre_pct) / 100;
    post_trigger_samples      = output_samples - pre_trigger_samples;

    trigger_fire_pos     = 0;
    current_stage_idx    = 0;
    stage_start_us       = micros();
    edge_counter         = 0;
    active_pulse_samples = 0;
    last_sample_state    = 0;
    uart_shift_reg       = 0;
    uart_bit_count       = 0;
    i2c_shift_reg        = 0;
    i2c_bit_count        = 0;
    i2c_in_frame         = false;
    is_first_sample      = true;

    current_state = TRIG_STATE_ARMED;
}

void trigger_engine_fire(uint32_t trigger_sample_idx) {
    if (current_state == TRIG_STATE_ARMED) {
        trigger_fire_pos          = trigger_sample_idx;
        trigger_fire_timestamp_us = device_timestamp_us();
        current_state             = TRIG_STATE_TRIGGERED;

        // Hardware strobe to synchronize external logic analyzers
        pinmux_pulse_sync_out(1);

        // Record structured event in instrument queue
        event_streamer_push(EVENT_TYPE_TRIGGER_FIRED, current_cfg.channel, 0, 0, "TRIGGER_FIRED");
        event_streamer_push(EVENT_TYPE_SYNC_STROBE, pinmux_get_sync_pin(), 0, 0, "SYNC_OUT");
    }
}

uint64_t trigger_engine_get_fire_timestamp_us() {
    return trigger_fire_timestamp_us;
}

void trigger_engine_set_complete() {
    current_state = TRIG_STATE_COMPLETE;
}

void trigger_engine_reset() {
    current_state = TRIG_STATE_IDLE;
}

uint32_t trigger_engine_get_trigger_pos() {
    return trigger_fire_pos;
}

uint32_t trigger_engine_get_pre_samples() {
    return pre_trigger_samples;
}

uint32_t trigger_engine_get_post_samples() {
    return post_trigger_samples;
}

bool trigger_engine_is_complete() {
    return (current_state == TRIG_STATE_COMPLETE);
}

// ─── Single Sample Evaluation ────────────────────────────────────────────────
static bool evaluate_condition(AdvTriggerMode mode, uint8_t ch, uint32_t param,
                               uint8_t pat_mask, uint8_t pat_val,
                               uint8_t qual_ch, uint8_t qual_lvl,
                               uint8_t curr, uint8_t prev, uint32_t pulse_ns) {
    uint8_t ch_mask = (ch <= 7) ? (1 << ch) : 0x01;
    bool curr_bit   = (curr & ch_mask) != 0;
    bool prev_bit   = (prev & ch_mask) != 0;

    switch (mode) {
        case TRIG_MODE_AUTO:
            return true;

        case TRIG_MODE_EDGE_RISING:
            return (!prev_bit && curr_bit);

        case TRIG_MODE_EDGE_FALLING:
            return (prev_bit && !curr_bit);

        case TRIG_MODE_EDGE_ANY:
            return (prev_bit != curr_bit);

        case TRIG_MODE_LEVEL_HIGH:
            return curr_bit;

        case TRIG_MODE_LEVEL_LOW:
            return !curr_bit;

        case TRIG_MODE_LEVEL_QUAL: {
            bool edge_hit = (!prev_bit && curr_bit); // Rising edge on ch
            bool qual_hit = (((curr >> qual_ch) & 0x01) == (qual_lvl & 0x01));
            return edge_hit && qual_hit;
        }

        case TRIG_MODE_WIDTH_GT:
            return (pulse_ns > param);

        case TRIG_MODE_WIDTH_LT:
            // Glitch/Runt pulse: fires on transition if previous pulse width was shorter than threshold
            return (prev_bit != curr_bit && pulse_ns > 0 && pulse_ns < param);

        case TRIG_MODE_PATTERN:
            return ((curr & pat_mask) == pat_val);

        case TRIG_MODE_EXT_TRIG:
            return digitalRead(PIN_EXT_TRIG) == HIGH;

        default:
            return false;
    }
}

bool trigger_engine_evaluate(uint8_t current_gpio, uint8_t prev_gpio, uint32_t pulse_duration_ns) {
    if (current_state != TRIG_STATE_ARMED) return false;

    if (current_cfg.mode == TRIG_MODE_SEQUENCE && current_cfg.stage_count > 0) {
        // Multi-stage composable sequence evaluator
        const TriggerStage& stage = current_cfg.stages[current_stage_idx];
        static uint32_t stage_sample_counter = 0;
        stage_sample_counter++;

        // Check microsecond timeout
        if (stage.timeout_us > 0 && (micros() - stage_start_us) > stage.timeout_us) {
            current_stage_idx    = 0;
            stage_start_us       = micros();
            stage_sample_counter = 0;
            return false;
        }

        // Check maximum sample wait window
        if (stage.max_wait_samples > 0 && stage_sample_counter > stage.max_wait_samples) {
            current_stage_idx    = 0;
            stage_start_us       = micros();
            stage_sample_counter = 0;
            return false;
        }

        // Evaluate stage condition and level qualification
        bool stage_hit = evaluate_condition(stage.mode, stage.channel, stage.param,
                                            stage.pattern_mask, stage.pattern_value,
                                            stage.qual_channel, stage.qual_level,
                                            current_gpio, prev_gpio, pulse_duration_ns);
        if (stage_hit) {
            // Check minimum sample wait window (e.g. wait at least 20 samples)
            if (stage.min_wait_samples > 0 && stage_sample_counter < stage.min_wait_samples) {
                return false; // Still within minimum hold-off window
            }

            current_stage_idx++;
            stage_start_us       = micros();
            stage_sample_counter = 0;
            if (current_stage_idx >= current_cfg.stage_count) {
                current_stage_idx = 0;
                return true; // Entire composable sequence matched!
            }
        }
        return false;
    }

    return evaluate_condition(current_cfg.mode, current_cfg.channel, current_cfg.param,
                              current_cfg.pattern_mask, current_cfg.pattern_value,
                              current_cfg.qual_channel, current_cfg.qual_level,
                              current_gpio, prev_gpio, pulse_duration_ns);
}

// ─── Stream Block Evaluator for Circular DMA Buffer ──────────────────────────
bool trigger_engine_evaluate_stream(const uint8_t* buffer, uint32_t from_idx, uint32_t to_idx,
                                   uint32_t sample_rate_hz, uint32_t buffer_size, uint32_t& match_idx) {
    if (current_state != TRIG_STATE_ARMED || !buffer || buffer_size == 0) return false;

    const uint32_t mask = buffer_size - 1;
    uint32_t idx = from_idx & mask;
    uint32_t end = to_idx & mask;
    if (idx == end) return false;

    const uint8_t ch = (current_cfg.channel <= 7) ? current_cfg.channel : 0;
    const uint8_t ch_mask = 1 << ch;

    // Fast-path 1: Edge Rising (Single-cycle bitwise evaluation)
    if (current_cfg.mode == TRIG_MODE_EDGE_RISING) {
        while (idx != end) {
            uint8_t curr = buffer[idx];
            if (is_first_sample) {
                last_sample_state = curr;
                is_first_sample = false;
                idx = (idx + 1) & mask;
                continue;
            }
            if ((curr & ch_mask) && !(last_sample_state & ch_mask)) {
                match_idx = idx;
                trigger_engine_fire(idx);
                last_sample_state = curr;
                return true;
            }
            last_sample_state = curr;
            idx = (idx + 1) & mask;
        }
        return false;
    }

    // Fast-path 2: Edge Falling
    if (current_cfg.mode == TRIG_MODE_EDGE_FALLING) {
        while (idx != end) {
            uint8_t curr = buffer[idx];
            if (is_first_sample) {
                last_sample_state = curr;
                is_first_sample = false;
                idx = (idx + 1) & mask;
                continue;
            }
            if (!(curr & ch_mask) && (last_sample_state & ch_mask)) {
                match_idx = idx;
                trigger_engine_fire(idx);
                last_sample_state = curr;
                return true;
            }
            last_sample_state = curr;
            idx = (idx + 1) & mask;
        }
        return false;
    }

    // Fast-path 3: Edge Any
    if (current_cfg.mode == TRIG_MODE_EDGE_ANY) {
        while (idx != end) {
            uint8_t curr = buffer[idx];
            if (is_first_sample) {
                last_sample_state = curr;
                is_first_sample = false;
                idx = (idx + 1) & mask;
                continue;
            }
            if ((curr ^ last_sample_state) & ch_mask) {
                match_idx = idx;
                trigger_engine_fire(idx);
                last_sample_state = curr;
                return true;
            }
            last_sample_state = curr;
            idx = (idx + 1) & mask;
        }
        return false;
    }

    // Fast-path 4: Pattern Match
    if (current_cfg.mode == TRIG_MODE_PATTERN) {
        const uint8_t pmask = current_cfg.pattern_mask;
        const uint8_t pval  = current_cfg.pattern_value;
        while (idx != end) {
            uint8_t curr = buffer[idx];
            if ((curr & pmask) == pval) {
                match_idx = idx;
                trigger_engine_fire(idx);
                last_sample_state = curr;
                return true;
            }
            last_sample_state = curr;
            idx = (idx + 1) & mask;
        }
        return false;
    }

    // Fast-path 5: Level High
    if (current_cfg.mode == TRIG_MODE_LEVEL_HIGH) {
        while (idx != end) {
            uint8_t curr = buffer[idx];
            if (curr & ch_mask) {
                match_idx = idx;
                trigger_engine_fire(idx);
                last_sample_state = curr;
                return true;
            }
            last_sample_state = curr;
            idx = (idx + 1) & mask;
        }
        return false;
    }

    // Fast-path 6: Level Low
    if (current_cfg.mode == TRIG_MODE_LEVEL_LOW) {
        while (idx != end) {
            uint8_t curr = buffer[idx];
            if (!(curr & ch_mask)) {
                match_idx = idx;
                trigger_engine_fire(idx);
                last_sample_state = curr;
                return true;
            }
            last_sample_state = curr;
            idx = (idx + 1) & mask;
        }
        return false;
    }

    // General / Pulse Width / Sequence / Protocol fallback evaluator
    float ns_per_sample = 1e9f / (float)sample_rate_hz;
    while (idx != end) {
        uint8_t curr = buffer[idx];

        if (is_first_sample) {
            last_sample_state = curr;
            active_pulse_samples = 1;
            is_first_sample = false;
            idx = (idx + 1) & mask;
            continue;
        }

        uint8_t prev = last_sample_state;
        bool curr_bit = (curr & ch_mask) != 0;
        bool prev_bit = (prev & ch_mask) != 0;

        if (curr_bit == prev_bit) {
            active_pulse_samples++;
        } else {
            uint32_t pulse_ns = (uint32_t)(active_pulse_samples * ns_per_sample);
            if (trigger_engine_evaluate(curr, prev, pulse_ns)) {
                match_idx = idx;
                trigger_engine_fire(idx);
                return true;
            }
            active_pulse_samples = 1;
        }

        uint32_t cur_pulse_ns = (uint32_t)(active_pulse_samples * ns_per_sample);
        if (trigger_engine_evaluate(curr, prev, cur_pulse_ns)) {
            match_idx = idx;
            trigger_engine_fire(idx);
            return true;
        }

        last_sample_state = curr;
        idx = (idx + 1) & mask;
    }

    return false;
}

// ─── Circular Buffer Re-Alignment ────────────────────────────────────────────
void trigger_engine_align_buffer(const uint8_t* src, uint8_t* dst, uint32_t output_samples, uint32_t ring_size) {
    if (!src || !dst || output_samples == 0) return;
    if (ring_size == 0) ring_size = (configured_ring_size > 0) ? configured_ring_size : output_samples;

    const uint32_t mask = ring_size - 1;
    // Start index in circular buffer: pre_trigger_samples before the trigger event
    uint32_t start_idx = (trigger_fire_pos + ring_size - (pre_trigger_samples & mask)) & mask;

    for (uint32_t i = 0; i < output_samples; i++) {
        dst[i] = src[(start_idx + i) & mask];
    }
}

// ─── Formatting & Diagnostics ────────────────────────────────────────────────
const char* trigger_mode_to_string(AdvTriggerMode m) {
    switch (m) {
        case TRIG_MODE_AUTO:         return "AUTO (Immediate Free-Run)";
        case TRIG_MODE_EDGE_RISING:  return "RISING Edge";
        case TRIG_MODE_EDGE_FALLING: return "FALLING Edge";
        case TRIG_MODE_EDGE_ANY:     return "ANY Edge (Toggle)";
        case TRIG_MODE_LEVEL_HIGH:   return "Level HIGH";
        case TRIG_MODE_LEVEL_LOW:    return "Level LOW";
        case TRIG_MODE_LEVEL_QUAL:   return "Level Qualified (Edge + Level)";
        case TRIG_MODE_WIDTH_GT:     return "Pulse Width > Threshold";
        case TRIG_MODE_WIDTH_LT:     return "Pulse Width < Threshold (Glitch/Runt)";
        case TRIG_MODE_PATTERN:      return "Pattern Match (Mask & Value)";
        case TRIG_MODE_EXT_TRIG:     return "External Trigger Input (GPIO 10)";
        case TRIG_MODE_N_EDGES:      return "N Consecutive Edges";
        case TRIG_MODE_SEQUENCE:     return "Multi-Stage Sequence (A -> B -> C)";
        case TRIG_MODE_UART_BYTE:    return "UART Byte Match";
        case TRIG_MODE_I2C_ADDR:     return "I2C Address Match";
        case TRIG_MODE_SPI_WORD:     return "SPI Word Match";
        default:                     return "Unknown";
    }
}

void trigger_print_status(Stream& out) {
    out.println("\r\n=== Hardware Trigger Engine Status ===");
    out.printf("  Active Mode:     %s\r\n", trigger_mode_to_string(current_cfg.mode));
    out.printf("  Trigger Channel: GPIO %u (CH%u)\r\n",
               (current_cfg.channel == PIN_EXT_TRIG) ? PIN_EXT_TRIG : (2 + current_cfg.channel),
               current_cfg.channel);
    out.printf("  Position:        %u%% Pre-Trigger | %u%% Post-Trigger\r\n",
               current_cfg.pre_trigger_pct, 100 - current_cfg.pre_trigger_pct);
    out.printf("  Trigger State:   %s\r\n",
               (current_state == TRIG_STATE_IDLE)      ? "IDLE" :
               (current_state == TRIG_STATE_ARMED)     ? "ARMED (Waiting for event)" :
               (current_state == TRIG_STATE_TRIGGERED) ? "TRIGGERED (Collecting post-samples)" : "COMPLETE");

    if (current_cfg.mode == TRIG_MODE_WIDTH_GT || current_cfg.mode == TRIG_MODE_WIDTH_LT) {
        out.printf("  Pulse Threshold: %lu ns\r\n", (unsigned long)current_cfg.param);
    } else if (current_cfg.mode == TRIG_MODE_LEVEL_QUAL) {
        out.printf("  Qualification:   CH%u must be %s while CH%u rises\r\n",
                   current_cfg.qual_channel, (current_cfg.qual_level ? "HIGH" : "LOW"), current_cfg.channel);
    } else if (current_cfg.mode == TRIG_MODE_PATTERN) {
        out.printf("  Pattern Match:   (GPIO & 0x%02X) == 0x%02X\r\n",
                   current_cfg.pattern_mask, current_cfg.pattern_value);
    } else if (current_cfg.mode == TRIG_MODE_SEQUENCE) {
        out.printf("  Sequence Stages: %u stages configured (Current: %u)\r\n",
                   current_cfg.stage_count, current_stage_idx);
        for (uint8_t i = 0; i < current_cfg.stage_count; i++) {
            out.printf("    [%u] %s on CH%u (param=%lu, timeout=%lu us)\r\n",
                       i + 1, trigger_mode_to_string(current_cfg.stages[i].mode),
                       current_cfg.stages[i].channel,
                       (unsigned long)current_cfg.stages[i].param,
                       (unsigned long)current_cfg.stages[i].timeout_us);
        }
    }

    if (current_state == TRIG_STATE_COMPLETE || current_state == TRIG_STATE_TRIGGERED) {
        out.printf("  Trigger Fire Pos: Sample #%lu (Pre: %lu samples, Post: %lu samples)\r\n",
                   (unsigned long)trigger_fire_pos,
                   (unsigned long)pre_trigger_samples,
                   (unsigned long)post_trigger_samples);
    }
}
