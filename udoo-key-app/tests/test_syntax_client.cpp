#include <iostream>
#include <cassert>
#include <cstring>
#include <QByteArray>
#include <QList>
#include "SyntaxClient.h"
#include "ITransport.h"
#include "instrument_protocol.h"

using namespace UdooKey;

class MockSyntaxTransport : public ITransport {
public:
    explicit MockSyntaxTransport(QObject* parent = nullptr) : ITransport(parent) {}

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
    QString portName() const override { return "MockSyntaxTransport"; }
    void setPortName(const QString&) override {}

    void injectData(const QByteArray& data) {
        emit dataReady(data);
    }

    QList<QByteArray> m_writtenPackets;
    bool m_open = true;
};

static void injectSyntaxResponse(MockSyntaxTransport& transport, uint16_t seq,
                                 uint8_t status, const QString& result) {
    QByteArray resUtf8 = result.toUtf8();
    uint16_t resLen = static_cast<uint16_t>(resUtf8.size());
    if (resLen > 512) resLen = 512;

    InstrumentBpSyntaxRespPayload payload;
    std::memset(&payload, 0, sizeof(payload));
    payload.status = status;
    payload.result_len = resLen;
    std::memcpy(payload.result, resUtf8.constData(), resLen);

    uint16_t payloadLen = sizeof(uint8_t) + sizeof(uint16_t) + resLen;

    InstrumentPacketHeader hdr;
    hdr.magic       = INSTRUMENT_PROTO_MAGIC;
    hdr.version     = INSTRUMENT_PROTO_VERSION;
    hdr.type        = PKT_TYPE_BP_SYNTAX_RESP;
    hdr.sequence    = seq;
    hdr.payload_len = payloadLen;
    hdr.crc16       = instrument_crc16(reinterpret_cast<const uint8_t*>(&payload), payloadLen);

    QByteArray raw;
    raw.resize(sizeof(hdr) + payloadLen);
    std::memcpy(raw.data(), &hdr, sizeof(hdr));
    std::memcpy(raw.data() + sizeof(hdr), &payload, payloadLen);

    transport.injectData(raw);
}

bool runSyntaxClientTests() {
    std::cout << "--- Running SyntaxClient (Phase 4 Interactive Syntax & Commands) Tests ---" << std::endl;

    MockSyntaxTransport transport;
    transport.open();
    SyntaxClient client(&transport);
    assert(client.isConnected());

    // ─────────────────────────────────────────────────────────────────────────
    // Test 1: Square-Bracket Bus Syntax Execution Framing
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        QString syntax = QStringLiteral("[ 0x50 0x00 ] [ 0x51 r:4 ]");
        bool ok = client.executeSyntax(syntax);
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());

        QByteArray tx = transport.m_writtenPackets.last();
        assert(tx.size() >= static_cast<int>(sizeof(InstrumentPacketHeader)));

        const auto* hdr = reinterpret_cast<const InstrumentPacketHeader*>(tx.constData());
        assert(hdr->magic == INSTRUMENT_PROTO_MAGIC);
        assert(hdr->version == INSTRUMENT_PROTO_VERSION);
        assert(hdr->type == PKT_TYPE_BP_SYNTAX_EXEC);

        const auto* payload = reinterpret_cast<const InstrumentBpSyntaxExecPayload*>(tx.constData() + sizeof(InstrumentPacketHeader));
        assert(std::string(payload->syntax) == "[ 0x50 0x00 ] [ 0x51 r:4 ]");

        // Verify response reception
        bool executedSignaled = false;
        uint8_t receivedStatus = 0xFF;
        QString receivedResult;
        auto conn = QObject::connect(&client, &SyntaxClient::syntaxExecuted, [&](uint8_t st, const QString& res) {
            executedSignaled = true;
            receivedStatus = st;
            receivedResult = res;
        });

        QString mockOutput = QStringLiteral("I2C START\r\nWRITE: 0x50\r\nWRITE: 0x00\r\nI2C STOP\r\nI2C START\r\nWRITE: 0x51\r\nREAD 4 BYTES: 0xAA 0x55 0x12 0x34\r\nI2C STOP");
        injectSyntaxResponse(transport, 1, 0, mockOutput);
        assert(executedSignaled);
        assert(receivedStatus == 0);
        assert(receivedResult == mockOutput);

        QObject::disconnect(conn);
        std::cout << "  [PASS] Square-bracket syntax execution and response parsing verified." << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 2: Macro Execution
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        bool ok = client.executeMacro(1); // Macro 1: I2C Scan
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());

        QByteArray tx = transport.m_writtenPackets.last();
        const auto* hdr = reinterpret_cast<const InstrumentPacketHeader*>(tx.constData());
        assert(hdr->type == PKT_TYPE_BP_SYNTAX_EXEC);

        const auto* payload = reinterpret_cast<const InstrumentBpSyntaxExecPayload*>(tx.constData() + sizeof(InstrumentPacketHeader));
        assert(std::string(payload->syntax) == "macro 1");

        bool macroSignaled = false;
        QString macroResult;
        auto conn = QObject::connect(&client, &SyntaxClient::syntaxExecuted, [&](uint8_t, const QString& res) {
            macroSignaled = true;
            macroResult = res;
        });

        QString mockMacroOutput = QStringLiteral("[EXEC MACRO 1: I2C Scan] -> 2 devices found: 0x3C 0x68");
        injectSyntaxResponse(transport, 2, 0, mockMacroOutput);
        assert(macroSignaled);
        assert(macroResult.contains("0x3C"));
        assert(macroResult.contains("0x68"));

        QObject::disconnect(conn);
        std::cout << "  [PASS] Macro execution (macro 1 -> I2C scan) verified." << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 3: Direct CLI Commands Execution
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        bool ok = client.executeCommand(QStringLiteral("v"));
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());

        QByteArray tx = transport.m_writtenPackets.last();
        const auto* hdr = reinterpret_cast<const InstrumentPacketHeader*>(tx.constData());
        assert(hdr->type == PKT_TYPE_BP_SYNTAX_EXEC);

        const auto* payload = reinterpret_cast<const InstrumentBpSyntaxExecPayload*>(tx.constData() + sizeof(InstrumentPacketHeader));
        assert(std::string(payload->syntax) == "v");

        bool cmdSignaled = false;
        QString cmdResult;
        auto conn = QObject::connect(&client, &SyntaxClient::syntaxExecuted, [&](uint8_t, const QString& res) {
            cmdSignaled = true;
            cmdResult = res;
        });

        QString mockVoltOutput = QStringLiteral("Voltages: ADC0(LDR)=1.65V ADC1=3.30V ADC2=0.00V");
        injectSyntaxResponse(transport, 3, 0, mockVoltOutput);
        assert(cmdSignaled);
        assert(cmdResult.contains("ADC0(LDR)=1.65V"));

        QObject::disconnect(conn);
        std::cout << "  [PASS] Direct CLI command execution ('v' -> ADC voltages) verified." << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 4: Error Handling
    // ─────────────────────────────────────────────────────────────────────────
    {
        bool errorSignaled = false;
        uint8_t errCode = 0;
        QString errReason;
        auto conn = QObject::connect(&client, &SyntaxClient::syntaxError, [&](uint8_t code, const QString& reason) {
            errorSignaled = true;
            errCode = code;
            errReason = reason;
        });

        // Simulate PKT_TYPE_ERROR
        QByteArray errPayload;
        errPayload.append(static_cast<char>(0x05));
        errPayload.append("Syntax error in command stream");

        InstrumentPacketHeader errHdr;
        errHdr.magic       = INSTRUMENT_PROTO_MAGIC;
        errHdr.version     = INSTRUMENT_PROTO_VERSION;
        errHdr.type        = PKT_TYPE_ERROR;
        errHdr.sequence    = 99;
        errHdr.payload_len = static_cast<uint16_t>(errPayload.size());
        errHdr.crc16       = instrument_crc16(reinterpret_cast<const uint8_t*>(errPayload.constData()), errPayload.size());

        QByteArray raw;
        raw.resize(sizeof(errHdr) + errPayload.size());
        std::memcpy(raw.data(), &errHdr, sizeof(errHdr));
        std::memcpy(raw.data() + sizeof(errHdr), errPayload.constData(), errPayload.size());

        transport.injectData(raw);

        assert(errorSignaled);
        assert(errCode == 0x05);
        assert(errReason == "Syntax error in command stream");

        QObject::disconnect(conn);
        std::cout << "  [PASS] Error packet reception and dispatch verified." << std::endl;
    }

    std::cout << "--- All SyntaxClient Tests PASSED ---" << std::endl;
    return true;
}
