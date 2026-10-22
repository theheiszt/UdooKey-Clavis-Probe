#include "interchip_link.h"

#define ESP32_UART Serial1

static uint16_t tx_seq = 0;
static uint32_t tx_packets = 0;
static uint32_t rx_packets = 0;

void interchip_init(uint32_t baud) {
    ESP32_UART.begin(baud);
}

bool interchip_send_packet(InstrumentPacketType type, uint16_t seq, const void* payload, uint16_t len) {
    InstrumentPacketHeader hdr;
    hdr.magic       = INSTRUMENT_PROTO_MAGIC;
    hdr.version     = INSTRUMENT_PROTO_VERSION;
    hdr.type        = (uint8_t)type;
    hdr.sequence    = seq ? seq : ++tx_seq;
    hdr.payload_len = len;
    hdr.crc16       = (payload && len > 0) ? instrument_crc16((const uint8_t*)payload, len) : 0;

    size_t written = ESP32_UART.write((const uint8_t*)&hdr, sizeof(hdr));
    if (written != sizeof(hdr)) return false;

    if (payload && len > 0) {
        ESP32_UART.write((const uint8_t*)payload, len);
    }
    tx_packets++;
    return true;
}

void interchip_stream_capture(const uint8_t* buffer, uint32_t length) {
    if (!buffer || length == 0) return;

    // Send metadata header first
    InstrumentCaptureInfoPayload info;
    memset(&info, 0, sizeof(info));
    info.total_samples    = length;
    info.whole_buffer_crc = instrument_crc16(buffer, length);

    interchip_send_packet(PKT_TYPE_CAPTURE_INFO_RESP, 0, &info, sizeof(info));

    // Stream sample data in 512-byte chunks
    uint16_t total_chunks = (length + INSTRUMENT_CHUNK_SIZE - 1) / INSTRUMENT_CHUNK_SIZE;
    uint32_t offset = 0;

    for (uint16_t chk = 0; chk < total_chunks; chk++) {
        uint16_t chunk_len = (length - offset > INSTRUMENT_CHUNK_SIZE) ? INSTRUMENT_CHUNK_SIZE : (uint16_t)(length - offset);

        InstrumentChunkDataPayload chunk_pkt;
        chunk_pkt.chunk_index  = chk;
        chunk_pkt.total_chunks = total_chunks;
        chunk_pkt.chunk_len    = chunk_len;
        memcpy(chunk_pkt.data, buffer + offset, chunk_len);

        interchip_send_packet(PKT_TYPE_CAPTURE_DATA_CHUNK, chk, &chunk_pkt, sizeof(chunk_pkt) - INSTRUMENT_CHUNK_SIZE + chunk_len);
        offset += chunk_len;
    }
}

bool interchip_available() {
    if (ESP32_UART.available() < (int)sizeof(InstrumentPacketHeader)) return false;
    // Check for magic 0x55 then 0xAA (little endian 0xAA55)
    uint8_t b1 = ESP32_UART.peek();
    return (b1 == (INSTRUMENT_PROTO_MAGIC & 0xFF));
}

bool interchip_read_packet(InstrumentPacketHeader& hdr, uint8_t* payload_buf, uint16_t max_len) {
    if (ESP32_UART.available() < (int)sizeof(InstrumentPacketHeader)) return false;

    uint8_t peek_b = ESP32_UART.peek();
    if (peek_b != (INSTRUMENT_PROTO_MAGIC & 0xFF)) {
        ESP32_UART.read(); // Discard non-sync byte
        return false;
    }

    ESP32_UART.readBytes((char*)&hdr, sizeof(hdr));
    if (hdr.magic != INSTRUMENT_PROTO_MAGIC) return false;

    uint16_t to_read = hdr.payload_len;
    if (to_read > max_len) to_read = max_len;

    if (payload_buf && to_read > 0) {
        ESP32_UART.readBytes((char*)payload_buf, to_read);

        // Verify CRC-16
        uint16_t calc_crc = instrument_crc16(payload_buf, to_read);
        if (calc_crc != hdr.crc16) {
            return false; // CRC error
        }
    }
    rx_packets++;
    return true;
}

uint32_t interchip_get_tx_count() {
    return tx_packets;
}

uint32_t interchip_get_rx_count() {
    return rx_packets;
}

void interchip_send_ping() {
    interchip_send_packet(PKT_TYPE_PING, 0, nullptr, 0);
}

