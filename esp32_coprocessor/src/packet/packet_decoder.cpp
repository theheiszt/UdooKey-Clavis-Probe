#include "packet_decoder.h"
#include <cstring>
#include <cstdio>

namespace Esp32Packet {

const char* captureSourceToString(CaptureSource src) {
    switch (src) {
        case CaptureSource::WIFI_PROMISC: return "Wi-Fi Sniff";
        case CaptureSource::BLE_ADV:      return "BLE Adv";
        case CaptureSource::NRF24:        return "nRF24L01";
        case CaptureSource::IR:           return "Infrared";
        case CaptureSource::USER_BUS:     return "User Bus";
        default:                          return "Unknown";
    }
}

void PacketDecoder::printHexDump(const uint8_t* data, size_t len, Print& out) {
    for (size_t i = 0; i < len; i += 16) {
        out.printf("  %04X: ", (unsigned int)i);
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < len) out.printf("%02X ", data[i + j]);
            else out.print("   ");
            if (j == 7) out.print(" ");
        }
        out.print(" |");
        for (size_t j = 0; j < 16 && (i + j) < len; ++j) {
            uint8_t c = data[i + j];
            out.print((c >= 32 && c <= 126) ? (char)c : '.');
        }
        out.println("|");
    }
}

bool PacketDecoder::decode80211(const uint8_t* data, size_t len, DecodedSummary& out) {
    if (len < 10) return false;

    uint16_t fc = data[0] | (data[1] << 8);
    uint8_t type = (fc >> 2) & 0x03;
    uint8_t subtype = (fc >> 4) & 0x0F;

    const char* typeName = (type == 0) ? "Mgmt" : ((type == 1) ? "Ctrl" : ((type == 2) ? "Data" : "Reserved"));
    const char* subName = "Unknown";

    if (type == 0) { // Management
        switch (subtype) {
            case 0x00: subName = "AssocReq"; break;
            case 0x01: subName = "AssocResp"; break;
            case 0x04: subName = "ProbeReq"; break;
            case 0x05: subName = "ProbeResp"; break;
            case 0x08: subName = "Beacon"; break;
            case 0x0B: subName = "Auth"; break;
            case 0x0C: subName = "Deauth"; break;
            default:   subName = "MgmtOther"; break;
        }
    } else if (type == 1) { // Control
        switch (subtype) {
            case 0x0B: subName = "RTS"; break;
            case 0x0C: subName = "CTS"; break;
            case 0x0D: subName = "ACK"; break;
            default:   subName = "CtrlOther"; break;
        }
    } else if (type == 2) { // Data
        switch (subtype) {
            case 0x00: subName = "Data"; break;
            case 0x08: subName = "QoS-Data"; break;
            case 0x04: subName = "Null"; break;
            case 0x0C: subName = "QoS-Null"; break;
            default:   subName = "DataOther"; break;
        }
    }

    snprintf(out.linkSummary, sizeof(out.linkSummary), "802.11 %s [%s]", typeName, subName);

    if (len >= 24) {
        char addr1[18], addr2[18];
        snprintf(addr1, sizeof(addr1), "%02X:%02X:%02X:%02X:%02X:%02X",
                 data[4], data[5], data[6], data[7], data[8], data[9]);
        snprintf(addr2, sizeof(addr2), "%02X:%02X:%02X:%02X:%02X:%02X",
                 data[10], data[11], data[12], data[13], data[14], data[15]);

        // Beacon / Probe SSID extraction
        if (type == 0 && (subtype == 0x08 || subtype == 0x05) && len > 38) {
            size_t tagOffset = 36;
            if (tagOffset + 2 <= len && data[tagOffset] == 0) { // Tag 0 = SSID
                uint8_t ssidLen = data[tagOffset + 1];
                if (tagOffset + 2 + ssidLen <= len) {
                    char ssid[33] = {0};
                    size_t copyLen = (ssidLen > 32) ? 32 : ssidLen;
                    memcpy(ssid, &data[tagOffset + 2], copyLen);
                    snprintf(out.overallSummary, sizeof(out.overallSummary),
                             "802.11 Beacon: SSID=\"%s\" BSSID=%s", ssid, addr2);
                    return true;
                }
            }
        }

        snprintf(out.overallSummary, sizeof(out.overallSummary),
                 "802.11 %s %s -> %s", subName, addr2, addr1);
    } else {
        snprintf(out.overallSummary, sizeof(out.overallSummary), "802.11 %s [%s]", typeName, subName);
    }
    return true;
}

bool PacketDecoder::decodeEthernet(const uint8_t* data, size_t len, DecodedSummary& out) {
    if (len < 14) return false;

    uint16_t etherType = (data[12] << 8) | data[13];
    snprintf(out.linkSummary, sizeof(out.linkSummary), "Ethernet II (Type: 0x%04X)", etherType);

    const uint8_t* payload = data + 14;
    size_t payloadLen = len - 14;

    if (etherType == 0x0800) {
        return decodeIpv4(payload, payloadLen, out);
    } else if (etherType == 0x0806) {
        return decodeArp(payload, payloadLen, out);
    } else if (etherType == 0x86DD) {
        return decodeIpv6(payload, payloadLen, out);
    } else {
        snprintf(out.overallSummary, sizeof(out.overallSummary), "Ethernet EtherType 0x%04X (len=%u)", etherType, (unsigned int)len);
        return true;
    }
}

bool PacketDecoder::decodeArp(const uint8_t* data, size_t len, DecodedSummary& out) {
    if (len < 28) return false;
    uint16_t oper = (data[6] << 8) | data[7];
    const char* opStr = (oper == 1) ? "Who has" : ((oper == 2) ? "Is at" : "ARP");
    snprintf(out.networkSummary, sizeof(out.networkSummary), "ARP (%s)", opStr);

    snprintf(out.overallSummary, sizeof(out.overallSummary),
             "ARP: %s %u.%u.%u.%u? Tell %u.%u.%u.%u",
             (oper == 1) ? "Who has" : "Reply",
             data[24], data[25], data[26], data[27],
             data[14], data[15], data[16], data[17]);
    return true;
}

bool PacketDecoder::decodeIpv4(const uint8_t* data, size_t len, DecodedSummary& out) {
    if (len < 20) return false;
    uint8_t ihl = (data[0] & 0x0F) * 4;
    if (len < ihl) return false;

    uint8_t proto = data[9];
    char srcIp[16], dstIp[16];
    snprintf(srcIp, sizeof(srcIp), "%u.%u.%u.%u", data[12], data[13], data[14], data[15]);
    snprintf(dstIp, sizeof(dstIp), "%u.%u.%u.%u", data[16], data[17], data[18], data[19]);

    snprintf(out.networkSummary, sizeof(out.networkSummary), "IPv4 %s -> %s", srcIp, dstIp);

    const uint8_t* transData = data + ihl;
    size_t transLen = len - ihl;

    if (proto == 1) { // ICMP
        return decodeIcmp(transData, transLen, out);
    } else if (proto == 6) { // TCP
        return decodeTcp(transData, transLen, out);
    } else if (proto == 17) { // UDP
        return decodeUdp(transData, transLen, out);
    } else {
        snprintf(out.overallSummary, sizeof(out.overallSummary), "IPv4 proto=%u %s -> %s", proto, srcIp, dstIp);
        return true;
    }
}

bool PacketDecoder::decodeIpv6(const uint8_t* data, size_t len, DecodedSummary& out) {
    if (len < 40) return false;
    uint8_t nextHeader = data[6];
    snprintf(out.networkSummary, sizeof(out.networkSummary), "IPv6 (Next: %u)", nextHeader);
    snprintf(out.overallSummary, sizeof(out.overallSummary), "IPv6 packet (next_hdr=%u, len=%u)", nextHeader, (unsigned int)len);
    return true;
}

bool PacketDecoder::decodeIcmp(const uint8_t* data, size_t len, DecodedSummary& out) {
    if (len < 4) return false;
    uint8_t type = data[0];
    uint8_t code = data[1];
    const char* typeStr = (type == 8) ? "Echo Req" : ((type == 0) ? "Echo Reply" : ((type == 3) ? "Unreachable" : "ICMP"));
    snprintf(out.transportSummary, sizeof(out.transportSummary), "ICMP %s (code %u)", typeStr, code);
    snprintf(out.overallSummary, sizeof(out.overallSummary), "%s | ICMP %s", out.networkSummary, typeStr);
    return true;
}

bool PacketDecoder::decodeUdp(const uint8_t* data, size_t len, DecodedSummary& out) {
    if (len < 8) return false;
    uint16_t srcPort = (data[0] << 8) | data[1];
    uint16_t dstPort = (data[2] << 8) | data[3];
    uint16_t ulen = (data[4] << 8) | data[5];

    snprintf(out.transportSummary, sizeof(out.transportSummary), "UDP %u -> %u (len %u)", srcPort, dstPort, ulen);

    const uint8_t* appData = data + 8;
    size_t appLen = (len > 8) ? (len - 8) : 0;

    if (srcPort == 53 || dstPort == 53) {
        return decodeDns(appData, appLen, out);
    } else if (srcPort == 67 || srcPort == 68 || dstPort == 67 || dstPort == 68) {
        return decodeDhcp(appData, appLen, out);
    } else {
        snprintf(out.overallSummary, sizeof(out.overallSummary), "%s | UDP %u -> %u", out.networkSummary, srcPort, dstPort);
        return true;
    }
}

bool PacketDecoder::decodeTcp(const uint8_t* data, size_t len, DecodedSummary& out) {
    if (len < 20) return false;
    uint16_t srcPort = (data[0] << 8) | data[1];
    uint16_t dstPort = (data[2] << 8) | data[3];
    uint8_t flags = data[13];

    char flagBuf[32] = {0};
    int fi = 0;
    if (flags & 0x02) flagBuf[fi++] = 'S';
    if (flags & 0x10) flagBuf[fi++] = 'A';
    if (flags & 0x01) flagBuf[fi++] = 'F';
    if (flags & 0x04) flagBuf[fi++] = 'R';
    if (flags & 0x08) flagBuf[fi++] = 'P';

    snprintf(out.transportSummary, sizeof(out.transportSummary), "TCP %u -> %u [%s]", srcPort, dstPort, flagBuf);
    snprintf(out.overallSummary, sizeof(out.overallSummary), "%s | TCP %u -> %u [%s]", out.networkSummary, srcPort, dstPort, flagBuf);
    return true;
}

bool PacketDecoder::decodeDns(const uint8_t* data, size_t len, DecodedSummary& out) {
    if (len < 12) return false;
    uint16_t txId = (data[0] << 8) | data[1];
    uint16_t flags = (data[2] << 8) | data[3];
    bool isResponse = (flags & 0x8000) != 0;
    uint16_t qdCount = (data[4] << 8) | data[5];
    uint16_t anCount = (data[6] << 8) | data[7];

    snprintf(out.appSummary, sizeof(out.appSummary), "DNS %s (TxID: 0x%04X)", isResponse ? "Resp" : "Query", txId);
    snprintf(out.overallSummary, sizeof(out.overallSummary), "%s | DNS %s TxID=0x%04X (Q:%u A:%u)",
             out.networkSummary, isResponse ? "Reply" : "Query", txId, qdCount, anCount);
    return true;
}

bool PacketDecoder::decodeDhcp(const uint8_t* data, size_t len, DecodedSummary& out) {
    if (len < 240) return false;
    uint8_t op = data[0];
    const char* opStr = (op == 1) ? "BootRequest" : ((op == 2) ? "BootReply" : "DHCP");

    snprintf(out.appSummary, sizeof(out.appSummary), "DHCP %s", opStr);
    snprintf(out.overallSummary, sizeof(out.overallSummary), "%s | DHCP %s (Yiaddr: %u.%u.%u.%u)",
             out.networkSummary, opStr, data[16], data[17], data[18], data[19]);
    return true;
}

bool PacketDecoder::decode(const uint8_t* data, size_t len, uint32_t linkType, DecodedSummary& out) {
    memset(&out, 0, sizeof(out));
    if (!data || len == 0) {
        strncpy(out.overallSummary, "<Empty Packet>", sizeof(out.overallSummary));
        return false;
    }

    if (linkType == LINKTYPE_IEEE802_11) {
        return decode80211(data, len, out);
    } else if (linkType == LINKTYPE_ETHERNET) {
        return decodeEthernet(data, len, out);
    } else if (linkType == LINKTYPE_USER0) {
        snprintf(out.linkSummary, sizeof(out.linkSummary), "Custom RF / Bus (USER0)");
        snprintf(out.overallSummary, sizeof(out.overallSummary), "USER0 Payload (%u bytes)", (unsigned int)len);
        return true;
    } else if (linkType == LINKTYPE_BLUETOOTH_LE_LL) {
        snprintf(out.linkSummary, sizeof(out.linkSummary), "Bluetooth LE Link Layer");
        snprintf(out.overallSummary, sizeof(out.overallSummary), "BLE LL Frame (%u bytes)", (unsigned int)len);
        return true;
    }

    snprintf(out.overallSummary, sizeof(out.overallSummary), "Raw Packet LinkType=%u (%u bytes)", (unsigned int)linkType, (unsigned int)len);
    return true;
}

void PacketDecoder::printSummary(const PacketRecord& rec, Print& out) {
    DecodedSummary summary;
    decode(rec.data, rec.captured_length, rec.link_type, summary);

    out.printf("[%010llu us] %-12s CH:%-2u RSSI:%-4d dBm Len:%-3u | %s\r\n",
               rec.timestamp_us,
               captureSourceToString((CaptureSource)rec.source),
               rec.channel,
               rec.rssi,
               rec.captured_length,
               summary.overallSummary);
}

} // namespace Esp32Packet
