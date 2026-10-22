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
 * @brief Bus Pirate Mode Multiplexer & Invariants Client for UDOO KEY
 *
 * Implements Phase 2 of the Bus Pirate Integration Roadmap.
 * Manages mutual exclusion on the Probe Bus (GP2..GP9), enforces the Safe Hi-Z invariant:
 *   CURRENT_MODE -> leaveMode() (Safe Hi-Z) -> enterMode() -> NEW_MODE
 * Communicates via canonical InstrumentProtocol v1.0 packets:
 *   - PKT_TYPE_MODE_SET    (0x66)
 *   - PKT_TYPE_MODE_STATUS (0x67)
 */
class ModeClient : public QObject {
    Q_OBJECT

public:
    explicit ModeClient(ITransport* transport = nullptr, QObject* parent = nullptr);
    ~ModeClient() override = default;

    // Transport management
    void setTransport(ITransport* transport);
    ITransport* transport() const { return m_transport; }
    bool isConnected() const;

    // Mode operations
    bool setMode(InstrumentModeId mode, uint32_t speedHz = 0);
    bool requestStatus();
    bool releaseToHiZ(); // Safe transition to INST_MODE_IDLE

    // Raw packet and byte stream processing
    void processIncomingPacket(const InstrumentPacketHeader& hdr, const QByteArray& payload);
    void processIncomingData(const QByteArray& data);

    // Active state queries
    InstrumentModeId currentMode() const { return m_currentMode; }
    QString currentModeName() const { return m_currentModeName; }
    uint32_t activeSpeedHz() const { return m_activeSpeedHz; }
    bool isProbeSpiActive() const { return m_probeSpiActive; }
    bool isSafeHiZVerified() const { return m_safeHiZVerified; }

    PinFunction pinFunction(uint8_t pin) const;
    QString pinFunctionDescription(uint8_t pin) const;
    const std::array<PinFunction, 8>& pinFunctions() const { return m_pinFunctions; }

    static QString modeNameString(InstrumentModeId mode);

signals:
    void modeChanged(InstrumentModeId newMode, InstrumentModeId previousMode, const QString& modeName);
    void modeStatusReceived(const InstrumentModeStatusPayload& status);
    void modeErrorReceived(uint8_t errorCode, const QString& reason);
    void pinFunctionsUpdated(const std::array<PinFunction, 8>& pinFunctions);

private:
    bool sendPacket(InstrumentPacketType type, const void* payload, uint16_t len);

    ITransport* m_transport = nullptr;
    uint16_t m_sequence = 0;
    QByteArray m_rxBuffer;

    InstrumentModeId m_currentMode = INST_MODE_IDLE;
    QString m_currentModeName = "IDLE (Safe Hi-Z)";
    uint32_t m_activeSpeedHz = 0;
    bool m_probeSpiActive = false;
    bool m_safeHiZVerified = true;
    std::array<PinFunction, 8> m_pinFunctions{};
};

} // namespace UdooKey
