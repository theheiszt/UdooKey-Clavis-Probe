#include <iostream>
#include <cassert>
#include <cstring>
#include <QByteArray>
#include <QVector>
#include <QList>
#include "BusToolsClient.h"
#include "ITransport.h"
#include "instrument_protocol.h"

using namespace UdooKey;

class MockBusToolsTransport : public ITransport {
public:
    explicit MockBusToolsTransport(QObject* parent = nullptr) : ITransport(parent) {}

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
    QString portName() const override { return "MockBusToolsTransport"; }
    void setPortName(const QString&) override {}

    void injectData(const QByteArray& data) {
        emit dataReady(data);
    }

    QList<QByteArray> m_writtenPackets;
    bool m_open = true;
};

// Helper to simulate incoming packet from device
static void injectDevicePacket(MockBusToolsTransport& transport, InstrumentPacketType type, uint16_t seq,
                               const void* payload, uint16_t payloadLen) {
    InstrumentPacketHeader hdr;
    hdr.magic       = INSTRUMENT_PROTO_MAGIC;
    hdr.version     = INSTRUMENT_PROTO_VERSION;
    hdr.type        = static_cast<uint8_t>(type);
    hdr.sequence    = seq;
    hdr.payload_len = payloadLen;
    hdr.crc16       = (payload && payloadLen > 0) ? instrument_crc16(reinterpret_cast<const uint8_t*>(payload), payloadLen) : 0;

    QByteArray raw;
    raw.resize(sizeof(hdr) + payloadLen);
    std::memcpy(raw.data(), &hdr, sizeof(hdr));
    if (payload && payloadLen > 0) {
        std::memcpy(raw.data() + sizeof(hdr), payload, payloadLen);
    }

    transport.injectData(raw);
}

bool runBusToolsClientTests() {
    std::cout << "--- Running BusToolsClient (Phase 3 Hardware Protocol Engines) Tests ---" << std::endl;

    MockBusToolsTransport transport;
    transport.open();
    BusToolsClient client(&transport);

    // ─────────────────────────────────────────────────────────────────────────
    // Test 1: UART Bridge Operations
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        bool ok = client.configureUart(115200, 8, 0, 1, false);
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());
        QByteArray tx = transport.m_writtenPackets.last();
        assert(tx.size() == sizeof(InstrumentPacketHeader) + sizeof(InstrumentUartConfigPayload));
        const auto* hdr = reinterpret_cast<const InstrumentPacketHeader*>(tx.constData());
        assert(hdr->magic == INSTRUMENT_PROTO_MAGIC);
        assert(hdr->type == PKT_TYPE_UART_CONFIG);

        const auto* cfg = reinterpret_cast<const InstrumentUartConfigPayload*>(tx.constData() + sizeof(InstrumentPacketHeader));
        assert(cfg->baudrate == 115200);
        assert(cfg->data_bits == 8);

        // Test sending UART data
        transport.m_writtenPackets.clear();
        QByteArray testData = "AT+GMR\r\n";
        ok = client.writeUart(testData);
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());
        tx = transport.m_writtenPackets.last();
        const auto* dataHdr = reinterpret_cast<const InstrumentPacketHeader*>(tx.constData());
        assert(dataHdr->type == PKT_TYPE_UART_DATA);

        // Test receiving UART data
        bool uartRxSignaled = false;
        QByteArray receivedUart;
        QObject::connect(&client, &BusToolsClient::uartDataReceived, [&](const QByteArray& data) {
            uartRxSignaled = true;
            receivedUart = data;
        });

        InstrumentUartDataPayload rxPayload;
        rxPayload.len = 4;
        std::memcpy(rxPayload.data, "OK\r\n", 4);
        injectDevicePacket(transport, PKT_TYPE_UART_DATA, 10, &rxPayload, sizeof(uint16_t) + 4);
        assert(uartRxSignaled);
        assert(receivedUart == "OK\r\n");

        // Test UART status query & reception
        bool statusSignaled = false;
        InstrumentUartStatusPayload receivedStatus{};
        QObject::connect(&client, &BusToolsClient::uartStatusReceived, [&](const InstrumentUartStatusPayload& st) {
            statusSignaled = true;
            receivedStatus = st;
        });

        InstrumentUartStatusPayload mockStatus;
        mockStatus.actual_baud = 115200;
        mockStatus.data_bits   = 8;
        mockStatus.parity      = 0;
        mockStatus.stop_bits   = 1;
        mockStatus.rx_buffered = 42;
        injectDevicePacket(transport, PKT_TYPE_UART_STATUS, 11, &mockStatus, sizeof(mockStatus));
        assert(statusSignaled);
        assert(receivedStatus.actual_baud == 115200);
        assert(receivedStatus.rx_buffered == 42);

        std::cout << "  [PASS] UART Bridge: config, transmit, receive, status verified." << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 2: I2C Engine Operations & Scanner
    // ─────────────────────────────────────────────────────────────────────────
    {
        // Test I2C config
        transport.m_writtenPackets.clear();
        bool ok = client.configureI2c(400000, true);
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());
        QByteArray tx = transport.m_writtenPackets.last();
        const auto* hdr = reinterpret_cast<const InstrumentPacketHeader*>(tx.constData());
        assert(hdr->type == PKT_TYPE_I2C_CONFIG);
        const auto* cfg = reinterpret_cast<const InstrumentI2cConfigPayload*>(tx.constData() + sizeof(InstrumentPacketHeader));
        assert(cfg->speed_hz == 400000);
        assert(cfg->enable_pullups == 1);

        // Test I2C scan request & response
        transport.m_writtenPackets.clear();
        ok = client.scanI2c();
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());
        tx = transport.m_writtenPackets.last();
        hdr = reinterpret_cast<const InstrumentPacketHeader*>(tx.constData());
        assert(hdr->type == PKT_TYPE_I2C_SCAN_REQ);

        bool scanCompleted = false;
        QVector<uint8_t> foundAddrs;
        QObject::connect(&client, &BusToolsClient::i2cScanCompleted, [&](const QVector<uint8_t>& addrs) {
            scanCompleted = true;
            foundAddrs = addrs;
        });

        InstrumentI2cScanRespPayload scanResp;
        scanResp.device_count = 2;
        scanResp.addresses[0] = 0x3C; // SSD1306 OLED
        scanResp.addresses[1] = 0x68; // MPU-6050 IMU
        injectDevicePacket(transport, PKT_TYPE_I2C_SCAN_RESP, 20, &scanResp, sizeof(uint8_t) + 2);
        assert(scanCompleted);
        assert(foundAddrs.size() == 2);
        assert(foundAddrs[0] == 0x3C);
        assert(foundAddrs[1] == 0x68);

        // Test I2C write-then-read transfer
        transport.m_writtenPackets.clear();
        QByteArray writeBytes = QByteArray::fromHex("75"); // WHO_AM_I register
        ok = client.transferI2c(0x68, writeBytes, 1, true);
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());
        tx = transport.m_writtenPackets.last();
        hdr = reinterpret_cast<const InstrumentPacketHeader*>(tx.constData());
        assert(hdr->type == PKT_TYPE_I2C_XFER);
        const auto* xfer = reinterpret_cast<const InstrumentI2cXferPayload*>(tx.constData() + sizeof(InstrumentPacketHeader));
        assert(xfer->addr_7bit == 0x68);
        assert(xfer->write_len == 1);
        assert(xfer->read_len == 1);

        bool xferCompleted = false;
        uint8_t xferStatus = 0xFF;
        QByteArray readResult;
        QObject::connect(&client, &BusToolsClient::i2cTransferCompleted, [&](uint8_t st, const QByteArray& data) {
            xferCompleted = true;
            xferStatus = st;
            readResult = data;
        });

        InstrumentI2cXferPayload xferResp;
        xferResp.addr_7bit = 0x68;
        xferResp.send_stop = 1;
        xferResp.write_len = 1;
        xferResp.read_len  = 1;
        xferResp.status    = 0; // Success
        xferResp.data[0]   = 0x68; // MPU-6050 WHO_AM_I response
        injectDevicePacket(transport, PKT_TYPE_I2C_XFER, 21, &xferResp, sizeof(InstrumentI2cXferPayload) - 256 + 1);
        assert(xferCompleted);
        assert(xferStatus == 0);
        assert(readResult.size() == 1);
        assert(static_cast<uint8_t>(readResult[0]) == 0x68);

        std::cout << "  [PASS] I2C Engine: config, bus scan (0x3C, 0x68), and write-then-read transfer verified." << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 3: SPI Master Engine & JEDEC SFDP Flash Probe
    // ─────────────────────────────────────────────────────────────────────────
    {
        // Test SPI config
        transport.m_writtenPackets.clear();
        bool ok = client.configureSpi(10000000, 0, 0, false, 5);
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());
        QByteArray tx = transport.m_writtenPackets.last();
        const auto* hdr = reinterpret_cast<const InstrumentPacketHeader*>(tx.constData());
        assert(hdr->type == PKT_TYPE_SPI_CONFIG);
        const auto* cfg = reinterpret_cast<const InstrumentSpiConfigPayload*>(tx.constData() + sizeof(InstrumentPacketHeader));
        assert(cfg->speed_hz == 10000000);
        assert(cfg->cs_pin == 5);

        // Test SPI buffer transfer
        transport.m_writtenPackets.clear();
        QByteArray mosiData = QByteArray::fromHex("9F000000"); // JEDEC ID command + 3 dummy bytes
        ok = client.transferSpi(mosiData, true);
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());
        tx = transport.m_writtenPackets.last();
        hdr = reinterpret_cast<const InstrumentPacketHeader*>(tx.constData());
        assert(hdr->type == PKT_TYPE_SPI_XFER);

        bool spiXferCompleted = false;
        QByteArray spiRx;
        QObject::connect(&client, &BusToolsClient::spiTransferCompleted, [&](const QByteArray& data) {
            spiXferCompleted = true;
            spiRx = data;
        });

        InstrumentSpiXferPayload spiResp;
        spiResp.assert_cs = 1;
        spiResp.data_len  = 4;
        spiResp.data[0]   = 0xFF; // dummy during cmd
        spiResp.data[1]   = 0xEF; // Winbond
        spiResp.data[2]   = 0x40; // SPI
        spiResp.data[3]   = 0x18; // 16MB
        injectDevicePacket(transport, PKT_TYPE_SPI_XFER, 30, &spiResp, sizeof(uint8_t) + sizeof(uint16_t) + 4);
        assert(spiXferCompleted);
        assert(spiRx.size() == 4);
        assert(static_cast<uint8_t>(spiRx[1]) == 0xEF);

        // Test Flash Probe Request & Decoding Response
        transport.m_writtenPackets.clear();
        ok = client.probeSpiFlash(5);
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());
        tx = transport.m_writtenPackets.last();
        hdr = reinterpret_cast<const InstrumentPacketHeader*>(tx.constData());
        assert(hdr->type == PKT_TYPE_SPI_FLASH_PROBE);

        bool flashSignaled = false;
        InstrumentFlashInfoPayload flashResult{};
        QObject::connect(&client, &BusToolsClient::spiFlashInfoReceived, [&](const InstrumentFlashInfoPayload& info) {
            flashSignaled = true;
            flashResult = info;
        });

        InstrumentFlashInfoPayload flashResp;
        flashResp.mfg_id         = 0xEF;
        flashResp.memory_type    = 0x40;
        flashResp.capacity_id    = 0x18;
        flashResp.capacity_bytes = 16777216; // 16 MB
        std::snprintf(flashResp.part_name, sizeof(flashResp.part_name), "Winbond W25Q128M");
        injectDevicePacket(transport, PKT_TYPE_SPI_FLASH_RESP, 31, &flashResp, sizeof(flashResp));
        assert(flashSignaled);
        assert(flashResult.mfg_id == 0xEF);
        assert(flashResult.capacity_bytes == 16777216);
        assert(std::string(flashResult.part_name) == "Winbond W25Q128M");

        std::cout << "  [PASS] SPI Engine: config, full-duplex transfer, and JEDEC SFDP flash probe verified." << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 4: Dallas 1-Wire Master Engine & Search ROM
    // ─────────────────────────────────────────────────────────────────────────
    {
        // Test 1-Wire config
        transport.m_writtenPackets.clear();
        bool ok = client.configure1Wire(6, true);
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());
        QByteArray tx = transport.m_writtenPackets.last();
        const auto* hdr = reinterpret_cast<const InstrumentPacketHeader*>(tx.constData());
        assert(hdr->type == PKT_TYPE_1WIRE_CONFIG);
        const auto* cfg = reinterpret_cast<const Instrument1WireConfigPayload*>(tx.constData() + sizeof(InstrumentPacketHeader));
        assert(cfg->pin == 6);
        assert(cfg->enable_pullup == 1);

        // Test 1-Wire search ROM request & response
        transport.m_writtenPackets.clear();
        ok = client.search1Wire();
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());
        tx = transport.m_writtenPackets.last();
        hdr = reinterpret_cast<const InstrumentPacketHeader*>(tx.constData());
        assert(hdr->type == PKT_TYPE_1WIRE_SEARCH_REQ);

        bool oneWireSearchCompleted = false;
        QVector<QByteArray> discoveredRoms;
        QObject::connect(&client, &BusToolsClient::oneWireSearchCompleted, [&](const QVector<QByteArray>& roms) {
            oneWireSearchCompleted = true;
            discoveredRoms = roms;
        });

        Instrument1WireSearchRespPayload searchResp;
        searchResp.device_count = 1;
        uint8_t ds18b20_rom[8] = {0x28, 0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33, 0x99};
        std::memcpy(searchResp.rom_ids[0], ds18b20_rom, 8);
        injectDevicePacket(transport, PKT_TYPE_1WIRE_SEARCH_RESP, 40, &searchResp, sizeof(uint8_t) + 8);
        assert(oneWireSearchCompleted);
        assert(discoveredRoms.size() == 1);
        assert(discoveredRoms[0].size() == 8);
        assert(static_cast<uint8_t>(discoveredRoms[0][0]) == 0x28); // DS18B20 family code

        // Test 1-Wire transaction (e.g. Convert T: Reset -> Skip ROM 0xCC -> Convert T 0x44)
        transport.m_writtenPackets.clear();
        QByteArray convertCmd = QByteArray::fromHex("CC44");
        ok = client.transfer1Wire(true, convertCmd, 0);
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());
        tx = transport.m_writtenPackets.last();
        hdr = reinterpret_cast<const InstrumentPacketHeader*>(tx.constData());
        assert(hdr->type == PKT_TYPE_1WIRE_XFER);

        bool oneWireXferCompleted = false;
        bool presenceDetected = false;
        QByteArray oneWireRx;
        QObject::connect(&client, &BusToolsClient::oneWireTransferCompleted, [&](bool presence, const QByteArray& data) {
            oneWireXferCompleted = true;
            presenceDetected = presence;
            oneWireRx = data;
        });

        Instrument1WireXferPayload xferResp;
        xferResp.issue_reset = 1;
        xferResp.presence_ok = 1;
        xferResp.write_len   = 2;
        xferResp.read_len    = 0;
        injectDevicePacket(transport, PKT_TYPE_1WIRE_XFER, 41, &xferResp, sizeof(Instrument1WireXferPayload) - 64);
        assert(oneWireXferCompleted);
        assert(presenceDetected);

        std::cout << "  [PASS] 1-Wire Engine: config, ROM search (DS18B20 0x28), and reset/write transfer verified." << std::endl;
    }

    std::cout << "--- All BusToolsClient Tests PASSED ---" << std::endl;
    return true;
}
