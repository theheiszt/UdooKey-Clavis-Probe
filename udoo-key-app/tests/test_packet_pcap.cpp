#include <iostream>
#include <cassert>
#include <cstring>
#include <QByteArray>
#include <QList>
#include "WirelessClient.h"
#include "ITransport.h"
#include "instrument_protocol.h"

using namespace UdooKey;

// Mock Transport for PCAP & Packet Unit Tests
class MockPcapTransport : public ITransport {
public:
    explicit MockPcapTransport(QObject* parent = nullptr) : ITransport(parent) {}

    bool open() override { m_open = true; emit connected(); return true; }
    void close() override { m_open = false; emit disconnected(); }
    bool isOpen() const override { return m_open; }

    qint64 write(const QByteArray& data) override {
        m_writtenPackets.append(data);
        return data.size();
    }
    qint64 write(const char* data, qint64 maxSize) override {
        m_writtenPackets.append(QByteArray(data, static_cast<int>(maxSize)));
        return maxSize;
    }
    QByteArray readAll() override { return {}; }
    qint64 bytesAvailable() const override { return 0; }
    QString portName() const override { return "MockPcapTransport"; }
    void setPortName(const QString&) override {}

    void injectData(const QByteArray& data) {
        emit dataReady(data);
    }

    QList<QByteArray> m_writtenPackets;
    bool m_open = true;
};

static void injectPacket(MockPcapTransport& transport, uint8_t type, uint16_t seq,
                         const void* payload, uint16_t plen) {
    InstrumentPacketHeader hdr;
    hdr.magic       = INSTRUMENT_PROTO_MAGIC;
    hdr.version     = INSTRUMENT_PROTO_VERSION;
    hdr.type        = type;
    hdr.sequence    = seq;
    hdr.payload_len = plen;
    hdr.crc16       = plen ? instrument_crc16(reinterpret_cast<const uint8_t*>(payload), plen) : 0;

    QByteArray raw;
    raw.resize(sizeof(hdr) + plen);
    std::memcpy(raw.data(), &hdr, sizeof(hdr));
    if (plen && payload) {
        std::memcpy(raw.data() + sizeof(hdr), payload, plen);
    }

    transport.injectData(raw);
}

#pragma pack(push, 1)
struct PcapGlobalHeaderVerify {
    uint32_t magic_number;  // 0xA1B2C3D4
    uint16_t version_major; // 2
    uint16_t version_minor; // 4
    int32_t  thiszone;      // 0
    uint32_t sigfigs;       // 0
    uint32_t snaplen;       // 65535
    uint32_t network;       // Link type
};

struct PcapRecordHeaderVerify {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
};
#pragma pack(pop)

bool runPacketPcapTests() {
    std::cout << "--- Running Packet & PCAP Protocol Tests ---" << std::endl;

    MockPcapTransport transport;
    transport.open();
    WirelessClient client(&transport);
    assert(client.isConnected());

    // ─────────────────────────────────────────────────────────────────────────
    // Test 1: Wi-Fi Promiscuous Sniffer Config (PKT_TYPE_WIFI_SNIFF_CONFIG 0x2D)
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        bool ok = client.configureWifiSniffer(true, 6, 2); // Channel 6, Control frames
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());

        const QByteArray& pkt = transport.m_writtenPackets.last();
        assert(pkt.size() == sizeof(InstrumentPacketHeader) + sizeof(InstrumentWifiSniffConfigPayload));

        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.magic == INSTRUMENT_PROTO_MAGIC);
        assert(hdr.type == PKT_TYPE_WIFI_SNIFF_CONFIG);
        assert(hdr.payload_len == sizeof(InstrumentWifiSniffConfigPayload));

        InstrumentWifiSniffConfigPayload cfg;
        std::memcpy(&cfg, pkt.constData() + sizeof(hdr), sizeof(cfg));
        assert(cfg.enabled == 1);
        assert(cfg.channel == 6);
        assert(cfg.filter_type == 2);

        uint16_t crc = instrument_crc16(reinterpret_cast<const uint8_t*>(&cfg), sizeof(cfg));
        assert(hdr.crc16 == crc);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 2: Wi-Fi Promiscuous Sniffer Status Telemetry (0x2E)
    // ─────────────────────────────────────────────────────────────────────────
    {
        InstrumentWifiSniffStatusPayload st;
        st.active          = 1;
        st.channel         = 6;
        st.filter_type     = 2;
        st.reserved        = 0;
        st.captured_count  = 1284;
        st.dropped_count   = 3;

        bool statusReceived = false;
        QMetaObject::Connection conn = QObject::connect(&client, &WirelessClient::wifiSnifferStatusReceived,
            [&](const InstrumentWifiSniffStatusPayload& resp) {
                assert(resp.active == 1);
                assert(resp.channel == 6);
                assert(resp.filter_type == 2);
                assert(resp.captured_count == 1284);
                assert(resp.dropped_count == 3);
                statusReceived = true;
            });

        injectPacket(transport, PKT_TYPE_WIFI_SNIFF_STATUS, 101, &st, sizeof(st));
        assert(statusReceived);
        QObject::disconnect(conn);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 3: Structured PacketRecord Delivery (0x50)
    // ─────────────────────────────────────────────────────────────────────────
    {
        InstrumentPacketRecordPayload rec;
        std::memset(&rec, 0, sizeof(rec));
        rec.timestamp_us   = 1000500ULL;
        rec.source         = 2; // BLE
        rec.channel        = 37;
        rec.rssi           = -72;
        rec.link_type      = 251; // LINKTYPE_BLUETOOTH_LE_LL
        rec.captured_length= 32;
        rec.original_length= 32;
        for (int i = 0; i < 32; ++i) {
            rec.data[i] = static_cast<uint8_t>(i ^ 0xAA);
        }

        bool recordReceived = false;
        QMetaObject::Connection conn = QObject::connect(&client, &WirelessClient::packetRecordReceived,
            [&](const InstrumentPacketRecordPayload& r) {
                assert(r.timestamp_us == 1000500ULL);
                assert(r.source == 2);
                assert(r.channel == 37);
                assert(r.rssi == -72);
                assert(r.link_type == 251);
                assert(r.captured_length == 32);
                assert(r.original_length == 32);
                assert(r.data[0] == (0 ^ 0xAA));
                assert(r.data[31] == (31 ^ 0xAA));
                recordReceived = true;
            });

        injectPacket(transport, PKT_TYPE_PACKET_RECORD, 102, &rec, sizeof(rec));
        assert(recordReceived);
        QObject::disconnect(conn);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 4: PCAP Dump Request Framing (0x51)
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        bool ok = client.requestPcapDump(true, true, 64);
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());

        const QByteArray& pkt = transport.m_writtenPackets.last();
        assert(pkt.size() == sizeof(InstrumentPacketHeader) + sizeof(InstrumentPcapDumpReqPayload));

        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.magic == INSTRUMENT_PROTO_MAGIC);
        assert(hdr.type == PKT_TYPE_PCAP_DUMP_REQ);
        assert(hdr.payload_len == sizeof(InstrumentPcapDumpReqPayload));

        InstrumentPcapDumpReqPayload req;
        std::memcpy(&req, pkt.constData() + sizeof(hdr), sizeof(req));
        assert(req.stream_binary == 1);
        assert(req.clear_after == 1);
        assert(req.max_packets == 64);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 5: Standard PCAP Global & Record Header Spec Verification
    // ─────────────────────────────────────────────────────────────────────────
    {
        assert(sizeof(PcapGlobalHeaderVerify) == 24);
        assert(sizeof(PcapRecordHeaderVerify) == 16);

        PcapGlobalHeaderVerify ghdr;
        ghdr.magic_number  = 0xA1B2C3D4;
        ghdr.version_major = 2;
        ghdr.version_minor = 4;
        ghdr.thiszone      = 0;
        ghdr.sigfigs       = 0;
        ghdr.snaplen       = 65535;
        ghdr.network       = 105; // IEEE 802.11

        assert(ghdr.magic_number == 0xA1B2C3D4);
        assert(ghdr.version_major == 2);
        assert(ghdr.version_minor == 4);
        assert(ghdr.network == 105);

        PcapRecordHeaderVerify rhdr;
        uint64_t ts_us = 1696000000123456ULL;
        rhdr.ts_sec  = static_cast<uint32_t>(ts_us / 1000000ULL);
        rhdr.ts_usec = static_cast<uint32_t>(ts_us % 1000000ULL);
        rhdr.incl_len = 64;
        rhdr.orig_len = 128;

        assert(rhdr.ts_sec == 1696000000U);
        assert(rhdr.ts_usec == 123456U);
        assert(rhdr.incl_len <= rhdr.orig_len);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 6: Protocol Decoding Invariants (802.11 & Ethernet II)
    // ─────────────────────────────────────────────────────────────────────────
    {
        // 802.11 Beacon frame fragment
        uint8_t beaconFrame[] = {
            0x80, 0x00,                         // Frame Control: Type 0 (Mgmt), Subtype 8 (Beacon)
            0x00, 0x00,                         // Duration: 0
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: Broadcast
            0x00, 0x1A, 0x79, 0x12, 0x34, 0x56, // Source: AP BSSID
            0x00, 0x1A, 0x79, 0x12, 0x34, 0x56  // BSSID
        };

        uint16_t fc = beaconFrame[0] | (beaconFrame[1] << 8);
        uint8_t type = (fc >> 2) & 0x03;
        uint8_t subtype = (fc >> 4) & 0x0F;
        assert(type == 0);    // Management frame
        assert(subtype == 8); // Beacon subtype

        // Ethernet II frame with IPv4 / UDP
        uint8_t ethFrame[] = {
            // Eth header (14 bytes)
            0x02, 0x00, 0x00, 0x00, 0x00, 0x01, // Dest MAC
            0x02, 0x00, 0x00, 0x00, 0x00, 0x02, // Src MAC
            0x08, 0x00,                         // EtherType: IPv4 (0x0800)
            // IPv4 header (20 bytes)
            0x45, 0x00, 0x00, 0x20,             // Ver=4, IHL=5, Total Len=32
            0x12, 0x34, 0x00, 0x00,             // ID, Flags/Fragment
            0x40, 0x11, 0x00, 0x00,             // TTL=64, Proto=17 (UDP), Header Checksum
            192, 168, 4, 1,                     // Src IP: 192.168.4.1 (ESP32 SoftAP)
            192, 168, 4, 2                      // Dst IP: 192.168.4.2
        };

        uint16_t etherType = (ethFrame[12] << 8) | ethFrame[13];
        assert(etherType == 0x0800); // IPv4
        uint8_t ipVer = (ethFrame[14] >> 4) & 0x0F;
        uint8_t ipIhl = (ethFrame[14] & 0x0F) * 4;
        uint8_t ipProto = ethFrame[23];
        assert(ipVer == 4);
        assert(ipIhl == 20);
        assert(ipProto == 17); // UDP
    }

    std::cout << "[PASS] All Packet & PCAP Protocol Tests passed!" << std::endl;
    return true;
}
