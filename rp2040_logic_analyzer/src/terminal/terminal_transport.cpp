#include "terminal_transport.h"
#include <stdarg.h>
#include <stdio.h>

void terminal_transport_init() {
    // Serial is auto-initialized by the Arduino-Pico core, ensure DTR/status is ready
}

bool terminal_transport_available() {
    return Serial.available() > 0;
}

int terminal_transport_read() {
    return Serial.read();
}

int terminal_transport_peek() {
    return Serial.peek();
}

size_t terminal_transport_write(uint8_t b) {
    return Serial.write(b);
}

size_t terminal_transport_write_buf(const uint8_t* buf, size_t len) {
    if (!buf || len == 0) return 0;
    return Serial.write(buf, len);
}

size_t terminal_transport_print(const char* str) {
    if (!str) return 0;
    return Serial.print(str);
}

size_t terminal_transport_printf(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n <= 0) return 0;
    return Serial.write((const uint8_t*)buf, (size_t)n);
}

void terminal_transport_flush() {
    Serial.flush();
}

bool terminal_transport_connected() {
    return (bool)Serial;
}
