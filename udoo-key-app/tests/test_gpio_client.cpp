#include "GpioClient.h"
#include "MockTransport.h"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace UdooKey;

class MockGpioTransport : public ITransport {
public:
    explicit MockGpioTransport(QObject* parent = nullptr) : ITransport(parent) {}

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
    QString portName() const override { return "MockGpioTransport"; }
    void setPortName(const QString&) override {}

    void injectData(const QByteArray& data) {
        emit dataReady(data);
    }

    QList<QByteArray> m_writtenPackets;
    bool m_open = true;
};

bool runGpioClientTests() {
    std::cout << "--- Running Bus Pirate GPIO Service Tests ---" << std::endl;

    MockGpioTransport transport;
    transport.open();

    GpioClient client(&transport);
    assert(client.isConnected());

    // 1. Test Pin Configuration Request Framing (0x60)
    {
        std::cout << "  [SubTest] GpioClient::configurePin framing & CRC..." << std::endl;
        transport.m_writtenPackets.clear();

        bool ok = client.configurePin(2, true, true, GPIO_PULL_UP);
        assert(ok);
        assert(transport.m_writtenPackets.size() == 1);

        const QByteArray& pkt = transport.m_writtenPackets.first();
        assert(pkt.size() == static_cast<int>(sizeof(InstrumentPacketHeader) + sizeof(InstrumentGpioConfigPayload)));

        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.magic == INSTRUMENT_PROTO_MAGIC);
        assert(hdr.version == INSTRUMENT_PROTO_VERSION);
        assert(hdr.type == PKT_TYPE_GPIO_CONFIG);
        assert(hdr.payload_len == sizeof(InstrumentGpioConfigPayload));

        InstrumentGpioConfigPayload payload;
        std::memcpy(&payload, pkt.constData() + sizeof(hdr), sizeof(payload));
        assert(payload.pin_or_idx == 2);
        assert(payload.is_output == 1);
        assert(payload.initial_level == 1);
        assert(payload.pull_mode == GPIO_PULL_UP);

        uint16_t expectedCrc = instrument_crc16(reinterpret_cast<const uint8_t*>(&payload), sizeof(payload));
        assert(hdr.crc16 == expectedCrc);
    }

    // 2. Test Pin Write Request Framing (0x62)
    {
        std::cout << "  [SubTest] GpioClient::writePin framing & CRC..." << std::endl;
        transport.m_writtenPackets.clear();

        bool ok = client.writePin(3, false);
        assert(ok);
        assert(transport.m_writtenPackets.size() == 1);

        const QByteArray& pkt = transport.m_writtenPackets.first();
        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.type == PKT_TYPE_GPIO_WRITE);

        InstrumentGpioWritePayload payload;
        std::memcpy(&payload, pkt.constData() + sizeof(hdr), sizeof(payload));
        assert(payload.pin_or_idx == 3);
        assert(payload.level == 0);
    }

    // 3. Test Pin Read Request Framing (0x61)
    {
        std::cout << "  [SubTest] GpioClient::readPin framing..." << std::endl;
        transport.m_writtenPackets.clear();

        bool ok = client.readPin(4);
        assert(ok);
        assert(transport.m_writtenPackets.size() == 1);

        const QByteArray& pkt = transport.m_writtenPackets.first();
        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.type == PKT_TYPE_GPIO_READ);

        InstrumentGpioReadPayload payload;
        std::memcpy(&payload, pkt.constData() + sizeof(hdr), sizeof(payload));
        assert(payload.pin_or_idx == 4);
    }

    // 4. Test Multi-pin Write Mask (0x64) and Read All (0x65)
    {
        std::cout << "  [SubTest] GpioClient::writeMask & readAll framing..." << std::endl;
        transport.m_writtenPackets.clear();

        assert(client.writeMask(0x55, 0x0F));
        assert(transport.m_writtenPackets.size() == 1);

        const QByteArray& pkt1 = transport.m_writtenPackets.first();
        InstrumentPacketHeader hdr1;
        std::memcpy(&hdr1, pkt1.constData(), sizeof(hdr1));
        assert(hdr1.type == PKT_TYPE_GPIO_WRITE_MASK);

        InstrumentGpioWriteMaskPayload maskPayload;
        std::memcpy(&maskPayload, pkt1.constData() + sizeof(hdr1), sizeof(maskPayload));
        assert(maskPayload.dir_mask == 0x55);
        assert(maskPayload.value_mask == 0x0F);

        transport.m_writtenPackets.clear();
        assert(client.readAll());
        assert(transport.m_writtenPackets.size() == 1);
        InstrumentPacketHeader hdr2;
        std::memcpy(&hdr2, transport.m_writtenPackets.first().constData(), sizeof(hdr2));
        assert(hdr2.type == PKT_TYPE_GPIO_READ_ALL);
        assert(hdr2.payload_len == 0);
    }

    // 5. Test Simulated Response Processing (0x63 Status, 0x65 ReadAll, 0xFF Error)
    {
        std::cout << "  [SubTest] GpioClient response reception and signal dispatching..." << std::endl;

        bool statusReceived = false;
        InstrumentGpioStatusPayload lastStatus{};
        auto c1 = QObject::connect(&client, &GpioClient::pinStatusReceived, [&](const InstrumentGpioStatusPayload& st) {
            statusReceived = true;
            lastStatus = st;
        });

        // Craft synthetic PKT_TYPE_GPIO_STATUS frame
        InstrumentGpioStatusPayload devStatus;
        devStatus.pin_or_idx     = 2;
        devStatus.gpio_num       = 2;
        devStatus.is_output      = 1;
        devStatus.logic_level    = 1;
        devStatus.pull_mode      = GPIO_PULL_UP;
        devStatus.all_pins_state = 0x85; // 10000101b

        InstrumentPacketHeader respHdr;
        respHdr.magic       = INSTRUMENT_PROTO_MAGIC;
        respHdr.version     = INSTRUMENT_PROTO_VERSION;
        respHdr.type        = PKT_TYPE_GPIO_STATUS;
        respHdr.sequence    = 10;
        respHdr.payload_len = sizeof(devStatus);
        respHdr.crc16       = instrument_crc16(reinterpret_cast<const uint8_t*>(&devStatus), sizeof(devStatus));

        QByteArray respPacket;
        respPacket.resize(sizeof(respHdr) + sizeof(devStatus));
        std::memcpy(respPacket.data(), &respHdr, sizeof(respHdr));
        std::memcpy(respPacket.data() + sizeof(respHdr), &devStatus, sizeof(devStatus));

        transport.injectData(respPacket);

        assert(statusReceived);
        assert(lastStatus.pin_or_idx == 2);
        assert(lastStatus.logic_level == 1);
        assert(lastStatus.all_pins_state == 0x85);
        assert(client.lastPinValues() == 0x85);
        assert((client.lastPinDirections() & (1 << 0)) != 0);
        QObject::disconnect(c1);

        // Test PKT_TYPE_GPIO_READ_ALL reception
        bool readAllReceived = false;
        InstrumentGpioReadAllPayload lastReadAll{};
        auto c2 = QObject::connect(&client, &GpioClient::allPinsReceived, [&](const InstrumentGpioReadAllPayload& all) {
            readAllReceived = true;
            lastReadAll = all;
        });

        InstrumentGpioReadAllPayload allPayload;
        allPayload.values_mask     = 0xAA;
        allPayload.directions_mask = 0xF0;

        respHdr.type        = PKT_TYPE_GPIO_READ_ALL;
        respHdr.payload_len = sizeof(allPayload);
        respHdr.crc16       = instrument_crc16(reinterpret_cast<const uint8_t*>(&allPayload), sizeof(allPayload));

        respPacket.resize(sizeof(respHdr) + sizeof(allPayload));
        std::memcpy(respPacket.data(), &respHdr, sizeof(respHdr));
        std::memcpy(respPacket.data() + sizeof(respHdr), &allPayload, sizeof(allPayload));

        transport.injectData(respPacket);
        assert(readAllReceived);
        assert(lastReadAll.values_mask == 0xAA);
        assert(lastReadAll.directions_mask == 0xF0);
        assert(client.lastPinValues() == 0xAA);
        assert(client.lastPinDirections() == 0xF0);
        QObject::disconnect(c2);

        // Test Error Packet reception
        bool errorReceived = false;
        uint8_t errCode = 0;
        QString errMsg;
        auto c3 = QObject::connect(&client, &GpioClient::errorReceived, [&](uint8_t code, const QString& msg) {
            errorReceived = true;
            errCode = code;
            errMsg = msg;
        });

        InstrumentErrorPayload errPayload;
        errPayload.error_code = INST_ERR_INVALID_PARAM;
        std::strncpy(errPayload.message, "Pin 10 is reserved for EXT_TRIG", sizeof(errPayload.message));

        respHdr.type        = PKT_TYPE_ERROR;
        respHdr.payload_len = sizeof(errPayload);
        respHdr.crc16       = instrument_crc16(reinterpret_cast<const uint8_t*>(&errPayload), sizeof(errPayload));

        respPacket.resize(sizeof(respHdr) + sizeof(errPayload));
        std::memcpy(respPacket.data(), &respHdr, sizeof(respHdr));
        std::memcpy(respPacket.data() + sizeof(respHdr), &errPayload, sizeof(errPayload));

        transport.injectData(respPacket);
        assert(errorReceived);
        assert(errCode == INST_ERR_INVALID_PARAM);
        assert(errMsg.contains("Pin 10 is reserved"));
        QObject::disconnect(c3);
    }

    // 6. Test Stream Fragmentation and Corrupt Frame Rejection
    {
        std::cout << "  [SubTest] GpioClient stream fragmentation & corruption resistance..." << std::endl;

        InstrumentGpioReadAllPayload allPayload;
        allPayload.values_mask     = 0x33;
        allPayload.directions_mask = 0xCC;

        InstrumentPacketHeader respHdr;
        respHdr.magic       = INSTRUMENT_PROTO_MAGIC;
        respHdr.version     = INSTRUMENT_PROTO_VERSION;
        respHdr.type        = PKT_TYPE_GPIO_READ_ALL;
        respHdr.sequence    = 99;
        respHdr.payload_len = sizeof(allPayload);
        respHdr.crc16       = instrument_crc16(reinterpret_cast<const uint8_t*>(&allPayload), sizeof(allPayload));

        QByteArray fullFrame;
        fullFrame.resize(sizeof(respHdr) + sizeof(allPayload));
        std::memcpy(fullFrame.data(), &respHdr, sizeof(respHdr));
        std::memcpy(fullFrame.data() + sizeof(respHdr), &allPayload, sizeof(allPayload));

        // Inject 5 bytes of garbage before the valid frame
        QByteArray noisyStream = "\xFF\xAA\x00\x12\x34" + fullFrame;

        // Split noisyStream into 3 separate chunks
        QByteArray chunk1 = noisyStream.left(7);
        QByteArray chunk2 = noisyStream.mid(7, 5);
        QByteArray chunk3 = noisyStream.mid(12);

        transport.injectData(chunk1);
        transport.injectData(chunk2);
        transport.injectData(chunk3);

        assert(client.lastPinValues() == 0x33);
        assert(client.lastPinDirections() == 0xCC);

        // Corrupt CRC test
        respHdr.crc16 ^= 0x55AA;
        std::memcpy(fullFrame.data(), &respHdr, sizeof(respHdr));
        client.writeMask(0, 0); // Reset cached state
        transport.injectData(fullFrame); // Should be dropped due to bad CRC
        assert(client.lastPinValues() == 0x33); // Unchanged from valid packet
    }

    std::cout << "  -> PASSED" << std::endl;
    return true;
}
