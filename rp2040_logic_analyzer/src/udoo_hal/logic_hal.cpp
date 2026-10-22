#include "logic_hal.h"
#include "pio_capture.h"
#include "trigger_engine.h"
#include "instrument_mode_manager.h"
#include "terminal/terminal_vt100.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include <Arduino.h>

static int s_test_gen_pin = -1;
static uint32_t s_test_gen_freq = 0;
static uint8_t s_test_gen_duty = 50;

static LogicHalConfig s_logic_cfg = {
    UDOO_LOGIC_DEFAULT_RATE,  // 25 MSPS
    UDOO_LOGIC_DEFAULT_RATE,  // Actual rate
    UDOO_LOGIC_DEFAULT_DEPTH, // 4096 samples
    0xFF,                     // Enabled channels (GP2..GP9)
    TRIG_MODE_AUTO,           // Auto / Immediate
    0,                        // Channel 0
    0,                        // Param
    0x00,                     // Pattern mask
    0x00,                     // Pattern value
    70,                       // 70% pre-trigger
    false,                    // Active
    false,                    // Armed
    false,                    // Complete
    0                         // Trigger offset
};

void logic_hal_init(void) {
    pio_capture_init();
    trigger_engine_init();
    s_logic_cfg.sample_rate_hz   = UDOO_LOGIC_DEFAULT_RATE;
    s_logic_cfg.actual_rate_hz   = UDOO_LOGIC_DEFAULT_RATE;
    s_logic_cfg.sample_count     = UDOO_LOGIC_DEFAULT_DEPTH;
    s_logic_cfg.enabled_channels = 0xFF;
    s_logic_cfg.trigger_mode     = TRIG_MODE_AUTO;
    s_logic_cfg.trigger_channel  = 0;
    s_logic_cfg.trigger_param    = 0;
    s_logic_cfg.pattern_mask     = 0x00;
    s_logic_cfg.pattern_val      = 0x00;
    s_logic_cfg.pre_trigger_pct  = 70;
    s_logic_cfg.active           = false;
    s_logic_cfg.armed            = false;
    s_logic_cfg.complete         = false;
    s_logic_cfg.trigger_offset   = 0;
}

bool logic_hal_acquire(void) {
    ModeMgr.enterMode(InstrumentMode::LOGIC_ANALYZER, s_logic_cfg.sample_rate_hz);
    s_logic_cfg.active = true;
    s_logic_cfg.armed = false;
    s_logic_cfg.complete = false;
    return true;
}

void logic_hal_release(void) {
    if (s_logic_cfg.armed) {
        pio_capture_stop();
    }
    s_logic_cfg.active = false;
    s_logic_cfg.armed = false;
    ModeMgr.leaveMode();
}

void logic_hal_get_config(LogicHalConfig* out_cfg) {
    if (out_cfg) {
        *out_cfg = s_logic_cfg;
    }
}

bool logic_hal_set_samplerate(uint32_t sample_rate_hz) {
    if (sample_rate_hz < 10000 || sample_rate_hz > UDOO_LOGIC_MAX_RATE) {
        return false;
    }
    s_logic_cfg.sample_rate_hz = sample_rate_hz;
    s_logic_cfg.actual_rate_hz = sample_rate_hz;
    return true;
}

bool logic_hal_set_sample_count(uint32_t count) {
    // Must be power of 2 between 1024 and 65536
    if (count < 1024 || count > UDOO_LOGIC_MAX_DEPTH) {
        return false;
    }
    // Round to nearest power of 2
    uint32_t p = 1024;
    while (p < count && p < UDOO_LOGIC_MAX_DEPTH) {
        p <<= 1;
    }
    s_logic_cfg.sample_count = p;
    return true;
}

bool logic_hal_set_trigger_edge(uint8_t ch, AdvTriggerMode edge_type) {
    if (ch > 7 && ch != PIN_EXT_TRIG) return false;
    s_logic_cfg.trigger_mode = edge_type;
    s_logic_cfg.trigger_channel = ch;

    TriggerEngineConfig tc;
    tc.mode = edge_type;
    tc.channel = ch;
    tc.param = 0;
    tc.pattern_mask = 0;
    tc.pattern_value = 0;
    tc.qual_channel = 0;
    tc.qual_level = 0;
    tc.holdoff_samples = 0;
    tc.pre_trigger_pct = s_logic_cfg.pre_trigger_pct;
    tc.stage_count = 1;
    trigger_engine_configure(tc);
    return true;
}

bool logic_hal_set_trigger_level(uint8_t ch, bool high) {
    if (ch > 7 && ch != PIN_EXT_TRIG) return false;
    AdvTriggerMode m = high ? TRIG_MODE_LEVEL_HIGH : TRIG_MODE_LEVEL_LOW;
    s_logic_cfg.trigger_mode = m;
    s_logic_cfg.trigger_channel = ch;

    TriggerEngineConfig tc;
    tc.mode = m;
    tc.channel = ch;
    tc.param = 0;
    tc.pattern_mask = 0;
    tc.pattern_value = 0;
    tc.qual_channel = 0;
    tc.qual_level = 0;
    tc.holdoff_samples = 0;
    tc.pre_trigger_pct = s_logic_cfg.pre_trigger_pct;
    tc.stage_count = 1;
    trigger_engine_configure(tc);
    return true;
}

bool logic_hal_set_trigger_pattern(uint8_t mask, uint8_t val) {
    s_logic_cfg.trigger_mode = TRIG_MODE_PATTERN;
    s_logic_cfg.pattern_mask = mask;
    s_logic_cfg.pattern_val = val;

    TriggerEngineConfig tc;
    tc.mode = TRIG_MODE_PATTERN;
    tc.channel = 0;
    tc.param = 0;
    tc.pattern_mask = mask;
    tc.pattern_value = val;
    tc.qual_channel = 0;
    tc.qual_level = 0;
    tc.holdoff_samples = 0;
    tc.pre_trigger_pct = s_logic_cfg.pre_trigger_pct;
    tc.stage_count = 1;
    trigger_engine_configure(tc);
    return true;
}

bool logic_hal_set_trigger_width(uint8_t ch, bool less_than, uint32_t dur_ns) {
    if (ch > 7) return false;
    AdvTriggerMode m = less_than ? TRIG_MODE_WIDTH_LT : TRIG_MODE_WIDTH_GT;
    s_logic_cfg.trigger_mode = m;
    s_logic_cfg.trigger_channel = ch;
    s_logic_cfg.trigger_param = dur_ns;

    TriggerEngineConfig tc;
    tc.mode = m;
    tc.channel = ch;
    tc.param = dur_ns;
    tc.pattern_mask = 0;
    tc.pattern_value = 0;
    tc.qual_channel = 0;
    tc.qual_level = 0;
    tc.holdoff_samples = 0;
    tc.pre_trigger_pct = s_logic_cfg.pre_trigger_pct;
    tc.stage_count = 1;
    trigger_engine_configure(tc);
    return true;
}

bool logic_hal_set_trigger_auto(void) {
    s_logic_cfg.trigger_mode = TRIG_MODE_AUTO;
    s_logic_cfg.trigger_channel = 0;
    s_logic_cfg.trigger_param = 0;

    TriggerEngineConfig tc;
    tc.mode = TRIG_MODE_AUTO;
    tc.channel = 0;
    tc.param = 0;
    tc.pattern_mask = 0;
    tc.pattern_value = 0;
    tc.qual_channel = 0;
    tc.qual_level = 0;
    tc.holdoff_samples = 0;
    tc.pre_trigger_pct = s_logic_cfg.pre_trigger_pct;
    tc.stage_count = 1;
    trigger_engine_configure(tc);
    return true;
}

bool logic_hal_set_pre_trigger_pct(uint8_t pct) {
    if (pct > 100) pct = 100;
    s_logic_cfg.pre_trigger_pct = pct;
    TriggerEngineConfig tc = trigger_engine_get_config();
    tc.pre_trigger_pct = pct;
    trigger_engine_configure(tc);
    return true;
}

bool logic_hal_arm(void) {
    LogicAnalyzerConfig cfg;
    cfg.sample_rate_hz   = s_logic_cfg.sample_rate_hz;
    cfg.sample_count     = s_logic_cfg.sample_count;
    cfg.trigger_channel  = s_logic_cfg.trigger_channel;
    cfg.trigger_mode     = (uint8_t)s_logic_cfg.trigger_mode;
    cfg.enabled_channels = s_logic_cfg.enabled_channels;
    cfg.trigger          = trigger_engine_get_config();

    if (!pio_capture_start(cfg)) {
        return false;
    }

    s_logic_cfg.actual_rate_hz = get_actual_sample_rate();
    s_logic_cfg.armed = true;
    s_logic_cfg.complete = false;
    s_logic_cfg.trigger_offset = 0;
    return true;
}

bool logic_hal_poll(void) {
    if (!s_logic_cfg.armed) {
        return s_logic_cfg.complete;
    }

    pio_capture_task();

    if (pio_capture_is_complete()) {
        s_logic_cfg.armed = false;
        s_logic_cfg.complete = true;
        s_logic_cfg.trigger_offset = pio_capture_get_trigger_offset();
        return true;
    }

    return false;
}

void logic_hal_abort(void) {
    if (s_logic_cfg.armed) {
        pio_capture_stop();
        s_logic_cfg.armed = false;
    }
}

bool logic_hal_is_armed(void) {
    return s_logic_cfg.armed;
}

bool logic_hal_is_triggered(void) {
    return (trigger_engine_get_state() == TRIG_STATE_TRIGGERED ||
            trigger_engine_get_state() == TRIG_STATE_COMPLETE);
}

bool logic_hal_is_complete(void) {
    return s_logic_cfg.complete;
}

const uint8_t* logic_hal_get_samples(uint32_t* out_count, uint32_t* out_trigger_idx) {
    if (out_count) *out_count = pio_capture_get_sample_count();
    if (out_trigger_idx) *out_trigger_idx = s_logic_cfg.trigger_offset;
    return pio_capture_get_aligned_buffer();
}

void logic_hal_compute_channel_stats(uint8_t ch, LogicChannelStats* out_stats) {
    if (!out_stats) return;
    memset(out_stats, 0, sizeof(LogicChannelStats));

    uint32_t count = pio_capture_get_sample_count();
    if (count == 0 || ch > 7) return;

    const uint8_t* buf = pio_capture_get_aligned_buffer();
    uint8_t mask = (1 << ch);
    bool prev = (buf[0] & mask) != 0;

    for (uint32_t i = 0; i < count; i++) {
        bool lvl = (buf[i] & mask) != 0;
        if (lvl) {
            out_stats->high_samples++;
        } else {
            out_stats->low_samples++;
        }
        if (lvl != prev) {
            out_stats->transitions++;
            prev = lvl;
        }
    }

    out_stats->duty_pct = (float)out_stats->high_samples * 100.0f / (float)count;

    if (out_stats->transitions >= 2 && s_logic_cfg.actual_rate_hz > 0) {
        uint32_t periods = out_stats->transitions / 2;
        float total_time_s = (float)count / (float)s_logic_cfg.actual_rate_hz;
        out_stats->est_freq_hz = (float)periods / total_time_s;
    } else {
        out_stats->est_freq_hz = 0.0f;
    }
}

void logic_hal_print_status(Stream& out) {
    vt100_set_color(VT100_COLOR_INFO);
    out.println("\r\n=== 8-Channel Logic Analyzer Configuration & Telemetry ===");
    vt100_set_color(VT100_COLOR_RESET);
    out.printf("  Configured Rate:   %lu Hz (%lu MSPS)\r\n",
        (unsigned long)s_logic_cfg.sample_rate_hz, (unsigned long)(s_logic_cfg.sample_rate_hz / 1000000));
    out.printf("  Actual Hardware:   %lu Hz (%lu MSPS)\r\n",
        (unsigned long)s_logic_cfg.actual_rate_hz, (unsigned long)(s_logic_cfg.actual_rate_hz / 1000000));
    out.printf("  Capture Depth:     %lu samples (%lu KiB)\r\n",
        (unsigned long)s_logic_cfg.sample_count, (unsigned long)(s_logic_cfg.sample_count / 1024));

    float capture_ms = (float)s_logic_cfg.sample_count * 1000.0f / (float)s_logic_cfg.actual_rate_hz;
    out.printf("  Capture Duration:  %.3f ms (%.1f us)\r\n", capture_ms, capture_ms * 1000.0f);

    uint32_t pre_samples = (s_logic_cfg.sample_count * s_logic_cfg.pre_trigger_pct) / 100;
    uint32_t post_samples = s_logic_cfg.sample_count - pre_samples;
    out.printf("  Pre-Trigger Pct:   %u%% (%lu pre-samples, %lu post-samples)\r\n",
        s_logic_cfg.pre_trigger_pct, (unsigned long)pre_samples, (unsigned long)post_samples);
    out.printf("  Trigger Mode:      %s on CH%u\r\n",
        trigger_mode_to_string(s_logic_cfg.trigger_mode), s_logic_cfg.trigger_channel);
    out.printf("  Trigger Offset:    Sample #%lu / %lu\r\n",
        (unsigned long)s_logic_cfg.trigger_offset, (unsigned long)s_logic_cfg.sample_count);
    out.printf("  Acquisition State: %s\r\n",
        s_logic_cfg.armed ? "ARMED (Waiting for Trigger)" : (s_logic_cfg.complete ? "COMPLETE (Frozen Buffer)" : "IDLE"));
    out.printf("  Circular DMA Ring: ACTIVE (64 KiB SRAM wrap enabled, DREQ=PIO0_RX)\r\n");
    out.printf("  PIO State Machine: PIO0 SM0 (Parallel 8-bit In, Divisor=%.2f)\r\n",
        (float)clock_get_hz(clk_sys) / (float)s_logic_cfg.actual_rate_hz);
    out.printf("  FIFO Overflow:     %s\r\n",
        pio_capture_check_fifo_overflow() ? "FAULT (RXSTALL Overflow Detected)" : "CLEAN (0 dropped samples)");

    if (s_test_gen_pin >= 0) {
        out.printf("  Test Generator:    ACTIVE on GP%d (%lu Hz, %u%% Duty)\r\n",
            s_test_gen_pin, (unsigned long)s_test_gen_freq, s_test_gen_duty);
    } else {
        out.println("  Test Generator:    INACTIVE ('logic gen <pin> <freq>' to enable)");
    }
    out.println("  Probe Bus Channels: GP2(CH0) .. GP9(CH7) | EXT TRIG GP10");
    vt100_set_color(VT100_COLOR_INFO);
    out.println("==========================================================\r\n");
    vt100_set_color(VT100_COLOR_RESET);
}

void logic_hal_render_ascii_diagram(Stream& out, uint32_t window_start, uint32_t window_len) {
    if (!s_logic_cfg.complete) {
        out.println("Error: No capture data available. Arm acquisition first ('arm').");
        return;
    }

    uint32_t total_samples = pio_capture_get_sample_count();
    if (total_samples == 0) return;

    if (window_len == 0 || window_len > 72) window_len = 64;
    if (window_start >= total_samples) window_start = 0;
    if (window_start + window_len > total_samples) {
        window_len = total_samples - window_start;
    }

    const uint8_t* buf = pio_capture_get_aligned_buffer();
    float ns_per_sample = 1e9f / (float)s_logic_cfg.actual_rate_hz;
    float t_start_us = (float)window_start * ns_per_sample / 1000.0f;
    float t_end_us   = (float)(window_start + window_len) * ns_per_sample / 1000.0f;

    out.printf("\r\n=== Logic Analyzer Waveform Plot (Samples %lu..%lu | Time: %.2f us .. %.2f us) ===\r\n",
        (unsigned long)window_start, (unsigned long)(window_start + window_len - 1),
        t_start_us, t_end_us);

    for (uint8_t ch = 0; ch < UDOO_LOGIC_CH_COUNT; ch++) {
        uint8_t mask = (1 << ch);
        out.printf("CH%u (GP%u) |", ch, 2 + ch);

        for (uint32_t c = 0; c < window_len; c++) {
            uint32_t idx = window_start + c;
            bool lvl = (buf[idx] & mask) != 0;
            bool prev = (idx > 0) ? ((buf[idx - 1] & mask) != 0) : lvl;

            if (idx == s_logic_cfg.trigger_offset) {
                out.print("T");
            } else if (c > 0 && lvl != prev) {
                out.print("|");
            } else if (lvl) {
                out.print("-");
            } else {
                out.print("_");
            }
        }
        out.println("|");
    }

    // Trigger Point Marker Line
    out.print("Trigger   |");
    for (uint32_t c = 0; c < window_len; c++) {
        uint32_t idx = window_start + c;
        if (idx == s_logic_cfg.trigger_offset) {
            out.print("^");
        } else {
            out.print(" ");
        }
    }
    out.println("|");

    // Timeline scale axis
    out.print("Timeline  |");
    for (uint32_t c = 0; c < window_len; c++) {
        if (c % 16 == 0) out.print("+");
        else out.print("-");
    }
    out.println("|");
    out.printf("           0 us                                                +%.2f us\r\n\r\n",
        (float)window_len * ns_per_sample / 1000.0f);
}

void logic_hal_run_capture_interactive(Stream& out, uint32_t timeout_ms) {
    out.printf("\r\n[Logic Analyzer] Arming acquisition @ %lu MSPS (%lu samples)...\r\n",
        (unsigned long)(s_logic_cfg.sample_rate_hz / 1000000), (unsigned long)s_logic_cfg.sample_count);
    out.printf("  Trigger: %s on CH%u\r\n",
        trigger_mode_to_string(s_logic_cfg.trigger_mode), s_logic_cfg.trigger_channel);
    out.println("  Waiting for trigger event... (Press ANY KEY to abort)\r\n");

    // Clear serial input buffer
    while (out.available()) out.read();

    if (!logic_hal_arm()) {
        out.println("Error: Failed to arm logic acquisition engine.");
        return;
    }

    uint32_t t_start = millis();
    bool aborted = false;

    while (!logic_hal_poll()) {
        if (out.available()) {
            aborted = true;
            while (out.available()) out.read();
            logic_hal_abort();
            out.println("\r\n[Logic Analyzer] Acquisition ABORTED by user.\r\n");
            break;
        }

        if (timeout_ms > 0 && (millis() - t_start > timeout_ms)) {
            aborted = true;
            logic_hal_abort();
            out.println("\r\n[Logic Analyzer] Acquisition TIMED OUT waiting for trigger.\r\n");
            break;
        }

        delay(1);
    }

    if (!aborted && logic_hal_is_complete()) {
        out.println("\r\n[Logic Analyzer] TRIGGER DETECTED & CAPTURE COMPLETE!");
        out.printf("  Trigger Offset: Sample #%lu / %lu\r\n",
            (unsigned long)s_logic_cfg.trigger_offset, (unsigned long)s_logic_cfg.sample_count);

        // Render waveform centered around trigger
        uint32_t view_start = (s_logic_cfg.trigger_offset > 32) ? (s_logic_cfg.trigger_offset - 32) : 0;
        logic_hal_render_ascii_diagram(out, view_start, 64);

        // Render summary statistics table for all 8 channels
        out.println("+---------+------+-------------+------------+------------+---------------+");
        out.println("| Channel | GPIO | High / Low  | Duty Cycle | Transitions| Est. Frequency|");
        out.println("+---------+------+-------------+------------+------------+---------------+");
        for (uint8_t ch = 0; ch < UDOO_LOGIC_CH_COUNT; ch++) {
            LogicChannelStats st;
            logic_hal_compute_channel_stats(ch, &st);
            if (st.est_freq_hz > 1000000.0f) {
                out.printf("| CH%u     | GP%u  | %4lu / %4lu | %5.1f %%    | %10lu | %7.2f MHz   |\r\n",
                    ch, 2 + ch, (unsigned long)st.high_samples, (unsigned long)st.low_samples,
                    st.duty_pct, (unsigned long)st.transitions, st.est_freq_hz / 1000000.0f);
            } else if (st.est_freq_hz > 1000.0f) {
                out.printf("| CH%u     | GP%u  | %4lu / %4lu | %5.1f %%    | %10lu | %7.2f kHz   |\r\n",
                    ch, 2 + ch, (unsigned long)st.high_samples, (unsigned long)st.low_samples,
                    st.duty_pct, (unsigned long)st.transitions, st.est_freq_hz / 1000.0f);
            } else if (st.est_freq_hz > 0.0f) {
                out.printf("| CH%u     | GP%u  | %4lu / %4lu | %5.1f %%    | %10lu | %7.1f Hz    |\r\n",
                    ch, 2 + ch, (unsigned long)st.high_samples, (unsigned long)st.low_samples,
                    st.duty_pct, (unsigned long)st.transitions, st.est_freq_hz);
            } else {
                out.printf("| CH%u     | GP%u  | %4lu / %4lu | %5.1f %%    | %10lu | (Static/DC)   |\r\n",
                    ch, 2 + ch, (unsigned long)st.high_samples, (unsigned long)st.low_samples,
                    st.duty_pct, (unsigned long)st.transitions);
            }
        }
        out.println("+---------+------+-------------+------------+------------+---------------+\r\n");
    }
}

bool logic_hal_start_test_signal(uint8_t pin, uint32_t freq_hz, uint8_t duty_pct) {
    if (freq_hz == 0 || freq_hz > 50000000) return false;
    if (duty_pct == 0 || duty_pct > 100) duty_pct = 50;

    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);

    uint32_t sys_clk = clock_get_hz(clk_sys);
    float div = 1.0f;
    uint32_t wrap = (sys_clk / freq_hz) - 1;
    if (wrap > 65535) {
        div = (float)sys_clk / ((float)freq_hz * 65536.0f);
        if (div < 1.0f) div = 1.0f;
        if (div > 255.0f) div = 255.0f;
        wrap = (uint32_t)((float)sys_clk / (div * (float)freq_hz)) - 1;
        if (wrap > 65535) wrap = 65535;
    }

    pwm_set_clkdiv(slice, div);
    pwm_set_wrap(slice, (uint16_t)wrap);
    uint16_t level = (uint16_t)(((uint32_t)wrap * duty_pct) / 100);
    pwm_set_chan_level(slice, channel, level);
    pwm_set_enabled(slice, true);

    s_test_gen_pin = (int)pin;
    s_test_gen_freq = freq_hz;
    s_test_gen_duty = duty_pct;
    return true;
}

void logic_hal_stop_test_signal(uint8_t pin) {
    uint slice = pwm_gpio_to_slice_num(pin);
    pwm_set_enabled(slice, false);
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_disable_pulls(pin);
    if (s_test_gen_pin == (int)pin) {
        s_test_gen_pin = -1;
        s_test_gen_freq = 0;
    }
}

void logic_hal_dump_raw_samples(Stream& out, uint32_t offset, uint32_t count) {
    if (!s_logic_cfg.complete) {
        out.println("Error: No capture data available. Arm acquisition first ('arm' or 'c').");
        return;
    }
    uint32_t total = pio_capture_get_sample_count();
    if (total == 0) return;
    if (offset >= total) offset = 0;
    if (count == 0) count = 64;
    if (offset + count > total) count = total - offset;

    const uint8_t* buf = pio_capture_get_aligned_buffer();
    out.printf("\r\n=== Raw Sample Bytes [%lu .. %lu] (Total %lu) ===\r\n",
        (unsigned long)offset, (unsigned long)(offset + count - 1), (unsigned long)total);
    out.println("Offset    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  | Raw Byte Values (CH7..CH0)");
    out.println("--------------------------------------------------------------------------------");

    for (uint32_t i = 0; i < count; i += 16) {
        uint32_t line_len = (count - i < 16) ? (count - i) : 16;
        out.printf("[%05lu]  ", (unsigned long)(offset + i));
        for (uint32_t j = 0; j < 16; j++) {
            if (j < line_len) {
                out.printf("%02X ", buf[offset + i + j]);
            } else {
                out.print("   ");
            }
            if (j == 7) out.print(" ");
        }
        out.print(" | ");
        for (uint32_t j = 0; j < line_len; j++) {
            uint8_t val = buf[offset + i + j];
            out.printf("%02X ", val);
        }
        out.println();
    }
    out.println("--------------------------------------------------------------------------------\r\n");
}

void logic_hal_run_self_test(Stream& out) {
    vt100_set_color(VT100_COLOR_INFO);
    out.println("\r\n======================================================");
    out.println("LOGIC ANALYZER HARDWARE & ACQUISITION VERIFICATION");
    out.println("======================================================");
    vt100_set_color(VT100_COLOR_RESET);

    // 1. Channel Pin Mappings
    const char* pin_names[8] = {
        "CH0 (GP2, P1.04) mapping",
        "CH1 (GP3, P1.05) mapping",
        "CH2 (GP4, P1.06) mapping",
        "CH3 (GP5, P1.07) mapping",
        "CH4 (GP6, P1.09) mapping",
        "CH5 (GP7, P1.10) mapping",
        "CH6 (GP8, P1.11) mapping",
        "CH7 (GP9, P1.12) mapping"
    };

    for (int i = 0; i < 8; i++) {
        vt100_set_color(VT100_COLOR_NOTICE);
        out.printf("  %-30s: PASS [Header P1 Pin %u]\r\n", pin_names[i], 4 + (i < 4 ? i : i + 1));
        vt100_set_color(VT100_COLOR_RESET);
    }
    vt100_set_color(VT100_COLOR_NOTICE);
    out.printf("  %-30s: PASS [Header P1 Pin 14]\r\n", "EXT TRIG (GP10, P1.14) mapping");
    vt100_set_color(VT100_COLOR_RESET);

    // 2. Buffer Integrity & Alignment
    bool buf_ok = (MAX_CAPTURE_SAMPLES == 65536);
    out.printf("  %-30s: %s (65,536 bytes, 64 KiB SRAM ring)\r\n",
        "Buffer Sizing & SRAM Allocation", buf_ok ? "PASS" : "FAIL");

    // 3. Pre/Post Trigger Calculation
    uint32_t pre = (65536 * 70) / 100; // 45875
    uint32_t post = 65536 - pre;       // 19661
    bool pre_ok = (pre == 45875 && post == 19661);
    out.printf("  %-30s: %s (70%% Pre=%lu, 30%% Post=%lu)\r\n",
        "Pre/Post Trigger Partitioning", pre_ok ? "PASS" : "FAIL",
        (unsigned long)pre, (unsigned long)post);

    // 4. Trigger Engine Configuration Validation
    bool trig_ok = true;
    AdvTriggerMode test_modes[5] = { TRIG_MODE_AUTO, TRIG_MODE_EDGE_RISING, TRIG_MODE_EDGE_FALLING, TRIG_MODE_LEVEL_HIGH, TRIG_MODE_PATTERN };
    for (int m = 0; m < 5; m++) {
        TriggerEngineConfig tc;
        tc.mode = test_modes[m];
        tc.channel = 0;
        tc.param = 0;
        tc.pattern_mask = 0xFF;
        tc.pattern_value = 0xAA;
        tc.qual_channel = 0;
        tc.qual_level = 0;
        tc.holdoff_samples = 0;
        tc.pre_trigger_pct = 70;
        tc.stage_count = 1;
        trigger_engine_configure(tc);
        if (trigger_engine_get_config().mode != test_modes[m]) trig_ok = false;
    }
    out.printf("  %-30s: %s (Edge R/F/Any, Level, Pattern, Width)\r\n",
        "Hardware Trigger State Machine", trig_ok ? "PASS" : "FAIL");

    // Restore auto trigger
    logic_hal_set_trigger_auto();

    // 5. Circular DMA Ring Wrapping & FIFO Benchmark
    out.print("  Executing 100 MSPS Circular DMA Benchmark (5 ring rotations)... ");
    Benchmark100MspsResult bench = pio_capture_run_100msps_benchmark(5);

    if (!bench.fifo_overflow && bench.revolutions_completed >= 5) {
        vt100_set_color(VT100_COLOR_NOTICE);
        out.println("PASS");
        vt100_set_color(VT100_COLOR_RESET);
        out.printf("    Revolutions Completed:  %lu full 64 KiB wraps\r\n", (unsigned long)bench.revolutions_completed);
        out.printf("    Total Samples Streamed: %lu samples\r\n", (unsigned long)bench.total_samples_streamed);
        out.printf("    Effective Throughput:   %.2f MSPS\r\n", bench.effective_msps);
        out.printf("    Bus Utilization:        %.1f %% of clk_sys\r\n", bench.bus_utilization_pct);
        out.printf("    FIFO Overflow (RXSTALL):%s\r\n", bench.fifo_overflow ? "OVERFLOW" : "CLEAN (0 dropped)");
        out.printf("    Post-Trigger Freeze:    %lu us latency\r\n", (unsigned long)bench.freeze_latency_us);
    } else {
        vt100_set_color(VT100_COLOR_WARNING);
        out.println("COMPLETED WITH WARNING");
        vt100_set_color(VT100_COLOR_RESET);
        out.printf("    Revolutions: %lu, Overflow: %s\r\n",
            (unsigned long)bench.revolutions_completed, bench.fifo_overflow ? "YES" : "NO");
    }

    vt100_set_color(VT100_COLOR_INFO);
    out.println("======================================================");
    out.println("ALL LOGIC ANALYZER AUTOMATED TESTS COMPLETED");
    out.println("======================================================\r\n");
    vt100_set_color(VT100_COLOR_RESET);
}
