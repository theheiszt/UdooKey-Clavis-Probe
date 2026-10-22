#include "terminal.h"
#include "terminal_transport.h"
#include "terminal_vt100.h"
#include "terminal_history.h"
#include "terminal_line_editor.h"
#include "bus_pirate_core.h"
#include "instrument_mode_manager.h"
#include "adc_subsystem.h"
#include "hardware_config.h"
#include "udoo_hal/gpio_hal.h"
#include "udoo_hal/spi_hal.h"
#include "udoo_hal/i2c_hal.h"
#include "udoo_hal/uart_hal.h"
#include "udoo_hal/onewire_hal.h"
#include "udoo_hal/meter_hal.h"
#include "udoo_hal/logic_hal.h"
#include "udoo_hal/usb_hal.h"
#include "udoo_hal/sensor_hal.h"
#include "eeprom_24c.h"
#include "i2c_sniffer.h"
#include "spiflash_full.h"
#include <ctype.h>
#include <string.h>

static char s_mode_name[16] = "HiZ";
static char s_prompt_buf[32] = "HiZ> ";

static void update_prompt() {
    snprintf(s_prompt_buf, sizeof(s_prompt_buf), "%s> ", s_mode_name);
    line_editor_set_prompt(s_prompt_buf);
}

void terminal_set_mode_name(const char* mode) {
    if (!mode || !*mode) return;
    strncpy(s_mode_name, mode, sizeof(s_mode_name) - 1);
    s_mode_name[sizeof(s_mode_name) - 1] = '\0';
    update_prompt();
}

const char* terminal_get_mode_name() {
    return s_mode_name;
}

void terminal_print_banner() {
    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("\r\n==================================================\r\n");
    terminal_transport_print(" UDOO KEY Universal Electronics Instrument (v2.1) \r\n");
    terminal_transport_print(" Dual-Core RP2040 Cortex-M0+ 133MHz | ESP32 Link  \r\n");
    terminal_transport_print(" Native USB CDC Console (115200 8N1) | Mode: VT100\r\n");
    terminal_transport_print("==================================================\r\n");
    vt100_set_color(VT100_COLOR_RESET);
    terminal_transport_print("Type '?' or 'help' for command reference.\r\n");
    terminal_transport_print("All probe outputs disabled (Safe HiZ boot state).\r\n\r\n");
}

void terminal_print_prompt() {
    vt100_set_color(VT100_COLOR_PROMPT);
    terminal_transport_print(s_prompt_buf);
    vt100_set_color(VT100_COLOR_RESET);
}

void terminal_init() {
    terminal_transport_init();
    vt100_init();
    terminal_history_init();
    terminal_set_mode_name("HiZ");
    line_editor_init(s_prompt_buf);
    terminal_print_banner();
    terminal_print_prompt();
}

// ─── DIO Subsystem (BP5-Style Digital I/O Mode) ────────────────────────────────
static bool parse_pin_arg(const char* str, uint8_t* out_gpio, uint8_t* out_idx, const char** next_token = nullptr) {
    if (!str) return false;
    while (isspace((unsigned char)*str)) str++;
    if (!*str) return false;

    const char* p = str;
    if ((p[0] == 'g' || p[0] == 'G') && (p[1] == 'p' || p[1] == 'P')) {
        p += 2;
    } else if ((p[0] == 'p' || p[0] == 'P') && isdigit((unsigned char)p[1])) {
        p += 1;
    }

    if (!isdigit((unsigned char)*p)) return false;

    char* endp = nullptr;
    long val = strtol(p, &endp, 10);
    if (endp == p) return false;

    if (next_token) {
        while (*endp && isspace((unsigned char)*endp)) endp++;
        *next_token = endp;
    }

    return gpio_hal_resolve_pin((uint8_t)val, out_gpio, out_idx);
}

static void cmd_dio_pins_table() {
    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("\r\n+-------+------+-------------+-----------+-------+----------+\r\n");
    terminal_transport_print("| Probe | GPIO | Connector   | Direction | Level | Pull     |\r\n");
    terminal_transport_print("+-------+------+-------------+-----------+-------+----------+\r\n");
    vt100_set_color(VT100_COLOR_RESET);

    uint8_t bus_val = gpio_hal_read_all();

    for (uint8_t idx = 0; idx < UDOO_PROBE_PIN_COUNT; idx++) {
        GpioHalPinStatus st;
        if (gpio_hal_get_status(idx, &st)) {
            const char* pull_str = "NONE";
            if (st.pull_mode == GPIO_HAL_PULL_UP) pull_str = "PULL-UP";
            else if (st.pull_mode == GPIO_HAL_PULL_DOWN) pull_str = "PULL-DN";

            terminal_transport_printf("| CH%-3u | GP%-2u | %-11s | %-9s | %-5s | %-8s |\r\n",
                st.probe_idx,
                st.gpio_num,
                st.header_pin_desc,
                st.is_output ? "OUTPUT" : "INPUT",
                st.logic_level ? "HIGH" : "LOW",
                pull_str
            );
        }
    }

    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("+-------+------+-------------+-----------+-------+----------+\r\n");
    vt100_set_color(VT100_COLOR_RESET);

    terminal_transport_printf("Probe Bus State: 0x%02X | Binary: 0b", bus_val);
    for (int b = 7; b >= 0; b--) {
        terminal_transport_printf("%c", (bus_val & (1 << b)) ? '1' : '0');
    }
    terminal_transport_print(" (CH7..CH0)\r\n\r\n");
}

static void cmd_dio_read(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        cmd_dio_pins_table();
        return;
    }

    uint8_t gpio = 0, idx = 0;
    if (!parse_pin_arg(arg, &gpio, &idx)) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Invalid probe pin. Valid pins: 0..7 or GP2..GP9 (Header P1.4..P1.12)\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    int val = gpio_hal_read(idx);
    terminal_transport_printf("Probe CH%u (GP%u): Level is %s (%d)\r\n",
        idx, gpio, val ? "HIGH" : "LOW", val);
}

static void cmd_dio_write(const char* arg) {
    uint8_t gpio = 0, idx = 0;
    const char* next = nullptr;
    if (!parse_pin_arg(arg, &gpio, &idx, &next) || !next || !*next) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Usage: write <pin> <0|1>  (e.g., 'w 0 1' or 'write GP2 0')\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    int level = -1;
    if (*next == '1' || strcasecmp(next, "high") == 0 || strcasecmp(next, "h") == 0) level = 1;
    else if (*next == '0' || strcasecmp(next, "low") == 0 || strcasecmp(next, "l") == 0) level = 0;

    if (level < 0) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Invalid level. Use 0 (LOW) or 1 (HIGH).\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    // Set value BEFORE enabling output buffer to prevent glitching
    gpio_hal_set_dir(idx, true, level != 0);
    terminal_transport_printf("Probe CH%u (GP%u) configured as OUTPUT, driven %s (%d).\r\n",
        idx, gpio, level ? "HIGH" : "LOW", level);
}

static void cmd_dio_dir(const char* arg) {
    uint8_t gpio = 0, idx = 0;
    const char* next = nullptr;
    if (!parse_pin_arg(arg, &gpio, &idx, &next) || !next || !*next) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Usage: dir <pin> <in|out>  (e.g., 'dir 0 out', 'd GP3 in')\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    bool is_out = false;
    if (strcasecmp(next, "out") == 0 || strcasecmp(next, "output") == 0 || strcmp(next, "1") == 0) {
        is_out = true;
    } else if (strcasecmp(next, "in") == 0 || strcasecmp(next, "input") == 0 || strcmp(next, "0") == 0) {
        is_out = false;
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Invalid direction. Use 'in' or 'out'.\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    gpio_hal_set_dir(idx, is_out, false);
    terminal_transport_printf("Probe CH%u (GP%u) direction set to %s.\r\n",
        idx, gpio, is_out ? "OUTPUT" : "INPUT");
}

static void cmd_dio_toggle(const char* arg) {
    uint8_t gpio = 0, idx = 0;
    if (!parse_pin_arg(arg, &gpio, &idx)) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Usage: toggle <pin>  (e.g., 't 0' or 'toggle GP4')\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    gpio_hal_toggle(idx);
    int new_val = gpio_hal_read(idx);
    terminal_transport_printf("Probe CH%u (GP%u) toggled to %s (%d).\r\n",
        idx, gpio, new_val ? "HIGH" : "LOW", new_val);
}

static void cmd_dio_pull(const char* arg) {
    uint8_t gpio = 0, idx = 0;
    const char* next = nullptr;
    if (!parse_pin_arg(arg, &gpio, &idx, &next) || !next || !*next) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Usage: pull <pin> <none|up|down>  (e.g., 'pull 0 up', 'p GP2 none')\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    GpioHalPullMode pm = GPIO_HAL_PULL_NONE;
    if (strcasecmp(next, "up") == 0 || strcasecmp(next, "u") == 0) pm = GPIO_HAL_PULL_UP;
    else if (strcasecmp(next, "down") == 0 || strcasecmp(next, "d") == 0) pm = GPIO_HAL_PULL_DOWN;
    else if (strcasecmp(next, "none") == 0 || strcasecmp(next, "off") == 0 || strcasecmp(next, "n") == 0) pm = GPIO_HAL_PULL_NONE;
    else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Invalid pull mode. Use 'none', 'up', or 'down'.\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    gpio_hal_set_pull(idx, pm);
    terminal_transport_printf("Probe CH%u (GP%u) pull resistor set to %s.\r\n",
        idx, gpio, (pm == GPIO_HAL_PULL_UP) ? "UP" : (pm == GPIO_HAL_PULL_DOWN) ? "DOWN" : "NONE");
}

static void cmd_dio_mask(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Usage: mask <dir_mask> <val_mask>  (e.g., 'mask 0xFF 0xAA')\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    char* endp = nullptr;
    long dir_mask = strtol(arg, &endp, 0);
    while (*endp && isspace((unsigned char)*endp)) endp++;
    if (!*endp) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Missing value mask. Usage: mask <dir_mask> <val_mask>\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }
    long val_mask = strtol(endp, nullptr, 0);

    gpio_hal_write_mask((uint8_t)dir_mask, (uint8_t)val_mask);
    terminal_transport_printf("Probe Bus mask applied: DirMask=0x%02X, ValMask=0x%02X\r\n",
        (uint8_t)dir_mask, (uint8_t)val_mask);
    cmd_dio_pins_table();
}

static bool handle_bp5_numeric_write(const char* cmd) {
    if (strcasecmp(s_mode_name, "DIO") != 0) return false;

    const char* p = cmd;
    while (isspace((unsigned char)*p)) p++;
    if (!*p) return false;

    if (isdigit((unsigned char)*p) || (p[0] == '0' && (p[1] == 'x' || p[1] == 'X' || p[1] == 'b' || p[1] == 'B'))) {
        long val = 0;
        char* endp = nullptr;
        if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) {
            val = strtol(p + 2, &endp, 2);
        } else {
            val = strtol(p, &endp, 0);
        }

        if (endp != p) {
            gpio_hal_write_mask(0xFF, (uint8_t)val);
            terminal_transport_printf("WRITE -> Probe Bus: 0x%02X (0b", (uint8_t)val);
            for (int b = 7; b >= 0; b--) {
                terminal_transport_printf("%c", (val & (1 << b)) ? '1' : '0');
            }
            terminal_transport_print(")\r\n");
            return true;
        }
    }
    return false;
}

static void cmd_dio_dispatch(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        cmd_dio_pins_table();
        return;
    }

    char sub[16] = {0};
    size_t i = 0;
    while (*arg && !isspace((unsigned char)*arg) && i < sizeof(sub) - 1) {
        sub[i++] = *arg++;
    }
    sub[i] = '\0';
    while (*arg && isspace((unsigned char)*arg)) arg++;

    if (strcasecmp(sub, "read") == 0 || strcasecmp(sub, "r") == 0) {
        cmd_dio_read(arg);
    } else if (strcasecmp(sub, "write") == 0 || strcasecmp(sub, "w") == 0) {
        cmd_dio_write(arg);
    } else if (strcasecmp(sub, "dir") == 0 || strcasecmp(sub, "d") == 0) {
        cmd_dio_dir(arg);
    } else if (strcasecmp(sub, "toggle") == 0 || strcasecmp(sub, "t") == 0) {
        cmd_dio_toggle(arg);
    } else if (strcasecmp(sub, "pull") == 0 || strcasecmp(sub, "p") == 0) {
        cmd_dio_pull(arg);
    } else if (strcasecmp(sub, "mask") == 0) {
        cmd_dio_mask(arg);
    } else if (strcasecmp(sub, "pins") == 0 || strcasecmp(sub, "dump") == 0 || strcasecmp(sub, "info") == 0) {
        cmd_dio_pins_table();
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Unknown DIO subcommand '%s'. Type 'help' for commands.\r\n", sub);
        vt100_set_color(VT100_COLOR_RESET);
    }
}

// ─── SPI Subsystem (BP5-Style Hardware SPI Master Mode) ───────────────────────
static uint32_t parse_freq_str(const char* str) {
    if (!str) return 0;
    while (isspace((unsigned char)*str)) str++;
    if (!*str) return 0;

    char* endp = nullptr;
    double val = strtod(str, &endp);
    if (endp == str) return 0;

    while (*endp && isspace((unsigned char)*endp)) endp++;
    if (*endp == 'm' || *endp == 'M') {
        val *= 1000000.0;
    } else if (*endp == 'k' || *endp == 'K') {
        val *= 1000.0;
    }
    return (uint32_t)val;
}

static void cmd_spi_status() {
    SpiHalConfig cfg;
    spi_hal_get_config(&cfg);

    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("\r\n=== RP2040 Hardware SPI Master Configuration ===\r\n");
    vt100_set_color(VT100_COLOR_RESET);
    terminal_transport_printf("  Controller:       RP2040 Hardware SPI0 Master\r\n");
    terminal_transport_printf("  Clock Speed:      %.2f MHz (%lu Hz)\r\n", cfg.speed_hz / 1000000.0f, (unsigned long)cfg.speed_hz);
    terminal_transport_printf("  SPI Mode:         Mode %u (CPOL=%u, CPHA=%u)\r\n",
        (cfg.cpol << 1) | cfg.cpha, cfg.cpol, cfg.cpha);
    terminal_transport_printf("  Bit Order:        %s\r\n", cfg.lsb_first ? "LSB First" : "MSB First (Standard)");
    terminal_transport_printf("  Chip Select:      Probe GP%u (Header P1.%u) | State: %s\r\n",
        cfg.cs_pin, cfg.cs_pin + 2, cfg.cs_asserted ? "ASSERTED (LOW)" : "IDLE (HIGH)");
    terminal_transport_printf("  Physical Pins:    SCK=GP2 (P1.4) | MOSI=GP3 (P1.5) | MISO=GP4 (P1.6)\r\n");
    terminal_transport_printf("  Bus Arbiter:      %s\r\n", cfg.acquired ? "Probe Bus Active (TFT Paused)" : "Display Bus Active");
    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("================================================\r\n\r\n");
    vt100_set_color(VT100_COLOR_RESET);
}

static void cmd_spi_cs(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    SpiHalConfig cfg;
    spi_hal_get_config(&cfg);

    if (!arg || !*arg) {
        terminal_transport_printf("Chip Select (GP%u): Currently %s\r\n",
            cfg.cs_pin, cfg.cs_asserted ? "ASSERTED (LOW)" : "IDLE (HIGH)");
        terminal_transport_print("Usage: cs [low|high|pin]  (e.g., 'cs low', 'cs high', 'cs GP6')\r\n");
        return;
    }

    if (strcasecmp(arg, "low") == 0 || strcasecmp(arg, "assert") == 0 || strcmp(arg, "0") == 0 || strcmp(arg, "[") == 0) {
        spi_hal_set_cs(true);
        terminal_transport_printf("/CS Asserted (LOW on GP%u)\r\n", cfg.cs_pin);
    } else if (strcasecmp(arg, "high") == 0 || strcasecmp(arg, "deassert") == 0 || strcmp(arg, "1") == 0 || strcmp(arg, "]") == 0) {
        spi_hal_set_cs(false);
        terminal_transport_printf("/CS Deasserted (HIGH on GP%u)\r\n", cfg.cs_pin);
    } else {
        // Assume pin specification
        uint8_t gpio = 0;
        if (gpio_hal_resolve_pin((uint8_t)atoi(arg), &gpio, nullptr) ||
            ((arg[0] == 'g' || arg[0] == 'G') && (arg[1] == 'p' || arg[1] == 'P') &&
             gpio_hal_resolve_pin((uint8_t)atoi(arg + 2), &gpio, nullptr))) {
            if (spi_hal_set_cs_pin(gpio)) {
                bp_cfg.cs_pin = gpio;
                terminal_transport_printf("Active SPI CS switched to Probe GP%u (Header P1.%u).\r\n", gpio, gpio + 2);
            } else {
                vt100_set_color(VT100_COLOR_ERROR);
                terminal_transport_print("Failed to set CS pin.\r\n");
                vt100_set_color(VT100_COLOR_RESET);
            }
        } else {
            vt100_set_color(VT100_COLOR_ERROR);
            terminal_transport_print("Invalid CS parameter. Use 'low', 'high', or probe pin (GP2..GP9).\r\n");
            vt100_set_color(VT100_COLOR_RESET);
        }
    }
}

static void cmd_spi_speed(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        SpiHalConfig cfg;
        spi_hal_get_config(&cfg);
        terminal_transport_printf("Current SPI Speed: %.2f MHz (%lu Hz)\r\n",
            cfg.speed_hz / 1000000.0f, (unsigned long)cfg.speed_hz);
        terminal_transport_print("Usage: speed <freq>  (e.g., 'speed 10M', 'speed 400k', 'speed 2000000')\r\n");
        return;
    }

    uint32_t freq = parse_freq_str(arg);
    if (freq < UDOO_SPI_MIN_SPEED || freq > UDOO_SPI_MAX_SPEED) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Speed must be between %u Hz (10 kHz) and %u Hz (30 MHz).\r\n",
            UDOO_SPI_MIN_SPEED, UDOO_SPI_MAX_SPEED);
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    SpiHalConfig cfg;
    spi_hal_get_config(&cfg);
    spi_hal_configure(freq, cfg.cpol, cfg.cpha, cfg.lsb_first);
    bp_cfg.speed_hz = freq;
    terminal_transport_printf("SPI Clock Speed set to %.2f MHz (%lu Hz).\r\n", freq / 1000000.0f, (unsigned long)freq);
}

static void cmd_spi_config(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    SpiHalConfig cfg;
    spi_hal_get_config(&cfg);

    if (!arg || !*arg) {
        cmd_spi_status();
        terminal_transport_print("Usage: config <mode: 0-3> [msb|lsb]  (e.g., 'mode 0', 'config 3 lsb')\r\n");
        return;
    }

    // Parse mode number 0, 1, 2, 3
    if (isdigit((unsigned char)*arg)) {
        int m = *arg - '0';
        if (m >= 0 && m <= 3) {
            uint8_t cpol = (m & 2) ? 1 : 0;
            uint8_t cpha = (m & 1) ? 1 : 0;
            bool lsb = cfg.lsb_first;

            const char* next = arg + 1;
            while (*next && isspace((unsigned char)*next)) next++;
            if (strcasecmp(next, "lsb") == 0) lsb = true;
            else if (strcasecmp(next, "msb") == 0) lsb = false;

            spi_hal_configure(cfg.speed_hz, cpol, cpha, lsb);
            bp_cfg.spi_polarity = cpol;
            bp_cfg.spi_phase = cpha;
            terminal_transport_printf("SPI configured to Mode %d (CPOL=%u, CPHA=%u), %s.\r\n",
                m, cpol, cpha, lsb ? "LSB First" : "MSB First");
            return;
        }
    } else if (strcasecmp(arg, "msb") == 0) {
        spi_hal_configure(cfg.speed_hz, cfg.cpol, cfg.cpha, false);
        terminal_transport_print("SPI Bit Order set to MSB First.\r\n");
        return;
    } else if (strcasecmp(arg, "lsb") == 0) {
        spi_hal_configure(cfg.speed_hz, cfg.cpol, cfg.cpha, true);
        terminal_transport_print("SPI Bit Order set to LSB First.\r\n");
        return;
    }

    vt100_set_color(VT100_COLOR_ERROR);
    terminal_transport_print("Invalid configuration. Usage: mode <0|1|2|3> [msb|lsb]\r\n");
    vt100_set_color(VT100_COLOR_RESET);
}

static void cmd_spi_read(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    uint16_t count = 1;
    if (arg && *arg) {
        if (*arg == ':') arg++;
        long c = strtol(arg, nullptr, 10);
        if (c > 0 && c <= 1024) count = (uint16_t)c;
    }

    terminal_transport_printf("SPI READ (%u byte%s):\r\n  HEX:   ", count, count > 1 ? "s" : "");
    char ascii_buf[64] = {0};
    size_t a_idx = 0;

    for (uint16_t i = 0; i < count; i++) {
        uint8_t val = spi_hal_read_byte(0xFF);
        terminal_transport_printf("0x%02X ", val);

        if (a_idx < sizeof(ascii_buf) - 1) {
            ascii_buf[a_idx++] = (val >= 32 && val <= 126) ? (char)val : '.';
        }

        if ((i + 1) % 16 == 0 && (i + 1) < count) {
            ascii_buf[a_idx] = '\0';
            terminal_transport_printf(" | %s\r\n         ", ascii_buf);
            a_idx = 0;
        }
    }
    ascii_buf[a_idx] = '\0';
    terminal_transport_printf(" | %s\r\n\r\n", ascii_buf);
}

static void cmd_spi_write_or_xfer(const char* arg, bool full_duplex) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Usage: write <byte1> [byte2 ...]  (e.g., 'w 0x9F' or 'w 0x06 0x01')\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    uint8_t tx_bytes[64];
    uint8_t rx_bytes[64];
    size_t count = 0;

    const char* p = arg;
    while (*p && count < sizeof(tx_bytes)) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        char* endp = nullptr;
        long val = strtol(p, &endp, 0);
        if (endp == p) break;

        tx_bytes[count++] = (uint8_t)val;
        p = endp;
    }

    if (count == 0) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("No valid data bytes parsed.\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    for (size_t i = 0; i < count; i++) {
        rx_bytes[i] = spi_hal_transfer(tx_bytes[i]);
    }

    terminal_transport_printf("SPI %s (%u byte%s):\r\n",
        full_duplex ? "TRANSFER" : "WRITE", (unsigned int)count, count > 1 ? "s" : "");

    terminal_transport_print("  TX: ");
    for (size_t i = 0; i < count; i++) {
        terminal_transport_printf("0x%02X ", tx_bytes[i]);
    }
    terminal_transport_print("\r\n  RX: ");
    for (size_t i = 0; i < count; i++) {
        terminal_transport_printf("0x%02X ", rx_bytes[i]);
    }
    terminal_transport_print("\r\n\r\n");
}

static void cmd_spi_flash(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    SpiHalConfig cfg;
    spi_hal_get_config(&cfg);

    if (!arg || !*arg || strcasecmp(arg, "probe") == 0 || strcasecmp(arg, "id") == 0) {
        flash_full_probe(cfg.cs_pin, Serial);
        return;
    }

    char sub[16] = {0};
    size_t i = 0;
    while (*arg && !isspace((unsigned char)*arg) && i < sizeof(sub) - 1) {
        sub[i++] = *arg++;
    }
    sub[i] = '\0';
    while (*arg && isspace((unsigned char)*arg)) arg++;

    if (strcasecmp(sub, "status") == 0) {
        flash_full_status(cfg.cs_pin, Serial);
    } else if (strcasecmp(sub, "sfdp") == 0) {
        flash_full_sfdp(cfg.cs_pin, Serial);
    } else if (strcasecmp(sub, "read") == 0) {
        char* endp = nullptr;
        uint32_t addr = (uint32_t)strtoul(arg, &endp, 0);
        uint32_t len = 64;
        if (endp && *endp) {
            len = (uint32_t)strtoul(endp, nullptr, 0);
            if (len == 0) len = 64;
        }
        flash_full_read_hex(cfg.cs_pin, addr, len, Serial);
    } else if (strcasecmp(sub, "dump") == 0) {
        uint32_t pages = (uint32_t)strtoul(arg, nullptr, 0);
        if (pages == 0) pages = 1;
        flash_full_dump(cfg.cs_pin, pages, Serial);
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Unknown flash subcommand '%s'. Options: probe, status, sfdp, read, dump\r\n", sub);
        vt100_set_color(VT100_COLOR_RESET);
    }
}

static void cmd_spi_dispatch(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        cmd_spi_status();
        return;
    }

    char sub[16] = {0};
    size_t i = 0;
    while (*arg && !isspace((unsigned char)*arg) && i < sizeof(sub) - 1) {
        sub[i++] = *arg++;
    }
    sub[i] = '\0';
    while (*arg && isspace((unsigned char)*arg)) arg++;

    if (strcasecmp(sub, "status") == 0 || strcasecmp(sub, "pins") == 0 || strcasecmp(sub, "info") == 0) {
        cmd_spi_status();
    } else if (strcasecmp(sub, "cs") == 0) {
        cmd_spi_cs(arg);
    } else if (strcasecmp(sub, "speed") == 0 || strcasecmp(sub, "baud") == 0) {
        cmd_spi_speed(arg);
    } else if (strcasecmp(sub, "mode") == 0 || strcasecmp(sub, "config") == 0) {
        cmd_spi_config(arg);
    } else if (strcasecmp(sub, "read") == 0 || strcasecmp(sub, "r") == 0) {
        cmd_spi_read(arg);
    } else if (strcasecmp(sub, "write") == 0 || strcasecmp(sub, "w") == 0) {
        cmd_spi_write_or_xfer(arg, false);
    } else if (strcasecmp(sub, "transfer") == 0 || strcasecmp(sub, "xfer") == 0 || strcasecmp(sub, "x") == 0) {
        cmd_spi_write_or_xfer(arg, true);
    } else if (strcasecmp(sub, "probe") == 0 || strcasecmp(sub, "flash") == 0) {
        cmd_spi_flash(arg);
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Unknown SPI subcommand '%s'. Type 'help' for commands.\r\n", sub);
        vt100_set_color(VT100_COLOR_RESET);
    }
}

// ─── I2C Subsystem (BP5-Style Hardware I2C Master Mode) ───────────────────────
static void cmd_i2c_status() {
    I2cHalConfig cfg;
    i2c_hal_get_config(&cfg);

    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("\r\n=== RP2040 Hardware I2C Master Configuration ===\r\n");
    vt100_set_color(VT100_COLOR_RESET);
    terminal_transport_printf("  Controller:       RP2040 Hardware I2C1 Master\r\n");
    terminal_transport_printf("  Clock Speed:      %lu kHz (%lu Hz)\r\n", (unsigned long)(cfg.speed_hz / 1000), (unsigned long)cfg.speed_hz);
    terminal_transport_printf("  Pull-Up Resistors: %s\r\n", cfg.pullups_enabled ? "Internal Weak Pull-Ups Enabled (~50k)" : "Disabled (External Pull-Ups Only)");
    terminal_transport_printf("  Physical Pins:    SDA=GP2 (Header P1.4) | SCL=GP3 (Header P1.5)\r\n");
    terminal_transport_printf("  Bus State:        %s\r\n", cfg.active ? "Active (Acquired)" : "Idle / Safe High-Z");
    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("================================================\r\n\r\n");
    vt100_set_color(VT100_COLOR_RESET);
}

static void cmd_i2c_speed(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        I2cHalConfig cfg;
        i2c_hal_get_config(&cfg);
        terminal_transport_printf("Current I2C Speed: %lu kHz (%lu Hz)\r\n",
            (unsigned long)(cfg.speed_hz / 1000), (unsigned long)cfg.speed_hz);
        terminal_transport_print("Usage: speed <freq>  (e.g., 'speed 100k', 'speed 400k', 'speed 1M')\r\n");
        return;
    }

    uint32_t freq = parse_freq_str(arg);
    if (freq < UDOO_I2C_MIN_SPEED || freq > UDOO_I2C_MAX_SPEED) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Speed must be between %u Hz (10 kHz) and %u Hz (1 MHz).\r\n",
            UDOO_I2C_MIN_SPEED, UDOO_I2C_MAX_SPEED);
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    i2c_hal_set_speed(freq);
    bp_cfg.speed_hz = freq;
    terminal_transport_printf("I2C Clock Speed set to %lu kHz (%lu Hz).\r\n",
        (unsigned long)(freq / 1000), (unsigned long)freq);
}

static void cmd_i2c_pull(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        I2cHalConfig cfg;
        i2c_hal_get_config(&cfg);
        terminal_transport_printf("Internal Pull-Ups: %s\r\n", cfg.pullups_enabled ? "ON" : "OFF");
        terminal_transport_print("Usage: pull <on|off|1|0>\r\n");
        return;
    }

    bool enable = false;
    if (strcasecmp(arg, "on") == 0 || strcasecmp(arg, "1") == 0 || strcasecmp(arg, "enable") == 0 || strcasecmp(arg, "up") == 0) {
        enable = true;
    } else if (strcasecmp(arg, "off") == 0 || strcasecmp(arg, "0") == 0 || strcasecmp(arg, "disable") == 0 || strcasecmp(arg, "none") == 0) {
        enable = false;
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Invalid pull parameter. Use 'on' or 'off'.\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    i2c_hal_set_pullups(enable);
    terminal_transport_printf("Internal Pull-Up Resistors on SDA (GP2) and SCL (GP3): %s.\r\n",
        enable ? "ENABLED (~50k)" : "DISABLED");
}

static void cmd_i2c_scan(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    bool verbose = false;
    if (arg && (strcasecmp(arg, "-v") == 0 || strcasecmp(arg, "verbose") == 0 || strcasecmp(arg, "v") == 0)) {
        verbose = true;
    }
    i2c_hal_scan(Serial, verbose);
}

static void cmd_i2c_read(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        terminal_transport_print("Usage: read <addr_7bit> [count]  (e.g., 'read 0x50 16' or 'r 0x68 6')\r\n");
        return;
    }

    char* endp = nullptr;
    uint32_t addr = strtoul(arg, &endp, 0);
    if (endp == arg) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Invalid target I2C address.\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }
    if (addr > 0x77) {
        addr >>= 1;
    }

    size_t count = 1;
    if (endp && *endp) {
        while (isspace((unsigned char)*endp)) endp++;
        if (*endp) {
            count = strtoul(endp, nullptr, 0);
            if (count == 0) count = 1;
            if (count > 256) count = 256;
        }
    }

    uint8_t buf[256];
    size_t got = i2c_hal_read((uint8_t)addr, buf, count, true);
    if (got == 0) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("No response (NACK) from I2C device at 0x%02X.\r\n", (uint8_t)addr);
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    terminal_transport_printf("Read %u byte(s) from 0x%02X:\r\n", (unsigned int)got, (uint8_t)addr);
    for (size_t row = 0; row < got; row += 16) {
        terminal_transport_printf("  %04X: ", (unsigned int)row);
        size_t chunk = (got - row > 16) ? 16 : (got - row);
        for (size_t col = 0; col < 16; col++) {
            if (col < chunk) terminal_transport_printf("%02X ", buf[row + col]);
            else terminal_transport_print("   ");
        }
        terminal_transport_print(" |");
        for (size_t col = 0; col < chunk; col++) {
            char ch = (char)buf[row + col];
            terminal_transport_printf("%c", (ch >= 32 && ch <= 126) ? ch : '.');
        }
        terminal_transport_print("|\r\n");
    }
}

static void cmd_i2c_write(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        terminal_transport_print("Usage: write <addr_7bit> <data...>  (e.g., 'write 0x50 0x00 0x12 0x34')\r\n");
        return;
    }

    char* endp = nullptr;
    uint32_t addr = strtoul(arg, &endp, 0);
    if (endp == arg) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Invalid target I2C address.\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }
    if (addr > 0x77) addr >>= 1;

    uint8_t buf[64];
    size_t count = 0;
    const char* p = endp;

    while (*p && count < sizeof(buf)) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        char* token_end = nullptr;
        uint32_t val = strtoul(p, &token_end, 0);
        if (token_end == p) break;
        buf[count++] = (uint8_t)val;
        p = token_end;
    }

    if (count == 0) {
        bool ack = i2c_hal_probe_addr((uint8_t)addr);
        terminal_transport_printf("I2C Address 0x%02X write probe: %s\r\n",
            (uint8_t)addr, ack ? "ACK (Device present)" : "NACK (No device)");
        return;
    }

    uint8_t status = i2c_hal_write((uint8_t)addr, buf, count, true);
    if (status == 0) {
        terminal_transport_printf("Wrote %u byte(s) to 0x%02X: ACK received.\r\n", (unsigned int)count, (uint8_t)addr);
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        const char* err_str = "General Error";
        if (status == 1) err_str = "Buffer Overflow";
        else if (status == 2) err_str = "NACK on Address";
        else if (status == 3) err_str = "NACK on Data";
        terminal_transport_printf("Write to 0x%02X failed: status %u (%s).\r\n", (uint8_t)addr, status, err_str);
        vt100_set_color(VT100_COLOR_RESET);
    }
}

static void cmd_i2c_eeprom(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg || strncasecmp(arg, "probe", 5) == 0) {
        uint8_t addr = 0x50;
        const char* p = arg ? strchr(arg, ' ') : nullptr;
        if (p) addr = (uint8_t)strtoul(p, nullptr, 0);
        eeprom_probe_chip(addr, Serial);
    } else if (strncasecmp(arg, "read", 4) == 0) {
        uint32_t mem_addr = 0;
        uint16_t len = 64;
        uint32_t a16 = 1;
        uint32_t i2c_addr = 0x50;
        sscanf(arg + 4, "%lx %hu %lu %lx", &mem_addr, &len, &a16, &i2c_addr);
        eeprom_read_hex((uint8_t)i2c_addr, mem_addr, len, a16 != 0, Serial);
    } else if (strncasecmp(arg, "write", 5) == 0) {
        uint32_t mem_addr = 0;
        uint32_t val = 0;
        uint32_t a16 = 1;
        uint32_t i2c_addr = 0x50;
        sscanf(arg + 5, "%lx %lx %lu %lx", &mem_addr, &val, &a16, &i2c_addr);
        eeprom_write_val((uint8_t)i2c_addr, mem_addr, (uint8_t)val, a16 != 0, Serial);
    } else if (strncasecmp(arg, "dump", 4) == 0) {
        uint32_t bytes = 256;
        uint32_t a16 = 1;
        uint32_t i2c_addr = 0x50;
        sscanf(arg + 4, "%lu %lu %lx", &bytes, &a16, &i2c_addr);
        eeprom_dump_all((uint8_t)i2c_addr, bytes, a16 != 0, Serial);
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Unknown eeprom subcommand. Usage: eeprom [probe|read|write|dump]\r\n");
        vt100_set_color(VT100_COLOR_RESET);
    }
}

static void cmd_i2c_sniff() {
    i2c_sniffer_start(UDOO_I2C_SDA_PIN, UDOO_I2C_SCL_PIN, Serial);
}

static void cmd_i2c_dispatch(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        cmd_i2c_status();
        return;
    }

    char sub[16] = {0};
    size_t i = 0;
    while (*arg && !isspace((unsigned char)*arg) && i < sizeof(sub) - 1) {
        sub[i++] = *arg++;
    }
    sub[i] = '\0';
    while (*arg && isspace((unsigned char)*arg)) arg++;

    if (strcasecmp(sub, "status") == 0 || strcasecmp(sub, "pins") == 0 || strcasecmp(sub, "info") == 0) {
        cmd_i2c_status();
    } else if (strcasecmp(sub, "scan") == 0 || strcasecmp(sub, "search") == 0 || strcasecmp(sub, "s") == 0) {
        cmd_i2c_scan(arg);
    } else if (strcasecmp(sub, "read") == 0 || strcasecmp(sub, "r") == 0) {
        cmd_i2c_read(arg);
    } else if (strcasecmp(sub, "write") == 0 || strcasecmp(sub, "w") == 0) {
        cmd_i2c_write(arg);
    } else if (strcasecmp(sub, "speed") == 0 || strcasecmp(sub, "baud") == 0 || strcasecmp(sub, "clk") == 0) {
        cmd_i2c_speed(arg);
    } else if (strcasecmp(sub, "pull") == 0 || strcasecmp(sub, "p") == 0) {
        cmd_i2c_pull(arg);
    } else if (strcasecmp(sub, "eeprom") == 0) {
        cmd_i2c_eeprom(arg);
    } else if (strcasecmp(sub, "sniff") == 0) {
        cmd_i2c_sniff();
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Unknown I2C subcommand '%s'. Type 'help' for commands.\r\n", sub);
        vt100_set_color(VT100_COLOR_RESET);
    }
}

// ─── UART Subsystem (BP5-Style Hardware UART Bridge & Monitor) ────────────────
static void cmd_uart_status() {
    UartHalConfig cfg;
    uart_hal_get_config(&cfg);

    char par_char = 'N';
    if (cfg.parity == UART_PARITY_EVEN_MODE) par_char = 'E';
    else if (cfg.parity == UART_PARITY_ODD_MODE) par_char = 'O';

    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("\r\n=== RP2040 Hardware UART Transceiver Configuration ===\r\n");
    vt100_set_color(VT100_COLOR_RESET);
    terminal_transport_printf("  Controller:       RP2040 Hardware UART1 (Serial2)\r\n");
    terminal_transport_printf("  Configured Baud:  %lu bps\r\n", (unsigned long)cfg.baudrate);
    terminal_transport_printf("  Actual HW Baud:   %lu bps\r\n", (unsigned long)cfg.actual_baud);
    terminal_transport_printf("  Framing Format:   %u%c%u (%u Data Bits, %s Parity, %u Stop Bit%s)\r\n",
        cfg.data_bits, par_char, cfg.stop_bits,
        cfg.data_bits,
        cfg.parity == UART_PARITY_NONE_MODE ? "No" : (cfg.parity == UART_PARITY_EVEN_MODE ? "Even" : "Odd"),
        cfg.stop_bits, cfg.stop_bits > 1 ? "s" : "");
    terminal_transport_printf("  Signal Inversion: %s\r\n", cfg.invert ? "INVERTED (Active LOW)" : "Standard (Idle HIGH Mark)");
    terminal_transport_printf("  Physical Pins:    TX=GP4 (Header P1.6) | RX=GP5 (Header P1.7)\r\n");
    terminal_transport_printf("  Peripheral State: %s\r\n", cfg.active ? "Active (Transceiver Running)" : "Idle / Safe High-Z");
    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("======================================================\r\n\r\n");
    vt100_set_color(VT100_COLOR_RESET);
}

static void cmd_uart_baud(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        UartHalConfig cfg;
        uart_hal_get_config(&cfg);
        terminal_transport_printf("Current UART Baud: %lu bps\r\n", (unsigned long)cfg.baudrate);
        terminal_transport_print("Usage: baud <rate>  (e.g., 'baud 9600', 'baud 115200', 'baud 1M')\r\n");
        return;
    }

    uint32_t baud = parse_freq_str(arg);
    if (baud < UDOO_UART_MIN_BAUD || baud > UDOO_UART_MAX_BAUD) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Baud rate must be between %u and %u bps.\r\n",
            UDOO_UART_MIN_BAUD, UDOO_UART_MAX_BAUD);
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    uart_hal_set_baudrate(baud);
    bp_cfg.speed_hz = baud;
    terminal_transport_printf("UART Baud Rate updated to %lu bps.\r\n", (unsigned long)baud);
}

static void cmd_uart_format(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        cmd_uart_status();
        terminal_transport_print("Usage: config <format>  (e.g., 'config 8N1', 'config 8E1', 'config 7E1')\r\n");
        return;
    }

    uint8_t bits = 8;
    UartHalParity par = UART_PARITY_NONE_MODE;
    uint8_t stops = 1;

    if (isdigit((unsigned char)arg[0])) {
        bits = arg[0] - '0';
        if (bits < 5 || bits > 8) bits = 8;
    }

    if (arg[1]) {
        char p = toupper((unsigned char)arg[1]);
        if (p == 'E') par = UART_PARITY_EVEN_MODE;
        else if (p == 'O') par = UART_PARITY_ODD_MODE;
        else par = UART_PARITY_NONE_MODE;
    }

    if (arg[1] && arg[2]) {
        if (arg[2] == '2') stops = 2;
        else stops = 1;
    }

    uart_hal_set_format(bits, par, stops);
    terminal_transport_printf("UART framing updated to %u%c%u.\r\n",
        bits, par == UART_PARITY_EVEN_MODE ? 'E' : (par == UART_PARITY_ODD_MODE ? 'O' : 'N'), stops);
}

static void cmd_uart_write(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        terminal_transport_print("Usage: write <\"string\"|0xNN ...>  (e.g., 'write \"AT\\r\\n\"' or 'w 0x55 0xAA')\r\n");
        return;
    }

    if (*arg == '"') {
        arg++;
        char out_buf[256];
        size_t out_len = 0;
        while (*arg && *arg != '"' && out_len < sizeof(out_buf) - 1) {
            if (*arg == '\\' && *(arg + 1)) {
                arg++;
                if (*arg == 'r') out_buf[out_len++] = '\r';
                else if (*arg == 'n') out_buf[out_len++] = '\n';
                else if (*arg == 't') out_buf[out_len++] = '\t';
                else if (*arg == '\\') out_buf[out_len++] = '\\';
                else if (*arg == '"') out_buf[out_len++] = '"';
                else if (*arg == '0') out_buf[out_len++] = '\0';
                else out_buf[out_len++] = *arg;
                arg++;
            } else {
                out_buf[out_len++] = *arg++;
            }
        }
        size_t written = uart_hal_write((const uint8_t*)out_buf, out_len);
        terminal_transport_printf("Sent %u byte(s) across TX (GP4).\r\n", (unsigned int)written);
    } else {
        bool is_hex = false;
        const char* p = arg;
        while (*p) {
            if (*p == '0' && (tolower(*(p+1)) == 'x')) { is_hex = true; break; }
            p++;
        }

        if (is_hex) {
            uint8_t buf[64];
            size_t count = 0;
            p = arg;
            while (*p && count < sizeof(buf)) {
                while (isspace((unsigned char)*p)) p++;
                if (!*p) break;
                char* token_end = nullptr;
                uint32_t val = strtoul(p, &token_end, 0);
                if (token_end == p) break;
                buf[count++] = (uint8_t)val;
                p = token_end;
            }
            size_t written = uart_hal_write(buf, count);
            terminal_transport_printf("Sent %u byte(s) across TX (GP4).\r\n", (unsigned int)written);
        } else {
            size_t written = uart_hal_write_str(arg);
            terminal_transport_printf("Sent %u byte(s) across TX (GP4).\r\n", (unsigned int)written);
        }
    }
}

static void cmd_uart_read(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    size_t count = 64;
    if (arg && *arg) {
        count = strtoul(arg, nullptr, 0);
        if (count == 0) count = 64;
        if (count > 256) count = 256;
    }

    int avail = uart_hal_available();
    if (avail == 0) {
        terminal_transport_print("RX FIFO empty. No incoming UART bytes.\r\n");
        return;
    }

    uint8_t buf[256];
    size_t got = uart_hal_read(buf, count);
    terminal_transport_printf("Read %u byte(s) from RX (GP5):\r\n", (unsigned int)got);
    for (size_t row = 0; row < got; row += 16) {
        terminal_transport_printf("  %04X: ", (unsigned int)row);
        size_t chunk = (got - row > 16) ? 16 : (got - row);
        for (size_t col = 0; col < 16; col++) {
            if (col < chunk) terminal_transport_printf("%02X ", buf[row + col]);
            else terminal_transport_print("   ");
        }
        terminal_transport_print(" |");
        for (size_t col = 0; col < chunk; col++) {
            char ch = (char)buf[row + col];
            terminal_transport_printf("%c", (ch >= 32 && ch <= 126) ? ch : '.');
        }
        terminal_transport_print("|\r\n");
    }
}

static void cmd_uart_bridge() {
    uart_hal_bridge(Serial);
}

static void cmd_uart_monitor(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    bool is_hex = (arg && (strcasecmp(arg, "hex") == 0 || strcasecmp(arg, "-x") == 0));
    uart_hal_monitor(Serial, is_hex, 0);
}

static void cmd_uart_autobaud() {
    terminal_transport_print("\r\n[Auto-Baud] Listening for transitions on RX (GP5) for 2.0s...\r\n");
    uint32_t detected = uart_hal_detect_baud(2000);
    if (detected > 0) {
        vt100_set_color(VT100_COLOR_VALUE);
        terminal_transport_printf("Detected Baud Rate: %lu bps!\r\n", (unsigned long)detected);
        vt100_set_color(VT100_COLOR_RESET);
        terminal_transport_print("Run 'baud <detected>' to apply this rate.\r\n");
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("No valid baud rate detected (line idle or noise).\r\n");
        vt100_set_color(VT100_COLOR_RESET);
    }
}

static void cmd_uart_loopback() {
    uart_hal_test_loopback(Serial);
}

static void cmd_uart_dispatch(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        cmd_uart_status();
        return;
    }

    char sub[16] = {0};
    size_t i = 0;
    while (*arg && !isspace((unsigned char)*arg) && i < sizeof(sub) - 1) {
        sub[i++] = *arg++;
    }
    sub[i] = '\0';
    while (*arg && isspace((unsigned char)*arg)) arg++;

    if (strcasecmp(sub, "status") == 0 || strcasecmp(sub, "pins") == 0 || strcasecmp(sub, "info") == 0) {
        cmd_uart_status();
    } else if (strcasecmp(sub, "baud") == 0 || strcasecmp(sub, "speed") == 0 || strcasecmp(sub, "clk") == 0) {
        cmd_uart_baud(arg);
    } else if (strcasecmp(sub, "config") == 0 || strcasecmp(sub, "format") == 0 || strcasecmp(sub, "mode") == 0) {
        cmd_uart_format(arg);
    } else if (strcasecmp(sub, "write") == 0 || strcasecmp(sub, "w") == 0 || strcasecmp(sub, "send") == 0) {
        cmd_uart_write(arg);
    } else if (strcasecmp(sub, "read") == 0 || strcasecmp(sub, "r") == 0) {
        cmd_uart_read(arg);
    } else if (strcasecmp(sub, "bridge") == 0) {
        cmd_uart_bridge();
    } else if (strcasecmp(sub, "monitor") == 0 || strcasecmp(sub, "mon") == 0) {
        cmd_uart_monitor(arg);
    } else if (strcasecmp(sub, "autobaud") == 0 || strcasecmp(sub, "detect") == 0) {
        cmd_uart_autobaud();
    } else if (strcasecmp(sub, "loopback") == 0 || strcasecmp(sub, "test") == 0) {
        cmd_uart_loopback();
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Unknown UART subcommand '%s'. Type 'help' for commands.\r\n", sub);
        vt100_set_color(VT100_COLOR_RESET);
    }
}

// ─── 1-Wire Subsystem (Milestone PROTO-5) ────────────────────────────────────
static void cmd_1wire_status() {
    OneWireHalConfig cfg;
    onewire_hal_get_config(&cfg);

    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("\r\n=== Dallas 1-Wire Bus Master Configuration ===\r\n");
    vt100_set_color(VT100_COLOR_RESET);
    terminal_transport_printf("  Protocol:         Dallas / Maxim 1-Wire (Standard Speed)\r\n");
    terminal_transport_printf("  Physical Data DQ: GP%u (Header P1 Pin %u, Probe CH%u)\r\n",
        cfg.pin, cfg.pin + 2, cfg.pin - 2);
    terminal_transport_printf("  Internal Pull-up: %s (~50k resistor)\r\n",
        cfg.pullup_enabled ? "ENABLED" : "DISABLED");
    terminal_transport_printf("  External Pull-up: 4.7k to 3.3V recommended for reliable multi-drop\r\n");
    terminal_transport_printf("  Driver State:     %s\r\n",
        cfg.active ? "Active (Open-Drain Master)" : "Idle / Safe High-Z");
    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("==============================================\r\n\r\n");
    vt100_set_color(VT100_COLOR_RESET);
}

static void cmd_1wire_scan() {
    onewire_hal_search_bus(Serial);
}

static void cmd_1wire_temp() {
    onewire_hal_read_all_temps(Serial);
}

static void cmd_1wire_reset() {
    terminal_transport_print("\r\nIssuing 1-Wire Reset pulse (480us)... ");
    bool presence = onewire_hal_reset();
    if (presence) {
        vt100_set_color(VT100_COLOR_VALUE);
        terminal_transport_print("PRESENCE DETECTED (ACK)\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        terminal_transport_print("At least one 1-Wire device is present and responding on DQ line.\r\n");
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("NO PRESENCE (NACK)\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        terminal_transport_print("Bus remained HIGH. Check device connections and pull-up resistor.\r\n");
    }
}

static void cmd_1wire_rom() {
    uint8_t rom[8] = {0};
    terminal_transport_print("\r\nIssuing 0x33 (Read ROM) command... ");
    bool ok = onewire_hal_read_single_rom(rom);
    if (ok) {
        vt100_set_color(VT100_COLOR_VALUE);
        terminal_transport_print("ROM READ SUCCESS\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        terminal_transport_printf("  64-Bit ROM ID: [");
        for (int i = 0; i < 8; i++) {
            terminal_transport_printf("%02X", rom[i]);
            if (i < 7) terminal_transport_print(":");
        }
        terminal_transport_printf("]\r\n  Family Code:   0x%02X (%s)\r\n",
            rom[0], onewire_hal_get_family_desc(rom[0]));
        terminal_transport_printf("  Serial Number: %02X%02X%02X%02X%02X%02X\r\n",
            rom[6], rom[5], rom[4], rom[3], rom[2], rom[1]);
        terminal_transport_printf("  CRC Byte:      0x%02X (Valid)\r\n", rom[7]);
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("FAILED\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        terminal_transport_print("No valid single-device ROM response (either no device, multiple devices, or CRC error).\r\n");
    }
}

static void cmd_1wire_pull(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        OneWireHalConfig cfg;
        onewire_hal_get_config(&cfg);
        terminal_transport_printf("1-Wire Internal Pull-up: %s\r\n", cfg.pullup_enabled ? "ON" : "OFF");
        terminal_transport_print("Usage: pull <on|off>\r\n");
        return;
    }

    if (strcasecmp(arg, "on") == 0 || strcmp(arg, "1") == 0) {
        onewire_hal_set_pullup(true);
        terminal_transport_print("Internal weak pull-up enabled on 1-Wire DQ pin.\r\n");
    } else if (strcasecmp(arg, "off") == 0 || strcmp(arg, "0") == 0) {
        onewire_hal_set_pullup(false);
        terminal_transport_print("Internal weak pull-up disabled. Ensure external 4.7k pull-up is fitted.\r\n");
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Invalid pull argument. Use 'pull on' or 'pull off'.\r\n");
        vt100_set_color(VT100_COLOR_RESET);
    }
}

static void cmd_1wire_pin(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        OneWireHalConfig cfg;
        onewire_hal_get_config(&cfg);
        terminal_transport_printf("Current 1-Wire DQ Pin: GP%u\r\n", cfg.pin);
        terminal_transport_print("Usage: pin <2..9>  (e.g., 'pin 6')\r\n");
        return;
    }

    long p = strtol(arg, nullptr, 10);
    if (p < 2 || p > 9) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Error: Pin must be within probe bus range GP2..GP9.\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    if (onewire_hal_set_pin((uint8_t)p)) {
        terminal_transport_printf("1-Wire DQ data line assigned to GP%ld.\r\n", p);
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Failed to reassign 1-Wire pin.\r\n");
        vt100_set_color(VT100_COLOR_RESET);
    }
}

static void cmd_1wire_write(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        terminal_transport_print("Usage: write <0xNN ...>  (e.g., 'write 0xCC 0x44')\r\n");
        return;
    }

    const char* p = arg;
    size_t count = 0;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        char* endp = nullptr;
        unsigned long val = strtoul(p, &endp, 0);
        if (endp == p) break;
        onewire_hal_write_byte((uint8_t)val);
        count++;
        p = endp;
    }
    terminal_transport_printf("Transmitted %u byte(s) across 1-Wire bus.\r\n", (unsigned int)count);
}

static void cmd_1wire_read(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    size_t count = 1;
    if (arg && *arg) {
        count = strtoul(arg, nullptr, 0);
        if (count == 0) count = 1;
        if (count > 64) count = 64;
    }

    terminal_transport_printf("Reading %u byte(s) from 1-Wire bus:\r\n  ", (unsigned int)count);
    for (size_t i = 0; i < count; i++) {
        uint8_t b = onewire_hal_read_byte();
        terminal_transport_printf("%02X ", b);
    }
    terminal_transport_print("\r\n");
}

static void cmd_1wire_dispatch(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        cmd_1wire_status();
        return;
    }

    char sub[16] = {0};
    size_t i = 0;
    while (*arg && !isspace((unsigned char)*arg) && i < sizeof(sub) - 1) {
        sub[i++] = *arg++;
    }
    sub[i] = '\0';
    while (*arg && isspace((unsigned char)*arg)) arg++;

    if (strcasecmp(sub, "status") == 0 || strcasecmp(sub, "pins") == 0 || strcasecmp(sub, "info") == 0) {
        cmd_1wire_status();
    } else if (strcasecmp(sub, "scan") == 0 || strcasecmp(sub, "search") == 0 || strcasecmp(sub, "s") == 0) {
        cmd_1wire_scan();
    } else if (strcasecmp(sub, "temp") == 0 || strcasecmp(sub, "read") == 0 || strcasecmp(sub, "t") == 0) {
        cmd_1wire_temp();
    } else if (strcasecmp(sub, "reset") == 0) {
        cmd_1wire_reset();
    } else if (strcasecmp(sub, "rom") == 0) {
        cmd_1wire_rom();
    } else if (strcasecmp(sub, "pull") == 0 || strcasecmp(sub, "p") == 0) {
        cmd_1wire_pull(arg);
    } else if (strcasecmp(sub, "pin") == 0) {
        cmd_1wire_pin(arg);
    } else if (strcasecmp(sub, "write") == 0 || strcasecmp(sub, "w") == 0) {
        cmd_1wire_write(arg);
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Unknown 1-Wire subcommand '%s'. Type 'help' for commands.\r\n", sub);
        vt100_set_color(VT100_COLOR_RESET);
    }
}

// ─── Meter Subsystem (Milestone PROTO-6) ─────────────────────────────────────
static void cmd_meter_status() {
    MeterHalConfig cfg;
    meter_hal_get_config(&cfg);

    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("\r\n=== Precision Voltmeter, Resistance & Continuity Mode ===\r\n");
    vt100_set_color(VT100_COLOR_RESET);
    terminal_transport_print("  Analog Inputs:    ADC0=GP26 (Header P2.31) | ADC1=GP27 (P2.32) | ADC2=GP28 (P2.34)\r\n");
    terminal_transport_print("  Power Monitor:    VSYS Rail (GP29, 1/3 Hardware Attenuation Divider)\r\n");
    terminal_transport_print("  Die Temperature:  Internal RP2040 Core Sensor\r\n");
    terminal_transport_print("  Acoustic Buzzer:  GP20 (Piezo Tone Driver, PWM Slice 2A)\r\n");
    terminal_transport_printf("  Cont. Threshold:  < %.1f ohms (Beep) | Release: > %.1f ohms\r\n",
        cfg.cont_thresh_ohms, cfg.cont_thresh_ohms + cfg.cont_hyst_ohms);
    terminal_transport_printf("  Excitation Ref:   %.1f ohms Pull-up to 3.3V\r\n", cfg.r_ref_ohms);
    terminal_transport_printf("  Buzzer Feedback:  %s\r\n", cfg.buzzer_enabled ? "ENABLED" : "MUTED");
    terminal_transport_printf("  Meter State:      %s\r\n", cfg.active ? "Active" : "Idle");
    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("=========================================================\r\n\r\n");
    vt100_set_color(VT100_COLOR_RESET);
}

static void cmd_meter_volt(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;

    if (!arg || !*arg || strcasecmp(arg, "all") == 0) {
        MeterVoltageReport rep;
        meter_hal_read_all_voltages(32, &rep);

        vt100_set_color(VT100_COLOR_INFO);
        terminal_transport_print("\r\n+---------+------+-----------------+-----------+----------+-----------+\r\n");
        terminal_transport_print("| Channel | GPIO | Connector       | Voltage   | Millivolt| Raw SAR   |\r\n");
        terminal_transport_print("+---------+------+-----------------+-----------+----------+-----------+\r\n");
        vt100_set_color(VT100_COLOR_RESET);

        terminal_transport_printf("| ADC0    | GP26 | Header P2.31    | %7.4f V | %6.1f mV | %4u /4095 |\r\n",
            rep.v0, rep.v0 * 1000.0f, rep.raw0);
        terminal_transport_printf("| ADC1    | GP27 | Header P2.32    | %7.4f V | %6.1f mV | %4u /4095 |\r\n",
            rep.v1, rep.v1 * 1000.0f, rep.raw1);
        terminal_transport_printf("| ADC2    | GP28 | Header P2.34    | %7.4f V | %6.1f mV | %4u /4095 |\r\n",
            rep.v2, rep.v2 * 1000.0f, rep.raw2);
        terminal_transport_printf("| VSYS    | GP29 | Internal Rail   | %7.4f V | %6.1f mV | (Scaled)  |\r\n",
            rep.vsys, rep.vsys * 1000.0f);
        terminal_transport_printf("| TEMP    | MUX4 | RP2040 Die      | %7.2f C | %6.1f F  | (Thermal) |\r\n",
            rep.core_temp_c, rep.core_temp_c * 1.8f + 32.0f);

        vt100_set_color(VT100_COLOR_INFO);
        terminal_transport_print("+---------+------+-----------------+-----------+----------+-----------+\r\n\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    uint8_t ch = 0;
    if (strcasecmp(arg, "vsys") == 0 || strcmp(arg, "3") == 0) ch = 3;
    else if (strcasecmp(arg, "temp") == 0 || strcmp(arg, "4") == 0) ch = 4;
    else if (isdigit((unsigned char)arg[0])) ch = arg[0] - '0';

    float val = meter_hal_read_channel(ch, 32);
    if (ch == 4) {
        terminal_transport_printf("RP2040 Die Temperature: %.2f C  (%.2f F)\r\n", val, val * 1.8f + 32.0f);
    } else if (ch == 3) {
        terminal_transport_printf("VSYS Power Rail: %.4f V  (%.1f mV)\r\n", val, val * 1000.0f);
    } else {
        terminal_transport_printf("ADC%u (GP%u): %.4f V  (%.1f mV)\r\n", ch, 26 + ch, val, val * 1000.0f);
    }
}

static void cmd_meter_res(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    uint8_t ch = 0;
    float r_ref = CONTINUITY_R_REF_OHMS;

    if (arg && *arg) {
        if (isdigit((unsigned char)*arg)) {
            ch = *arg - '0';
            while (*arg && !isspace((unsigned char)*arg)) arg++;
            while (arg && isspace((unsigned char)*arg)) arg++;
            if (*arg) {
                r_ref = strtof(arg, nullptr);
                if (r_ref <= 0.0f) r_ref = CONTINUITY_R_REF_OHMS;
            }
        }
    }

    float r = meter_hal_read_resistance(ch, r_ref);
    terminal_transport_printf("\r\nResistance Measurement on ADC%u (GP%u) [R_ref = %.1f ohms]:\r\n  ",
        ch, 26 + ch, r_ref);

    if (r >= 1e6f) {
        vt100_set_color(VT100_COLOR_WARNING);
        terminal_transport_print("OPEN CIRCUIT  (R > 1.0 M-ohm)\r\n");
    } else if (r >= 1000.0f) {
        vt100_set_color(VT100_COLOR_VALUE);
        terminal_transport_printf("%.3f k-ohms  (%.1f ohms)\r\n", r / 1000.0f, r);
    } else if (r < 1.0f) {
        vt100_set_color(VT100_COLOR_VALUE);
        terminal_transport_printf("%.2f ohms  (CONTINUOUS / SHORT)\r\n", r);
    } else {
        vt100_set_color(VT100_COLOR_VALUE);
        terminal_transport_printf("%.1f ohms\r\n", r);
    }
    vt100_set_color(VT100_COLOR_RESET);
}

static void cmd_meter_cont(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    uint8_t ch = 0;
    if (arg && isdigit((unsigned char)*arg)) {
        ch = *arg - '0';
    }
    meter_hal_run_continuity(Serial, ch);
}

static void cmd_meter_scope(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    uint8_t ch = 0;
    uint32_t rate = 50000;

    if (arg && *arg) {
        if (isdigit((unsigned char)*arg)) {
            ch = *arg - '0';
            while (*arg && !isspace((unsigned char)*arg)) arg++;
            while (arg && isspace((unsigned char)*arg)) arg++;
            if (*arg) {
                rate = parse_freq_str(arg);
                if (rate == 0) rate = 50000;
            }
        }
    }
    meter_hal_render_scope(Serial, ch, rate);
}

static void cmd_meter_buzzer(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    uint16_t freq = CONTINUITY_BUZZER_FREQ_HZ;
    uint16_t dur = 200;

    if (arg && *arg) {
        char* endp = nullptr;
        freq = (uint16_t)strtoul(arg, &endp, 10);
        if (freq == 0) freq = CONTINUITY_BUZZER_FREQ_HZ;
        if (endp && *endp) {
            while (*endp && isspace((unsigned char)*endp)) endp++;
            dur = (uint16_t)strtoul(endp, nullptr, 10);
            if (dur == 0) dur = 200;
        }
    }
    terminal_transport_printf("Emitting test tone: %u Hz for %u ms on GP20...\r\n", freq, dur);
    meter_hal_beep(freq, dur);
}

static void cmd_meter_thresh(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        MeterHalConfig cfg;
        meter_hal_get_config(&cfg);
        terminal_transport_printf("Current Continuity Threshold: < %.1f ohms (Release: > %.1f ohms)\r\n",
            cfg.cont_thresh_ohms, cfg.cont_thresh_ohms + cfg.cont_hyst_ohms);
        terminal_transport_print("Usage: thresh <ohms> [hyst_ohms]  (e.g., 'thresh 25 10')\r\n");
        return;
    }

    char* endp = nullptr;
    float th = strtof(arg, &endp);
    float hy = 10.0f;
    if (endp && *endp) {
        while (*endp && isspace((unsigned char)*endp)) endp++;
        if (*endp) hy = strtof(endp, nullptr);
    }
    meter_hal_set_continuity_threshold(th, hy);
    terminal_transport_printf("Continuity threshold updated: < %.1f ohms (Release: > %.1f ohms).\r\n",
        th, th + hy);
}

static void cmd_meter_dispatch(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        cmd_meter_status();
        return;
    }

    char sub[16] = {0};
    size_t i = 0;
    while (*arg && !isspace((unsigned char)*arg) && i < sizeof(sub) - 1) {
        sub[i++] = *arg++;
    }
    sub[i] = '\0';
    while (*arg && isspace((unsigned char)*arg)) arg++;

    if (strcasecmp(sub, "status") == 0 || strcasecmp(sub, "pins") == 0 || strcasecmp(sub, "info") == 0) {
        cmd_meter_status();
    } else if (strcasecmp(sub, "volt") == 0 || strcasecmp(sub, "v") == 0) {
        cmd_meter_volt(arg);
    } else if (strcasecmp(sub, "res") == 0 || strcasecmp(sub, "r") == 0) {
        cmd_meter_res(arg);
    } else if (strcasecmp(sub, "cont") == 0 || strcasecmp(sub, "c") == 0) {
        cmd_meter_cont(arg);
    } else if (strcasecmp(sub, "scope") == 0 || strcasecmp(sub, "wave") == 0) {
        cmd_meter_scope(arg);
    } else if (strcasecmp(sub, "buzzer") == 0 || strcasecmp(sub, "beep") == 0) {
        cmd_meter_buzzer(arg);
    } else if (strcasecmp(sub, "thresh") == 0) {
        cmd_meter_thresh(arg);
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Unknown Meter subcommand '%s'. Type 'help' for commands.\r\n", sub);
        vt100_set_color(VT100_COLOR_RESET);
    }
}

// ─── Logic Analyzer Subsystem (Milestone PROTO-7) ────────────────────────────
static void cmd_logic_status() {
    logic_hal_print_status(Serial);
}

static void cmd_logic_arm(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    uint32_t timeout_ms = 5000;
    if (arg && *arg) {
        timeout_ms = strtoul(arg, nullptr, 0);
    }
    logic_hal_run_capture_interactive(Serial, timeout_ms);
}

static void cmd_logic_rate(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        LogicHalConfig cfg;
        logic_hal_get_config(&cfg);
        terminal_transport_printf("Current Sample Rate: %lu Hz (%lu MSPS)\r\n",
            (unsigned long)cfg.sample_rate_hz, (unsigned long)(cfg.sample_rate_hz / 1000000));
        terminal_transport_print("Usage: rate <freq>  (e.g., 'rate 100M', 'rate 50M', 'rate 25M', 'rate 10M', 'rate 1M')\r\n");
        return;
    }

    uint32_t rate = parse_freq_str(arg);
    if (!logic_hal_set_samplerate(rate)) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Error: Sample rate must be between 10 kHz (10000) and 100 MHz (100000000).\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    LogicHalConfig cfg;
    logic_hal_get_config(&cfg);
    terminal_transport_printf("Logic Analyzer sample rate configured to %lu Hz (%lu MSPS).\r\n",
        (unsigned long)cfg.sample_rate_hz, (unsigned long)(cfg.sample_rate_hz / 1000000));
}

static void cmd_logic_depth(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        LogicHalConfig cfg;
        logic_hal_get_config(&cfg);
        terminal_transport_printf("Current Capture Depth: %lu samples (%lu KiB)\r\n",
            (unsigned long)cfg.sample_count, (unsigned long)(cfg.sample_count / 1024));
        terminal_transport_print("Usage: depth <samples>  (e.g., 'depth 1024', 'depth 4096', 'depth 64k')\r\n");
        return;
    }

    uint32_t depth = 4096;
    if (strcasecmp(arg, "1k") == 0) depth = 1024;
    else if (strcasecmp(arg, "2k") == 0) depth = 2048;
    else if (strcasecmp(arg, "4k") == 0) depth = 4096;
    else if (strcasecmp(arg, "8k") == 0) depth = 8192;
    else if (strcasecmp(arg, "16k") == 0) depth = 16384;
    else if (strcasecmp(arg, "32k") == 0) depth = 32768;
    else if (strcasecmp(arg, "64k") == 0) depth = 65536;
    else depth = strtoul(arg, nullptr, 0);

    if (!logic_hal_set_sample_count(depth)) {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_print("Error: Sample depth must be power of 2 between 1024 and 65536.\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    LogicHalConfig cfg;
    logic_hal_get_config(&cfg);
    terminal_transport_printf("Capture depth configured to %lu samples (%lu KiB SRAM ring).\r\n",
        (unsigned long)cfg.sample_count, (unsigned long)(cfg.sample_count / 1024));
}

static void cmd_logic_trig(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        LogicHalConfig cfg;
        logic_hal_get_config(&cfg);
        terminal_transport_printf("Current Trigger: %s on CH%u\r\n",
            trigger_mode_to_string(cfg.trigger_mode), cfg.trigger_channel);
        terminal_transport_print("Usage:\r\n");
        terminal_transport_print("  trig auto / trig none                 Immediate / free-run\r\n");
        terminal_transport_print("  trig edge <ch> <rise|fall|any>        Edge trigger on CH0..CH7\r\n");
        terminal_transport_print("  trig level <ch> <high|low>            Level trigger on CH0..CH7\r\n");
        terminal_transport_print("  trig pattern <mask_hex> <val_hex>     8-bit bus pattern match\r\n");
        terminal_transport_print("  trig width <ch> <lt|gt> <ns>          Pulse width runt/glitch detector\r\n");
        terminal_transport_print("  trig pre <pct>                        Pre-trigger percent (0..100%)\r\n");
        return;
    }

    char sub[16] = {0};
    size_t i = 0;
    while (*arg && !isspace((unsigned char)*arg) && i < sizeof(sub) - 1) {
        sub[i++] = *arg++;
    }
    sub[i] = '\0';
    while (*arg && isspace((unsigned char)*arg)) arg++;

    if (strcasecmp(sub, "auto") == 0 || strcasecmp(sub, "none") == 0 || strcasecmp(sub, "imm") == 0) {
        logic_hal_set_trigger_auto();
        terminal_transport_print("Trigger set to AUTO (Immediate / Free Run).\r\n");
    } else if (strcasecmp(sub, "edge") == 0) {
        uint8_t ch = 0;
        char etype[16] = "rise";
        sscanf(arg, "%hhu %15s", &ch, etype);
        AdvTriggerMode m = TRIG_MODE_EDGE_RISING;
        if (strcasecmp(etype, "fall") == 0 || strcasecmp(etype, "falling") == 0) m = TRIG_MODE_EDGE_FALLING;
        else if (strcasecmp(etype, "any") == 0 || strcasecmp(etype, "toggle") == 0) m = TRIG_MODE_EDGE_ANY;
        logic_hal_set_trigger_edge(ch, m);
        terminal_transport_printf("Trigger configured: %s on CH%u (GP%u).\r\n",
            trigger_mode_to_string(m), ch, 2 + ch);
    } else if (strcasecmp(sub, "level") == 0) {
        uint8_t ch = 0;
        char lvl[16] = "high";
        sscanf(arg, "%hhu %15s", &ch, lvl);
        bool high = (strcasecmp(lvl, "low") != 0 && strcmp(lvl, "0") != 0);
        logic_hal_set_trigger_level(ch, high);
        terminal_transport_printf("Trigger configured: LEVEL %s on CH%u (GP%u).\r\n",
            high ? "HIGH" : "LOW", ch, 2 + ch);
    } else if (strcasecmp(sub, "pattern") == 0 || strcasecmp(sub, "pat") == 0) {
        uint32_t mask = 0xFF, val = 0x00;
        sscanf(arg, "%lx %lx", &mask, &val);
        logic_hal_set_trigger_pattern((uint8_t)mask, (uint8_t)val);
        terminal_transport_printf("Trigger configured: PATTERN (GPIO & 0x%02X) == 0x%02X.\r\n",
            (unsigned int)mask, (unsigned int)val);
    } else if (strcasecmp(sub, "width") == 0) {
        uint8_t ch = 0;
        char comp[8] = "lt";
        uint32_t dur_ns = 100;
        sscanf(arg, "%hhu %7s %lu", &ch, comp, &dur_ns);
        bool lt = (strcasecmp(comp, "gt") != 0);
        logic_hal_set_trigger_width(ch, lt, dur_ns);
        terminal_transport_printf("Trigger configured: PULSE WIDTH %s %lu ns on CH%u (GP%u).\r\n",
            lt ? "< (Glitch/Runt)" : ">", (unsigned long)dur_ns, ch, 2 + ch);
    } else if (strcasecmp(sub, "pre") == 0) {
        uint8_t pct = (uint8_t)strtoul(arg, nullptr, 10);
        logic_hal_set_pre_trigger_pct(pct);
        terminal_transport_printf("Pre-trigger capture buffer set to %u%% (Post-trigger: %u%%).\r\n",
            pct, 100 - pct);
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Unknown trigger subcommand '%s'. Type 'trig' for syntax.\r\n", sub);
        vt100_set_color(VT100_COLOR_RESET);
    }
}

static void cmd_logic_dump(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    uint32_t start = 0;
    uint32_t len = 64;

    if (arg && *arg) {
        char* endp = nullptr;
        start = strtoul(arg, &endp, 0);
        if (endp && *endp) {
            while (*endp && isspace((unsigned char)*endp)) endp++;
            if (*endp) len = strtoul(endp, nullptr, 0);
        }
    } else {
        LogicHalConfig cfg;
        logic_hal_get_config(&cfg);
        if (cfg.complete && cfg.trigger_offset > 32) {
            start = cfg.trigger_offset - 32;
        }
    }

    logic_hal_render_ascii_diagram(Serial, start, len);
}

static void cmd_logic_stats() {
    if (!logic_hal_is_complete()) {
        terminal_transport_print("No capture data available. Arm acquisition first ('arm').\r\n");
        return;
    }

    terminal_transport_print("\r\n+---------+------+-------------+------------+------------+---------------+\r\n");
    terminal_transport_print("| Channel | GPIO | High / Low  | Duty Cycle | Transitions| Est. Frequency|\r\n");
    terminal_transport_print("+---------+------+-------------+------------+------------+---------------+\r\n");
    for (uint8_t ch = 0; ch < UDOO_LOGIC_CH_COUNT; ch++) {
        LogicChannelStats st;
        logic_hal_compute_channel_stats(ch, &st);
        if (st.est_freq_hz > 1000000.0f) {
            terminal_transport_printf("| CH%u     | GP%u  | %4lu / %4lu | %5.1f %%    | %10lu | %7.2f MHz   |\r\n",
                ch, 2 + ch, (unsigned long)st.high_samples, (unsigned long)st.low_samples,
                st.duty_pct, (unsigned long)st.transitions, st.est_freq_hz / 1000000.0f);
        } else if (st.est_freq_hz > 1000.0f) {
            terminal_transport_printf("| CH%u     | GP%u  | %4lu / %4lu | %5.1f %%    | %10lu | %7.2f kHz   |\r\n",
                ch, 2 + ch, (unsigned long)st.high_samples, (unsigned long)st.low_samples,
                st.duty_pct, (unsigned long)st.transitions, st.est_freq_hz / 1000.0f);
        } else if (st.est_freq_hz > 0.0f) {
            terminal_transport_printf("| CH%u     | GP%u  | %4lu / %4lu | %5.1f %%    | %10lu | %7.1f Hz    |\r\n",
                ch, 2 + ch, (unsigned long)st.high_samples, (unsigned long)st.low_samples,
                st.duty_pct, (unsigned long)st.transitions, st.est_freq_hz);
        } else {
            terminal_transport_printf("| CH%u     | GP%u  | %4lu / %4lu | %5.1f %%    | %10lu | (Static/DC)   |\r\n",
                ch, 2 + ch, (unsigned long)st.high_samples, (unsigned long)st.low_samples,
                st.duty_pct, (unsigned long)st.transitions);
        }
    }
    terminal_transport_print("+---------+------+-------------+------------+------------+---------------+\r\n\r\n");
}

static void cmd_logic_gen(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        terminal_transport_print("Usage: gen <pin> <freq_hz> [duty_pct]  |  gen stop\r\n");
        terminal_transport_print("  Example: 'gen 10 1000' (1 kHz square wave on GP10 EXT TRIG)\r\n");
        terminal_transport_print("  Example: 'gen 14 1000000' (1 MHz square wave on GP14 PRESCALER)\r\n");
        return;
    }
    if (strcasecmp(arg, "stop") == 0 || strcasecmp(arg, "off") == 0) {
        logic_hal_stop_test_signal(10);
        logic_hal_stop_test_signal(14);
        logic_hal_stop_test_signal(25);
        terminal_transport_print("Test signal generator stopped.\r\n");
        return;
    }
    char* endp = nullptr;
    uint32_t pin = strtoul(arg, &endp, 0);
    if (endp == arg) {
        terminal_transport_print("Invalid pin number.\r\n");
        return;
    }
    while (*endp && isspace((unsigned char)*endp)) endp++;
    uint32_t freq = strtoul(endp, &endp, 0);
    if (freq == 0) freq = 1000;
    while (*endp && isspace((unsigned char)*endp)) endp++;
    uint32_t duty = 50;
    if (*endp) duty = strtoul(endp, nullptr, 0);

    if (logic_hal_start_test_signal((uint8_t)pin, freq, (uint8_t)duty)) {
        terminal_transport_printf("Test Generator ACTIVE: GP%lu -> %lu Hz (%lu%% Duty)\r\n",
            (unsigned long)pin, (unsigned long)freq, (unsigned long)duty);
    } else {
        terminal_transport_print("Failed to start PWM test generator.\r\n");
    }
}

static void cmd_logic_raw(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    uint32_t offset = 0;
    uint32_t count = 64;
    if (arg && *arg) {
        char* endp = nullptr;
        offset = strtoul(arg, &endp, 0);
        while (endp && *endp && isspace((unsigned char)*endp)) endp++;
        if (endp && *endp) count = strtoul(endp, nullptr, 0);
    }
    logic_hal_dump_raw_samples(Serial, offset, count);
}

static void cmd_logic_dispatch(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        cmd_logic_status();
        return;
    }

    char sub[16] = {0};
    size_t i = 0;
    while (*arg && !isspace((unsigned char)*arg) && i < sizeof(sub) - 1) {
        sub[i++] = *arg++;
    }
    sub[i] = '\0';
    while (*arg && isspace((unsigned char)*arg)) arg++;

    if (strcasecmp(sub, "status") == 0 || strcasecmp(sub, "pins") == 0 || strcasecmp(sub, "info") == 0) {
        cmd_logic_status();
    } else if (strcasecmp(sub, "arm") == 0 || strcasecmp(sub, "run") == 0 || strcasecmp(sub, "start") == 0 || strcasecmp(sub, "c") == 0) {
        cmd_logic_arm(arg);
    } else if (strcasecmp(sub, "rate") == 0 || strcasecmp(sub, "speed") == 0) {
        cmd_logic_rate(arg);
    } else if (strcasecmp(sub, "depth") == 0 || strcasecmp(sub, "samples") == 0) {
        cmd_logic_depth(arg);
    } else if (strcasecmp(sub, "trig") == 0 || strcasecmp(sub, "trigger") == 0) {
        cmd_logic_trig(arg);
    } else if (strcasecmp(sub, "dump") == 0 || strcasecmp(sub, "view") == 0 || strcasecmp(sub, "plot") == 0) {
        cmd_logic_dump(arg);
    } else if (strcasecmp(sub, "raw") == 0 || strcasecmp(sub, "bytes") == 0 || strcasecmp(sub, "hex") == 0) {
        cmd_logic_raw(arg);
    } else if (strcasecmp(sub, "gen") == 0 || strcasecmp(sub, "pwm") == 0 || strcasecmp(sub, "sig") == 0) {
        cmd_logic_gen(arg);
    } else if (strcasecmp(sub, "test") == 0 || strcasecmp(sub, "selftest") == 0 || strcasecmp(sub, "bench") == 0) {
        logic_hal_run_self_test(Serial);
    } else if (strcasecmp(sub, "stats") == 0 || strcasecmp(sub, "edges") == 0) {
        cmd_logic_stats();
    } else if (strcasecmp(sub, "abort") == 0 || strcasecmp(sub, "stop") == 0) {
        logic_hal_abort();
        terminal_transport_print("Acquisition stopped.\r\n");
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Unknown Logic subcommand '%s'. Type 'help' for commands.\r\n", sub);
        vt100_set_color(VT100_COLOR_RESET);
    }
}

// ─── USB Probing & Sniffing Subsystem (Milestone PROTO-8) ────────────────────
static void cmd_usb_status() {
    usb_hal_print_status(Serial);
}

static void cmd_usb_probe() {
    UsbBusState st;
    usb_hal_probe_state(&st);
    terminal_transport_printf("Instantaneous Probe State: D+=%u, D-=%u -> %s [%s]\r\n",
        st.dp_level, st.dm_level, usb_hal_state_to_str(st.state), usb_hal_speed_to_str(st.speed));
}

static void cmd_usb_speed() {
    UsbBusSpeed spd = usb_hal_detect_speed();
    terminal_transport_printf("Detected USB Speed: %s\r\n", usb_hal_speed_to_str(spd));
}

static void cmd_usb_monitor(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    uint32_t timeout_ms = 5000;
    if (arg && *arg) timeout_ms = strtoul(arg, nullptr, 0);
    usb_hal_run_monitor(Serial, timeout_ms);
}

static void cmd_usb_sniff(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    uint32_t count = 16;
    uint32_t timeout_ms = 3000;
    if (arg && *arg) {
        char* endp = nullptr;
        count = strtoul(arg, &endp, 0);
        if (endp && *endp) {
            while (*endp && isspace((unsigned char)*endp)) endp++;
            if (*endp) timeout_ms = strtoul(endp, nullptr, 0);
        }
    }
    usb_hal_run_sniffer(Serial, count, timeout_ms);
}

static void cmd_usb_setup(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        terminal_transport_print("Usage: setup <hex_bytes...>  (e.g., 'setup 80 06 00 01 00 00 12 00')\r\n");
        return;
    }
    uint8_t bytes[8] = { 0 };
    size_t idx = 0;
    const char* p = arg;
    while (*p && idx < 8) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        char* endp = nullptr;
        bytes[idx++] = (uint8_t)strtoul(p, &endp, 16);
        if (endp == p) break;
        p = endp;
    }
    char desc[64] = { 0 };
    usb_hal_decode_setup_request(bytes, desc, sizeof(desc));
    terminal_transport_printf("Decoded USB Setup Request: %s\r\n", desc);
}

static void cmd_usb_pulldown(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        bool curr = usb_hal_get_pulldowns();
        usb_hal_set_pulldowns(!curr);
    } else if (strcasecmp(arg, "on") == 0 || strcasecmp(arg, "1") == 0 || strcasecmp(arg, "enable") == 0) {
        usb_hal_set_pulldowns(true);
    } else if (strcasecmp(arg, "off") == 0 || strcasecmp(arg, "0") == 0 || strcasecmp(arg, "disable") == 0) {
        usb_hal_set_pulldowns(false);
    } else {
        terminal_transport_print("Usage: pulldown [on|off]  (controls weak ~50k pull-downs on GP2/GP3)\r\n");
        return;
    }
    terminal_transport_printf("USB Probe Weak Pull-Downs: %s\r\n",
        usb_hal_get_pulldowns() ? "ENABLED (open lines pull to SE0 / 0V)" : "DISABLED (pure high-Z, floating)");
}

static void cmd_usb_dispatch(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        cmd_usb_status();
        return;
    }
    char sub[16] = {0};
    size_t i = 0;
    while (*arg && !isspace((unsigned char)*arg) && i < sizeof(sub) - 1) {
        sub[i++] = *arg++;
    }
    sub[i] = '\0';
    while (*arg && isspace((unsigned char)*arg)) arg++;

    if (strcasecmp(sub, "status") == 0 || strcasecmp(sub, "pins") == 0 || strcasecmp(sub, "info") == 0) {
        cmd_usb_status();
    } else if (strcasecmp(sub, "probe") == 0 || strcasecmp(sub, "state") == 0) {
        cmd_usb_probe();
    } else if (strcasecmp(sub, "speed") == 0 || strcasecmp(sub, "detect") == 0) {
        cmd_usb_speed();
    } else if (strcasecmp(sub, "monitor") == 0 || strcasecmp(sub, "mon") == 0) {
        cmd_usb_monitor(arg);
    } else if (strcasecmp(sub, "sniff") == 0 || strcasecmp(sub, "run") == 0 || strcasecmp(sub, "packets") == 0) {
        cmd_usb_sniff(arg);
    } else if (strcasecmp(sub, "setup") == 0) {
        cmd_usb_setup(arg);
    } else if (strcasecmp(sub, "pulldown") == 0 || strcasecmp(sub, "pull") == 0 || strcasecmp(sub, "pd") == 0) {
        cmd_usb_pulldown(arg);
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Unknown USB subcommand '%s'. Type 'help' for commands.\r\n", sub);
        vt100_set_color(VT100_COLOR_RESET);
    }
}

// ─── Sensor Testing Subsystem (Milestone PROTO-8) ────────────────────────────
static void cmd_sensor_status() {
    sensor_hal_print_status(Serial);
}

static void cmd_sensor_scan(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg || strcasecmp(arg, "all") == 0) {
        sensor_hal_scan_all(Serial);
    } else if (strcasecmp(arg, "i2c") == 0) {
        sensor_hal_scan_i2c(Serial);
    } else if (strcasecmp(arg, "spi") == 0) {
        sensor_hal_probe_spi(Serial);
    } else if (strcasecmp(arg, "1wire") == 0 || strcasecmp(arg, "onewire") == 0) {
        sensor_hal_scan_1wire(Serial);
    } else {
        sensor_hal_scan_all(Serial);
    }
}

static void cmd_sensor_ntc(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    uint8_t ch = 0;
    float r_ref = 1000.0f;
    float r0 = 10000.0f;
    float beta = 3950.0f;
    float t0 = 25.0f;

    if (arg && *arg) {
        char* endp = nullptr;
        ch = (uint8_t)strtoul(arg, &endp, 10);
        if (endp && *endp) {
            r_ref = strtof(endp, &endp);
            if (endp && *endp) {
                r0 = strtof(endp, &endp);
                if (endp && *endp) {
                    beta = strtof(endp, &endp);
                    if (endp && *endp) {
                        t0 = strtof(endp, nullptr);
                    }
                }
            }
        }
    }
    sensor_hal_read_ntc(Serial, ch, r_ref, r0, beta, t0);
}

static void cmd_sensor_photo(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    uint8_t ch = 0;
    if (arg && isdigit((unsigned char)*arg)) ch = *arg - '0';
    sensor_hal_read_photo(Serial, ch);
}

static void cmd_sensor_pulse(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    uint8_t pin = 2;
    uint32_t timeout_ms = 1000;
    if (arg && *arg) {
        char* endp = nullptr;
        pin = (uint8_t)strtoul(arg, &endp, 10);
        if (endp && *endp) {
            timeout_ms = strtoul(endp, nullptr, 10);
        }
    }
    sensor_hal_measure_pulse(Serial, pin, timeout_ms);
}

static void cmd_sensor_dispatch(const char* arg) {
    while (arg && isspace((unsigned char)*arg)) arg++;
    if (!arg || !*arg) {
        cmd_sensor_status();
        return;
    }
    char sub[16] = {0};
    size_t i = 0;
    while (*arg && !isspace((unsigned char)*arg) && i < sizeof(sub) - 1) {
        sub[i++] = *arg++;
    }
    sub[i] = '\0';
    while (*arg && isspace((unsigned char)*arg)) arg++;

    if (strcasecmp(sub, "status") == 0 || strcasecmp(sub, "pins") == 0 || strcasecmp(sub, "info") == 0) {
        cmd_sensor_status();
    } else if (strcasecmp(sub, "scan") == 0 || strcasecmp(sub, "all") == 0) {
        cmd_sensor_scan(arg);
    } else if (strcasecmp(sub, "i2c") == 0) {
        sensor_hal_scan_i2c(Serial);
    } else if (strcasecmp(sub, "spi") == 0 || strcasecmp(sub, "jedec") == 0) {
        sensor_hal_probe_spi(Serial);
    } else if (strcasecmp(sub, "1wire") == 0 || strcasecmp(sub, "temp") == 0) {
        sensor_hal_scan_1wire(Serial);
    } else if (strcasecmp(sub, "ntc") == 0 || strcasecmp(sub, "thermistor") == 0) {
        cmd_sensor_ntc(arg);
    } else if (strcasecmp(sub, "photo") == 0 || strcasecmp(sub, "ldr") == 0 || strcasecmp(sub, "lux") == 0) {
        cmd_sensor_photo(arg);
    } else if (strcasecmp(sub, "pulse") == 0 || strcasecmp(sub, "freq") == 0) {
        cmd_sensor_pulse(arg);
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Unknown Sensor subcommand '%s'. Type 'help' for commands.\r\n", sub);
        vt100_set_color(VT100_COLOR_RESET);
    }
}

// ─── Help Subsystem ───────────────────────────────────────────────────────────
static void cmd_help() {
    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("\r\n================ Global Command Help ================\r\n");
    vt100_set_color(VT100_COLOR_RESET);
    terminal_transport_print("General Commands:\r\n");
    terminal_transport_print("  ? / help          Show this command help menu\r\n");
    terminal_transport_print("  m [mode]          Change operating mode (HiZ, logic, meter, etc.)\r\n");
    terminal_transport_print("  hiz               Immediately disable outputs & enter safe HiZ\r\n");
    terminal_transport_print("  info / i          Display system clocks, SRAM, flash, & voltages\r\n");
    terminal_transport_print("  version / v       Display firmware version & build date\r\n");
    terminal_transport_print("  cls / clear       Clear terminal screen\r\n");
    terminal_transport_print("  config [opt]      Configure terminal settings (vt100 on/off)\r\n");
    terminal_transport_print("  selftest          Run automated hardware diagnostic self-test\r\n");
    terminal_transport_print("  reset / reboot    Reboot RP2040 microcontroller\r\n\r\n");

    terminal_transport_print("Operating Modes (type 'm <mode>'):\r\n");
    terminal_transport_print("  m hiz             Safe High-Impedance (all pins uncommitted)\r\n");
    terminal_transport_print("  m dio             Direct Digital I/O Control on GP2..GP9\r\n");
    terminal_transport_print("  m logic           8-Channel 100MSPS Logic Analyzer\r\n");
    terminal_transport_print("  m meter           Precision Voltmeter, Resistance & Continuity\r\n");
    terminal_transport_print("  m i2c             I2C Master Scanner & Protocol Tools\r\n");
    terminal_transport_print("  m spi             SPI Master & Flash Interrogator\r\n");
    terminal_transport_print("  m uart            UART Bridge & Monitor\r\n");
    terminal_transport_print("  m 1wire           Dallas 1-Wire Search & Read\r\n");
    terminal_transport_print("  m usb             USB Physical Bus Probing & Protocol Sniffing\r\n");
    terminal_transport_print("  m sensor          Universal Sensor Testing & Auto-Identification\r\n\r\n");

    if (strcasecmp(s_mode_name, "DIO") == 0) {
        terminal_transport_print("DIO Mode Commands (Probe Bus GP2..GP9):\r\n");
        terminal_transport_print("  r / read [pin]             Read probe pin or display all 8 pins\r\n");
        terminal_transport_print("  w / write <pin> <0|1>      Configure pin as output and drive level\r\n");
        terminal_transport_print("  d / dir <pin> <in|out>     Configure pin direction\r\n");
        terminal_transport_print("  t / toggle <pin>           Toggle logic level of pin\r\n");
        terminal_transport_print("  p / pull <pin> <u|d|none>  Configure pull resistor (up/down/none)\r\n");
        terminal_transport_print("  mask <dir_mask> <val_mask> Write 8-bit masks across probe bus\r\n");
        terminal_transport_print("  pins / dump                Display full probe bus status table\r\n");
        terminal_transport_print("  0b<bits> / 0x<hex> / <dec> Direct 8-bit bus write (BP5 style)\r\n\r\n");
    } else if (strcasecmp(s_mode_name, "SPI") == 0) {
        terminal_transport_print("SPI Mode Commands (Probe Bus GP2..GP5):\r\n");
        terminal_transport_print("  [ / cs low                 Assert Chip Select (CS LOW on GP5)\r\n");
        terminal_transport_print("  ] / cs high                Deassert Chip Select (CS HIGH on GP5)\r\n");
        terminal_transport_print("  r / read [count]           Read byte(s) from SPI bus (dummy 0xFF)\r\n");
        terminal_transport_print("  w / write <data...>        Write byte(s) to SPI bus\r\n");
        terminal_transport_print("  x / transfer <data...>     Full-duplex SPI transfer (TX & RX)\r\n");
        terminal_transport_print("  speed <hz>                 Set SPI clock speed (10k to 30M, e.g. '10M')\r\n");
        terminal_transport_print("  mode <0|1|2|3>             Set SPI mode (CPOL/CPHA)\r\n");
        terminal_transport_print("  cs <pin>                   Set active CS probe pin (default GP5)\r\n");
        terminal_transport_print("  probe / flash probe        Interrogate SPI Flash JEDEC ID (0x9F)\r\n");
        terminal_transport_print("  flash <cmd>                Flash utilities (status, sfdp, read, dump)\r\n");
        terminal_transport_print("  pins / status              Display active SPI configuration & pins\r\n");
        terminal_transport_print("  [ 0x9F r:3 ]               Bus Pirate syntax execution\r\n\r\n");
    } else if (strcasecmp(s_mode_name, "I2C") == 0) {
        terminal_transport_print("I2C Mode Commands (Probe Bus GP2=SDA, GP3=SCL):\r\n");
        terminal_transport_print("  scan [-v]                  Scan 7-bit I2C bus (0x08..0x77) with device identification\r\n");
        terminal_transport_print("  r / read <addr> [count]    Read byte(s) from target 7-bit I2C address\r\n");
        terminal_transport_print("  w / write <addr> <data...> Write data byte(s) to target 7-bit I2C address\r\n");
        terminal_transport_print("  speed <freq>               Set I2C bus clock (10k to 1M, e.g. '100k', '400k', '1M')\r\n");
        terminal_transport_print("  pull <on|off>              Toggle internal weak pull-up resistors (~50k)\r\n");
        terminal_transport_print("  eeprom [probe|read|write]  24Cxx I2C EEPROM utility suite\r\n");
        terminal_transport_print("  sniff                      Real-time I2C bus traffic sniffer\r\n");
        terminal_transport_print("  pins / status              Display active I2C configuration & pin status\r\n");
        terminal_transport_print("  [ 0x50 0x00 r:16 ]         Bus Pirate syntax execution\r\n\r\n");
    } else if (strcasecmp(s_mode_name, "UART") == 0) {
        terminal_transport_print("UART Mode Commands (Probe Bus GP4=TX, GP5=RX):\r\n");
        terminal_transport_print("  baud <rate>                Set baud rate (300 to 1M, e.g. '9600', '115200', '1M')\r\n");
        terminal_transport_print("  config [8N1|8E1|7E1...]    Set framing (data bits, parity, stop bits)\r\n");
        terminal_transport_print("  w / write <\"str\"|0xNN...>   Transmit ASCII string or raw hex bytes\r\n");
        terminal_transport_print("  r / read [count]           Read incoming bytes from RX FIFO\r\n");
        terminal_transport_print("  bridge                     Transparent bidirectional USB CDC <-> UART bridge\r\n");
        terminal_transport_print("  monitor [hex]              Passive live RX data stream monitor\r\n");
        terminal_transport_print("  autobaud                   Detect baud rate from incoming RX transitions\r\n");
        terminal_transport_print("  loopback                   Run hardware loopback test (GP4 jumpered to GP5)\r\n");
        terminal_transport_print("  pins / status              Display active UART configuration & pins\r\n\r\n");
    } else if (strcasecmp(s_mode_name, "1WIRE") == 0) {
        terminal_transport_print("1-Wire Mode Commands (Probe Bus GP6:DQ):\r\n");
        terminal_transport_print("  scan / search              Enumerate all devices using Search ROM (0xF0)\r\n");
        terminal_transport_print("  temp / read                Read all DS18B20/DS18S20 temperature sensors\r\n");
        terminal_transport_print("  reset                      Issue reset pulse and check presence detect\r\n");
        terminal_transport_print("  rom                        Read 64-bit ROM of single attached device (0x33)\r\n");
        terminal_transport_print("  pull <on|off>              Toggle internal weak pull-up resistor (~50k)\r\n");
        terminal_transport_print("  pin <2..9>                 Select physical probe GPIO pin (default GP6)\r\n");
        terminal_transport_print("  w / write <0xNN...>        Write raw bytes to 1-Wire bus\r\n");
        terminal_transport_print("  r / read [count]           Read raw bytes from 1-Wire bus\r\n");
        terminal_transport_print("  pins / status              Display active 1-Wire configuration & pin status\r\n\r\n");
    } else if (strcasecmp(s_mode_name, "METER") == 0) {
        terminal_transport_print("Meter Mode Commands (Header P2 ADC0..ADC2, VSYS, Buzzer GP20):\r\n");
        terminal_transport_print("  v / volt [0|1|2|vsys|temp] Measure calibrated voltage or display all channels\r\n");
        terminal_transport_print("  r / res [ch] [r_ref]       Measure resistance across voltage divider bridge\r\n");
        terminal_transport_print("  c / cont [ch]              Run real-time continuity monitor with acoustic beeper\r\n");
        terminal_transport_print("  scope [ch] [rate]          Capture analog waveform and render ASCII mini-scope\r\n");
        terminal_transport_print("  buzzer [freq] [ms]         Generate acoustic diagnostic test tone\r\n");
        terminal_transport_print("  thresh <ohms> [hyst]       Configure continuity trigger threshold and hysteresis\r\n");
        terminal_transport_print("  pins / status              Display active ADC configuration & pin status\r\n\r\n");
    } else if (strcasecmp(s_mode_name, "LOGIC") == 0) {
        terminal_transport_print("Logic Analyzer Commands (Probe Bus GP2..GP9, EXT TRIG GP10):\r\n");
        terminal_transport_print("  arm / run / c [ms]         Run interactive capture & render ASCII timing diagram\r\n");
        terminal_transport_print("  rate / speed <hz>          Set sample rate (10k to 100M, e.g. '100M', '25M', '1M')\r\n");
        terminal_transport_print("  depth / samples <count>    Set capture depth (1024 to 65536, e.g. '4k', '64k')\r\n");
        terminal_transport_print("  trig [edge|level|pat|w]    Configure hardware trigger engine\r\n");
        terminal_transport_print("  dump / view [start] [len]  Render horizontal multi-channel ASCII timing diagram\r\n");
        terminal_transport_print("  stats / edges              Display transitions, duty cycle, & estimated frequency\r\n");
        terminal_transport_print("  abort / stop               Abort running acquisition\r\n");
        terminal_transport_print("  pins / status              Display active logic configuration & trigger state\r\n\r\n");
    } else if (strcasecmp(s_mode_name, "USB") == 0) {
        terminal_transport_print("USB Mode Commands (Target D+=GP2, D-=GP3, GND=P1.3):\r\n");
        terminal_transport_print("  probe / state              Display instantaneous line voltages & bus states (SE0/SE1/J/K)\r\n");
        terminal_transport_print("  speed / detect             Detect target bus speed (Low-Speed 1.5M vs Full-Speed 12M)\r\n");
        terminal_transport_print("  mon / monitor [ms]         Run live physical bus transition & event monitor\r\n");
        terminal_transport_print("  sniff / run [count] [ms]   Capture & decode USB packets, PIDs, CRC5/CRC16, & SETUP requests\r\n");
        terminal_transport_print("  setup <hex...>             Decode arbitrary 8-byte SETUP control transfer request\r\n");
        terminal_transport_print("  pins / status              Display active USB probing configuration & telemetry\r\n\r\n");
    } else if (strcasecmp(s_mode_name, "SENSOR") == 0) {
        terminal_transport_print("Sensor Mode Commands (Multi-Bus Sensor Identification):\r\n");
        terminal_transport_print("  scan / all                 Run autonomous scan across I2C, SPI, 1-Wire & Analog\r\n");
        terminal_transport_print("  i2c / scan i2c             Scan I2C bus & auto-identify known sensors by register ID\r\n");
        terminal_transport_print("  spi / jedec                Query SPI sensor / Flash memory JEDEC ID (0x9F)\r\n");
        terminal_transport_print("  1wire / temp               Search 1-Wire bus for Dallas DS18B20/DS18S20 temperature sensors\r\n");
        terminal_transport_print("  ntc [ch] [r_ref] [r0] [b]  Calculate NTC thermistor temperature via Steinhart-Hart / Beta equation\r\n");
        terminal_transport_print("  photo / ldr [ch]           Read LDR / Photocell and estimate ambient illumination (Lux)\r\n");
        terminal_transport_print("  pulse / freq [pin]         High-precision pulse width, duty cycle, & frequency measurement\r\n");
        terminal_transport_print("  pins / status              Display active sensor test configuration & pins\r\n\r\n");
    }

    terminal_transport_print("Quick Hardware Shortcuts:\r\n");
    terminal_transport_print("  scan              Quick I2C bus scan\r\n");
    terminal_transport_print("  flash probe       Quick SPI flash ID read\r\n");
    terminal_transport_print("  adc               Read 3-channel analog voltages\r\n");
    terminal_transport_print("  temp              Read RP2040 core temperature & VSYS rail\r\n");
    terminal_transport_print("  buzzer [hz] [ms]  Generate acoustic diagnostic tone\r\n");
    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("=====================================================\r\n");
    vt100_set_color(VT100_COLOR_RESET);
}

// ─── Info Subsystem ───────────────────────────────────────────────────────────
static void cmd_info() {
    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("\r\n=== UDOO KEY System Identity & Hardware Telemetry ===\r\n");
    vt100_set_color(VT100_COLOR_RESET);
    terminal_transport_printf("  Board:            SECO UDOO KEY (Model: BD REV O)\r\n");
    terminal_transport_printf("  Primary Engine:   Raspberry Pi RP2040 Dual Cortex-M0+ @ %lu MHz\r\n", clock_get_hz(clk_sys) / 1000000);
    terminal_transport_printf("  Coprocessor:      Espressif ESP32-WROVER-E (1 Mbps UART Link)\r\n");
    terminal_transport_printf("  Flash Memory:     16 MB Quad-SPI Flash\r\n");
    terminal_transport_printf("  SRAM Capacity:    264 KB Striped SRAM\r\n");

    float vsys_v = adc_read_calibrated_voltage(ADC_CH_VSYS, 16) * 3.0f;
    float temp_c = adc_read_internal_temp_c();
    terminal_transport_printf("  VSYS Power Rail:  %.2f V\r\n", vsys_v);
    terminal_transport_printf("  Core Temperature: %.1f C\r\n", temp_c);
    terminal_transport_printf("  Probe Pins:       Header P1 GP2..GP9 (CH0..CH7) | EXT TRIG GP10\r\n");
    terminal_transport_printf("  Analog Inputs:    Header P2 GP26 (ADC0), GP27 (ADC1), GP28 (ADC2)\r\n");
    terminal_transport_printf("  Active Mode:      %s\r\n", s_mode_name);
    terminal_transport_printf("  Terminal Mode:    %s\r\n", vt100_is_enabled() ? "VT100 / ANSI Color" : "Raw ASCII");
    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("=====================================================\r\n");
    vt100_set_color(VT100_COLOR_RESET);
}

// ─── Version Subsystem ────────────────────────────────────────────────────────
static void cmd_version() {
    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("\r\n=== Firmware Version & Build Metadata ===\r\n");
    vt100_set_color(VT100_COLOR_RESET);
    terminal_transport_printf("  Firmware:         UDOO-KEY-OS v2.1-bp-terminal\r\n");
    terminal_transport_printf("  Build Date:       %s %s\r\n", __DATE__, __TIME__);
    terminal_transport_printf("  Core Framework:   Arduino-Pico (Earle Philhower Core)\r\n");
    terminal_transport_printf("  USB Subsystem:    Native USB 1.1 Full-Speed CDC ACM (TinyUSB)\r\n");
    terminal_transport_printf("  Logic Engine:     PIO0 SM0 Circular DMA (Frozen Baseline v2.0)\r\n");
    terminal_transport_printf("  SUMP Protocol:    Rev 2 Compliant (100 MSPS, 64 KiB Depth)\r\n");
    vt100_set_color(VT100_COLOR_INFO);
    terminal_transport_print("=========================================\r\n");
    vt100_set_color(VT100_COLOR_RESET);
}

// ─── Mode Switching Subsystem ─────────────────────────────────────────────────
static void cmd_mode(const char* arg) {
    if (!arg || !*arg) {
        // Interactive Mode Selection Menu
        terminal_transport_print("\r\nAvailable Instrument Operating Modes:\r\n");
        terminal_transport_print("  1. HiZ (Safe High-Impedance State)\r\n");
        terminal_transport_print("  2. LOGIC (8-Channel 100MSPS Logic Analyzer)\r\n");
        terminal_transport_print("  3. METER (Multifunction Voltmeter & Continuity)\r\n");
        terminal_transport_print("  4. I2C (I2C Master & Protocol Tools)\r\n");
        terminal_transport_print("  5. SPI (SPI Master & Flash Interrogator)\r\n");
        terminal_transport_print("  6. UART (Serial Bridge & Monitor)\r\n");
        terminal_transport_print("  7. 1WIRE (Dallas 1-Wire Search & Read)\r\n");
        terminal_transport_print("  8. DIO (Direct Digital I/O Control on GP2..GP9)\r\n");
        terminal_transport_print("  9. USB (USB Probing & Protocol Sniffing on GP2/GP3)\r\n");
        terminal_transport_print("  10. SENSOR (Universal Sensor Testing Suite)\r\n");
        terminal_transport_print("\r\nUsage: m <mode_name>  (e.g., 'm dio', 'm logic', 'm usb', 'm sensor')\r\n");
        return;
    }

    // Auto-release previous hardware modes
    if (strcasecmp(s_mode_name, "SPI") == 0 && strcasecmp(arg, "spi") != 0 && strcmp(arg, "5") != 0) {
        spi_hal_release();
    }
    if (strcasecmp(s_mode_name, "I2C") == 0 && strcasecmp(arg, "i2c") != 0 && strcmp(arg, "4") != 0) {
        i2c_hal_release();
    }
    if (strcasecmp(s_mode_name, "UART") == 0 && strcasecmp(arg, "uart") != 0 && strcmp(arg, "6") != 0) {
        uart_hal_release();
    }
    if (strcasecmp(s_mode_name, "1WIRE") == 0 && strcasecmp(arg, "1wire") != 0 && strcmp(arg, "7") != 0) {
        onewire_hal_release();
    }
    if (strcasecmp(s_mode_name, "METER") == 0 && strcasecmp(arg, "meter") != 0 && strcmp(arg, "3") != 0) {
        meter_hal_release();
    }
    if (strcasecmp(s_mode_name, "LOGIC") == 0 && strcasecmp(arg, "logic") != 0 && strcmp(arg, "2") != 0) {
        logic_hal_release();
    }
    if (strcasecmp(s_mode_name, "USB") == 0 && strcasecmp(arg, "usb") != 0 && strcmp(arg, "9") != 0) {
        usb_hal_release();
    }
    if (strcasecmp(s_mode_name, "SENSOR") == 0 && strcasecmp(arg, "sensor") != 0 && strcmp(arg, "10") != 0) {
        sensor_hal_release();
    }

    if (strcasecmp(arg, "hiz") == 0 || strcmp(arg, "1") == 0) {
        bp_set_mode(BP_MODE_HIZ, &Serial);
        terminal_set_mode_name("HiZ");
        terminal_transport_print("Entered safe High-Impedance (HiZ) mode. All outputs disabled.\r\n");
    } else if (strcasecmp(arg, "logic") == 0 || strcmp(arg, "2") == 0) {
        bp_set_mode(BP_MODE_LOGIC, &Serial);
        terminal_set_mode_name("LOGIC");
        logic_hal_acquire();
        terminal_transport_print("Entered LOGIC ANALYZER mode. 8-channel PIO capture ready.\r\n");
        cmd_logic_status();
    } else if (strcasecmp(arg, "meter") == 0 || strcmp(arg, "3") == 0) {
        bp_set_mode(BP_MODE_HIZ, &Serial);
        terminal_set_mode_name("METER");
        meter_hal_acquire();
        terminal_transport_print("Entered METER mode. Precision Voltmeter & Continuity active.\r\n");
        cmd_meter_status();
    } else if (strcasecmp(arg, "i2c") == 0 || strcmp(arg, "4") == 0) {
        bp_set_mode(BP_MODE_I2C, &Serial);
        terminal_set_mode_name("I2C");
        i2c_hal_acquire(bp_cfg.speed_hz ? bp_cfg.speed_hz : 400000, true);
        terminal_transport_print("Entered I2C mode. Hardware I2C1 enabled on GP2 (SDA), GP3 (SCL).\r\n");
        cmd_i2c_status();
    } else if (strcasecmp(arg, "spi") == 0 || strcmp(arg, "5") == 0) {
        bp_set_mode(BP_MODE_SPI, &Serial);
        terminal_set_mode_name("SPI");
        spi_hal_acquire(bp_cfg.speed_hz, bp_cfg.spi_polarity, bp_cfg.spi_phase, false, bp_cfg.cs_pin);
        terminal_transport_print("Entered SPI mode. Hardware SPI0 master active on Probe Bus (GP2..GP5).\r\n");
        cmd_spi_status();
    } else if (strcasecmp(arg, "uart") == 0 || strcmp(arg, "6") == 0) {
        bp_set_mode(BP_MODE_UART, &Serial);
        terminal_set_mode_name("UART");
        uart_hal_acquire(bp_cfg.speed_hz ? bp_cfg.speed_hz : 115200, 8, UART_PARITY_NONE_MODE, 1, false);
        terminal_transport_print("Entered UART mode. Hardware UART1 active on GP4 (TX), GP5 (RX).\r\n");
        cmd_uart_status();
    } else if (strcasecmp(arg, "1wire") == 0 || strcmp(arg, "7") == 0) {
        bp_set_mode(BP_MODE_1WIRE, &Serial);
        terminal_set_mode_name("1WIRE");
        onewire_hal_acquire(UDOO_ONEWIRE_DEFAULT_PIN, true);
        terminal_transport_print("Entered 1-WIRE mode. GP6 (DQ) active with pull-up.\r\n");
        cmd_1wire_status();
    } else if (strcasecmp(arg, "dio") == 0 || strcmp(arg, "8") == 0) {
        bp_set_mode(BP_MODE_DIO, &Serial);
        terminal_set_mode_name("DIO");
        terminal_transport_print("Entered DIO mode. Direct GPIO control on Probe Bus (GP2..GP9).\r\n");
        cmd_dio_pins_table();
    } else if (strcasecmp(arg, "usb") == 0 || strcmp(arg, "9") == 0) {
        bp_set_mode(BP_MODE_USB, &Serial);
        terminal_set_mode_name("USB");
        usb_hal_acquire();
        terminal_transport_print("Entered USB mode. Physical bus probing & protocol sniffer active.\r\n");
        cmd_usb_status();
    } else if (strcasecmp(arg, "sensor") == 0 || strcmp(arg, "10") == 0) {
        bp_set_mode(BP_MODE_SENSOR, &Serial);
        terminal_set_mode_name("SENSOR");
        sensor_hal_acquire();
        terminal_transport_print("Entered SENSOR mode. Multi-bus sensor testing & identification active.\r\n");
        cmd_sensor_status();
    } else {
        vt100_set_color(VT100_COLOR_ERROR);
        terminal_transport_printf("Unknown mode '%s'. Type 'm' to list modes.\r\n", arg);
        vt100_set_color(VT100_COLOR_RESET);
    }
}

// ─── Command Execution Dispatcher ─────────────────────────────────────────────
void terminal_execute_command(const char* line) {
    if (!line) return;

    // Skip leading whitespace
    while (isspace((unsigned char)*line)) line++;
    if (!*line) return;

    char cmd[32] = {0};
    const char* p = line;
    size_t i = 0;
    while (*p && !isspace((unsigned char)*p) && i < sizeof(cmd) - 1) {
        cmd[i++] = *p++;
    }
    cmd[i] = '\0';

    // Skip whitespace before argument
    while (isspace((unsigned char)*p)) p++;
    const char* arg = p;

    bool in_dio_mode = (strcasecmp(s_mode_name, "DIO") == 0);
    bool in_spi_mode = (strcasecmp(s_mode_name, "SPI") == 0);
    bool in_i2c_mode = (strcasecmp(s_mode_name, "I2C") == 0);
    bool in_uart_mode = (strcasecmp(s_mode_name, "UART") == 0);
    bool in_onewire_mode = (strcasecmp(s_mode_name, "1WIRE") == 0);
    bool in_meter_mode = (strcasecmp(s_mode_name, "METER") == 0);
    bool in_logic_mode = (strcasecmp(s_mode_name, "LOGIC") == 0);
    bool in_usb_mode = (strcasecmp(s_mode_name, "USB") == 0);
    bool in_sensor_mode = (strcasecmp(s_mode_name, "SENSOR") == 0);

    if (in_dio_mode && handle_bp5_numeric_write(cmd)) {
        return;
    }

    if (in_i2c_mode && (line[0] == '[' || strcmp(cmd, "[") == 0)) {
        i2c_hal_exec_bp_syntax(line, Serial);
        return;
    }

    // ─── Primary Built-in Commands ────────────────────────────────────────────
    if (strcmp(cmd, "?") == 0 || strcasecmp(cmd, "help") == 0 || strcasecmp(cmd, "h") == 0) {
        cmd_help();
    } else if (strcasecmp(cmd, "info") == 0 || strcasecmp(cmd, "i") == 0) {
        cmd_info();
    } else if (strcasecmp(cmd, "version") == 0 || strcasecmp(cmd, "v") == 0) {
        cmd_version();
    } else if (strcasecmp(cmd, "m") == 0 || strcasecmp(cmd, "mode") == 0) {
        cmd_mode(arg);
    } else if (strcasecmp(cmd, "hiz") == 0) {
        cmd_mode("hiz");
    } else if (strcasecmp(cmd, "cls") == 0 || strcasecmp(cmd, "clear") == 0) {
        vt100_clear_screen();
    } else if (strcasecmp(cmd, "config") == 0 || strcasecmp(cmd, "c") == 0) {
        if (strcasecmp(arg, "vt100 on") == 0) {
            vt100_set_enabled(true);
            terminal_transport_print("VT100 ANSI mode enabled.\r\n");
        } else if (strcasecmp(arg, "vt100 off") == 0) {
            vt100_set_enabled(false);
            terminal_transport_print("VT100 mode disabled (Raw ASCII mode).\r\n");
        } else {
            terminal_transport_printf("Terminal Config: VT100=%s\r\n", vt100_is_enabled() ? "ON" : "OFF");
            terminal_transport_print("Usage: config vt100 [on|off]\r\n");
        }
    } else if (in_onewire_mode && strcasecmp(cmd, "reset") == 0) {
        cmd_1wire_reset();
    } else if (strcasecmp(cmd, "reset") == 0 || strcasecmp(cmd, "reboot") == 0) {
        terminal_transport_print("\r\nRebooting RP2040 instrument...\r\n");
        terminal_transport_flush();
        delay(100);
        rp2040.reboot();
    } else if (in_dio_mode && (strcasecmp(cmd, "r") == 0 || strcasecmp(cmd, "read") == 0)) {
        cmd_dio_read(arg);
    } else if (in_dio_mode && (strcasecmp(cmd, "w") == 0 || strcasecmp(cmd, "write") == 0)) {
        cmd_dio_write(arg);
    } else if (in_dio_mode && (strcasecmp(cmd, "d") == 0 || strcasecmp(cmd, "dir") == 0)) {
        cmd_dio_dir(arg);
    } else if (in_dio_mode && (strcasecmp(cmd, "t") == 0 || strcasecmp(cmd, "toggle") == 0)) {
        cmd_dio_toggle(arg);
    } else if (in_dio_mode && (strcasecmp(cmd, "p") == 0 || strcasecmp(cmd, "pull") == 0)) {
        cmd_dio_pull(arg);
    } else if (in_dio_mode && strcasecmp(cmd, "mask") == 0) {
        cmd_dio_mask(arg);
    } else if (in_dio_mode && (strcasecmp(cmd, "pins") == 0 || strcasecmp(cmd, "dump") == 0)) {
        cmd_dio_pins_table();
    } else if (strcasecmp(cmd, "dio") == 0) {
        cmd_dio_dispatch(arg);
    } else if (in_spi_mode && (strcmp(cmd, "[") == 0 || (strcasecmp(cmd, "cs") == 0 && (strcasecmp(arg, "low") == 0 || strcmp(arg, "0") == 0)))) {
        spi_hal_set_cs(true);
        SpiHalConfig cfg;
        spi_hal_get_config(&cfg);
        terminal_transport_printf("/CS Asserted (LOW on GP%u)\r\n", cfg.cs_pin);
    } else if (in_spi_mode && (strcmp(cmd, "]") == 0 || (strcasecmp(cmd, "cs") == 0 && (strcasecmp(arg, "high") == 0 || strcmp(arg, "1") == 0)))) {
        spi_hal_set_cs(false);
        SpiHalConfig cfg;
        spi_hal_get_config(&cfg);
        terminal_transport_printf("/CS Deasserted (HIGH on GP%u)\r\n", cfg.cs_pin);
    } else if (in_spi_mode && strcasecmp(cmd, "cs") == 0) {
        cmd_spi_cs(arg);
    } else if (in_spi_mode && (strcasecmp(cmd, "r") == 0 || strcasecmp(cmd, "read") == 0)) {
        cmd_spi_read(arg);
    } else if (in_spi_mode && (strcasecmp(cmd, "w") == 0 || strcasecmp(cmd, "write") == 0)) {
        cmd_spi_write_or_xfer(arg, false);
    } else if (in_spi_mode && (strcasecmp(cmd, "x") == 0 || strcasecmp(cmd, "xfer") == 0 || strcasecmp(cmd, "transfer") == 0)) {
        cmd_spi_write_or_xfer(arg, true);
    } else if (in_spi_mode && (strcasecmp(cmd, "speed") == 0 || strcasecmp(cmd, "baud") == 0 || strcasecmp(cmd, "clk") == 0)) {
        cmd_spi_speed(arg);
    } else if (in_spi_mode && (strcasecmp(cmd, "mode") == 0 || strcasecmp(cmd, "config") == 0)) {
        cmd_spi_config(arg);
    } else if (in_spi_mode && (strcasecmp(cmd, "probe") == 0 || strcasecmp(cmd, "id") == 0)) {
        cmd_spi_flash("probe");
    } else if (in_spi_mode && strcasecmp(cmd, "flash") == 0) {
        cmd_spi_flash(arg);
    } else if (in_spi_mode && (strcasecmp(cmd, "pins") == 0 || strcasecmp(cmd, "status") == 0)) {
        cmd_spi_status();
    } else if (strcasecmp(cmd, "spi") == 0) {
        cmd_spi_dispatch(arg);
    } else if (in_i2c_mode && (strcasecmp(cmd, "scan") == 0 || strcasecmp(cmd, "search") == 0 || strcasecmp(cmd, "s") == 0)) {
        cmd_i2c_scan(arg);
    } else if (in_i2c_mode && (strcasecmp(cmd, "r") == 0 || strcasecmp(cmd, "read") == 0)) {
        cmd_i2c_read(arg);
    } else if (in_i2c_mode && (strcasecmp(cmd, "w") == 0 || strcasecmp(cmd, "write") == 0)) {
        cmd_i2c_write(arg);
    } else if (in_i2c_mode && (strcasecmp(cmd, "speed") == 0 || strcasecmp(cmd, "baud") == 0 || strcasecmp(cmd, "clk") == 0)) {
        cmd_i2c_speed(arg);
    } else if (in_i2c_mode && (strcasecmp(cmd, "pull") == 0 || strcasecmp(cmd, "p") == 0)) {
        cmd_i2c_pull(arg);
    } else if (in_i2c_mode && strcasecmp(cmd, "eeprom") == 0) {
        cmd_i2c_eeprom(arg);
    } else if (in_i2c_mode && strcasecmp(cmd, "sniff") == 0) {
        cmd_i2c_sniff();
    } else if (in_i2c_mode && (strcasecmp(cmd, "pins") == 0 || strcasecmp(cmd, "status") == 0)) {
        cmd_i2c_status();
    } else if (strcasecmp(cmd, "i2c") == 0) {
        cmd_i2c_dispatch(arg);
    } else if (strcasecmp(cmd, "scan") == 0) {
        cmd_i2c_scan(arg);
    } else if (in_uart_mode && (strcasecmp(cmd, "baud") == 0 || strcasecmp(cmd, "speed") == 0 || strcasecmp(cmd, "clk") == 0)) {
        cmd_uart_baud(arg);
    } else if (in_uart_mode && (strcasecmp(cmd, "config") == 0 || strcasecmp(cmd, "format") == 0 || strcasecmp(cmd, "mode") == 0)) {
        cmd_uart_format(arg);
    } else if (in_uart_mode && (strcasecmp(cmd, "w") == 0 || strcasecmp(cmd, "write") == 0 || strcasecmp(cmd, "send") == 0)) {
        cmd_uart_write(arg);
    } else if (in_uart_mode && (strcasecmp(cmd, "r") == 0 || strcasecmp(cmd, "read") == 0)) {
        cmd_uart_read(arg);
    } else if (in_uart_mode && strcasecmp(cmd, "bridge") == 0) {
        cmd_uart_bridge();
    } else if (in_uart_mode && (strcasecmp(cmd, "monitor") == 0 || strcasecmp(cmd, "mon") == 0)) {
        cmd_uart_monitor(arg);
    } else if (in_uart_mode && (strcasecmp(cmd, "autobaud") == 0 || strcasecmp(cmd, "detect") == 0)) {
        cmd_uart_autobaud();
    } else if (in_uart_mode && (strcasecmp(cmd, "loopback") == 0 || strcasecmp(cmd, "test") == 0)) {
        cmd_uart_loopback();
    } else if (in_uart_mode && (strcasecmp(cmd, "pins") == 0 || strcasecmp(cmd, "status") == 0)) {
        cmd_uart_status();
    } else if (strcasecmp(cmd, "uart") == 0) {
        cmd_uart_dispatch(arg);
    } else if (in_onewire_mode && (strcasecmp(cmd, "scan") == 0 || strcasecmp(cmd, "search") == 0 || strcasecmp(cmd, "s") == 0)) {
        cmd_1wire_scan();
    } else if (in_onewire_mode && (strcasecmp(cmd, "temp") == 0 || strcasecmp(cmd, "t") == 0)) {
        cmd_1wire_temp();
    } else if (in_onewire_mode && strcasecmp(cmd, "rom") == 0) {
        cmd_1wire_rom();
    } else if (in_onewire_mode && (strcasecmp(cmd, "pull") == 0 || strcasecmp(cmd, "p") == 0)) {
        cmd_1wire_pull(arg);
    } else if (in_onewire_mode && strcasecmp(cmd, "pin") == 0) {
        cmd_1wire_pin(arg);
    } else if (in_onewire_mode && (strcasecmp(cmd, "w") == 0 || strcasecmp(cmd, "write") == 0)) {
        cmd_1wire_write(arg);
    } else if (in_onewire_mode && (strcasecmp(cmd, "r") == 0 || strcasecmp(cmd, "read") == 0)) {
        cmd_1wire_read(arg);
    } else if (in_onewire_mode && (strcasecmp(cmd, "pins") == 0 || strcasecmp(cmd, "status") == 0)) {
        cmd_1wire_status();
    } else if (strcasecmp(cmd, "1wire") == 0 || strcasecmp(cmd, "onewire") == 0) {
        cmd_1wire_dispatch(arg);
    } else if (in_meter_mode && (strcasecmp(cmd, "v") == 0 || strcasecmp(cmd, "volt") == 0 || strcasecmp(cmd, "voltage") == 0)) {
        cmd_meter_volt(arg);
    } else if (in_meter_mode && (strcasecmp(cmd, "r") == 0 || strcasecmp(cmd, "res") == 0 || strcasecmp(cmd, "resistance") == 0)) {
        cmd_meter_res(arg);
    } else if (in_meter_mode && (strcasecmp(cmd, "c") == 0 || strcasecmp(cmd, "cont") == 0 || strcasecmp(cmd, "continuity") == 0)) {
        cmd_meter_cont(arg);
    } else if (in_meter_mode && (strcasecmp(cmd, "scope") == 0 || strcasecmp(cmd, "wave") == 0 || strcasecmp(cmd, "waveform") == 0)) {
        cmd_meter_scope(arg);
    } else if (in_meter_mode && (strcasecmp(cmd, "buzzer") == 0 || strcasecmp(cmd, "beep") == 0)) {
        cmd_meter_buzzer(arg);
    } else if (in_meter_mode && (strcasecmp(cmd, "thresh") == 0 || strcasecmp(cmd, "threshold") == 0)) {
        cmd_meter_thresh(arg);
    } else if (in_meter_mode && (strcasecmp(cmd, "pins") == 0 || strcasecmp(cmd, "status") == 0)) {
        cmd_meter_status();
    } else if (strcasecmp(cmd, "meter") == 0) {
        cmd_meter_dispatch(arg);
    } else if (in_logic_mode && (strcasecmp(cmd, "arm") == 0 || strcasecmp(cmd, "run") == 0 || strcasecmp(cmd, "start") == 0 || strcasecmp(cmd, "c") == 0 || strcasecmp(cmd, "capture") == 0)) {
        cmd_logic_arm(arg);
    } else if (in_logic_mode && (strcasecmp(cmd, "rate") == 0 || strcasecmp(cmd, "speed") == 0 || strcasecmp(cmd, "samplerate") == 0)) {
        cmd_logic_rate(arg);
    } else if (in_logic_mode && (strcasecmp(cmd, "depth") == 0 || strcasecmp(cmd, "samples") == 0 || strcasecmp(cmd, "count") == 0)) {
        cmd_logic_depth(arg);
    } else if (in_logic_mode && (strcasecmp(cmd, "trig") == 0 || strcasecmp(cmd, "trigger") == 0)) {
        cmd_logic_trig(arg);
    } else if (in_logic_mode && (strcasecmp(cmd, "dump") == 0 || strcasecmp(cmd, "view") == 0 || strcasecmp(cmd, "plot") == 0 || strcasecmp(cmd, "ascii") == 0)) {
        cmd_logic_dump(arg);
    } else if (in_logic_mode && (strcasecmp(cmd, "raw") == 0 || strcasecmp(cmd, "bytes") == 0 || strcasecmp(cmd, "hex") == 0)) {
        cmd_logic_raw(arg);
    } else if (in_logic_mode && (strcasecmp(cmd, "gen") == 0 || strcasecmp(cmd, "pwm") == 0 || strcasecmp(cmd, "sig") == 0)) {
        cmd_logic_gen(arg);
    } else if (in_logic_mode && (strcasecmp(cmd, "test") == 0 || strcasecmp(cmd, "selftest") == 0 || strcasecmp(cmd, "bench") == 0)) {
        logic_hal_run_self_test(Serial);
    } else if (in_logic_mode && (strcasecmp(cmd, "stats") == 0 || strcasecmp(cmd, "edges") == 0)) {
        cmd_logic_stats();
    } else if (in_logic_mode && (strcasecmp(cmd, "abort") == 0 || strcasecmp(cmd, "stop") == 0)) {
        logic_hal_abort();
        terminal_transport_print("Acquisition stopped.\r\n");
    } else if (in_logic_mode && (strcasecmp(cmd, "pins") == 0 || strcasecmp(cmd, "status") == 0)) {
        cmd_logic_status();
    } else if (strcasecmp(cmd, "logic") == 0 || strcasecmp(cmd, "la") == 0) {
        cmd_logic_dispatch(arg);
    } else if (in_usb_mode && (strcasecmp(cmd, "probe") == 0 || strcasecmp(cmd, "state") == 0)) {
        cmd_usb_probe();
    } else if (in_usb_mode && (strcasecmp(cmd, "speed") == 0 || strcasecmp(cmd, "detect") == 0)) {
        cmd_usb_speed();
    } else if (in_usb_mode && (strcasecmp(cmd, "mon") == 0 || strcasecmp(cmd, "monitor") == 0)) {
        cmd_usb_monitor(arg);
    } else if (in_usb_mode && (strcasecmp(cmd, "sniff") == 0 || strcasecmp(cmd, "packets") == 0 || strcasecmp(cmd, "run") == 0)) {
        cmd_usb_sniff(arg);
    } else if (in_usb_mode && strcasecmp(cmd, "setup") == 0) {
        cmd_usb_setup(arg);
    } else if (in_usb_mode && (strcasecmp(cmd, "pulldown") == 0 || strcasecmp(cmd, "pull") == 0 || strcasecmp(cmd, "pd") == 0)) {
        cmd_usb_pulldown(arg);
    } else if (in_usb_mode && (strcasecmp(cmd, "pins") == 0 || strcasecmp(cmd, "status") == 0)) {
        cmd_usb_status();
    } else if (strcasecmp(cmd, "usb") == 0) {
        cmd_usb_dispatch(arg);
    } else if (in_sensor_mode && (strcasecmp(cmd, "scan") == 0 || strcasecmp(cmd, "all") == 0)) {
        cmd_sensor_scan(arg);
    } else if (in_sensor_mode && strcasecmp(cmd, "i2c") == 0) {
        sensor_hal_scan_i2c(Serial);
    } else if (in_sensor_mode && (strcasecmp(cmd, "spi") == 0 || strcasecmp(cmd, "jedec") == 0)) {
        sensor_hal_probe_spi(Serial);
    } else if (in_sensor_mode && (strcasecmp(cmd, "1wire") == 0 || strcasecmp(cmd, "temp") == 0)) {
        sensor_hal_scan_1wire(Serial);
    } else if (in_sensor_mode && (strcasecmp(cmd, "ntc") == 0 || strcasecmp(cmd, "thermistor") == 0)) {
        cmd_sensor_ntc(arg);
    } else if (in_sensor_mode && (strcasecmp(cmd, "photo") == 0 || strcasecmp(cmd, "ldr") == 0)) {
        cmd_sensor_photo(arg);
    } else if (in_sensor_mode && (strcasecmp(cmd, "pulse") == 0 || strcasecmp(cmd, "freq") == 0)) {
        cmd_sensor_pulse(arg);
    } else if (in_sensor_mode && (strcasecmp(cmd, "pins") == 0 || strcasecmp(cmd, "status") == 0)) {
        cmd_sensor_status();
    } else if (strcasecmp(cmd, "sensor") == 0) {
        cmd_sensor_dispatch(arg);
    } else {
        // Fall back to Bus Pirate command suite (I2C, SPI, ADC, frequency, etc.)
        bp_execute_command(line, Serial);
    }
}

// ─── Main Terminal Task ───────────────────────────────────────────────────────
void terminal_task() {
    static char s_line_buf[LINE_EDITOR_MAX_LEN];

    while (terminal_transport_available()) {
        char c = (char)terminal_transport_read();

        // Feed byte into line editor. Returns true when Enter is pressed.
        if (line_editor_feed(c, s_line_buf, sizeof(s_line_buf))) {
            terminal_execute_command(s_line_buf);
            terminal_print_prompt();
        }
    }
}
