#ifndef TERMINAL_TRANSPORT_H
#define TERMINAL_TRANSPORT_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void   terminal_transport_init();
bool   terminal_transport_available();
int    terminal_transport_read();
int    terminal_transport_peek();
size_t terminal_transport_write(uint8_t b);
size_t terminal_transport_write_buf(const uint8_t* buf, size_t len);
size_t terminal_transport_print(const char* str);
size_t terminal_transport_printf(const char* fmt, ...);
void   terminal_transport_flush();
bool   terminal_transport_connected();

#ifdef __cplusplus
}
#endif

#endif // TERMINAL_TRANSPORT_H
