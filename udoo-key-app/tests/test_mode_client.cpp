#include "ModeClient.h"
#include "MockTransport.h"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace UdooKey;

class MockModeTransport : public ITransport {
public:
    explicit MockModeTransport(QObject* parent = nullptr) : ITransport(parent) {}

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
    QString portName() const override { return "MockModeTransport"; }
    void setPortName(const QString&) override {}

    void injectData(const QByteArray& data) {
        emit dataReady(data);
    }

    QList<QByteArray> m_writtenPackets;
    bool m_open = true;
};

bool runModeClientTests() {
    std::cout << "--- Running Bus Pirate Mode Multiplexer & Invariants Tests ---" << std::endl;

    MockModeTransport transport;
    transport.open();

    ModeClient client(&transport);
    assert(client.isConnected());
    assert(client.currentMode() == INST_MODE_IDLE);
    assert(client.isSafeHiZVerified());

    // 1. Test Mode Set Framing (0x66)
    {
        std::cout << "  [SubTest] ModeClient::setMode (I2C @ 400kHz) framing & CRC..." << std::endl;
        transport.m_writtenPackets.clear();

        bool ok = client.setMode(INST_MODE_I2C, 400000);
        assert(ok);
        assert(transport.m_writtenPackets.size() == 1);

        const QByteArray& pkt = transport.m_writtenPackets.first();
        assert(pkt.size() == static_cast<int>(sizeof(InstrumentPacketHeader) + sizeof(InstrumentModeSetPayload)));

        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.magic == INSTRUMENT_PROTO_MAGIC);
        assert(hdr.version == INSTRUMENT_PROTO_VERSION);
        assert(hdr.type == PKT_TYPE_MODE_SET);
        assert(hdr.payload_len == sizeof(InstrumentModeSetPayload));

        InstrumentModeSetPayload payload;
        std::memcpy(&payload, pkt.constData() + sizeof(hdr), sizeof(payload));
        assert(payload.mode == INST_MODE_I2C);
        assert(payload.speed_hz == 400000);

        uint16_t expectedCrc = instrument_crc16(reinterpret_cast<const uint8_t*>(&payload), sizeof(payload));
        assert(hdr.crc16 == expectedCrc);
    }

    // 2. Test Release to Safe Hi-Z (0x66 to INST_MODE_IDLE)
    {
        std::cout << "  [SubTest] ModeClient::releaseToHiZ framing..." << std::endl;
        transport.m_writtenPackets.clear();

        bool ok = client.releaseToHiZ();
        assert(ok);
        assert(transport.m_writtenPackets.size() == 1);

        const QByteArray& pkt = transport.m_writtenPackets.first();
        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.type == PKT_TYPE_MODE_SET);

        InstrumentModeSetPayload payload;
        std::memcpy(&payload, pkt.constData() + sizeof(hdr), sizeof(payload));
        assert(payload.mode == INST_MODE_IDLE);
        assert(payload.speed_hz == 0);
    }

    // 3. Test Mode Status Request (0x67)
    {
        std::cout << "  [SubTest] ModeClient::requestStatus framing..." << std::endl;
        transport.m_writtenPackets.clear();

        bool ok = client.requestStatus();
        assert(ok);
        assert(transport.m_writtenPackets.size() == 1);

        const QByteArray& pkt = transport.m_writtenPackets.first();
        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.type == PKT_TYPE_MODE_STATUS);
        assert(hdr.payload_len == 0);
    }

    // 4. Test Simulated Mode Status Response Reception & Pin Mux Dispatching
    {
        std::cout << "  [SubTest] ModeClient response reception & pin assignment dispatching..." << std::endl;

        bool statusReceived = false;
        bool modeChangedReceived = false;
        InstrumentModeId changedNewMode = INST_MODE_IDLE;
        InstrumentModeId changedPrevMode = INST_MODE_IDLE;

        auto c1 = QObject::connect(&client, &ModeClient::modeStatusReceived, [&](const InstrumentModeStatusPayload&) {
            statusReceived = true;
        });

        auto c2 = QObject::connect(&client, &ModeClient::modeChanged, [&](InstrumentModeId newM, InstrumentModeId prevM, const QString&) {
            modeChangedReceived = true;
            changedNewMode = newM;
            changedPrevMode = prevM;
        });

        // Craft synthetic I2C Mode response
        InstrumentModeStatusPayload respPayload;
        std::memset(&respPayload, 0, sizeof(respPayload));
        respPayload.active_mode       = INST_MODE_I2C;
        respPayload.previous_mode     = INST_MODE_IDLE;
        respPayload.probe_spi_active  = 0;
        respPayload.safe_hiz_verified = 1;
        respPayload.active_speed_hz   = 400000;
        std::strncpy(respPayload.mode_name, "I2C", sizeof(respPayload.mode_name) - 1);
        respPayload.pin_functions[0]  = PIN_FN_I2C_SDA; // GP2
        respPayload.pin_functions[1]  = PIN_FN_I2C_SCL; // GP3
        for (int i = 2; i < 8; ++i) respPayload.pin_functions[i] = PIN_FN_DEFAULT;

        InstrumentPacketHeader respHdr;
        respHdr.magic       = INSTRUMENT_PROTO_MAGIC;
        respHdr.version     = INSTRUMENT_PROTO_VERSION;
        respHdr.type        = PKT_TYPE_MODE_STATUS;
        respHdr.sequence    = 15;
        respHdr.payload_len = sizeof(respPayload);
        respHdr.crc16       = instrument_crc16(reinterpret_cast<const uint8_t*>(&respPayload), sizeof(respPayload));

        QByteArray respPacket;
        respPacket.resize(sizeof(respHdr) + sizeof(respPayload));
        std::memcpy(respPacket.data(), &respHdr, sizeof(respHdr));
        std::memcpy(respPacket.data() + sizeof(respHdr), &respPayload, sizeof(respPayload));

        transport.injectData(respPacket);

        assert(statusReceived);
        assert(modeChangedReceived);
        assert(changedNewMode == INST_MODE_I2C);
        assert(changedPrevMode == INST_MODE_IDLE);
        assert(client.currentMode() == INST_MODE_I2C);
        assert(client.currentModeName() == "I2C");
        assert(client.activeSpeedHz() == 400000);
        assert(!client.isProbeSpiActive());
        assert(client.isSafeHiZVerified());
        assert(client.pinFunction(2) == PIN_FN_I2C_SDA);
        assert(client.pinFunction(3) == PIN_FN_I2C_SCL);
        assert(client.pinFunction(4) == PIN_FN_DEFAULT);
        assert(client.pinFunctionDescription(2) == "I2C SDA");
        assert(client.pinFunctionDescription(3) == "I2C SCL");
        assert(client.pinFunctionDescription(4) == "Safe Hi-Z Input");
        QObject::disconnect(c1);
        QObject::disconnect(c2);
    }

    // 5. Test Probe SPI Arbiter & Active Probe Bus State
    {
        std::cout << "  [SubTest] ModeClient SPI Probe Arbiter status & pin functions..." << std::endl;

        InstrumentModeStatusPayload spiPayload;
        std::memset(&spiPayload, 0, sizeof(spiPayload));
        spiPayload.active_mode       = INST_MODE_SPI;
        spiPayload.previous_mode     = INST_MODE_I2C;
        spiPayload.probe_spi_active  = 1; // Probe SPI owns GP2..GP5!
        spiPayload.safe_hiz_verified = 1;
        spiPayload.active_speed_hz   = 10000000;
        std::strncpy(spiPayload.mode_name, "SPI", sizeof(spiPayload.mode_name) - 1);
        spiPayload.pin_functions[0]  = PIN_FN_SPI_SCK;  // GP2
        spiPayload.pin_functions[1]  = PIN_FN_SPI_MOSI; // GP3
        spiPayload.pin_functions[2]  = PIN_FN_SPI_MISO; // GP4
        spiPayload.pin_functions[3]  = PIN_FN_SPI_CS;   // GP5
        for (int i = 4; i < 8; ++i) spiPayload.pin_functions[i] = PIN_FN_DEFAULT;

        InstrumentPacketHeader respHdr;
        respHdr.magic       = INSTRUMENT_PROTO_MAGIC;
        respHdr.version     = INSTRUMENT_PROTO_VERSION;
        respHdr.type        = PKT_TYPE_MODE_STATUS;
        respHdr.sequence    = 16;
        respHdr.payload_len = sizeof(spiPayload);
        respHdr.crc16       = instrument_crc16(reinterpret_cast<const uint8_t*>(&spiPayload), sizeof(spiPayload));

        QByteArray respPacket;
        respPacket.resize(sizeof(respHdr) + sizeof(spiPayload));
        std::memcpy(respPacket.data(), &respHdr, sizeof(respHdr));
        std::memcpy(respPacket.data() + sizeof(respHdr), &spiPayload, sizeof(spiPayload));

        transport.injectData(respPacket);

        assert(client.currentMode() == INST_MODE_SPI);
        assert(client.isProbeSpiActive() == true);
        assert(client.pinFunction(2) == PIN_FN_SPI_SCK);
        assert(client.pinFunction(3) == PIN_FN_SPI_MOSI);
        assert(client.pinFunction(4) == PIN_FN_SPI_MISO);
        assert(client.pinFunction(5) == PIN_FN_SPI_CS);
        assert(client.pinFunctionDescription(2) == "SPI SCK");
        assert(client.pinFunctionDescription(5) == "SPI CS#");
    }

    // 6. Test Error Handling (Acquisition Busy / Conflict)
    {
        std::cout << "  [SubTest] ModeClient error packet reception (Acquisition running)..." << std::endl;

        bool errorReceived = false;
        uint8_t errCode = 0;
        QString errText;
        auto c3 = QObject::connect(&client, &ModeClient::modeErrorReceived, [&](uint8_t code, const QString& reason) {
            errorReceived = true;
            errCode = code;
            errText = reason;
        });

        InstrumentErrorPayload errPayload;
        errPayload.error_code = INST_ERR_BUSY;
        std::strncpy(errPayload.message, "Cannot switch mode: Acquisition is running", sizeof(errPayload.message));

        InstrumentPacketHeader errHdr;
        errHdr.magic       = INSTRUMENT_PROTO_MAGIC;
        errHdr.version     = INSTRUMENT_PROTO_VERSION;
        errHdr.type        = PKT_TYPE_ERROR;
        errHdr.sequence    = 17;
        errHdr.payload_len = sizeof(errPayload);
        errHdr.crc16       = instrument_crc16(reinterpret_cast<const uint8_t*>(&errPayload), sizeof(errPayload));

        QByteArray errPacket;
        errPacket.resize(sizeof(errHdr) + sizeof(errPayload));
        std::memcpy(errPacket.data(), &errHdr, sizeof(errHdr));
        std::memcpy(errPacket.data() + sizeof(errHdr), &errPayload, sizeof(errPayload));

        transport.injectData(errPacket);

        assert(errorReceived);
        assert(errCode == INST_ERR_BUSY);
        assert(errText.contains("Acquisition is running"));
        QObject::disconnect(c3);
    }

    // 7. Test Stream Chunking & Noise Tolerance
    {
        std::cout << "  [SubTest] ModeClient stream chunking & noise tolerance..." << std::endl;

        InstrumentModeStatusPayload uartPayload;
        std::memset(&uartPayload, 0, sizeof(uartPayload));
        uartPayload.active_mode       = INST_MODE_UART;
        uartPayload.previous_mode     = INST_MODE_SPI;
        uartPayload.probe_spi_active  = 0;
        uartPayload.safe_hiz_verified = 1;
        uartPayload.active_speed_hz   = 115200;
        std::strncpy(uartPayload.mode_name, "UART", sizeof(uartPayload.mode_name) - 1);
        uartPayload.pin_functions[2]  = PIN_FN_UART_TX; // GP4
        uartPayload.pin_functions[3]  = PIN_FN_UART_RX; // GP5

        InstrumentPacketHeader hdr;
        hdr.magic       = INSTRUMENT_PROTO_MAGIC;
        hdr.version     = INSTRUMENT_PROTO_VERSION;
        hdr.type        = PKT_TYPE_MODE_STATUS;
        hdr.sequence    = 18;
        hdr.payload_len = sizeof(uartPayload);
        hdr.crc16       = instrument_crc16(reinterpret_cast<const uint8_t*>(&uartPayload), sizeof(uartPayload));

        QByteArray frame;
        frame.resize(sizeof(hdr) + sizeof(uartPayload));
        std::memcpy(frame.data(), &hdr, sizeof(hdr));
        std::memcpy(frame.data() + sizeof(hdr), &uartPayload, sizeof(uartPayload));

        QByteArray noisyData = "\x00\xFF\x55\x12" + frame;
        transport.injectData(noisyData.left(8));
        transport.injectData(noisyData.mid(8, 10));
        transport.injectData(noisyData.mid(18));

        assert(client.currentMode() == INST_MODE_UART);
        assert(client.activeSpeedHz() == 115200);
        assert(client.pinFunction(4) == PIN_FN_UART_TX);
        assert(client.pinFunction(5) == PIN_FN_UART_RX);
    }

    std::cout << "  -> PASSED" << std::endl;
    return true;
}
