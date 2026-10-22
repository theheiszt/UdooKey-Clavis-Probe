#ifndef INTERCHIP_LINK_H
#define INTERCHIP_LINK_H

#include <Arduino.h>
#include "hardware_config.h"
#include "instrument_protocol.h"

// Initialize Inter-MCU Serial Transport
void interchip_init(uint32_t baud = INTERMCU_UART_BAUDRATE);

// Fast binary packet transmission with CRC-16
bool interchip_send_packet(InstrumentPacketType type, uint16_t seq, const void* payload, uint16_t len);

// Stream logic capture buffer directly to ESP32 in chunks
void interchip_stream_capture(const uint8_t* buffer, uint32_t length);

// Process incoming requests from ESP32
bool interchip_available();
bool interchip_read_packet(InstrumentPacketHeader& hdr, uint8_t* payload_buf, uint16_t max_len);

// Diagnostics & link health stats
uint32_t interchip_get_tx_count();
uint32_t interchip_get_rx_count();
void interchip_send_ping();

#endif // INTERCHIP_LINK_H

