#include "pcap_service.h"
#include "packet_decoder.h"
#include <cstring>
#include <strings.h>

namespace Esp32Packet {

PcapService& PcapService::instance() {
    static PcapService s_instance;
    return s_instance;
}

PcapService::PcapService() {
    clearBuffer();
}

void PcapService::startCapture(uint32_t linkType) {
    m_activeLinkType = linkType;
    m_capturing = true;
}

void PcapService::stopCapture() {
    m_capturing = false;
}

void PcapService::clearBuffer() {
    m_head = 0;
    m_tail = 0;
    m_packetCount = 0;
    m_droppedCount = 0;
}

bool PcapService::pushPacket(const PacketRecord& rec) {
    if (!m_capturing) return false;

    size_t nextHead = (m_head + 1) % RING_BUFFER_CAPACITY;
    if (nextHead == m_tail && m_packetCount > 0) {
        // Buffer is full: advance tail (drop oldest) and record drop
        m_tail = (m_tail + 1) % RING_BUFFER_CAPACITY;
        m_droppedCount++;
    } else {
        m_packetCount++;
    }

    m_records[m_head] = rec;
    m_head = nextHead;
    return true;
}

void PcapService::printStatus(Print& out) {
    out.println("\r\n=== PCAP PACKET BUFFER STATUS ===");
    out.printf("  Capture State : %s\r\n", m_capturing ? "ACTIVE" : "STOPPED");
    out.printf("  Active Link   : DLT %lu (%s)\r\n",
               (unsigned long)m_activeLinkType,
               (m_activeLinkType == LINKTYPE_IEEE802_11) ? "802.11 Wireless" :
               ((m_activeLinkType == LINKTYPE_ETHERNET) ? "Ethernet" : "Custom"));
    out.printf("  Buffered Pkts : %u / %u\r\n", (unsigned int)m_packetCount, (unsigned int)RING_BUFFER_CAPACITY);
    out.printf("  Dropped Pkts  : %u (Buffer Overruns)\r\n", (unsigned int)m_droppedCount);
    out.println("=================================");
}

void PcapService::dumpHex(Print& out) {
    out.println("\r\n=== PCAP PACKET BUFFER DUMP ===");
    if (m_packetCount == 0) {
        out.println("Buffer is empty. No packets captured.");
        return;
    }

    size_t idx = m_tail;
    size_t num = 0;
    while (num < m_packetCount) {
        const auto& rec = m_records[idx];
        out.printf("\r\n--- Packet #%u (Source: %s, Length: %u wire / %u saved) ---\r\n",
                   (unsigned int)(num + 1),
                   captureSourceToString((CaptureSource)rec.source),
                   rec.original_length, rec.captured_length);
        PacketDecoder::printSummary(rec, out);
        PacketDecoder::printHexDump(rec.data, rec.captured_length, out);

        idx = (idx + 1) % RING_BUFFER_CAPACITY;
        num++;
    }
    out.println("===============================");
}

void PcapService::dumpBinary(Stream& out) {
    // Write standard 24-byte PCAP Global Header
    PcapGlobalHeader gh;
    gh.magic_number  = 0xA1B2C3D4;
    gh.version_major = 2;
    gh.version_minor = 4;
    gh.thiszone      = 0;
    gh.sigfigs       = 0;
    gh.snaplen       = 65535;
    gh.network       = m_activeLinkType;
    out.write((const uint8_t*)&gh, sizeof(gh));

    // Write all buffered packet records
    size_t idx = m_tail;
    size_t num = 0;
    while (num < m_packetCount) {
        const auto& rec = m_records[idx];
        PcapRecordHeader ph;
        ph.ts_sec   = static_cast<uint32_t>(rec.timestamp_us / 1000000ULL);
        ph.ts_usec  = static_cast<uint32_t>(rec.timestamp_us % 1000000ULL);
        ph.incl_len = rec.captured_length;
        ph.orig_len = rec.original_length;

        out.write((const uint8_t*)&ph, sizeof(ph));
        out.write(rec.data, rec.captured_length);

        idx = (idx + 1) % RING_BUFFER_CAPACITY;
        num++;
    }
}

void PcapService::executeCommand(const char* args, Print& out) {
    while (*args && isspace(*args)) args++;
    if (!*args || strcasecmp(args, "status") == 0) {
        printStatus(out);
        return;
    }

    if (strcasecmp(args, "start") == 0) {
        startCapture();
        out.println("PCAP packet recording started.");
    } else if (strcasecmp(args, "stop") == 0) {
        stopCapture();
        out.println("PCAP packet recording stopped.");
    } else if (strcasecmp(args, "clear") == 0) {
        clearBuffer();
        out.println("PCAP packet buffer cleared.");
    } else if (strcasecmp(args, "dump") == 0) {
        dumpHex(out);
    } else {
        out.println("Available commands: pcap status | pcap start | pcap stop | pcap dump | pcap clear");
    }
}

} // namespace Esp32Packet
