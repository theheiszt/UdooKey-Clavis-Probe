#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <Arduino.h>
#include "pio_capture.h"

#define PROTOCOL_MAGIC 0x5A

enum SerialCommand : uint8_t {
    CMD_GET_INFO        = 0x01,
    CMD_SET_CONFIG      = 0x02,
    CMD_START_CAPTURE   = 0x03,
    CMD_STOP_CAPTURE    = 0x04,
    CMD_STREAM_DATA     = 0x05
};

#pragma pack(push, 1)
struct SerialHeader {
    uint8_t magic;
    uint8_t command;
    uint16_t length;
};

struct SerialConfigPayload {
    uint32_t sample_rate;
    uint32_t sample_count;
    uint8_t  trigger_channel;
    uint8_t  trigger_mode;
};
#pragma pack(pop)

void serial_protocol_init();
void serial_protocol_process(LogicAnalyzerConfig& cfg, bool& start_requested, bool& stop_requested);
void serial_protocol_send_buffer(const uint8_t* buffer, uint32_t length, uint32_t sample_rate);

#endif // SERIAL_PROTOCOL_H
