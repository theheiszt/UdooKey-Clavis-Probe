#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QVector>
#include <cstdint>
#include "instrument_protocol.h"
#include "ITransport.h"

namespace UdooKey {

/**
 * @brief Bus Pirate Hardware Protocol Engines Client for UDOO KEY
 *
 * Implements Phase 3 of the Bus Pirate Integration Roadmap.
 * Provides client-side control and data transfer for:
 *   1. Secondary UART Bridge (opcodes 0x68 - 0x6A)
 *   2. I2C Master Engine & 7-bit Address Scanner (opcodes 0x6C - 0x6F)
 *   3. SPI Master Engine & JEDEC SFDP Flash Probe (opcodes 0x70 - 0x73)
 *   4. Dallas 1-Wire Master Engine & Search ROM (opcodes 0x74 - 0x77)
 *
 * All operations communicate over the canonical InstrumentProtocol v1.0 framing:
 *   Magic (0xAA55), Version (0x01), Opcode, Sequence, Length, CRC-16 (CCITT-FALSE).
 */
class BusToolsClient : public QObject {
    Q_OBJECT

public:
    explicit BusToolsClient(ITransport* transport = nullptr, QObject* parent = nullptr);
    ~BusToolsClient() override = default;

    // Transport management
    void setTransport(ITransport* transport);
    ITransport* transport() const { return m_transport; }
    bool isConnected() const;

    // ─── UART Bridge Operations (0x68 - 0x6A) ────────────────────────────────
    bool configureUart(uint32_t baudrate = 115200, uint8_t dataBits = 8,
                       uint8_t parity = 0, uint8_t stopBits = 1, bool invert = false);
    bool writeUart(const QByteArray& data);
    bool queryUartStatus();

    // ─── I2C Engine Operations (0x6C - 0x6F) ─────────────────────────────────
    bool configureI2c(uint32_t speedHz = 400000, bool enablePullups = true);
    bool scanI2c();
    bool transferI2c(uint8_t addr7Bit, const QByteArray& writeData, uint16_t readLen, bool sendStop = true);

    // ─── SPI Engine & Flash Probe Operations (0x70 - 0x73) ───────────────────
    bool configureSpi(uint32_t speedHz = 10000000, uint8_t cpol = 0, uint8_t cpha = 0,
                      bool lsbFirst = false, uint8_t csPin = 5);
    bool transferSpi(const QByteArray& txData, bool assertCs = true);
    bool probeSpiFlash(uint8_t csPin = 5);

    // ─── 1-Wire Engine Operations (0x74 - 0x77) ──────────────────────────────
    bool configure1Wire(uint8_t pin = 6, bool enablePullup = true);
    bool search1Wire();
    bool transfer1Wire(bool issueReset, const QByteArray& writeData, uint16_t readLen);

    // ─── Raw packet and byte stream processing ───────────────────────────────
    void processIncomingPacket(const InstrumentPacketHeader& hdr, const QByteArray& payload);
    void processIncomingData(const QByteArray& data);

signals:
    // UART Signals
    void uartDataReceived(const QByteArray& data);
    void uartStatusReceived(const InstrumentUartStatusPayload& status);

    // I2C Signals
    void i2cScanCompleted(const QVector<uint8_t>& discoveredAddresses);
    void i2cTransferCompleted(uint8_t status, const QByteArray& readData);

    // SPI Signals
    void spiTransferCompleted(const QByteArray& rxData);
    void spiFlashInfoReceived(const InstrumentFlashInfoPayload& flashInfo);

    // 1-Wire Signals
    void oneWireSearchCompleted(const QVector<QByteArray>& romIds);
    void oneWireTransferCompleted(bool presenceOk, const QByteArray& readData);

    // General Control Signals
    void commandAcknowledged(uint8_t opcode);
    void errorReceived(uint8_t errorCode, const QString& reason);

private:
    bool sendPacket(InstrumentPacketType type, const void* payload = nullptr, uint16_t len = 0);

    ITransport* m_transport{nullptr};
    uint16_t    m_sequence{0};
    QByteArray  m_rxBuffer;
};

} // namespace UdooKey
