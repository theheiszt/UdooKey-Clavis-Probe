#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <cstdint>
#include <array>
#include "instrument_protocol.h"
#include "ITransport.h"

namespace UdooKey {

/**
 * @brief Bus Pirate Generic GPIO Service Client for UDOO KEY
 *
 * Implements Phase 1 of the Bus Pirate Integration Roadmap.
 * Communicates via canonical InstrumentProtocol v1.0 packets (0xAA55 framing, CRC-16 CCITT-FALSE):
 *   - PKT_TYPE_GPIO_CONFIG     (0x60)
 *   - PKT_TYPE_GPIO_READ       (0x61)
 *   - PKT_TYPE_GPIO_WRITE      (0x62)
 *   - PKT_TYPE_GPIO_STATUS     (0x63)
 *   - PKT_TYPE_GPIO_WRITE_MASK (0x64)
 *   - PKT_TYPE_GPIO_READ_ALL   (0x65)
 */
class GpioClient : public QObject {
    Q_OBJECT

public:
    explicit GpioClient(ITransport* transport = nullptr, QObject* parent = nullptr);
    ~GpioClient() override = default;

    // Transport management
    void setTransport(ITransport* transport);
    ITransport* transport() const { return m_transport; }
    bool isConnected() const;

    // Pin operations
    bool configurePin(uint8_t pinOrIdx, bool isOutput, bool initialLevel = false, GpioPullMode pull = GPIO_PULL_NONE);
    bool writePin(uint8_t pinOrIdx, bool level);
    bool readPin(uint8_t pinOrIdx);
    bool writeMask(uint8_t dirMask, uint8_t valueMask);
    bool readAll();

    // Raw packet and stream processing
    void processIncomingPacket(const InstrumentPacketHeader& hdr, const QByteArray& payload);
    void processIncomingData(const QByteArray& data);

    // Cached bus state
    uint8_t lastPinValues() const { return m_lastPinValues; }
    uint8_t lastPinDirections() const { return m_lastPinDirections; }
    InstrumentGpioStatusPayload lastPinStatus(uint8_t pinOrIdx) const;

signals:
    void pinStatusReceived(const InstrumentGpioStatusPayload& status);
    void allPinsReceived(const InstrumentGpioReadAllPayload& status);
    void errorReceived(uint8_t errorCode, const QString& message);

private:
    bool sendPacket(InstrumentPacketType type, const void* payload, uint16_t len);

    ITransport* m_transport = nullptr;
    uint16_t m_sequence = 0;
    QByteArray m_rxBuffer;

    uint8_t m_lastPinValues = 0;
    uint8_t m_lastPinDirections = 0;
    std::array<InstrumentGpioStatusPayload, 8> m_pinStatusHistory{};
};

} // namespace UdooKey
