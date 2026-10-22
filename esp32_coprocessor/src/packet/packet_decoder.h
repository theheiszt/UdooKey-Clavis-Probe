#pragma once

#include <Arduino.h>
#include "packet_record.h"

namespace Esp32Packet {

struct DecodedSummary {
    char linkSummary[64];
    char networkSummary[64];
    char transportSummary[64];
    char appSummary[64];
    char overallSummary[128];
};

class PacketDecoder {
public:
    static bool decode(const uint8_t* data, size_t len, uint32_t linkType, DecodedSummary& out);
    static void printSummary(const PacketRecord& rec, Print& out);
    static void printHexDump(const uint8_t* data, size_t len, Print& out);

private:
    static bool decode80211(const uint8_t* data, size_t len, DecodedSummary& out);
    static bool decodeEthernet(const uint8_t* data, size_t len, DecodedSummary& out);
    static bool decodeArp(const uint8_t* data, size_t len, DecodedSummary& out);
    static bool decodeIpv4(const uint8_t* data, size_t len, DecodedSummary& out);
    static bool decodeIpv6(const uint8_t* data, size_t len, DecodedSummary& out);
    static bool decodeIcmp(const uint8_t* data, size_t len, DecodedSummary& out);
    static bool decodeUdp(const uint8_t* data, size_t len, DecodedSummary& out);
    static bool decodeTcp(const uint8_t* data, size_t len, DecodedSummary& out);
    static bool decodeDns(const uint8_t* data, size_t len, DecodedSummary& out);
    static bool decodeDhcp(const uint8_t* data, size_t len, DecodedSummary& out);
};

} // namespace Esp32Packet
