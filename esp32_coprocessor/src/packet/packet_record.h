#pragma once

#include <cstdint>
#include <cstddef>

namespace Esp32Packet {

enum class CaptureSource : uint8_t {
    NONE         = 0,
    WIFI_PROMISC = 1,
    BLE_ADV      = 2,
    NRF24        = 3,
    IR           = 4,
    USER_BUS     = 5
};

// Standard PCAP Link-Layer Header Types (DLT)
enum PcapLinkType : uint32_t {
    LINKTYPE_NULL              = 0,
    LINKTYPE_ETHERNET          = 1,
    LINKTYPE_IEEE802_11        = 105,
    LINKTYPE_USER0             = 147,
    LINKTYPE_BLUETOOTH_LE_LL   = 251
};

#pragma pack(push, 1)
struct PacketRecord {
    uint64_t timestamp_us;     // Microsecond timestamp
    uint8_t  source;           // CaptureSource
    uint8_t  channel;          // RF or protocol channel
    int16_t  rssi;             // Signal strength in dBm (if applicable)
    uint16_t captured_length;  // Length of data stored in buffer
    uint16_t original_length;  // Wire length of packet before truncation
    uint32_t link_type;        // PCAP DLT link type
    uint8_t  data[256];        // Bounded static buffer per record
};
#pragma pack(pop)

const char* captureSourceToString(CaptureSource src);

} // namespace Esp32Packet
