#pragma once

#include <Arduino.h>
#include "packet_record.h"

namespace Esp32Packet {

#pragma pack(push, 1)
struct PcapGlobalHeader {
    uint32_t magic_number;  // 0xA1B2C3D4
    uint16_t version_major; // 2
    uint16_t version_minor; // 4
    int32_t  thiszone;      // GMT to local correction (0)
    uint32_t sigfigs;       // Accuracy of timestamps (0)
    uint32_t snaplen;       // Max length of captured packets (65535)
    uint32_t network;       // Data link type (e.g., LINKTYPE_IEEE802_11 = 105)
};

struct PcapRecordHeader {
    uint32_t ts_sec;   // Timestamp seconds
    uint32_t ts_usec;  // Timestamp microseconds
    uint32_t incl_len; // Number of octets of packet saved in file
    uint32_t orig_len; // Actual length of packet
};
#pragma pack(pop)

class PcapService {
public:
    static PcapService& instance();

    void startCapture(uint32_t linkType = LINKTYPE_IEEE802_11);
    void stopCapture();
    bool isCapturing() const { return m_capturing; }
    void clearBuffer();

    bool pushPacket(const PacketRecord& rec);
    size_t packetCount() const { return m_packetCount; }
    size_t droppedCount() const { return m_droppedCount; }

    void printStatus(Print& out);
    void dumpHex(Print& out);
    void dumpBinary(Stream& out);

    void executeCommand(const char* args, Print& out);

private:
    PcapService();

    static const size_t RING_BUFFER_CAPACITY = 64; // Bounded packet storage in SRAM
    PacketRecord m_records[RING_BUFFER_CAPACITY];
    size_t m_head{0};
    size_t m_tail{0};
    size_t m_packetCount{0};
    size_t m_droppedCount{0};
    uint32_t m_activeLinkType{LINKTYPE_IEEE802_11};
    bool m_capturing{false};
};

} // namespace Esp32Packet
