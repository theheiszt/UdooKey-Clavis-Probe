#include <iostream>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <QByteArray>
#include "instrument_protocol.h"
#include "ITransport.h"

namespace {

class MockSelfTestTransport : public UdooKey::ITransport {
public:
    explicit MockSelfTestTransport(QObject* parent = nullptr) : UdooKey::ITransport(parent) {}

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
    QString portName() const override { return "MockSelfTestTransport"; }
    void setPortName(const QString&) override {}

    void injectData(const QByteArray& data) {
        emit dataReady(data);
    }

    QList<QByteArray> m_writtenPackets;
    bool m_open = true;
};

// Helper to construct framed InstrumentProtocol packet
static QByteArray buildInstrumentPacket(uint8_t type, uint16_t seq, const void* payload, uint16_t plen) {
    InstrumentPacketHeader hdr;
    hdr.magic       = INSTRUMENT_PROTO_MAGIC;
    hdr.version     = INSTRUMENT_PROTO_VERSION;
    hdr.type        = type;
    hdr.sequence    = seq;
    hdr.payload_len = plen;
    hdr.crc16       = (plen && payload) ? instrument_crc16(reinterpret_cast<const uint8_t*>(payload), plen) : 0;

    QByteArray raw;
    raw.resize(sizeof(hdr) + plen);
    std::memcpy(raw.data(), &hdr, sizeof(hdr));
    if (plen && payload) {
        std::memcpy(raw.data() + sizeof(hdr), payload, plen);
    }
    return raw;
}

} // anonymous namespace

bool runSelfTestTests() {
    std::cout << "--- Running Hardware Diagnostic & Self-Test Suite Tests ---" << std::endl;

    // ─────────────────────────────────────────────────────────────────────────
    // Test 1: Payload Sizing & Struct Invariants
    // ─────────────────────────────────────────────────────────────────────────
    {
        assert(sizeof(InstrumentPacketHeader) == 10);
        assert(sizeof(InstrumentSelfTestPayload) == (9 * sizeof(uint8_t) + 128));
        assert(INSTRUMENT_PROTO_MAGIC == 0xAA55);
        assert(INSTRUMENT_PROTO_VERSION == 0x01);
        assert(PKT_TYPE_SELF_TEST_REQ == 0x20);
        assert(PKT_TYPE_SELF_TEST_RESP == 0x21);
        std::cout << "  [PASS] Test 1: InstrumentSelfTestPayload layout and protocol constants verified" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 2: Framing of PKT_TYPE_SELF_TEST_REQ
    // ─────────────────────────────────────────────────────────────────────────
    {
        MockSelfTestTransport transport;
        transport.open();

        QByteArray reqPkt = buildInstrumentPacket(PKT_TYPE_SELF_TEST_REQ, 101, nullptr, 0);
        assert(reqPkt.size() == sizeof(InstrumentPacketHeader));

        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, reqPkt.constData(), sizeof(hdr));
        assert(hdr.magic == INSTRUMENT_PROTO_MAGIC);
        assert(hdr.version == INSTRUMENT_PROTO_VERSION);
        assert(hdr.type == PKT_TYPE_SELF_TEST_REQ);
        assert(hdr.sequence == 101);
        assert(hdr.payload_len == 0);
        assert(hdr.crc16 == 0);

        transport.write(reqPkt);
        assert(transport.m_writtenPackets.size() == 1);
        std::cout << "  [PASS] Test 2: Hardware Self-Test request framing verified" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 3: Framing & Verification of PKT_TYPE_SELF_TEST_RESP (All Passing)
    // ─────────────────────────────────────────────────────────────────────────
    {
        InstrumentSelfTestPayload st;
        std::memset(&st, 0, sizeof(st));
        st.gpio_ok    = 1;
        st.uart_ok    = 1;
        st.spi_ok     = 1;
        st.adc_ok     = 1;
        st.buttons_ok = 1;
        st.display_ok = 1;
        st.pio_ok     = 1;
        st.dma_ok     = 1;
        st.trigger_ok = 1;
        const char* msg = "All RP2040 subsystems operational. PIO SM0..SM3 OK. DMA ch0..ch3 OK. ADC calibrated.";
        std::strncpy(st.details, msg, sizeof(st.details) - 1);

        QByteArray respPkt = buildInstrumentPacket(PKT_TYPE_SELF_TEST_RESP, 102, &st, sizeof(st));
        assert(respPkt.size() == sizeof(InstrumentPacketHeader) + sizeof(InstrumentSelfTestPayload));

        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, respPkt.constData(), sizeof(hdr));
        assert(hdr.magic == INSTRUMENT_PROTO_MAGIC);
        assert(hdr.type == PKT_TYPE_SELF_TEST_RESP);
        assert(hdr.sequence == 102);
        assert(hdr.payload_len == sizeof(InstrumentSelfTestPayload));

        // Validate CCITT-FALSE CRC16
        uint16_t expectedCrc = instrument_crc16(reinterpret_cast<const uint8_t*>(&st), sizeof(st));
        assert(hdr.crc16 == expectedCrc);

        // Parse payload
        InstrumentSelfTestPayload parsed;
        std::memcpy(&parsed, respPkt.constData() + sizeof(hdr), sizeof(parsed));
        assert(parsed.gpio_ok == 1);
        assert(parsed.uart_ok == 1);
        assert(parsed.spi_ok == 1);
        assert(parsed.adc_ok == 1);
        assert(parsed.buttons_ok == 1);
        assert(parsed.display_ok == 1);
        assert(parsed.pio_ok == 1);
        assert(parsed.dma_ok == 1);
        assert(parsed.trigger_ok == 1);
        assert(std::strcmp(parsed.details, msg) == 0);

        std::cout << "  [PASS] Test 3: Hardware Self-Test PASS response framing & CCITT CRC16 verified" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 4: Fault Detection (Subsystem Failure Isolation)
    // ─────────────────────────────────────────────────────────────────────────
    {
        InstrumentSelfTestPayload stFault;
        std::memset(&stFault, 0, sizeof(stFault));
        stFault.gpio_ok    = 1;
        stFault.uart_ok    = 1;
        stFault.spi_ok     = 1;
        stFault.adc_ok     = 0; // Simulated ADC rail out of range (>3.3V)
        stFault.buttons_ok = 1;
        stFault.display_ok = 1;
        stFault.pio_ok     = 0; // Simulated PIO FIFO stall
        stFault.dma_ok     = 1;
        stFault.trigger_ok = 1;
        std::strncpy(stFault.details, "FAULT: ADC CH0 rail over-voltage, PIO SM1 FIFO overrun", sizeof(stFault.details) - 1);

        QByteArray respFault = buildInstrumentPacket(PKT_TYPE_SELF_TEST_RESP, 103, &stFault, sizeof(stFault));

        InstrumentSelfTestPayload parsedFault;
        std::memcpy(&parsedFault, respFault.constData() + sizeof(InstrumentPacketHeader), sizeof(parsedFault));
        assert(parsedFault.gpio_ok == 1);
        assert(parsedFault.adc_ok == 0);
        assert(parsedFault.pio_ok == 0);
        assert(std::string(parsedFault.details).find("FAULT") != std::string::npos);

        std::cout << "  [PASS] Test 4: Subsystem Fault detection and error isolation verified" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 5: Packet Corruption & Tamper Detection
    // ─────────────────────────────────────────────────────────────────────────
    {
        InstrumentSelfTestPayload st;
        std::memset(&st, 0, sizeof(st));
        st.gpio_ok = 1;
        st.pio_ok  = 1;

        QByteArray validPkt = buildInstrumentPacket(PKT_TYPE_SELF_TEST_RESP, 104, &st, sizeof(st));
        
        // Corrupt a byte in the payload
        QByteArray corruptedPkt = validPkt;
        corruptedPkt[sizeof(InstrumentPacketHeader) + 5] ^= 0xFF;

        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, corruptedPkt.constData(), sizeof(hdr));

        uint16_t computedCrc = instrument_crc16(
            reinterpret_cast<const uint8_t*>(corruptedPkt.constData() + sizeof(hdr)),
            hdr.payload_len);

        assert(computedCrc != hdr.crc16); // Must fail CRC check
        std::cout << "  [PASS] Test 5: Packet payload tamper & CRC corruption detected" << std::endl;
    }

    std::cout << "All Hardware Diagnostic & Self-Test tests passed successfully!" << std::endl;
    return true;
}
