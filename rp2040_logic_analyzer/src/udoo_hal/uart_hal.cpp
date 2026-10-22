#include "uart_hal.h"
#include "gpio_hal.h"
#include "instrument_mode_manager.h"
#include "../button_nav.h"
#include <Arduino.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static UartHalConfig s_uart_cfg = {
    .baudrate    = UDOO_UART_DEFAULT_BAUD,
    .actual_baud = 0,
    .data_bits   = 8,
    .parity      = UART_PARITY_NONE_MODE,
    .stop_bits   = 1,
    .invert      = false,
    .active      = false
};

static uint16_t get_serial_config(uint8_t data_bits, UartHalParity parity, uint8_t stop_bits) {
    if (parity == UART_PARITY_EVEN_MODE) {
        if (stop_bits == 2) {
            if (data_bits == 5) return SERIAL_5E2;
            if (data_bits == 6) return SERIAL_6E2;
            if (data_bits == 7) return SERIAL_7E2;
            return SERIAL_8E2;
        } else {
            if (data_bits == 5) return SERIAL_5E1;
            if (data_bits == 6) return SERIAL_6E1;
            if (data_bits == 7) return SERIAL_7E1;
            return SERIAL_8E1;
        }
    } else if (parity == UART_PARITY_ODD_MODE) {
        if (stop_bits == 2) {
            if (data_bits == 5) return SERIAL_5O2;
            if (data_bits == 6) return SERIAL_6O2;
            if (data_bits == 7) return SERIAL_7O2;
            return SERIAL_8O2;
        } else {
            if (data_bits == 5) return SERIAL_5O1;
            if (data_bits == 6) return SERIAL_6O1;
            if (data_bits == 7) return SERIAL_7O1;
            return SERIAL_8O1;
        }
    } else {
        if (stop_bits == 2) {
            if (data_bits == 5) return SERIAL_5N2;
            if (data_bits == 6) return SERIAL_6N2;
            if (data_bits == 7) return SERIAL_7N2;
            return SERIAL_8N2;
        } else {
            if (data_bits == 5) return SERIAL_5N1;
            if (data_bits == 6) return SERIAL_6N1;
            if (data_bits == 7) return SERIAL_7N1;
            return SERIAL_8N1;
        }
    }
}

void uart_hal_init(void) {
    s_uart_cfg.baudrate    = UDOO_UART_DEFAULT_BAUD;
    s_uart_cfg.actual_baud = 0;
    s_uart_cfg.data_bits   = 8;
    s_uart_cfg.parity      = UART_PARITY_NONE_MODE;
    s_uart_cfg.stop_bits   = 1;
    s_uart_cfg.invert      = false;
    s_uart_cfg.active      = false;
}

bool uart_hal_acquire(uint32_t baudrate, uint8_t data_bits, UartHalParity parity, uint8_t stop_bits, bool invert) {
    if (baudrate < UDOO_UART_MIN_BAUD) baudrate = UDOO_UART_MIN_BAUD;
    if (baudrate > UDOO_UART_MAX_BAUD) baudrate = UDOO_UART_MAX_BAUD;

    if (data_bits < 5 || data_bits > 8) data_bits = 8;
    if (stop_bits < 1 || stop_bits > 2) stop_bits = 1;

    s_uart_cfg.baudrate  = baudrate;
    s_uart_cfg.data_bits = data_bits;
    s_uart_cfg.parity    = parity;
    s_uart_cfg.stop_bits = stop_bits;
    s_uart_cfg.invert    = invert;

    // Coordinate with instrument mode manager
    ModeMgr.enterMode(InstrumentMode::UART, baudrate);

    Serial2.setTX(UDOO_UART_TX_PIN);
    Serial2.setRX(UDOO_UART_RX_PIN);
    Serial2.setInvertTX(invert);
    Serial2.setInvertRX(invert);

    uint16_t cfg = get_serial_config(data_bits, parity, stop_bits);
    Serial2.begin(baudrate, cfg);

    s_uart_cfg.actual_baud = (uint32_t)Serial2.getActualBaud();
    s_uart_cfg.active = true;

    return true;
}

void uart_hal_release(void) {
    if (!s_uart_cfg.active) return;

    Serial2.end();

    // Restore GP4 and GP5 to Safe High-Z Inputs
    gpio_set_function(UDOO_UART_TX_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(UDOO_UART_TX_PIN, GPIO_IN);
    gpio_disable_pulls(UDOO_UART_TX_PIN);

    gpio_set_function(UDOO_UART_RX_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(UDOO_UART_RX_PIN, GPIO_IN);
    gpio_disable_pulls(UDOO_UART_RX_PIN);

    s_uart_cfg.active = false;
    s_uart_cfg.actual_baud = 0;
}

bool uart_hal_set_baudrate(uint32_t baudrate) {
    if (baudrate < UDOO_UART_MIN_BAUD) baudrate = UDOO_UART_MIN_BAUD;
    if (baudrate > UDOO_UART_MAX_BAUD) baudrate = UDOO_UART_MAX_BAUD;

    s_uart_cfg.baudrate = baudrate;
    if (s_uart_cfg.active) {
        uint16_t cfg = get_serial_config(s_uart_cfg.data_bits, s_uart_cfg.parity, s_uart_cfg.stop_bits);
        Serial2.end();
        Serial2.begin(baudrate, cfg);
        s_uart_cfg.actual_baud = (uint32_t)Serial2.getActualBaud();
    }
    return true;
}

bool uart_hal_set_format(uint8_t data_bits, UartHalParity parity, uint8_t stop_bits) {
    if (data_bits < 5 || data_bits > 8) data_bits = 8;
    if (stop_bits < 1 || stop_bits > 2) stop_bits = 1;

    s_uart_cfg.data_bits = data_bits;
    s_uart_cfg.parity    = parity;
    s_uart_cfg.stop_bits = stop_bits;

    if (s_uart_cfg.active) {
        uint16_t cfg = get_serial_config(data_bits, parity, stop_bits);
        Serial2.end();
        Serial2.begin(s_uart_cfg.baudrate, cfg);
        s_uart_cfg.actual_baud = (uint32_t)Serial2.getActualBaud();
    }
    return true;
}

void uart_hal_get_config(UartHalConfig* out_cfg) {
    if (out_cfg) {
        *out_cfg = s_uart_cfg;
    }
}

size_t uart_hal_write(const uint8_t* data, size_t len) {
    if (!s_uart_cfg.active) {
        uart_hal_acquire(s_uart_cfg.baudrate, s_uart_cfg.data_bits, s_uart_cfg.parity, s_uart_cfg.stop_bits, s_uart_cfg.invert);
    }
    if (!data || len == 0) return 0;
    return Serial2.write(data, len);
}

size_t uart_hal_write_str(const char* str) {
    if (!str) return 0;
    return uart_hal_write((const uint8_t*)str, strlen(str));
}

size_t uart_hal_read(uint8_t* buf, size_t max_len) {
    if (!s_uart_cfg.active || !buf || max_len == 0) return 0;
    size_t count = 0;
    while (Serial2.available() && count < max_len) {
        buf[count++] = (uint8_t)Serial2.read();
    }
    return count;
}

int uart_hal_available(void) {
    if (!s_uart_cfg.active) return 0;
    return Serial2.available();
}

void uart_hal_bridge(Stream& host) {
    if (!s_uart_cfg.active) {
        uart_hal_acquire(s_uart_cfg.baudrate, s_uart_cfg.data_bits, s_uart_cfg.parity, s_uart_cfg.stop_bits, s_uart_cfg.invert);
    }

    host.println("\r\n=== Transparent UART Bridge Active ===");
    host.printf("  Link: %lu baud (%u%c%u) | TX: GP4 (P1.6), RX: GP5 (P1.7)\r\n",
        (unsigned long)s_uart_cfg.baudrate,
        s_uart_cfg.data_bits,
        s_uart_cfg.parity == UART_PARITY_EVEN_MODE ? 'E' : (s_uart_cfg.parity == UART_PARITY_ODD_MODE ? 'O' : 'N'),
        s_uart_cfg.stop_bits);
    host.println("  Escape Sequences: Ctrl+] (0x1D) or Ctrl+X (0x18) to exit.\r\n");

    while (true) {
        // Host (USB CDC) -> Target UART (GP4)
        while (host.available()) {
            char c = (char)host.read();
            if (c == 0x1D || c == 0x18) { // Ctrl+] or Ctrl+X
                host.println("\r\n[UART Bridge Exited]");
                return;
            }
            Serial2.write((uint8_t)c);
        }

        // Target UART (GP5) -> Host (USB CDC)
        while (Serial2.available()) {
            char c = (char)Serial2.read();
            host.write((uint8_t)c);
        }

        tight_loop_contents();
    }
}

void uart_hal_monitor(Stream& host, bool hex_mode, uint32_t duration_ms) {
    if (!s_uart_cfg.active) {
        uart_hal_acquire(s_uart_cfg.baudrate, s_uart_cfg.data_bits, s_uart_cfg.parity, s_uart_cfg.stop_bits, s_uart_cfg.invert);
    }

    host.println("\r\n=== UART Passive RX Monitor Active ===");
    host.printf("  Listening on GP5 (P1.7) at %lu baud | Mode: %s\r\n",
        (unsigned long)s_uart_cfg.baudrate, hex_mode ? "HEX DUMP" : "ASCII STREAM");
    host.println("  Press any key to exit.\r\n");

    uint32_t start_ms = millis();
    size_t byte_count = 0;

    while (true) {
        if (host.available()) {
            while (host.available()) host.read();
            break;
        }
        if (duration_ms > 0 && (millis() - start_ms >= duration_ms)) {
            break;
        }

        if (Serial2.available()) {
            uint8_t b = (uint8_t)Serial2.read();
            if (hex_mode) {
                if (byte_count % 16 == 0) {
                    if (byte_count > 0) host.println();
                    host.printf("[%06lu ms] ", millis() - start_ms);
                }
                host.printf("%02X ", b);
            } else {
                host.write(b);
            }
            byte_count++;
        }
        tight_loop_contents();
    }
    host.printf("\r\n[Monitor stopped. Captured %u bytes.]\r\n", (unsigned int)byte_count);
}

bool uart_hal_test_loopback(Stream& out) {
    if (!s_uart_cfg.active) {
        uart_hal_acquire(s_uart_cfg.baudrate, s_uart_cfg.data_bits, s_uart_cfg.parity, s_uart_cfg.stop_bits, s_uart_cfg.invert);
    }

    out.println("\r\n=== UART Hardware Loopback Diagnostic ===");
    out.printf("  Testing GP4 (TX) -> GP5 (RX) at %lu baud (%u%c%u)...\r\n",
        (unsigned long)s_uart_cfg.baudrate,
        s_uart_cfg.data_bits,
        s_uart_cfg.parity == UART_PARITY_EVEN_MODE ? 'E' : (s_uart_cfg.parity == UART_PARITY_ODD_MODE ? 'O' : 'N'),
        s_uart_cfg.stop_bits);
    out.println("  (Note: Requires jumper wire connecting Probe CH2/GP4 to Probe CH3/GP5)");

    // Flush any leftover RX bytes
    while (Serial2.available()) Serial2.read();

    const char test_pattern[] = "UDOO-KEY-UART-TEST-0123456789";
    size_t len = strlen(test_pattern);
    Serial2.write((const uint8_t*)test_pattern, len);
    Serial2.flush();

    delay(20);

    char rx_buf[64] = {0};
    size_t rx_len = 0;
    uint32_t start_ms = millis();
    while (rx_len < len && (millis() - start_ms < 200)) {
        if (Serial2.available()) {
            rx_buf[rx_len++] = (char)Serial2.read();
        }
    }

    bool matched = (rx_len == len) && (memcmp(test_pattern, rx_buf, len) == 0);
    if (matched) {
        out.printf("  [PASS] Loopback test PASSED! Received %u/%u bytes perfectly.\r\n", (unsigned int)rx_len, (unsigned int)len);
        out.printf("         Echoed string: \"%s\"\r\n", rx_buf);
    } else {
        out.printf("  [FAIL] Loopback mismatch: sent %u bytes, received %u bytes.\r\n", (unsigned int)len, (unsigned int)rx_len);
        if (rx_len > 0) {
            rx_buf[rx_len] = '\0';
            out.printf("         Received string: \"%s\"\r\n", rx_buf);
        } else {
            out.println("         No bytes received. Verify physical jumper between GP4 and GP5.");
        }
    }
    return matched;
}

uint32_t uart_hal_detect_baud(uint32_t timeout_ms) {
    pinMode(UDOO_UART_RX_PIN, INPUT_PULLUP);
    uint32_t start_ms = millis();
    uint32_t min_pulse = 0xFFFFFFFF;
    uint32_t edges_seen = 0;

    uint32_t last_edge = time_us_32();
    bool last_state = digitalRead(UDOO_UART_RX_PIN);

    while (millis() - start_ms < timeout_ms) {
        bool state = digitalRead(UDOO_UART_RX_PIN);
        if (state != last_state) {
            uint32_t now = time_us_32();
            uint32_t dt = now - last_edge;
            last_edge = now;
            last_state = state;
            edges_seen++;
            if (dt >= 1 && dt < min_pulse) {
                min_pulse = dt;
            }
        }
    }

    if (edges_seen < 4 || min_pulse == 0xFFFFFFFF || min_pulse == 0) {
        return 0;
    }

    static const uint32_t std_bauds[] = {
        300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600, 1000000
    };

    uint32_t raw_baud = 1000000UL / min_pulse;
    uint32_t best_baud = 0;
    uint32_t min_diff = 0xFFFFFFFF;

    for (size_t i = 0; i < sizeof(std_bauds)/sizeof(std_bauds[0]); i++) {
        uint32_t diff = (raw_baud > std_bauds[i]) ? (raw_baud - std_bauds[i]) : (std_bauds[i] - raw_baud);
        if (diff < min_diff) {
            min_diff = diff;
            best_baud = std_bauds[i];
        }
    }

    return best_baud;
}
