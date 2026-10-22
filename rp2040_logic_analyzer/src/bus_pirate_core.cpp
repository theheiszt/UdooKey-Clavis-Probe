#include "bus_pirate_core.h"
#include "spiflash_full.h"
#include "onewire_search.h"
#include "i2c_sniffer.h"
#include "macro_engine.h"
#include "sump_protocol.h"
#include "display_ui.h"
#include "spi_slave_transport.h"
#include "acquisition_api.h"
#include "hardware_config.h"
#include "adc_subsystem.h"
#include <ctype.h>

#define ESP32_SERIAL Serial1

BpConfig bp_cfg = {
    .mode             = BP_MODE_HIZ,
    .speed_hz         = 100000,
    .spi_polarity     = 0,
    .spi_phase        = 0,
    .spi_bits         = 8,
    .pullups_enabled  = 0,
    .power_enabled    = 0,
    .cs_pin           = 5, // Probe Bus GP5 = CS
    .bus_started      = 0
};

char last_cmd_output[256] = "Bus Pirate & Instrument Mode Manager Initialized.";
char active_mode_name[16] = "HiZ";

static const char* MODE_NAMES[] = {
    "HiZ", "1-WIRE", "I2C", "SPI", "UART", "2-WIRE", "3-WIRE", "JTAG/SWD", "DIO", "LOGIC", "LED", "USB", "SENSOR"
};

void bp_core_init() {
    ModeMgr.init();
    analogReadResolution(12);
    adc_subsystem_init();
}

void bp_set_mode(BpMode new_mode, Stream* out) {
    if (new_mode >= BP_MODE_COUNT) return;
    bp_cfg.mode = new_mode;
    strncpy(active_mode_name, MODE_NAMES[new_mode], sizeof(active_mode_name));

    InstrumentMode im = InstrumentMode::IDLE;
    uint32_t speed = bp_cfg.speed_hz;

    switch (new_mode) {
        case BP_MODE_HIZ:      im = InstrumentMode::IDLE; break;
        case BP_MODE_1WIRE:    im = InstrumentMode::ONE_WIRE; break;
        case BP_MODE_I2C:      im = InstrumentMode::I2C; speed = speed ? speed : 400000; break;
        case BP_MODE_SPI:      im = InstrumentMode::SPI; speed = speed ? speed : 10000000; break;
        case BP_MODE_UART:     im = InstrumentMode::UART; speed = speed ? speed : 115200; break;
        case BP_MODE_JTAG_SWD: im = InstrumentMode::SWD; break;
        case BP_MODE_DIO:      im = InstrumentMode::DIO; break;
        case BP_MODE_LOGIC:    im = InstrumentMode::LOGIC_ANALYZER; break;
        case BP_MODE_USB:      im = InstrumentMode::USB_SNIFFER; break;
        case BP_MODE_SENSOR:   im = InstrumentMode::SENSOR_TEST; break;
        default:               im = InstrumentMode::IDLE; break;
    }

    ModeMgr.enterMode(im, speed);
    snprintf(last_cmd_output, sizeof(last_cmd_output), "%s active on Probe Bus (GP2..GP9).", active_mode_name);

    if (out) {
        out->printf("\r\nMode selected: %s\r\n", active_mode_name);
        out->print(last_cmd_output);
        out->print("\r\n");
        ModeMgr.printPinout(*out);
    }
}

// ─── Hardware Measurement Implementations ────────────────────────────────────
float bp_read_adc_voltage(uint8_t pin) {
    if (pin == PIN_ADC0) return adc_read_calibrated_voltage(ADC_CH_0, 16);
    if (pin == PIN_ADC1) return adc_read_calibrated_voltage(ADC_CH_1, 16);
    if (pin == PIN_ADC2) return adc_read_calibrated_voltage(ADC_CH_2, 16);
    if (pin == PIN_INTERNAL_VSYS_SENSE) return adc_read_calibrated_voltage(ADC_CH_VSYS, 16);
    return 0.0f;
}

uint32_t bp_measure_frequency(uint8_t pin, uint32_t gate_ms) {
    // RP2040 PWM Counter Mode on pin (must support PWM input)
    uint slice = pwm_gpio_to_slice_num(pin);
    gpio_set_function(pin, GPIO_FUNC_PWM);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);
    pwm_init(slice, &cfg, false);

    pwm_set_counter(slice, 0);
    pwm_set_enabled(slice, true);
    delay(gate_ms);
    pwm_set_enabled(slice, false);

    uint16_t count = pwm_get_counter(slice);
    return (uint32_t)count * (1000 / gate_ms);
}

void bp_generate_pwm(uint8_t pin, uint32_t freq_hz, uint8_t duty_percent) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pin);
    uint chan  = pwm_gpio_to_channel(pin);

    uint32_t clock_freq = clock_get_hz(clk_sys);
    uint32_t div16 = (clock_freq * 16) / (freq_hz * 65536) + 1;
    if (div16 < 16) div16 = 16;
    uint32_t wrap = (clock_freq * 16) / (div16 * freq_hz) - 1;

    pwm_set_clkdiv_int_frac(slice, div16 / 16, div16 % 16);
    pwm_set_wrap(slice, wrap);
    pwm_set_chan_level(slice, chan, (wrap * duty_percent) / 100);
    pwm_set_enabled(slice, true);
}

void bp_stop_pwm(uint8_t pin) {
    uint slice = pwm_gpio_to_slice_num(pin);
    pwm_set_enabled(slice, false);
    gpio_set_function(pin, GPIO_FUNC_SIO);
    pinMode(pin, INPUT);
}

// ─── Bus Pirate Command & Macro Syntax Parser ────────────────────────────────
void bp_execute_command(const char* cmd_line, Stream& out) {
    if (!cmd_line || !*cmd_line) return;

    // Skip leading whitespace
    while (isspace(*cmd_line)) cmd_line++;
    if (!*cmd_line) return;

    // 1. Check for Shell Named Commands first
    if (strncmp(cmd_line, "scan", 4) == 0 || strncmp(cmd_line, "i2c scan", 8) == 0) {
        out.println("\r\n[I2C Scanner] Searching 7-bit addresses (0x08 - 0x77)...");
        Wire.begin();
        uint8_t count = 0;
        char scan_buf[128] = "I2C Found: ";

        for (uint8_t addr = 8; addr < 120; addr++) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                out.printf("  -> Device detected at address 0x%02X\r\n", addr);
                char hex[8];
                snprintf(hex, sizeof(hex), "0x%02X ", addr);
                strncat(scan_buf, hex, sizeof(scan_buf) - strlen(scan_buf) - 1);
                count++;
            }
        }
        if (count == 0) {
            out.println("  No I2C devices responded on bus.");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "I2C Scan: No devices");
        } else {
            out.printf("Done: %u devices found.\r\n", count);
            strncpy(last_cmd_output, scan_buf, sizeof(last_cmd_output));
        }
        return;
    }

    // 0. SUMP Protocol toggle
    if (strcmp(cmd_line, "sump") == 0 || strcmp(cmd_line, "binmode sump") == 0) {
        sump_set_active(true);
        out.println("\r\n[SUMP Mode Active] Listening for Sigrok / PulseView / SUMP client on USB CDC.");
        snprintf(last_cmd_output, sizeof(last_cmd_output), "Mode: SUMP (Sigrok)");
        return;
    }

    // 0b. Macro commands
    if (strncmp(cmd_line, "macro", 5) == 0) {
        const char* sub = cmd_line + 5;
        while (isspace(*sub)) sub++;
        if (!*sub || strcmp(sub, "list") == 0 || strcmp(sub, "-l") == 0) {
            macro_list(out);
        } else if (strncmp(sub, "set ", 4) == 0) {
            uint8_t mid = atoi(sub + 4);
            const char* rest = strchr(sub + 4, ' ');
            if (rest) {
                while (isspace(*rest)) rest++;
                char name[24] = {0};
                const char* cmd_part = strchr(rest, ' ');
                if (cmd_part) {
                    size_t nlen = cmd_part - rest;
                    if (nlen > 23) nlen = 23;
                    strncpy(name, rest, nlen);
                    while (isspace(*cmd_part)) cmd_part++;
                    macro_set(mid, name, cmd_part);
                    out.printf("Macro %d set: [%s] -> %s\r\n", mid, name, cmd_part);
                }
            }
        } else {
            uint8_t mid = atoi(sub);
            macro_run(mid, out);
        }
        return;
    }

    // 0c. Real-time I2C Bus Sniffer
    if (strncmp(cmd_line, "sniff", 5) == 0) {
        i2c_sniffer_start(2, 3, out);
        return;
    }

    // 0d. Probe Pinout visualizer
    if (strcmp(cmd_line, "pinout") == 0 || strcmp(cmd_line, "pins") == 0) {
        out.println("\r\n=== UDOO Key Probe Header (Channels 0 - 7) ===");
        uint32_t gpio_in = sio_hw->gpio_in;
        for (int ch = 0; ch < 8; ch++) {
            uint8_t pin = 2 + ch;
            bool lvl = (gpio_in & (1UL << pin)) != 0;
            out.printf("  Ch%d (GPIO %d): [%s] (Logic %s)\r\n", ch, pin, lvl ? "HIGH" : "LOW ", lvl ? "1" : "0");
        }
        snprintf(last_cmd_output, sizeof(last_cmd_output), "Pinout: 8 Channels probed");
        return;
    }

    // 0e. Hardware Validation 1.0 Shortcuts
    if (strcmp(cmd_line, "self") == 0 || strcmp(cmd_line, "selftest") == 0) {
        SelfTestResult st = acquisition_run_self_test(out);
        snprintf(last_cmd_output, sizeof(last_cmd_output),
                 "Self-Test: %u/%u passed (%s)",
                 st.passed_tests, st.total_tests, st.all_passed ? "PASS" : "FAIL");
        return;
    }
    if (strcmp(cmd_line, "hammer") == 0) {
        SystemStressResult s_res = acquisition_run_system_stress_test(out, 3000);
        snprintf(last_cmd_output, sizeof(last_cmd_output),
                 "Full Stress: %s (drops=%lu, fifo_ovf=%lu)",
                 s_res.passed ? "PASS" : "FAIL", (unsigned long)s_res.samples_dropped, (unsigned long)s_res.fifo_overflow_count);
        return;
    }

    // Enhanced SPI Flash Suite
    if (strncmp(cmd_line, "flash", 5) == 0) {
        const char* sub = cmd_line + 5;
        while (isspace(*sub)) sub++;
        if (!*sub || strcmp(sub, "probe") == 0 || strcmp(sub, "id") == 0) {
            flash_full_probe(bp_cfg.cs_pin, out);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Flash: JEDEC Probed");
        } else if (strcmp(sub, "status") == 0 || strcmp(sub, "sr") == 0) {
            flash_full_status(bp_cfg.cs_pin, out);
        } else if (strcmp(sub, "sfdp") == 0) {
            flash_full_sfdp(bp_cfg.cs_pin, out);
        } else if (strncmp(sub, "read", 4) == 0) {
            uint32_t addr = 0, len = 256;
            sscanf(sub + 4, "%lx %lu", &addr, &len);
            flash_full_read_hex(bp_cfg.cs_pin, addr, len, out);
        } else if (strncmp(sub, "dump", 4) == 0) {
            uint32_t pages = 16;
            sscanf(sub + 4, "%lu", &pages);
            flash_full_dump(bp_cfg.cs_pin, pages, out);
        } else if (strncmp(sub, "write", 5) == 0) {
            uint32_t addr = 0, val = 0;
            sscanf(sub + 5, "%lx %lx", &addr, &val);
            flash_full_write_byte(bp_cfg.cs_pin, addr, (uint8_t)val, out);
        } else if (strncmp(sub, "erase_sector", 12) == 0) {
            uint32_t addr = 0;
            sscanf(sub + 12, "%lx", &addr);
            flash_full_erase_sector(bp_cfg.cs_pin, addr, out);
        } else if (strncmp(sub, "erase_block", 11) == 0) {
            uint32_t addr = 0;
            sscanf(sub + 11, "%lx", &addr);
            flash_full_erase_block(bp_cfg.cs_pin, addr, out);
        } else if (strncmp(sub, "erase_chip", 10) == 0) {
            flash_full_erase_chip(bp_cfg.cs_pin, out);
        } else if (strncmp(sub, "protect on", 10) == 0) {
            flash_full_protect(bp_cfg.cs_pin, true, out);
        } else if (strncmp(sub, "protect off", 11) == 0) {
            flash_full_protect(bp_cfg.cs_pin, false, out);
        }
        return;
    }

    // ─── First-Class Hardware Trigger Engine ─────────────────────────────────
    if (strncmp(cmd_line, "trig", 4) == 0 || strncmp(cmd_line, "trigger", 7) == 0) {
        const char* sub = (strncmp(cmd_line, "trig", 4) == 0) ? (cmd_line + 4) : (cmd_line + 7);
        while (isspace(*sub)) sub++;

        TriggerEngineConfig tc = trigger_engine_get_config();

        if (!*sub || strncmp(sub, "status", 6) == 0) {
            trigger_print_status(out);
            return;
        }

        if (strncmp(sub, "auto", 4) == 0) {
            tc.mode = TRIG_MODE_AUTO;
            trigger_engine_configure(tc);
            out.println("\r\n[Trigger] Mode set to AUTO (Immediate Free-Run)");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Trig: AUTO");
            return;
        }

        if (strncmp(sub, "edge", 4) == 0) {
            sub += 4;
            while (isspace(*sub)) sub++;
            uint8_t ch = 0;
            if (isdigit(*sub)) { ch = atoi(sub); while (isdigit(*sub)) sub++; }
            while (isspace(*sub)) sub++;
            if (strncmp(sub, "fall", 4) == 0) tc.mode = TRIG_MODE_EDGE_FALLING;
            else if (strncmp(sub, "any", 3) == 0) tc.mode = TRIG_MODE_EDGE_ANY;
            else tc.mode = TRIG_MODE_EDGE_RISING;
            tc.channel = ch;
            trigger_engine_configure(tc);
            out.printf("\r\n[Trigger] Mode set to %s on CH%u (GPIO %u)\r\n",
                       trigger_mode_to_string(tc.mode), ch, 2 + ch);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Trig: CH%u %s", ch,
                     (tc.mode == TRIG_MODE_EDGE_FALLING) ? "FALL" : (tc.mode == TRIG_MODE_EDGE_ANY) ? "ANY" : "RISE");
            return;
        }

        if (strncmp(sub, "level", 5) == 0) {
            sub += 5;
            while (isspace(*sub)) sub++;
            uint8_t ch = 0;
            if (isdigit(*sub)) { ch = atoi(sub); while (isdigit(*sub)) sub++; }
            while (isspace(*sub)) sub++;
            if (strncmp(sub, "low", 3) == 0 || strncmp(sub, "0", 1) == 0) tc.mode = TRIG_MODE_LEVEL_LOW;
            else tc.mode = TRIG_MODE_LEVEL_HIGH;
            tc.channel = ch;
            trigger_engine_configure(tc);
            out.printf("\r\n[Trigger] Mode set to %s on CH%u\r\n",
                       trigger_mode_to_string(tc.mode), ch);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Trig: CH%u %s", ch, (tc.mode == TRIG_MODE_LEVEL_LOW) ? "LOW" : "HIGH");
            return;
        }

        if (strncmp(sub, "qual", 4) == 0) {
            sub += 4;
            while (isspace(*sub)) sub++;
            uint8_t edge_ch = 0, qual_ch = 1;
            char edge_type[8] = "rise", qual_lvl[8] = "high";
            sscanf(sub, "%hhu %7s %hhu %7s", &edge_ch, edge_type, &qual_ch, qual_lvl);
            tc.mode = TRIG_MODE_LEVEL_QUAL;
            tc.channel = edge_ch;
            tc.qual_channel = qual_ch;
            tc.qual_level = (strncmp(qual_lvl, "low", 3) == 0 || strncmp(qual_lvl, "0", 1) == 0) ? 0 : 1;
            trigger_engine_configure(tc);
            out.printf("\r\n[Trigger] Level Qualification: CH%u %s while CH%u is %s\r\n",
                       edge_ch, edge_type, qual_ch, tc.qual_level ? "HIGH" : "LOW");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Trig: QL C%u+C%u", edge_ch, qual_ch);
            return;
        }

        if (strncmp(sub, "width", 5) == 0) {
            sub += 5;
            while (isspace(*sub)) sub++;
            uint8_t ch = 0;
            char comp[4] = "lt";
            uint32_t dur_ns = 100;
            sscanf(sub, "%hhu %3s %lu", &ch, comp, &dur_ns);
            tc.mode = (strncmp(comp, "gt", 2) == 0) ? TRIG_MODE_WIDTH_GT : TRIG_MODE_WIDTH_LT;
            tc.channel = ch;
            tc.param = dur_ns;
            trigger_engine_configure(tc);
            out.printf("\r\n[Trigger] Pulse-Width Glitch/Runt Detector on CH%u: pulse %s %lu ns\r\n",
                       ch, (tc.mode == TRIG_MODE_WIDTH_LT) ? "< (Glitch/Runt)" : ">", (unsigned long)dur_ns);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Trig: C%u %s%luns", ch, (tc.mode == TRIG_MODE_WIDTH_LT) ? "<" : ">", (unsigned long)dur_ns);
            return;
        }

        if (strncmp(sub, "pat", 3) == 0 || strncmp(sub, "pattern", 7) == 0) {
            const char* pstr = strchr(sub, ' ');
            uint32_t mask = 0xFF, val = 0x00;
            if (pstr) sscanf(pstr, "%lx %lx", &mask, &val);
            tc.mode = TRIG_MODE_PATTERN;
            tc.pattern_mask = (uint8_t)mask;
            tc.pattern_value = (uint8_t)val;
            trigger_engine_configure(tc);
            out.printf("\r\n[Trigger] Pattern match: (GPIO & 0x%02X) == 0x%02X\r\n",
                       tc.pattern_mask, tc.pattern_value);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Trig: PAT &0x%02X=0x%02X", tc.pattern_mask, tc.pattern_value);
            return;
        }

        if (strncmp(sub, "ext", 3) == 0) {
            tc.mode = TRIG_MODE_EXT_TRIG;
            tc.channel = PIN_EXT_TRIG;
            trigger_engine_configure(tc);
            out.println("\r\n[Trigger] External Trigger Input enabled on GPIO 10 (EXT TRIG)");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Trig: EXT (GP10)");
            return;
        }

        if (strncmp(sub, "pos", 3) == 0 || strncmp(sub, "pre", 3) == 0) {
            const char* pstr = strchr(sub, ' ');
            if (pstr) {
                uint8_t pct = atoi(pstr);
                if (pct > 100) pct = 100;
                tc.pre_trigger_pct = pct;
                trigger_engine_configure(tc);
                out.printf("\r\n[Trigger] Buffer Position set: %u%% Pre-Trigger | %u%% Post-Trigger\r\n",
                           pct, 100 - pct);
                snprintf(last_cmd_output, sizeof(last_cmd_output), "Trig Pos: %u%% Pre", pct);
            } else {
                out.printf("\r\n[Trigger] Current Position: %u%% Pre-Trigger\r\n", tc.pre_trigger_pct);
            }
            return;
        }

        if (strncmp(sub, "seq", 3) == 0) {
            const char* ssub = sub + 3;
            while (isspace(*ssub)) ssub++;
            if (strncmp(ssub, "clear", 5) == 0) {
                tc.stage_count = 0;
                tc.mode = TRIG_MODE_AUTO;
                trigger_engine_configure(tc);
                out.println("\r\n[Trigger Sequence] Cleared all stages.");
                return;
            }
            if (strncmp(ssub, "add", 3) == 0) {
                if (tc.stage_count >= MAX_TRIGGER_STAGES) {
                    out.printf("\r\n[Trigger Sequence] Maximum stages (%u) reached.\r\n", MAX_TRIGGER_STAGES);
                    return;
                }
                ssub += 3;
                while (isspace(*ssub)) ssub++;
                TriggerStage& st = tc.stages[tc.stage_count];
                st.timeout_us = 0;
                st.channel = 0;
                st.param = 0;
                st.pattern_mask = 0xFF;
                st.pattern_value = 0x00;

                if (strncmp(ssub, "edge", 4) == 0) {
                    uint8_t ch = 0; char etype[8] = "rise"; uint32_t tout = 0;
                    sscanf(ssub + 4, "%hhu %7s %lu", &ch, etype, &tout);
                    st.mode = (strncmp(etype, "fall", 4) == 0) ? TRIG_MODE_EDGE_FALLING : TRIG_MODE_EDGE_RISING;
                    st.channel = ch;
                    st.timeout_us = tout;
                } else if (strncmp(ssub, "pat", 3) == 0) {
                    uint32_t mask = 0xFF, val = 0, tout = 0;
                    sscanf(ssub + 3, "%lx %lx %lu", &mask, &val, &tout);
                    st.mode = TRIG_MODE_PATTERN;
                    st.pattern_mask = (uint8_t)mask;
                    st.pattern_value = (uint8_t)val;
                    st.timeout_us = tout;
                } else {
                    st.mode = TRIG_MODE_AUTO;
                }
                tc.stage_count++;
                tc.mode = TRIG_MODE_SEQUENCE;
                trigger_engine_configure(tc);
                out.printf("\r\n[Trigger Sequence] Added Stage #%u: %s (Timeout: %lu us)\r\n",
                           tc.stage_count, trigger_mode_to_string(st.mode), (unsigned long)st.timeout_us);
                return;
            }
            if (strncmp(ssub, "arm", 3) == 0) {
                tc.mode = TRIG_MODE_SEQUENCE;
                trigger_engine_configure(tc);
                trigger_engine_arm(8192, tc.pre_trigger_pct);
                out.println("\r\n[Trigger Sequence] Armed multi-stage sequence!");
                return;
            }
        }

        if (strncmp(sub, "proto", 5) == 0) {
            char proto[8] = "uart";
            uint32_t val = 0x55;
            sscanf(sub + 5, "%7s %lx", proto, &val);
            if (strncmp(proto, "i2c", 3) == 0) {
                tc.mode = TRIG_MODE_I2C_ADDR;
                tc.param = val;
                out.printf("\r\n[Trigger] I2C Address Trigger set to 0x%02X\r\n", (uint8_t)val);
            } else if (strncmp(proto, "spi", 3) == 0) {
                tc.mode = TRIG_MODE_SPI_WORD;
                tc.param = val;
                out.printf("\r\n[Trigger] SPI Word Trigger set to 0x%02X\r\n", (uint8_t)val);
            } else {
                tc.mode = TRIG_MODE_UART_BYTE;
                tc.param = val;
                out.printf("\r\n[Trigger] UART Byte Trigger set to 0x%02X\r\n", (uint8_t)val);
            }
            trigger_engine_configure(tc);
            return;
        }

        if (strncmp(sub, "arm", 3) == 0) {
            trigger_engine_arm(8192, tc.pre_trigger_pct);
            out.println("\r\n[Trigger] Armed! Waiting for condition in circular SRAM buffer...");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Trig: ARMED");
            return;
        }

        if (strncmp(sub, "stop", 4) == 0 || strncmp(sub, "reset", 5) == 0) {
            trigger_engine_reset();
            out.println("\r\n[Trigger] Engine reset to IDLE.");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Trig: IDLE");
            return;
        }

        out.println("\r\nUsage: trig <auto|edge|level|qual|width|pat|ext|pos|seq|proto|arm|status>");
        return;
    }

    // ─── Hardware Front-End Control (Protection & 74HC74 Prescaler) ──────────
    if (strncmp(cmd_line, "frontend", 8) == 0 || strncmp(cmd_line, "fe", 2) == 0) {
        const char* sub = (strncmp(cmd_line, "frontend", 8) == 0) ? (cmd_line + 8) : (cmd_line + 2);
        while (isspace(*sub)) sub++;
        if (!*sub || strncmp(sub, "status", 6) == 0) {
            frontend_print_status(out);
            return;
        }
        if (strncmp(sub, "protect on", 10) == 0 || strncmp(sub, "prot on", 7) == 0 || strncmp(sub, "prot 1", 6) == 0) {
            frontend_set_protection(true);
            out.println("\r\n[Front-End] Input Protection & Level Shifting ENABLED (Safe)");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "FE Protect: ON");
            return;
        }
        if (strncmp(sub, "protect off", 11) == 0 || strncmp(sub, "prot off", 8) == 0 || strncmp(sub, "prot 0", 6) == 0) {
            frontend_set_protection(false);
            out.println("\r\n[Front-End] Input Protection DISABLED (Direct Bypass)");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "FE Protect: OFF");
            return;
        }
        if (strncmp(sub, "prescaler", 9) == 0 || strncmp(sub, "div", 3) == 0) {
            const char* dstr = strchr(sub, ' ');
            if (dstr) {
                uint8_t div = atoi(dstr);
                frontend_set_prescaler(div);
                out.printf("\r\n[Front-End] 74HC74 Prescaler set to 1:%u\r\n", frontend_get_prescaler());
                snprintf(last_cmd_output, sizeof(last_cmd_output), "Prescaler: 1:%u", frontend_get_prescaler());
            } else {
                out.printf("\r\n[Front-End] Current prescaler: 1:%u (Usage: frontend prescaler <1|2|4>)\r\n", frontend_get_prescaler());
            }
            return;
        }
        out.println("\r\nUsage: frontend <status | protect on/off | prescaler 1/2/4>");
        return;
    }

    // ─── Scope & Acquisition API Commands ────────────────────────────────────
    if (strncmp(cmd_line, "rate", 4) == 0 || strncmp(cmd_line, "samplerate", 10) == 0) {
        const char* sub = (strncmp(cmd_line, "rate", 4) == 0) ? (cmd_line + 4) : (cmd_line + 10);
        while (isspace(*sub)) sub++;
        if (isdigit(*sub)) {
            uint32_t r = atol(sub);
            if (r > 0) {
                bp_cfg.speed_hz = r;
                out.printf("\r\n[Acquisition] Sample rate set to %lu Hz\r\n", (unsigned long)r);
                snprintf(last_cmd_output, sizeof(last_cmd_output), "Rate: %lu Hz", (unsigned long)r);
            }
        } else {
            out.printf("\r\n[Acquisition] Actual Sample Rate: %lu Hz\r\n", (unsigned long)get_actual_sample_rate());
        }
        return;
    }

    if (strncmp(cmd_line, "getcap", 6) == 0) {
        const uint8_t* buf = pio_capture_get_aligned_buffer();
        uint32_t trig_off  = pio_capture_get_trigger_offset();
        uint32_t total     = 8192;
        out.printf("\r\n=== CAPTURE DUMP: %lu samples (Trigger Marker @ sample #%lu) ===\r\n",
                   (unsigned long)total, (unsigned long)trig_off);
        for (uint32_t i = 0; i < 256; i++) {
            if (i % 16 == 0) out.printf("\r\n%04X: ", i);
            out.printf("%02X ", buf[i]);
        }
        out.println("\r\n... [Truncated for terminal: 256 of 8192 samples shown. Use companion app for full binary stream]");
        return;
    }

    if (strcmp(cmd_line, "start") == 0 || strcmp(cmd_line, "capture") == 0 || strcmp(cmd_line, "run") == 0 || strcmp(cmd_line, "cap") == 0) {
        if (!acquisition_is_running()) {
            CaptureConfig acq_cfg;
            acq_cfg.sample_rate_hz   = bp_cfg.speed_hz ? bp_cfg.speed_hz : 10000000;
            acq_cfg.sample_count     = 8192;
            acq_cfg.enabled_channels = 0xFF;
            acq_cfg.trigger          = trigger_engine_get_config();
            acquisition_configure(acq_cfg);
            if (acquisition_arm()) {
                bp_set_mode(BP_MODE_LOGIC, &out);
                out.println("\r\n[Acquisition] Logic Analyzer armed. Waiting for trigger...");
                snprintf(last_cmd_output, sizeof(last_cmd_output), "Scope Armed: %luHz", (unsigned long)acq_cfg.sample_rate_hz);
            } else {
                out.println("\r\n[Acquisition] Failed to arm. Check instrument mode.");
            }
        } else {
            out.println("\r\n[Acquisition] Already running.");
        }
        return;
    }

    if (strcmp(cmd_line, "stop") == 0 || strcmp(cmd_line, "abort") == 0) {
        if (acquisition_is_running()) {
            acquisition_abort();
            out.println("\r\n[Acquisition] Capture aborted.");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Scope Aborted");
        } else {
            out.println("\r\n[Acquisition] Not running.");
        }
        return;
    }

    // ─── Instrument Mode Manager CLI Command ──────────────────────────────────
    if (strncmp(cmd_line, "mode", 4) == 0) {
        const char* sub = cmd_line + 4;
        while (isspace(*sub)) sub++;
        if (!*sub || strcmp(sub, "status") == 0) {
            ModeMgr.printStatus(out);
            ModeMgr.printPinout(out);
        } else if (strcmp(sub, "pins") == 0 || strcmp(sub, "pinout") == 0) {
            ModeMgr.printPinout(out);
        } else if (strcmp(sub, "idle") == 0 || strcmp(sub, "hiz") == 0) {
            ModeMgr.enterMode(InstrumentMode::IDLE, 0, &out);
        } else if (strcmp(sub, "logic") == 0 || strcmp(sub, "la") == 0) {
            ModeMgr.enterMode(InstrumentMode::LOGIC_ANALYZER, 0, &out);
        } else if (strcmp(sub, "spi") == 0) {
            ModeMgr.enterMode(InstrumentMode::SPI, bp_cfg.speed_hz, &out);
        } else if (strcmp(sub, "i2c") == 0) {
            ModeMgr.enterMode(InstrumentMode::I2C, bp_cfg.speed_hz, &out);
        } else if (strcmp(sub, "uart") == 0) {
            ModeMgr.enterMode(InstrumentMode::UART, bp_cfg.speed_hz, &out);
        } else if (strcmp(sub, "swd") == 0) {
            ModeMgr.enterMode(InstrumentMode::SWD, 0, &out);
        } else if (strcmp(sub, "onewire") == 0 || strcmp(sub, "1wire") == 0) {
            ModeMgr.enterMode(InstrumentMode::ONE_WIRE, 0, &out);
        } else if (strcmp(sub, "pwm") == 0) {
            ModeMgr.enterMode(InstrumentMode::PWM_GENERATOR, bp_cfg.speed_hz ? bp_cfg.speed_hz : 1000, &out);
        } else if (strcmp(sub, "glitch") == 0) {
            ModeMgr.enterMode(InstrumentMode::GLITCH, 0, &out);
        } else if (strcmp(sub, "eeprom") == 0) {
            ModeMgr.enterMode(InstrumentMode::EEPROM, bp_cfg.speed_hz, &out);
        } else if (strcmp(sub, "flash") == 0) {
            ModeMgr.enterMode(InstrumentMode::FLASH, bp_cfg.speed_hz, &out);
        } else {
            out.println("\r\n=== INSTRUMENT MODE MANAGER COMMANDS ===");
            out.println("  mode [status]   : Show active mode and pin assignment");
            out.println("  mode pins       : Print physical probe bus pinout (GP2..GP9)");
            out.println("  mode idle       : Switch probe bus to Safe High-Z");
            out.println("  mode logic      : 8-Channel 100 MSPS Logic Analyzer");
            out.println("  mode spi        : SPI Master (GP2:SCK, GP3:MOSI, GP4:MISO, GP5:CS)");
            out.println("  mode i2c        : I2C Master (GP2:SDA, GP3:SCL)");
            out.println("  mode uart       : UART Transceiver (GP4:TX, GP5:RX)");
            out.println("  mode swd        : ARM SWD Debugger (GP7:SWCLK, GP8:SWDIO)");
            out.println("  mode onewire    : 1-Wire Master (GP6:DQ)");
            out.println("  mode pwm        : PWM Clock Generator (GP2:OUT)");
            out.println("  mode glitch     : Hardware Fault Injector (GP2:OUT)");
            out.println("  mode eeprom     : I2C EEPROM Shell");
            out.println("  mode flash      : SPI NOR Flash Shell");
        }
        return;
    }

    // ─── Inter-MCU Transport Selection & Status (Sections 10, 13, 14) ────────
    if (strncmp(cmd_line, "transport", 9) == 0) {
        const char* sub = cmd_line + 9;
        while (isspace(*sub)) sub++;
        if (strncmp(sub, "spi", 3) == 0) {
            out.println("\r\nEnabling jumper-wired MCU SPI...");
            out.println("  ESP32 IO14 -> RP2040 GP2 : SCK");
            out.println("  ESP32 IO12 -> RP2040 GP3 : MOSI");
            out.println("  ESP32 IO35 -> RP2040 GP4 : MISO");
            out.println("  ESP32 IO15 -> RP2040 GP5 : CS");
            out.println("  GND        -> GND");
            out.println("\r\nWARNING:");
            out.println("  GP2-GP5 are now reserved for inter-chip transport.");
            out.println("  Logic analyzer and external SPI probe modes are unavailable.");

            if (ModeMgr.enterMode(InstrumentMode::INTERCHIP_SPI, 10000000, &out)) {
                out.println("\r\nMCU SPI transport: ACTIVE");
                out.println("Clock: 10 MHz");
                snprintf(last_cmd_output, sizeof(last_cmd_output), "Transport: Jumper SPI Active (GP2..GP5 @ 10MHz)");
            } else {
                snprintf(last_cmd_output, sizeof(last_cmd_output), "Transport: Failed to enter INTERCHIP_SPI (resource busy)");
            }
            return;
        } else if (strncmp(sub, "uart", 4) == 0) {
            out.println("\r\nRestoring Native Factory UART transport (GP0/GP1)...");
            ModeMgr.enterMode(InstrumentMode::IDLE, 0, &out);
            out.println("GP2-GP5 released to Safe Hi-Z.");
            out.println("MCU UART transport: ACTIVE");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Transport: Native Factory UART Active (GP0/GP1)");
            return;
        } else {
            bool spi_active = spi_slave_transport_is_active();
            out.println("\r\n==================================================");
            out.println(" MCU TRANSPORT");
            out.println("==================================================");
            out.println(" Factory UART       : GP0/GP1 (Hardwired to ESP32 IO22/IO19)");
            out.println(" UART status        : AVAILABLE (Default Inter-MCU Bus)");
            out.println("");
            out.println(" Jumper SPI         : GP2-GP5 (Optional Jumper to ESP32 UEXT)");
            out.println(" SPI clock          : 10 MHz");
            out.printf( " SPI status         : %s\r\n", spi_active ? "ACTIVE (GP2..GP5 Owned)" : "NOT CONFIGURED / SAFE HI-Z");
            out.println("");
            out.printf( " Current transport  : %s\r\n", spi_active ? "INTERCHIP_SPI (Optional Jumper)" : "UART (Native Factory)");
            out.println("==================================================");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Transport: %s", spi_active ? "SPI (GP2..GP5)" : "UART (GP0/GP1)");
            return;
        }
    }

    // ─── SPI Transport & Hardware Verification Test Suite ────────────────────
    if (strncmp(cmd_line, "test", 4) == 0) {
        const char* sub = cmd_line + 4;
        while (isspace(*sub)) sub++;

        // Section 15: Automated Inter-MCU SPI Transport Self-Test
        if (strncmp(sub, "mcu_spi", 7) == 0) {
            out.println("\r\n==================================================");
            out.println(" MCU SPI TRANSPORT VERIFICATION TEST (Section 15)");
            out.println("==================================================");
            out.println("1. Engaging INTERCHIP_SPI mode on GP2..GP5...");
            if (!ModeMgr.enterMode(InstrumentMode::INTERCHIP_SPI, 10000000, &out)) {
                out.println("[FAIL] Cannot engage INTERCHIP_SPI (pins busy).");
                snprintf(last_cmd_output, sizeof(last_cmd_output), "test mcu_spi: FAIL (Pins busy)");
                return;
            }

            out.println("2. Generating deterministic synthetic 00..FF test pattern (8192 bytes)...");
            spi_slave_test_generate_synthetic(8192);

            out.println("3. Signaling ESP32 master over Factory UART to execute SPI test suite...");
            PacketHeader req = { PROTOCOL_SYNC_BYTE, PKT_PIRATE_TERMINAL, 12 };
            ESP32_SERIAL.write((uint8_t*)&req, sizeof(req));
            ESP32_SERIAL.write((const uint8_t*)"test mcu_spi", 12);

            out.println("4. Awaiting ESP32 master SPI verification results (1500 ms timeout)...");
            unsigned long t_start = millis();
            bool got_response = false;
            while (millis() - t_start < 1500) {
                if (ESP32_SERIAL.available()) {
                    while (ESP32_SERIAL.available()) {
                        char c = ESP32_SERIAL.read();
                        out.write(c);
                        got_response = true;
                    }
                    break;
                }
                delay(10);
            }

            if (!got_response) {
                out.println("\r\nMCU SPI SELF TEST");
                out.println("-----------------");
                out.println("Clock                 : 10 MHz");
                out.println("TX packets            : 0");
                out.println("RX packets            : 0");
                out.println("CRC failures          : 0");
                out.println("Sequence errors       : 0");
                out.println("Timeouts              : 1 (No response from ESP32 master)");
                out.println("Data mismatches       : 0");
                out.println("");
                out.println("RESULT: FAIL (Check jumper wires: ESP32 UEXT IO14/12/35/15 <-> RP2040 GP2..GP5)");
            }

            out.println("\r\n5. Restoring Safe Mode and Native Factory UART...");
            ModeMgr.enterMode(InstrumentMode::IDLE, 0, &out);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "test mcu_spi: %s", got_response ? "PASS" : "FAIL (No jumpers)");
            return;
        }
        if (strncmp(sub, "self", 4) == 0) {
            SelfTestResult st = acquisition_run_self_test(out);
            snprintf(last_cmd_output, sizeof(last_cmd_output),
                     "Self-Test: %u/%u passed (%s)",
                     st.passed_tests, st.total_tests, st.all_passed ? "PASS" : "FAIL");
            return;
        } else if (strncmp(sub, "freq", 4) == 0 || strncmp(sub, "source", 6) == 0) {
            FrequencySweepResult f_res = acquisition_run_frequency_sweep(out);
            snprintf(last_cmd_output, sizeof(last_cmd_output),
                     "Freq Sweep: %u/%u passed (%s)",
                     f_res.stages_passed, f_res.stages_count, f_res.passed ? "PASS" : "FAIL");
            return;
        } else if (strncmp(sub, "ext", 3) == 0) {
            const char* num_str = strchr(sub, ' ');
            uint32_t iter = 1000;
            if (num_str) {
                uint32_t i = atoi(num_str);
                if (i > 0) iter = i;
            }
            ExternalTriggerResult ext_res = acquisition_run_ext_trigger_test(iter, out);
            snprintf(last_cmd_output, sizeof(last_cmd_output),
                     "Ext Trig: %.2f%% det (%s, hits=%lu)",
                     ext_res.determinism_pct, ext_res.passed ? "PASS" : "FAIL", (unsigned long)ext_res.exact_hits);
            return;
        } else if (strncmp(sub, "hammer", 6) == 0 || strncmp(sub, "full_stress", 11) == 0) {
            const char* num_str = strchr(sub, ' ');
            uint32_t ms = 3000;
            if (num_str) {
                uint32_t m = atoi(num_str);
                if (m > 0) ms = m;
            }
            SystemStressResult s_res = acquisition_run_system_stress_test(out, ms);
            snprintf(last_cmd_output, sizeof(last_cmd_output),
                     "Full Stress: %s (drops=%lu, fifo_ovf=%lu)",
                     s_res.passed ? "PASS" : "FAIL", (unsigned long)s_res.samples_dropped, (unsigned long)s_res.fifo_overflow_count);
            return;
        } else if (strncmp(sub, "la_stress", 9) == 0 || strncmp(sub, "stress", 6) == 0) {
            bool full = (strstr(sub, "full") != nullptr || strstr(sub, "10s") != nullptr);
            StressTestSummary sum = acquisition_run_stress_test(out, full);
            snprintf(last_cmd_output, sizeof(last_cmd_output),
                     "Stress: %u/%u passed (%s)",
                     sum.stages_passed, sum.stages_run, sum.overall_pass ? "PASS" : "FAIL");
            return;
        } else if (strncmp(sub, "pattern", 7) == 0) {
            const char* num_str = strchr(sub, ' ');
            uint32_t cycles = 1;
            if (num_str) {
                uint32_t c = atoi(num_str);
                if (c > 0) cycles = c;
            }
            PatternIntegrityResult pres = acquisition_run_pattern_test(cycles, out);
            snprintf(last_cmd_output, sizeof(last_cmd_output),
                     "Pattern: %s (%llu tested, %u drop)",
                     pres.passed ? "PASS" : "FAIL", (unsigned long long)pres.samples_tested, pres.missing_samples);
            return;
        } else if (strncmp(sub, "100msps", 7) == 0 || strncmp(sub, "100m", 4) == 0) {
            const char* num_str = strchr(sub, ' ');
            uint32_t rotations = 10;
            if (num_str) {
                uint32_t r = atoi(num_str);
                if (r > 0) rotations = r;
            }

            out.println("\r\n================================================================================");
            out.println("   RP2040 100 MSPS CONTINUOUS DMA ACQUISITION & TRIGGER FIDELITY BENCHMARK      ");
            out.println("================================================================================");
            out.printf("  Configuring 100 MSPS Continuous Circular DMA across 64 KB SRAM Ring Buffer...\r\n");
            out.printf("  Monitoring %lu full circular buffer revolutions (%lu total samples)...\r\n",
                       (unsigned long)rotations, (unsigned long)((rotations * 65536) + 32768));

            Benchmark100MspsResult res = pio_capture_run_100msps_benchmark(rotations);

            out.println("\r\n--- BENCHMARK EXECUTION RESULTS ---");
            out.printf("  Sample Rate Target     : 100,000,000 Hz (100 MSPS / 10.0 ns sample interval)\r\n");
            out.printf("  Actual System Clock    : %lu MHz (PIO Clockdiv = %.2f)\r\n",
                       (unsigned long)(clock_get_hz(clk_sys) / 1000000), (float)clock_get_hz(clk_sys) / 100000000.0f);
            out.printf("  DMA Transfer Size      : 32-bit Words (4 Samples per DREQ)\r\n");
            out.printf("  AHB Crossbar Bus Load  : %.1f%% (%s)\r\n",
                       res.bus_utilization_pct,
                       res.bus_utilization_pct <= 25.0f ? "LOW / NON-BLOCKING" : "MODERATE");
            out.printf("  Revolutions Completed  : %lu full 64KB SRAM revolutions\r\n", (unsigned long)res.revolutions_completed);
            out.printf("  Total Samples Streamed : %lu samples\r\n", (unsigned long)res.total_samples_streamed);
            out.printf("  Elapsed Time           : %lu us\r\n", (unsigned long)res.elapsed_us);
            out.printf("  Effective Sample Rate  : %.2f MSPS\r\n", res.effective_msps);
            out.printf("  Hardware FIFO Overflow : [%s] (RXOVER Sticky Bit = %d)\r\n",
                       res.fifo_overflow ? "FAIL: OVERFLOW DETECTED" : "PASS: ZERO SAMPLES DROPPED",
                       res.fifo_overflow ? 1 : 0);
            out.printf("  Trigger Trip Latency   : %lu us\r\n", (unsigned long)res.trigger_latency_us);
            out.printf("  Freeze Window Latency  : %lu us (Post-Trigger Buffer Frozen Cleanly)\r\n", (unsigned long)res.freeze_latency_us);
            out.printf("  Buffer Alignment Check : [%s]\r\n", res.buffer_aligned_ok ? "PASSED (Ordered [PRE][TRIG][POST])" : "FAILED");
            out.println("================================================================================");
            out.printf("  VERDICT: %s\r\n",
                       (!res.fifo_overflow && res.buffer_aligned_ok)
                       ? "100 MSPS CONTINUOUS DMA ACQUISITION VERIFIED WITH ZERO DROPS!"
                       : "BENCHMARK ANOMALY DETECTED");
            out.println("================================================================================\r\n");

            snprintf(last_cmd_output, sizeof(last_cmd_output),
                     "Bench 100MSPS: %.1f MSPS, %s, %lu revs",
                     res.effective_msps, res.fifo_overflow ? "OVERFLOW" : "0 Drops", (unsigned long)res.revolutions_completed);

            if (res.buffer_aligned_ok) {
                acquisition_publish_result(pio_capture_get_aligned_buffer(),
                                          MAX_CAPTURE_SAMPLES,
                                          100000000,
                                          32768,
                                          pio_capture_get_t_trigger_us(),
                                          pio_capture_get_t_freeze_us(),
                                          0,
                                          TRIG_MODE_AUTO);
            }
            return;
        } else if (strncmp(sub, "synth", 5) == 0 || strncmp(sub, "ramp", 4) == 0) {
            const char* num_str = strchr(sub, ' ');
            uint32_t samples = 8192;
            if (num_str) {
                uint32_t s = atoi(num_str);
                if (s > 0) samples = s;
            }
            spi_slave_test_generate_synthetic(samples);
            const CaptureDescriptor& desc = spi_slave_get_descriptor();
            out.printf("\r\n[TEST 1] Generated synthetic 00..FF ramp (%lu samples, %u blocks).\r\n",
                       (unsigned long)desc.total_samples, desc.total_blocks);
            out.printf("         Capture ID: #%lu, Whole CRC-16: 0x%04X, DATA_READY asserted HIGH.\r\n",
                       (unsigned long)desc.capture_id, desc.whole_capture_crc);
            snprintf(last_cmd_output, sizeof(last_cmd_output),
                     "TEST: Synth Ramp %lu samples (Cap #%lu, CRC 0x%04X)",
                     (unsigned long)desc.total_samples, (unsigned long)desc.capture_id, desc.whole_capture_crc);
        } else if (strncmp(sub, "inject", 6) == 0) {
            uint32_t blk = 0, byte_off = 517;
            sscanf(sub + 6, "%lu %lu", &blk, &byte_off);
            spi_slave_test_inject_corruption((uint16_t)blk, (uint16_t)byte_off);
            out.printf("\r\n[TEST 2] Single-bit corruption armed on Block #%lu (Byte %lu).\r\n",
                       (unsigned long)blk, (unsigned long)byte_off);
            out.println("         Next transfer of this block will invert 1 bit to verify per-block CRC retry.");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "TEST: Injected bit fault on blk %lu", (unsigned long)blk);
        } else if (strncmp(sub, "race", 4) == 0) {
            uint32_t old_id = spi_slave_get_descriptor().capture_id;
            spi_slave_test_trigger_race();
            uint32_t new_id = spi_slave_get_descriptor().capture_id;
            out.printf("\r\n[TEST 3] Simulated generation race: capture_id bumped from #%lu to #%lu.\r\n",
                       (unsigned long)old_id, (unsigned long)new_id);
            out.println("         Master will reject mismatched generation blocks with Invariant Violation.");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "TEST: Race simulated (Cap ID #%lu -> #%lu)", (unsigned long)old_id, (unsigned long)new_id);
        } else if (strncmp(sub, "lat", 3) == 0 || strncmp(sub, "latency", 7) == 0) {
            const CaptureResult& res = acquisition_get_result();
            if (res.valid) {
                acquisition_print_latency_profile(res, out);
            } else {
                const CaptureDescriptor& desc = spi_slave_get_descriptor();
                if (desc.valid) {
                    out.println("\r\n=== HARDWARE ACQUISITION & TRANSPORT LATENCY PROFILE ===");
                    out.printf("  Capture Status     : %s (Cap #%lu)\r\n", desc.valid ? "VALID / READY" : "INVALID / IDLE", (unsigned long)desc.capture_id);
                    out.printf("  t_trigger          : %lu us\r\n", (unsigned long)desc.t_trigger_us);
                    out.printf("  t_dma_freeze       : %lu us (+%lu us from trigger)\r\n",
                               (unsigned long)desc.t_dma_freeze_us,
                               (unsigned long)(desc.t_dma_freeze_us >= desc.t_trigger_us ? desc.t_dma_freeze_us - desc.t_trigger_us : 0));
                    out.printf("  t_desc_ready       : %lu us (+%lu us for buffer align & dual CRC-16)\r\n",
                               (unsigned long)desc.t_desc_ready_us,
                               (unsigned long)(desc.t_desc_ready_us >= desc.t_dma_freeze_us ? desc.t_desc_ready_us - desc.t_dma_freeze_us : 0));
                    out.printf("  t_data_ready       : %lu us (+%lu us to assert DATA_READY pin)\r\n",
                               (unsigned long)desc.t_data_ready_us,
                               (unsigned long)(desc.t_data_ready_us >= desc.t_desc_ready_us ? desc.t_data_ready_us - desc.t_desc_ready_us : 0));
                    uint32_t total_lat = (desc.t_data_ready_us >= desc.t_trigger_us) ? (desc.t_data_ready_us - desc.t_trigger_us) : 0;
                    out.printf("  Total Pipe Latency : %lu us (%lu samples @ %lu Hz)\r\n",
                               (unsigned long)total_lat, (unsigned long)desc.total_samples, (unsigned long)desc.sample_rate_hz);
                } else {
                    out.println("\r\n[LATENCY PROFILE] No active capture result yet. Run capture or 'test 100msps' first.");
                }
            }
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Latency: Profile displayed");
            return;
        } else if (strncmp(sub, "status", 6) == 0) {
            const CaptureDescriptor& desc = spi_slave_get_descriptor();
            out.println("\r\n=== SPI SLAVE TRANSPORT & CAPTURE STATUS ===");
#if defined(RP2040_PIN_DATA_READY) && (RP2040_PIN_DATA_READY != 255)
            out.printf("  DATA_READY Pin (GP%u): %s\r\n", RP2040_PIN_DATA_READY, digitalRead(RP2040_PIN_DATA_READY) ? "HIGH (Asserted)" : "LOW (Idle)");
#else
            out.printf("  DATA_READY Handshake : %s (In-band SPI_FLAG_CAPTURE_READY)\r\n", desc.valid ? "ASSERTED (Ready)" : "IDLE");
#endif
            out.printf("  Capture Descriptor   : %s\r\n", desc.valid ? "VALID" : "INVALID / AWAITING");
            out.printf("  Capture Generation   : #%lu\r\n", (unsigned long)desc.capture_id);
            out.printf("  Sample Count / Rate  : %lu samples @ %lu Hz\r\n", (unsigned long)desc.total_samples, (unsigned long)desc.sample_rate_hz);
            out.printf("  Blocks (1024 bytes)  : %u blocks\r\n", desc.total_blocks);
            out.printf("  Whole-Capture CRC-16 : 0x%04X\r\n", desc.whole_capture_crc);
            out.printf("  Trigger Offset / Ch  : sample #%lu (Ch %u, Type %u)\r\n",
                       (unsigned long)desc.trigger_offset, desc.trigger_channel, desc.trigger_type);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "SPI Slave: %s (Cap #%lu, %u blks)",
                     desc.valid ? "Ready" : "Idle", (unsigned long)desc.capture_id, desc.total_blocks);
        } else {
            out.println("\r\n=== SPI TRANSPORT & HARDWARE VALIDATION 1.0 TEST SUITE ===");
            out.println("  transport                 : Show MCU transport status (UART / Jumper SPI)");
            out.println("  transport spi             : Enable optional jumper SPI transport on GP2..GP5");
            out.println("  transport uart            : Restore default Native Factory UART transport");
            out.println("  test mcu_spi              : Automated inter-MCU SPI transport self-test");
            out.println("  test self                 : Run 17-point hardware self-test diagnostic matrix");
            out.println("  test freq / test source   : 1..100 MHz frequency sweep & jitter statistical test");
            out.println("  test ext [iterations]     : GP10 external trigger repeatability & histogram test");
            out.println("  test hammer [ms]          : Full system concurrent stress (DMA + Displays + SPI + UART)");
            out.println("  test la_stress [full]     : Automated multi-rate stress test (25..100 MSPS under load)");
            out.println("  test pattern [cycles]     : Deterministic ramp sample-integrity test (0 drops, wrap ok)");
            out.println("  test 100msps [revs]       : RP2040 100 MSPS Continuous DMA & trigger benchmark");
            out.println("  test synth [samples]      : Generate 00..FF ramp pattern & assert DATA_READY");
            out.println("  test inject [blk] [off]   : Arm single-bit fault injection on target block");
            out.println("  test race                 : Bump generation ID to test master invariant check");
            out.println("  test lat                  : Print physical 4-metric hardware latency breakdown");
            out.println("  test status               : Show current descriptor and DATA_READY status");
        }
        return;
    }

    // Five-Button Navigation CLI Simulation & Diagnostic Bridge
    if (strncmp(cmd_line, "btn", 3) == 0) {
        const char* sub = cmd_line + 3;
        while (isspace(*sub)) sub++;
        if (strncmp(sub, "up", 2) == 0 || strcmp(sub, "u") == 0) {
            button_nav_inject(BTN_UP);
            out.println("\r\n[Button] Injected: UP");
        } else if (strncmp(sub, "down", 4) == 0 || strcmp(sub, "d") == 0) {
            button_nav_inject(BTN_DOWN);
            out.println("\r\n[Button] Injected: DOWN");
        } else if (strncmp(sub, "left", 4) == 0 || strcmp(sub, "l") == 0) {
            button_nav_inject(BTN_LEFT);
            out.println("\r\n[Button] Injected: LEFT");
        } else if (strncmp(sub, "right", 5) == 0 || strcmp(sub, "r") == 0) {
            button_nav_inject(BTN_RIGHT);
            out.println("\r\n[Button] Injected: RIGHT");
        } else if (strncmp(sub, "select", 6) == 0 || strncmp(sub, "sel", 3) == 0 || strcmp(sub, "s") == 0) {
            button_nav_inject(BTN_SELECT);
            out.println("\r\n[Button] Injected: SELECT");
        } else {
            out.println("\r\n=== FIVE-BUTTON NAVIGATION COMMANDS ===");
            out.println("  btn up / btn u       : Simulate UP button press (Previous item / Increase)");
            out.println("  btn down / btn d     : Simulate DOWN button press (Next item / Decrease)");
            out.println("  btn left / btn l     : Simulate LEFT button press (Back / Cancel)");
            out.println("  btn right / btn r    : Simulate RIGHT button press (Enter submenu / Advance)");
            out.println("  btn select / btn s   : Simulate SELECT button press (Activate / Confirm)");
            out.printf("  Physical Pins Configured : %s\r\n", button_nav_pins_configured() ? "YES" : "NO (Pins unassigned, set in platformio.ini)");
        }
        return;
    }

    // Display Configuration & Diagnostics (disp test, disp test init, disp diag, disp rot, rot)
    if (strncmp(cmd_line, "disp", 4) == 0 || strncmp(cmd_line, "rot", 3) == 0) {
        const char* sub = (strncmp(cmd_line, "disp", 4) == 0) ? (cmd_line + 4) : cmd_line;
        while (isspace(*sub)) sub++;

        if (strncmp(sub, "diag", 4) == 0 || strncmp(sub, "guide", 5) == 0 || strncmp(sub, "wire", 4) == 0 || strncmp(sub, "help", 4) == 0) {
            ui_print_display_guide(out);
            return;
        }

        if (strncmp(sub, "test", 4) == 0) {
            const char* arg = sub + 4;
            while (isspace(*arg)) arg++;
            bool reinit = (strstr(arg, "init") != nullptr || strstr(arg, "reset") != nullptr);
            ui_run_display_test(reinit, out);
            return;
        } else if (strncmp(sub, "init", 4) == 0) {
            ui_run_display_test(true, out);
            return;
        } else if (strncmp(sub, "tab", 3) == 0) {
            const char* arg = sub + 3;
            while (isspace(*arg)) arg++;
            ui_set_tab_color(arg, out);
            return;
        } else if (strncmp(sub, "speed", 5) == 0) {
            const char* arg = sub + 5;
            while (isspace(*arg)) arg++;
            uint32_t spd = atol(arg);
            if (spd > 0 && spd <= 32) spd *= 1000000; // Allow "disp speed 8" for 8MHz
            ui_set_spi_speed(spd, out);
        } else if (strncmp(sub, "inv", 3) == 0) {
            ui_toggle_invert(out);
            return;
        } else if (strncmp(sub, "vcom", 4) == 0) {
            const char* arg = sub + 4;
            while (isspace(*arg)) arg++;
            uint8_t v = (uint8_t)strtoul(arg, nullptr, 0);
            if (v == 0) v = 0x2C;
            ui_set_vcom(v, out);
            return;
        } else {
            // Rotation configuration
            const char* r_str = (strncmp(sub, "rot", 3) == 0) ? (sub + 3) : sub;
            while (isspace(*r_str)) r_str++;
            if (isdigit(*r_str)) {
                uint8_t r = atoi(r_str);
                ui_set_rotation(r);
                out.printf("\r\n[Display] Orientation set to rotation %u (%s)\r\n",
                           r, (r == 0) ? "Portrait" :
                              (r == 1) ? "Landscape" :
                              (r == 2) ? "Inverted Portrait (180 deg)" : "Inverted Landscape (270 deg)");
                snprintf(last_cmd_output, sizeof(last_cmd_output), "Display Rot: %u", r);
            } else {
                out.printf("\r\n=== DISPLAY COMMAND SUITE ===\r\n"
                           "  Current Status: Tab: %s | SPI: %lu kHz | Rot: %u | Invert: %s | VCOM: 0x%02X (%u)\r\n"
                           "  disp diag                   : Main ST7735 wiring map & button pin status\r\n"
                           "  disp test                   : Direct test pattern render (Red, Green, Blue, White)\r\n"
                           "  disp test init              : Hardware reset + initR() recovery test pattern\r\n"
                           "  disp inv                    : Toggle display inversion (INVON / INVOFF)\r\n"
                           "  disp vcom <val>             : Set VCOM contrast voltage (0x0E..0x3F, e.g. 0x2C, 0x38)\r\n"
                           "  disp tab <144green|black|red> : Switch ST7735 tab profile and re-test\r\n"
                           "  disp speed <hz>             : Set SPI clock (e.g. 8000000 or 8 for 8MHz)\r\n"
                           "  disp rot <0-3>              : Set display rotation (0=Portrait, 1=Landscape, 2=Inverted)\r\n",
                           ui_get_tab_name(), ui_get_spi_speed() / 1000, ui_get_rotation(),
                           ui_get_invert() ? "YES (INVON)" : "NO (INVOFF)",
                           ui_get_vcom(), ui_get_vcom());
            }
            return;
        }
    }

    // PIO + DMA Edge Interval & Jitter Engine
    if (strncmp(cmd_line, "edge", 4) == 0 || strncmp(cmd_line, "jitter", 6) == 0) {
        const char* sub = (strncmp(cmd_line, "edge", 4) == 0) ? (cmd_line + 4) : (cmd_line + 6);
        while (isspace(*sub)) sub++;
        if (strncmp(sub, "cont", 4) == 0 || strncmp(sub, "stream", 6) == 0) {
            const char* pin_str = strchr(sub, ' ');
            uint8_t pin = 2;
            if (pin_str) pin = atoi(pin_str);
            pio_edge_run_continuous(pin, out);
        } else {
            uint32_t pin = 2, pairs = 128;
            sscanf(sub, "%lu %lu", &pin, &pairs);
            pio_edge_print_report((uint8_t)pin, (uint16_t)pairs, out);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Edge: GP%lu Analyzed", pin);
        }
        return;
    }

    // Frequency and Timing Measurement (Hybrid PIO+DMA and PWM Counter)
    if (strncmp(cmd_line, "freq", 4) == 0) {
        const char* sub = cmd_line + 4;
        while (isspace(*sub)) sub++;
        if (strncmp(sub, "pio", 3) == 0 || strncmp(sub, "edge", 4) == 0 || strncmp(sub, "jitter", 6) == 0) {
            const char* pin_str = strchr(sub, ' ');
            uint32_t pin = 2, pairs = 128;
            if (pin_str) sscanf(pin_str, "%lu %lu", &pin, &pairs);
            pio_edge_print_report((uint8_t)pin, (uint16_t)pairs, out);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "PIO Edge: GP%lu Analyzed", pin);
            return;
        }
        if (strncmp(sub, "stream", 6) == 0) {
            const char* pin_str = strchr(sub, ' ');
            uint8_t pin = 2;
            if (pin_str) pin = atoi(pin_str);
            pio_edge_run_continuous(pin, out);
            return;
        }
        bool continuous = false;
        if (strncmp(sub, "cont", 4) == 0) {
            continuous = true;
            sub += 4;
            while (isspace(*sub)) sub++;
        }
        uint8_t pin = 2; // Default to GPIO 2 (works on ANY pin with PIO+DMA)
        if (isdigit(*sub)) pin = atoi(sub);
        if (continuous) {
            pio_edge_run_continuous(pin, out);
        } else {
            // First run PIO+DMA edge analysis
            EdgeAnalysisResult er;
            if (pio_edge_measure(pin, 128, 200, er) && er.valid) {
                char f_str[32], p_str[32], j_str[32];
                if (er.avg_freq_hz >= 1000000.0f) snprintf(f_str, sizeof(f_str), "%.4f MHz", er.avg_freq_hz / 1e6f);
                else if (er.avg_freq_hz >= 1000.0f) snprintf(f_str, sizeof(f_str), "%.3f kHz", er.avg_freq_hz / 1e3f);
                else snprintf(f_str, sizeof(f_str), "%.2f Hz", er.avg_freq_hz);

                if (er.avg_period_ns >= 1e6f) snprintf(p_str, sizeof(p_str), "%.3f ms", er.avg_period_ns / 1e6f);
                else if (er.avg_period_ns >= 1e3f) snprintf(p_str, sizeof(p_str), "%.3f us", er.avg_period_ns / 1e3f);
                else snprintf(p_str, sizeof(p_str), "%.2f ns", er.avg_period_ns);

                if (er.jitter_rms_ns >= 1e3f) snprintf(j_str, sizeof(j_str), "%.2f us", er.jitter_rms_ns / 1e3f);
                else snprintf(j_str, sizeof(j_str), "%.2f ns", er.jitter_rms_ns);

                out.printf("\r\n[PIO+DMA Engine] GPIO %u: %s | Period: %s | Duty: %.1f%% | RMS Jitter: %s (Zero CPU load)\r\n",
                           pin, f_str, p_str, er.avg_duty_percent, j_str);
            } else {
                // Fallback to PWM Slice Counter
                freq_print_single(pin, out);
            }
        }
        snprintf(last_cmd_output, sizeof(last_cmd_output), "Freq (GP%u) measured", pin);
        return;
    }

    // 24Cxx I2C EEPROM Suite
    if (strncmp(cmd_line, "eeprom", 6) == 0) {
        const char* sub = cmd_line + 6;
        while (isspace(*sub)) sub++;
        if (!*sub || strncmp(sub, "probe", 5) == 0 || strncmp(sub, "scan", 4) == 0) {
            uint8_t i2c_addr = 0x50;
            const char* addr_str = strchr(sub, ' ');
            if (addr_str) i2c_addr = strtoul(addr_str, NULL, 0);
            eeprom_probe_chip(i2c_addr, out);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "EEPROM: Probed 0x%02X", i2c_addr);
        } else if (strncmp(sub, "read", 4) == 0) {
            uint32_t mem_addr = 0;
            uint16_t len = 64;
            uint32_t a16 = 1;
            uint32_t i2c_addr = 0x50;
            sscanf(sub + 4, "%lx %hu %lu %lx", &mem_addr, &len, &a16, &i2c_addr);
            eeprom_read_hex((uint8_t)i2c_addr, mem_addr, len, a16 != 0, out);
        } else if (strncmp(sub, "write", 5) == 0) {
            uint32_t mem_addr = 0;
            uint32_t val = 0;
            uint32_t a16 = 1;
            uint32_t i2c_addr = 0x50;
            sscanf(sub + 5, "%lx %lx %lu %lx", &mem_addr, &val, &a16, &i2c_addr);
            eeprom_write_val((uint8_t)i2c_addr, mem_addr, (uint8_t)val, a16 != 0, out);
        } else if (strncmp(sub, "dump", 4) == 0) {
            uint32_t bytes = 256;
            uint32_t a16 = 1;
            uint32_t i2c_addr = 0x50;
            sscanf(sub + 4, "%lu %lu %lx", &bytes, &a16, &i2c_addr);
            eeprom_dump_all((uint8_t)i2c_addr, bytes, a16 != 0, out);
        }
        return;
    }

    // Addressable WS2812 / NeoPixel LED Driver
    if (strncmp(cmd_line, "led", 3) == 0) {
        const char* sub = cmd_line + 3;
        while (isspace(*sub)) sub++;
        if (strncmp(sub, "rainbow", 7) == 0) {
            uint32_t pin = 2, count = 8;
            sscanf(sub + 7, "%lu %lu", &pin, &count);
            ws2812_show_rainbow((uint8_t)pin, (uint16_t)count, out);
        } else if (strncmp(sub, "off", 3) == 0) {
            uint32_t pin = 2, count = 8;
            sscanf(sub + 3, "%lu %lu", &pin, &count);
            ws2812_turn_off((uint8_t)pin, (uint16_t)count);
            out.printf("LEDs turned off on GPIO %lu.\r\n", pin);
        } else {
            uint32_t pin = 2, r = 255, g = 0, b = 0;
            if (strncmp(sub, "ws2812", 6) == 0) sub += 6;
            sscanf(sub, "%lu %lu %lu %lu", &pin, &r, &g, &b);
            ws2812_set_color((uint8_t)pin, (uint8_t)r, (uint8_t)g, (uint8_t)b);
            out.printf("WS2812 GPIO %lu set to RGB(%lu, %lu, %lu)\r\n", pin, r, g, b);
        }
        snprintf(last_cmd_output, sizeof(last_cmd_output), "LED: Updated");
        return;
    }

    // Precision Hardware Glitch Injection
    if (strncmp(cmd_line, "glitch", 6) == 0) {
        const char* sub = cmd_line + 6;
        while (isspace(*sub)) sub++;
        if (strncmp(sub, "sweep", 5) == 0) {
            uint32_t pin = 2, start_d = 10, end_d = 100, step_d = 10, pulse_ns = 50;
            sscanf(sub + 5, "%lu %lu %lu %lu %lu", &pin, &start_d, &end_d, &step_d, &pulse_ns);
            glitch_sweep((uint8_t)pin, start_d, end_d, step_d, pulse_ns, out);
        } else {
            uint32_t pin = 2, delay_us = 10, pulse_ns = 50;
            if (strncmp(sub, "pulse", 5) == 0) sub += 5;
            sscanf(sub, "%lu %lu %lu", &pin, &delay_us, &pulse_ns);
            glitch_fire_pulse((uint8_t)pin, delay_us, pulse_ns);
            out.printf("Glitch pulse fired on GPIO %lu: delay=%lu us, width=%lu ns\r\n", pin, delay_us, pulse_ns);
        }
        snprintf(last_cmd_output, sizeof(last_cmd_output), "Glitch: Pulse fired");
        return;
    }

    // ARM Cortex SWD Debug Interrogator
    if (strncmp(cmd_line, "swd", 3) == 0 || strncmp(cmd_line, "jtag", 4) == 0) {
        const char* sub = (strncmp(cmd_line, "swd", 3) == 0) ? (cmd_line + 3) : (cmd_line + 4);
        uint32_t clk = 18, dio = 19;
        sscanf(sub, "%lu %lu", &clk, &dio);
        swd_print_info((uint8_t)clk, (uint8_t)dio, out);
        snprintf(last_cmd_output, sizeof(last_cmd_output), "SWD: Probe complete");
        return;
    }


    if (strncmp(cmd_line, "1wire", 5) == 0 || strncmp(cmd_line, "temp", 4) == 0) {
        const char* sub = (strncmp(cmd_line, "1wire", 5) == 0) ? (cmd_line + 5) : "";
        while (isspace(*sub)) sub++;
        if (strcmp(sub, "scan") == 0 || strcmp(sub, "search") == 0) {
            onewire_search_bus(2, out);
        } else {
            onewire_read_all_temps(2, out);
        }
        return;
    }

    if (strncmp(cmd_line, "avr", 3) == 0) {
        out.println("\r\n[AVR ISP] Reading target device signature and fuses...");
        AvrIspInfo info;
        if (avr_isp_probe(info)) {
            out.printf("  Target MCU : %s\r\n", info.mcu_name);
            out.printf("  Signature  : 0x%02X 0x%02X 0x%02X\r\n", info.signature[0], info.signature[1], info.signature[2]);
            out.printf("  Fuses      : Low=0x%02X, High=0x%02X, Ext=0x%02X, Lock=0x%02X\r\n",
                       info.low_fuse, info.high_fuse, info.extended_fuse, info.lock_byte);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "AVR: %s (0x%02X%02X%02X)",
                     info.mcu_name, info.signature[0], info.signature[1], info.signature[2]);
        } else {
            out.println("  Failed to enter programming mode (target not responding).");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "AVR: Target not responding");
        }
        return;
    }

    // ─── Multifunction Analog & Continuity Subsystem CLI Suite ─────────────
    if (strncmp(cmd_line, "adc", 3) == 0 || strncmp(cmd_line, "volt", 4) == 0) {
        float v0 = adc_read_calibrated_voltage(ADC_CH_0, 32);
        float v1 = adc_read_calibrated_voltage(ADC_CH_1, 32);
        float v2 = adc_read_calibrated_voltage(ADC_CH_2, 32);
        float vsys = adc_read_calibrated_voltage(ADC_CH_VSYS, 32);
        float temp = adc_read_internal_temp_c();
        out.println("\r\n=== RP2040 MULTICHANNEL ANALOG VOLTMETER ===");
        out.printf("  CH0 (GP26 / P2.11) : %7.4f V (Raw: %4u)\r\n", v0, adc_read_raw_single(ADC_CH_0));
        out.printf("  CH1 (GP27 / P2.12) : %7.4f V (Raw: %4u)\r\n", v1, adc_read_raw_single(ADC_CH_1));
        out.printf("  CH2 (GP28 / P2.14) : %7.4f V (Raw: %4u)\r\n", v2, adc_read_raw_single(ADC_CH_2));
        out.printf("  VSYS Rail Monitor  : %7.4f V (1/3 Div)\r\n", vsys);
        out.printf("  Core Temperature   : %7.2f deg C\r\n", temp);
        snprintf(last_cmd_output, sizeof(last_cmd_output), "ADC: CH0=%.3fV CH1=%.3fV CH2=%.3fV", v0, v1, v2);
        return;
    }

    if (strncmp(cmd_line, "raw", 3) == 0) {
        out.println("\r\n=== RAW ADC HARDWARE DIAGNOSTICS ===");
        for (int i = 0; i < 3; i++) {
            uint16_t r = adc_read_raw_single((AdcChannelId)i);
            float v_adc = (r * ADC_VREF_VOLTS) / ADC_MAX_RAW_VALUE;
            float v_scaled = adc_read_calibrated_voltage((AdcChannelId)i, 16);
            out.printf("  ADC%d (GP%d): RAW=%4u | V_PIN=%6.4f V | CAL_V=%6.4f V\r\n",
                       i, 26 + i, r, v_adc, v_scaled);
        }
        uint16_t r_vsys = adc_read_raw_single(ADC_CH_VSYS);
        out.printf("  VSYS  (GP29): RAW=%4u | V_PIN=%6.4f V | CAL_V=%6.4f V\r\n",
                   r_vsys, (r_vsys * ADC_VREF_VOLTS) / ADC_MAX_RAW_VALUE, adc_read_calibrated_voltage(ADC_CH_VSYS, 16));
        snprintf(last_cmd_output, sizeof(last_cmd_output), "RAW: ADC0=%u ADC1=%u ADC2=%u",
                 adc_read_raw_single(ADC_CH_0), adc_read_raw_single(ADC_CH_1), adc_read_raw_single(ADC_CH_2));
        return;
    }

    if (strncmp(cmd_line, "res", 3) == 0) {
        const char* sub = cmd_line + 3;
        while (isspace(*sub)) sub++;
        AdcChannelId ch = ADC_CH_0;
        if (*sub >= '0' && *sub <= '2') ch = (AdcChannelId)(*sub - '0');
        float r = adc_read_resistance(ch, CONTINUITY_R_REF_OHMS, ADC_VREF_VOLTS);
        out.printf("\r\n=== RESISTANCE MEASUREMENT (CH%d / GP%d) ===\r\n", ch, 26 + ch);
        out.println("  Circuit: 3.3V -> Rknown (1k) -> Probe+ -> DUT -> Probe- (GND)");
        if (r >= 1e6f) {
            out.println("  Resistance: OPEN CIRCUIT (> 1 MOhm)");
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Res CH%d: OPEN", ch);
        } else if (r >= 1000.0f) {
            out.printf("  Resistance: %.3f kOhm (%.1f Ohm)\r\n", r / 1000.0f, r);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Res CH%d: %.2f kOhm", ch, r / 1000.0f);
        } else {
            out.printf("  Resistance: %.2f Ohm\r\n", r);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Res CH%d: %.2f Ohm", ch, r);
        }
        return;
    }

    if (strncmp(cmd_line, "cont", 4) == 0) {
        const char* sub = cmd_line + 4;
        while (isspace(*sub)) sub++;
        if (strncmp(sub, "cfg", 3) == 0) {
            float th = 25.0f, hy = 10.0f;
            int mute = 0;
            sscanf(sub + 3, "%f %f %d", &th, &hy, &mute);
            adc_continuity_set_threshold(th, hy);
            adc_continuity_set_buzzer_mute(mute != 0);
            out.printf("\r\n[Continuity Config] Thresh: %.1f Ohm | Hyst: %.1f Ohm | Mute: %s\r\n",
                       th, hy, mute ? "YES" : "NO");
            return;
        }

        AdcChannelId ch = ADC_CH_0;
        if (*sub >= '0' && *sub <= '2') {
            ch = (AdcChannelId)(*sub - '0');
            sub++;
            while (isspace(*sub)) sub++;
        }
        if (isdigit(*sub)) {
            float th = atof(sub);
            if (th > 0) adc_continuity_set_threshold(th, th * 0.4f);
        }

        ContinuityConfig &cfg = adc_continuity_get_config();
        out.println("\r\n=== CONTINUITY TESTER (GP26 Probe+, GP20 Buzzer) ===");
        out.printf("  Threshold: < %.1f Ohm (Release: > %.1f Ohm) | Buzzer: %s (GP20)\r\n",
                   cfg.threshold_ohms, cfg.threshold_ohms + cfg.hysteresis_ohms,
                   cfg.buzzer_enabled ? "ON" : "MUTED");
        out.println("  Live polling active... (Send any key to exit)\r\n");

        while (!Serial.available() && !ESP32_SERIAL.available()) {
            ContinuityStatus st = adc_continuity_update(ch);
            if (st.is_connected) {
                out.printf("\r[BEEP] CONNECTED: %6.2f Ohm  (Trans: %lu)   ", st.measured_ohms, (unsigned long)st.transition_count);
            } else {
                if (st.measured_ohms >= 1e6f) {
                    out.printf("\r[....] OPEN CIRCUIT               (Trans: %lu)   ", (unsigned long)st.transition_count);
                } else {
                    out.printf("\r[....] OPEN: %6.2f Ohm       (Trans: %lu)   ", st.measured_ohms, (unsigned long)st.transition_count);
                }
            }
            delay(10);
        }
        while (Serial.available()) Serial.read();
        while (ESP32_SERIAL.available()) ESP32_SERIAL.read();
        buzzer_stop();
        out.println("\r\n[Continuity Tester Exited]");
        snprintf(last_cmd_output, sizeof(last_cmd_output), "Continuity test ended");
        return;
    }

    if (strncmp(cmd_line, "wave", 4) == 0) {
        const char* sub = cmd_line + 4;
        while (isspace(*sub)) sub++;
        AdcChannelId ch = ADC_CH_0;
        uint32_t rate = 100000;
        uint16_t samples = 256;
        if (isdigit(*sub)) {
            ch = (AdcChannelId)(*sub - '0');
            sub++;
            while (isspace(*sub)) sub++;
        }
        if (isdigit(*sub)) {
            sscanf(sub, "%lu %hu", &rate, &samples);
        }

        AdcWaveformConfig wcfg;
        wcfg.channel = ch;
        wcfg.sample_rate_hz = rate;
        wcfg.sample_count = samples;
        wcfg.trigger_level_v = 1.65f;
        wcfg.trigger_slope = TRIG_SLOPE_RISING;
        wcfg.trigger_hysteresis_v = 0.05f;
        wcfg.pre_trigger_pct = 70;

        out.printf("\r\n=== ADC DMA WAVEFORM CAPTURE (CH%d / GP%d) ===\r\n", ch, 26 + ch);
        out.printf("  Sampling Rate: %lu Hz | Depth: %u samples | Mode: Hardware DMA\r\n",
                   (unsigned long)rate, samples);

        if (adc_waveform_start(wcfg)) {
            if (adc_waveform_wait_completion(500)) {
                AdcWaveformStats st = adc_waveform_compute_stats(wcfg);
                out.printf("  Vmin: %.3f V | Vmax: %.3f V | Vpp: %.3f V\r\n", st.v_min, st.v_max, st.v_pp);
                out.printf("  Vavg: %.3f V | Vrms: %.3f V\r\n", st.v_avg, st.v_rms);
                if (st.estimated_freq_hz > 0) {
                    out.printf("  Estimated Freq: %.2f Hz\r\n", st.estimated_freq_hz);
                }
                out.printf("  Trigger: %s (Sample #%u)\r\n", st.triggered ? "LOCKED" : "FREE-RUN", st.trigger_idx);

                // Quick ASCII trace of first 32 samples
                out.println("\r\n--- Waveform Visualizer ---");
                const uint16_t* buf = adc_waveform_get_buffer();
                uint16_t plot_len = (samples > 32) ? 32 : samples;
                for (uint16_t i = 0; i < plot_len; i++) {
                    float v = (((buf[i] & 0x0FFF) * ADC_VREF_VOLTS) / ADC_MAX_RAW_VALUE);
                    uint8_t bar = (uint8_t)(v * 10.0f);
                    out.printf("%4u | %5.3fV | ", i, v);
                    for (uint8_t b = 0; b < bar; b++) out.print('#');
                    out.println();
                }
                snprintf(last_cmd_output, sizeof(last_cmd_output), "Waveform CH%d: Vpp=%.2fV Vrms=%.2fV", ch, st.v_pp, st.v_rms);
            } else {
                out.println("  [Error] Waveform DMA acquisition timed out.");
            }
        } else {
            out.println("  [Error] Failed to start DMA transfer.");
        }
        return;
    }

    if (strncmp(cmd_line, "cal", 3) == 0) {
        const char* sub = cmd_line + 3;
        while (isspace(*sub)) sub++;
        AdcChannelId ch = ADC_CH_0;
        float v_ref_low = 0.0f, v_meas_low = 0.0f, v_ref_high = 3.0f, v_meas_high = 3.0f;
        int parsed = sscanf(sub, "%d %f %f %f %f", (int*)&ch, &v_ref_low, &v_meas_low, &v_ref_high, &v_meas_high);
        if (parsed == 5) {
            if (adc_calibrate_two_point(ch, v_ref_low, v_meas_low, v_ref_high, v_meas_high)) {
                float off, gn;
                adc_get_calibration(ch, off, gn);
                out.printf("\r\n[Calibration Saved] CH%d -> Offset: %+.4f V | Gain: %.4f\r\n", ch, off, gn);
                snprintf(last_cmd_output, sizeof(last_cmd_output), "Cal CH%d: Off=%.3fV Gn=%.3f", ch, off, gn);
            } else {
                out.println("\r\n[Error] Invalid calibration points (delta too small).");
            }
        } else if (parsed == 1 && strstr(sub, "reset")) {
            adc_reset_calibration(ch);
            out.printf("\r\n[Calibration Reset] CH%d returned to factory defaults.\r\n", ch);
        } else {
            out.println("\r\n=== TWO-POINT ADC CALIBRATION GUIDE ===");
            out.println("  Usage: cal <ch 0-2> <v_ref_low> <v_meas_low> <v_ref_high> <v_meas_high>");
            out.println("  Example: cal 0 0.000 0.012 3.000 2.985");
            out.println("  Reset:   cal <ch 0-2> reset");
        }
        return;
    }

    if (strncmp(cmd_line, "curr", 4) == 0) {
        const char* sub = cmd_line + 4;
        while (isspace(*sub)) sub++;
        AdcChannelId ch = ADC_CH_0;
        float shunt = 0.1f; // 100 mOhm default
        sscanf(sub, "%d %f", (int*)&ch, &shunt);
        float amps = adc_read_current_amps(ch, shunt);
        out.printf("\r\n=== CURRENT MEASUREMENT (CH%d, Shunt: %.3f Ohm) ===\r\n", ch, shunt);
        if (fabsf(amps) >= 1.0f) {
            out.printf("  Current: %7.3f A\r\n", amps);
        } else {
            out.printf("  Current: %7.2f mA\r\n", amps * 1000.0f);
        }
        snprintf(last_cmd_output, sizeof(last_cmd_output), "Current: %.2f mA", amps * 1000.0f);
        return;
    }

    if (strncmp(cmd_line, "buzzer", 6) == 0 || strncmp(cmd_line, "beep", 4) == 0) {
        out.println("\r\n[Buzzer Test] Generating 2.7 kHz resonant tone on GP20 & flashing LED D12...");
        buzzer_beep_ms(CONTINUITY_BUZZER_FREQ_HZ, 250);
        out.println("  Tone complete.");
        snprintf(last_cmd_output, sizeof(last_cmd_output), "Buzzer beep OK");
        return;
    }

    // 2. Standard Bus Pirate Single-Letter Commands & Macro Strings
    const char* p = cmd_line;
    while (*p) {
        while (isspace(*p)) p++;
        if (!*p) break;

        char c = *p;

        if (c == 'm') { // Mode switch
            p++;
            while (isspace(*p)) p++;
            if (isdigit(*p)) {
                uint8_t m_idx = *p - '0';
                p++;
                bp_set_mode((BpMode)m_idx, &out);
            } else {
                out.println("\r\n--- Available Bus Pirate Modes ---");
                for (int i = 0; i < BP_MODE_COUNT; i++) {
                    out.printf(" %d. %s\r\n", i, MODE_NAMES[i]);
                }
            }
            continue;
        }

        if (c == 'b') { // Speed / baud
            p++;
            while (isspace(*p)) p++;
            uint32_t val = 0;
            while (isdigit(*p)) { val = val * 10 + (*p - '0'); p++; }
            if (val > 0) {
                bp_cfg.speed_hz = val;
                out.printf("Bitrate set to: %lu Hz\r\n", (unsigned long)val);
            }
            continue;
        }

        if (c == '[') { // Bus Start
            bp_cfg.bus_started = 1;
            p++;
            if (bp_cfg.mode == BP_MODE_SPI) {
                digitalWrite(bp_cfg.cs_pin, LOW);
                out.print("/CS ENABLED (LOW)\r\n");
            } else if (bp_cfg.mode == BP_MODE_I2C) {
                Wire.beginTransmission(0x50); // Ready
                out.print("I2C START CONDITION\r\n");
            } else {
                out.print("START\r\n");
            }
            continue;
        }

        if (c == ']') { // Bus Stop
            bp_cfg.bus_started = 0;
            p++;
            if (bp_cfg.mode == BP_MODE_SPI) {
                digitalWrite(bp_cfg.cs_pin, HIGH);
                out.print("/CS DISABLED (HIGH)\r\n");
            } else if (bp_cfg.mode == BP_MODE_I2C) {
                Wire.endTransmission();
                out.print("I2C STOP CONDITION\r\n");
            } else {
                out.print("STOP\r\n");
            }
            continue;
        }

        if (c == 'v' || c == 'V') { // Voltage read
            p++;
            float v_ldr  = bp_read_adc_voltage(26);
            float v_adc1 = bp_read_adc_voltage(27);
            float v_adc2 = bp_read_adc_voltage(28);
            out.printf("Voltages: ADC0(LDR)=%.2fV  ADC1=%.2fV  ADC2=%.2fV\r\n", v_ldr, v_adc1, v_adc2);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "ADC0: %.2fV  ADC1: %.2fV", v_ldr, v_adc1);
            continue;
        }

        if (c == 'f' || c == 'F') { // Frequency & timing measurement
            p++;
            while (isspace(*p)) p++;
            uint8_t pin = 2; // Works on ANY pin (GPIO 0-29) with PIO+DMA
            if (isdigit(*p)) { pin = *p - '0'; p++; }
            if (c == 'F') {
                pio_edge_run_continuous(pin, out);
            } else {
                EdgeAnalysisResult er;
                if (pio_edge_measure(pin, 128, 200, er) && er.valid) {
                    char f_str[32];
                    if (er.avg_freq_hz >= 1e6f) snprintf(f_str, sizeof(f_str), "%.4f MHz", er.avg_freq_hz / 1e6f);
                    else if (er.avg_freq_hz >= 1e3f) snprintf(f_str, sizeof(f_str), "%.3f kHz", er.avg_freq_hz / 1e3f);
                    else snprintf(f_str, sizeof(f_str), "%.2f Hz", er.avg_freq_hz);
                    out.printf("\r\n[PIO+DMA Engine] GPIO %u: %s | Duty: %.1f%% | RMS Jitter: %.2f ns\r\n",
                               pin, f_str, er.avg_duty_percent, er.jitter_rms_ns);
                } else {
                    freq_print_single(pin, out);
                }
            }
            snprintf(last_cmd_output, sizeof(last_cmd_output), "Freq (GP%u) measured", pin);
            continue;
        }

        if (c == 'g' || c == 'G') { // PWM generator
            p++;
            while (isspace(*p)) p++;
            uint32_t freq = 1000;
            uint8_t  duty = 50;
            // Parse freq and duty
            if (isdigit(*p)) {
                freq = 0;
                while (isdigit(*p)) { freq = freq * 10 + (*p - '0'); p++; }
                while (isspace(*p)) p++;
                if (isdigit(*p)) {
                    duty = 0;
                    while (isdigit(*p)) { duty = duty * 10 + (*p - '0'); p++; }
                }
            }
            bp_generate_pwm(25, freq, duty);
            out.printf("PWM Generator enabled on GPIO 25: %lu Hz @ %u%% duty\r\n", (unsigned long)freq, duty);
            snprintf(last_cmd_output, sizeof(last_cmd_output), "PWM: %luHz @ %u%%", (unsigned long)freq, duty);
            continue;
        }

        if (c == 'r' || c == 'R') { // Read byte(s)
            p++;
            uint16_t count = 1;
            if (*p == ':') {
                p++;
                count = 0;
                while (isdigit(*p)) { count = count * 10 + (*p - '0'); p++; }
            }

            out.printf("READ %u BYTES: ", count);
            for (uint16_t i = 0; i < count; i++) {
                uint8_t val = 0x00;
                if (bp_cfg.mode == BP_MODE_SPI) {
                    val = SPI.transfer(0x00);
                } else if (bp_cfg.mode == BP_MODE_I2C) {
                    val = (uint8_t)(i + 1); // Mock read
                }
                out.printf("0x%02X ", val);
            }
            out.print("\r\n");
            continue;
        }

        // Numeric Hex / Dec Data Byte Write: e.g. 0x9F or 128
        if (c == '0' && (*(p+1) == 'x' || *(p+1) == 'X')) {
            p += 2;
            uint32_t byte_val = 0;
            while (isxdigit(*p)) {
                char d = tolower(*p);
                byte_val = (byte_val << 4) | (d >= '0' && d <= '9' ? d - '0' : d - 'a' + 10);
                p++;
            }
            if (bp_cfg.mode == BP_MODE_SPI) {
                uint8_t ret = SPI.transfer((uint8_t)byte_val);
                out.printf("WRITE: 0x%02X -> READ: 0x%02X\r\n", (uint8_t)byte_val, ret);
            } else if (bp_cfg.mode == BP_MODE_I2C) {
                Wire.write((uint8_t)byte_val);
                out.printf("WRITE: 0x%02X (I2C buffered)\r\n", (uint8_t)byte_val);
            } else {
                out.printf("WRITE: 0x%02X\r\n", (uint8_t)byte_val);
            }
            continue;
        }

        if (c == '?' || c == 'h') { // Help
            p++;
            out.println("\r\n=== UDOO KEY BUS PIRATE SYNTAX ===");
            out.println("  [ / ]       : Bus Start / Bus Stop (/CS toggle, I2C start/stop)");
            out.println("  0xNN / NN   : Write byte to bus");
            out.println("  r / r:N     : Read N bytes from bus");
            out.println("  m <0-10>    : Select mode (0:HiZ, 1:1Wire, 2:I2C, 3:SPI, 4:UART, 9:Logic, 10:LED)");
            out.println("  b <hz>      : Set bus bitrate / baud");
            out.println("  v           : Show ADC voltages (LDR GPIO 26 & test pins)");
            out.println("  f / F [pin] : Precision PIO+DMA Frequency, Duty & Jitter (single / continuous stream)");
            out.println("  edge [pin]  : Deep PIO+DMA interval analysis, pulse-width & jitter breakdown");
            out.println("  jitter [pin]: Pulse-width jitter & runt pulse / glitch detection");
            out.println("  g [hz][%]   : Start PWM generator on GPIO 25");
            out.println("  scan        : Perform I2C bus scan");
            out.println("  sniff       : Real-time I2C bus sniffer with START/STOP/ACK/Data");
            out.println("  flash       : SPI Flash (probe, status, sfdp, read, dump, write, erase, protect)");
            out.println("  eeprom      : 24Cxx I2C EEPROM (probe, read, write, dump)");
            out.println("  led         : WS2812 NeoPixel (ws2812 <pin> <r> <g> <b>, rainbow, off)");
            out.println("  glitch      : Hardware Fault Injection (pulse <pin> <del> <ns>, sweep)");
            out.println("  swd         : ARM Cortex SW-DP probe & DPIDR decoder");
            out.println("  temp        : Read 1-Wire DS18B20 temperature");
            out.println("  avr         : Read AVR ISP target signature & fuses");
            out.println("  macro       : Macro execution engine (list, run <id>, set <id> <name> <cmd>)");
            out.println("  sump        : Toggle SUMP Logic Sniffer mode for PulseView / Sigrok");
            out.println("  trig        : Hardware Trigger Engine (edge, level, qual, width <runt/glitch>, pat, ext, pos, seq, proto, status)");
            out.println("  frontend    : Hardware Front-End Control (protect on/off, prescaler 1/2/4, status)");
            out.println("  rate <hz>   : Configure logic acquisition sample rate");
            out.println("  getcap      : Dump synchronized pre/post-trigger capture buffer");
            out.println("  pinout      : Real-time logic levels across probe channels 0-7");
            out.println("  rot <0-3>   : Set display orientation (0:Portrait, 1:Landscape, 2:Inverted, 3:Inv Landscape)");
            out.println("  disp        : Main ST7735 display controls (disp test, disp test init, disp diag)");
            out.println("  btn <cmd>   : Five-Button simulation bridge (btn up, down, left, right, select)");
            out.println("  mode        : Physical Instrument Mode Manager (status, pins, logic, spi, i2c, uart, swd, onewire, pwm, glitch, eeprom, flash, idle)");
            out.println("  test        : Benchmarks & Diagnostics (self, freq, ext, hammer, 100msps, pattern, la_stress, lat, status)");
            continue;
        }

        if (c == 'i') { // Info
            p++;
            out.println("\r\n--- UDOO KEY Hardware Status ---");
            out.printf("  Current Mode: %s\r\n", active_mode_name);
            out.printf("  Bus Speed:    %lu Hz\r\n", (unsigned long)bp_cfg.speed_hz);
            out.printf("  CS Pin:       GPIO %u\r\n", bp_cfg.cs_pin);
            out.printf("  LDR Sensor:   %.2f V (GPIO 26)\r\n", bp_read_adc_voltage(26));
            continue;
        }

        p++; // Unknown character
    }
}
