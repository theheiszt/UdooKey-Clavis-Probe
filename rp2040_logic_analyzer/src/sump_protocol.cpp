#include "sump_protocol.h"
#include "display_ui.h"
#include "trigger_engine.h"

static bool s_sump_active = false;
static uint8_t s_cmd[5];
static uint8_t s_cmd_pos = 0;
static uint32_t s_last_byte_ms = 0;

static uint32_t s_divider = 1;
static uint32_t s_read_count = 65536;
static uint32_t s_delay_count = 0;
static uint32_t s_trigger_mask = 0;
static uint32_t s_trigger_val = 0;
static bool s_trigger_edge = false;
static volatile bool s_sump_armed = false;

void sump_init() {
    s_sump_active = false;
    s_cmd_pos = 0;
    s_last_byte_ms = 0;
    s_divider = 1;
    s_read_count = 65536;
    s_delay_count = 0;
    s_trigger_mask = 0;
    s_trigger_val = 0;
    s_trigger_edge = false;
    s_sump_armed = false;
}

bool sump_is_active() {
    return s_sump_active;
}

void sump_set_active(bool active) {
    s_sump_active = active;
    if (!active) {
        s_sump_armed = false;
        pio_capture_stop();
    }
}

static void send_meta(Stream& port) {
    // 0x01: Device Name (Null-terminated string)
    port.write((uint8_t)SUMP_META_NAME);
    port.print("UDOO KEY Logic Analyzer");
    port.write((uint8_t)0x00);

    // 0x02: FPGA/Engine Version (Null-terminated string)
    port.write((uint8_t)SUMP_META_FPGA_VERSION);
    port.print("RP2040 PIO+DMA v2.0");
    port.write((uint8_t)0x00);

    // 0x03: CPU Version (Null-terminated string)
    port.write((uint8_t)SUMP_META_CPU_VERSION);
    port.print("Dual-Core Cortex-M0+ 133MHz");
    port.write((uint8_t)0x00);

    // 0x20: Number of usable probes (32-bit big-endian integer: 8)
    port.write((uint8_t)0x20);
    port.write((uint8_t)0x00);
    port.write((uint8_t)0x00);
    port.write((uint8_t)0x00);
    port.write((uint8_t)0x08);

    // 0x21: Sample RAM (65,536 Bytes) - 4 bytes Big-Endian
    uint32_t ram_size = MAX_CAPTURE_SAMPLES;
    port.write((uint8_t)SUMP_META_SAMPLE_RAM);
    port.write((uint8_t)(ram_size >> 24));
    port.write((uint8_t)(ram_size >> 16));
    port.write((uint8_t)(ram_size >> 8));
    port.write((uint8_t)(ram_size & 0xFF));

    // 0x23: Max Sample Rate (100,000,000 Hz) - 4 bytes Big-Endian
    uint32_t max_rate = 100000000;
    port.write((uint8_t)SUMP_META_SAMPLE_RATE);
    port.write((uint8_t)(max_rate >> 24));
    port.write((uint8_t)(max_rate >> 16));
    port.write((uint8_t)(max_rate >> 8));
    port.write((uint8_t)(max_rate & 0xFF));

    // 0x40: Number of Probes (8 channels)
    port.write((uint8_t)SUMP_META_PROBES_B);
    port.write((uint8_t)8);

    // 0x41: Protocol Version (2)
    port.write((uint8_t)SUMP_META_PROTOCOL_B);
    port.write((uint8_t)2);

    // 0x00: End of metadata
    port.write((uint8_t)SUMP_META_END);
    port.flush();
}

static void execute_short_cmd(uint8_t cmd, Stream& port) {
    switch (cmd) {
        case SUMP_CMD_RESET:
            s_sump_armed = false;
            pio_capture_stop();
            s_cmd_pos = 0;
            break;

        case SUMP_CMD_ID:
            s_sump_active = true;
            port.write((const uint8_t*)"1ALS", 4);
            port.flush();
            break;

        case SUMP_CMD_META:
            s_sump_active = true;
            send_meta(port);
            break;

        case SUMP_CMD_ARM: {
            s_sump_active = true;
            LogicAnalyzerConfig cfg;
            uint32_t rate = 100000000 / (s_divider > 0 ? s_divider : 1);
            if (rate > 100000000) rate = 100000000;
            if (rate < 2000) rate = 2000;

            cfg.sample_rate_hz = rate;
            cfg.sample_count = (s_read_count <= MAX_CAPTURE_SAMPLES) ? s_read_count : MAX_CAPTURE_SAMPLES;
            cfg.enabled_channels = 0xFF;

            // Calculate pre-trigger ratio from s_read_count and s_delay_count
            uint8_t pre_pct = 0;
            if (s_read_count > 0 && s_delay_count < s_read_count) {
                pre_pct = (uint8_t)(((uint64_t)(s_read_count - s_delay_count) * 100ULL) / s_read_count);
                if (pre_pct > 100) pre_pct = 100;
            }
            cfg.trigger.pre_trigger_pct = pre_pct;

            if (s_trigger_mask != 0) {
                // Count set bits to distinguish single-channel edge/level from pattern
                uint8_t bit_count = 0;
                uint8_t last_ch = 0;
                for (int i = 0; i < 8; i++) {
                    if (s_trigger_mask & (1 << i)) {
                        bit_count++;
                        last_ch = i;
                    }
                }

                if (bit_count == 1) {
                    cfg.trigger_channel = last_ch;
                    cfg.trigger.channel = last_ch;
                    if (s_trigger_edge) {
                        cfg.trigger_mode = (s_trigger_val & (1 << last_ch)) ? TRIG_RISING : TRIG_FALLING;
                        cfg.trigger.mode = (s_trigger_val & (1 << last_ch)) ? TRIG_MODE_EDGE_RISING : TRIG_MODE_EDGE_FALLING;
                    } else {
                        cfg.trigger_mode = (s_trigger_val & (1 << last_ch)) ? TRIG_HIGH : TRIG_LOW;
                        cfg.trigger.mode = (s_trigger_val & (1 << last_ch)) ? TRIG_MODE_LEVEL_HIGH : TRIG_MODE_LEVEL_LOW;
                    }
                } else {
                    cfg.trigger_channel = last_ch;
                    cfg.trigger_mode = TRIG_IMMEDIATE;
                    cfg.trigger.mode = TRIG_MODE_PATTERN;
                    cfg.trigger.pattern_mask = s_trigger_mask & 0xFF;
                    cfg.trigger.pattern_value = s_trigger_val & 0xFF;
                }
            } else {
                cfg.trigger_channel = 0;
                cfg.trigger_mode = TRIG_IMMEDIATE;
                cfg.trigger.mode = TRIG_MODE_AUTO;
            }

            pio_capture_start(cfg);
            s_sump_armed = true;
            break;
        }

        case SUMP_CMD_FINISH:
            s_sump_armed = false;
            pio_capture_stop();
            s_cmd_pos = 0;
            break;

        case SUMP_CMD_QUERY_INPUT: {
            // Read immediate pin states on GPIO 2..9
            uint32_t pins = sio_hw->gpio_in;
            uint8_t ch_val = (pins >> CAPTURE_PIN_BASE) & 0xFF;
            port.write(ch_val);
            port.flush();
            break;
        }

        default:
            break;
    }
}

static void execute_long_cmd(uint8_t* cmd) {
    uint32_t val = (uint32_t)cmd[1] | ((uint32_t)cmd[2] << 8) |
                   ((uint32_t)cmd[3] << 16) | ((uint32_t)cmd[4] << 24);

    switch (cmd[0]) {
        case SUMP_CMD_SET_SAMPLE_RATE: // 0x80
            s_divider = val + 1;
            break;

        case SUMP_CMD_SET_COUNTS: // 0x81
            s_read_count = ((val & 0xFFFF) + 1) * 4;
            s_delay_count = (((val >> 16) & 0xFFFF) + 1) * 4;
            if (s_read_count > MAX_CAPTURE_SAMPLES) {
                s_read_count = MAX_CAPTURE_SAMPLES;
            }
            break;

        case SUMP_CMD_SET_FLAGS: // 0x82
            break;

        case SUMP_CMD_SET_BTRG0_MASK: // 0xC0
            s_trigger_mask = val & 0xFF;
            break;

        case SUMP_CMD_SET_BTRG0_VALUE: // 0xC1
            s_trigger_val = val & 0xFF;
            break;

        case SUMP_CMD_SET_BTRG0_CONFIG: // 0xC2
            // Bit 3 determines edge vs level trigger
            s_trigger_edge = (val & 0x08) != 0;
            break;

        case SUMP_CMD_SET_BTRG1_MASK: // 0xC4
        case SUMP_CMD_SET_BTRG2_MASK: // 0xC8
        case SUMP_CMD_SET_BTRG3_MASK: // 0xCC
            s_trigger_mask |= (val & 0xFF);
            break;

        case SUMP_CMD_SET_BTRG1_VALUE: // 0xC5
        case SUMP_CMD_SET_BTRG2_VALUE: // 0xC9
        case SUMP_CMD_SET_BTRG3_VALUE: // 0xCD
            s_trigger_val |= (val & 0xFF);
            break;

        case SUMP_CMD_SET_BTRG1_CONFIG: // 0xC6
        case SUMP_CMD_SET_BTRG2_CONFIG: // 0xCA
        case SUMP_CMD_SET_BTRG3_CONFIG: // 0xCE
            if ((val & 0x08) != 0) s_trigger_edge = true;
            break;

        default:
            break;
    }
}

void sump_process_byte(uint8_t byte, Stream& port) {
    uint32_t now = millis();
    // Reset buffer if byte arrival stalled mid-command
    if (s_cmd_pos > 0 && (now - s_last_byte_ms > 250)) {
        s_cmd_pos = 0;
    }
    s_last_byte_ms = now;

    // Reset command always immediately clears command state
    if (byte == SUMP_CMD_RESET) {
        s_cmd_pos = 0;
        execute_short_cmd(SUMP_CMD_RESET, port);
        return;
    }

    s_cmd[s_cmd_pos++] = byte;

    // Check if first byte is a short command (MSB == 0)
    if ((s_cmd[0] & 0x80) == 0) {
        execute_short_cmd(s_cmd[0], port);
        s_cmd_pos = 0;
    } else if (s_cmd_pos >= 5) {
        // 5-byte long command complete
        execute_long_cmd(s_cmd);
        s_cmd_pos = 0;
    }
}

void sump_task(Stream& port) {
    if (!s_sump_armed) return;

    // Service acquisition state machine (evaluates trigger stream & post-trigger capture)
    pio_capture_task();

    if (capture_complete) {
        s_sump_armed = false;

        // Use chronologically aligned buffer [0 = oldest, count-1 = newest]
        const uint8_t* aligned = pio_capture_get_aligned_buffer();
        uint32_t count = pio_capture_get_sample_count();
        if (count == 0 || count > MAX_CAPTURE_SAMPLES) {
            count = (s_read_count <= MAX_CAPTURE_SAMPLES) ? s_read_count : MAX_CAPTURE_SAMPLES;
        }
        if (!aligned) aligned = capture_buffer;

        // SUMP / OLS standard transmits samples in reverse order (newest to oldest)
        uint8_t chunk[256];
        uint32_t chunk_idx = 0;

        for (int32_t i = (int32_t)count - 1; i >= 0; i--) {
            chunk[chunk_idx++] = aligned[i];
            if (chunk_idx >= sizeof(chunk)) {
                port.write(chunk, chunk_idx);
                chunk_idx = 0;
            }
        }
        if (chunk_idx > 0) {
            port.write(chunk, chunk_idx);
        }
        port.flush();

        // Update on-screen UI waveform display
        ui_notify_capture_complete();
    }
}
