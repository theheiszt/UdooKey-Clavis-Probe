#include "serial_protocol.h"

void serial_protocol_init() {
    Serial.begin(115200);
}

void serial_protocol_process(LogicAnalyzerConfig& cfg, bool& start_requested, bool& stop_requested) {
    if (Serial.available() < (int)sizeof(SerialHeader)) return;

    if (Serial.peek() != PROTOCOL_MAGIC) {
        Serial.read(); // Skip invalid byte
        return;
    }

    SerialHeader hdr;
    Serial.readBytes((char*)&hdr, sizeof(hdr));

    switch (hdr.command) {
        case CMD_GET_INFO: {
            Serial.write(PROTOCOL_MAGIC);
            Serial.write(CMD_GET_INFO);
            Serial.println("UDOO-KEY-LOGIC-ANALYZER-V1.0");
            break;
        }
        case CMD_SET_CONFIG: {
            if (hdr.length == sizeof(SerialConfigPayload)) {
                SerialConfigPayload p;
                Serial.readBytes((char*)&p, sizeof(p));
                cfg.sample_rate_hz   = p.sample_rate;
                cfg.sample_count     = p.sample_count;
                cfg.trigger_channel = p.trigger_channel;
                cfg.trigger_mode    = p.trigger_mode;
            }
            break;
        }
        case CMD_START_CAPTURE: {
            start_requested = true;
            break;
        }
        case CMD_STOP_CAPTURE: {
            stop_requested = true;
            break;
        }
    }
}

void serial_protocol_send_buffer(const uint8_t* buffer, uint32_t length, uint32_t sample_rate) {
    SerialHeader hdr = { PROTOCOL_MAGIC, CMD_STREAM_DATA, (uint16_t)sizeof(uint32_t) };
    Serial.write((uint8_t*)&hdr, sizeof(hdr));

    // Send length & sample rate headers
    Serial.write((uint8_t*)&length, sizeof(uint32_t));
    Serial.write((uint8_t*)&sample_rate, sizeof(uint32_t));

    // Send binary logic sample buffer
    Serial.write(buffer, length);
    Serial.flush();
}
