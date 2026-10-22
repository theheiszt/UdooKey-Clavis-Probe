#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <cstdint>
#include "instrument_protocol.h"
#include "ITransport.h"

namespace UdooKey {

/**
 * @brief Bus Pirate Interactive Syntax & Command Execution Client for UDOO KEY
 *
 * Implements Phase 4 of the Bus Pirate Integration Roadmap.
 * Enables the host application to:
 *   - Execute standard Bus Pirate square-bracket syntax strings, e.g. "[ 0x50 0x00 ] [ 0x51 r:4 ]"
 *   - Execute stored macros, e.g. "macro 1" (I2C Scan), "macro 2" (EEPROM), etc.
 *   - Dispatch direct CLI and diagnostic commands.
 *
 * Communicates via canonical InstrumentProtocol v1.0 packets:
 *   - PKT_TYPE_BP_SYNTAX_EXEC (0x78)
 *   - PKT_TYPE_BP_SYNTAX_RESP (0x79)
 */
class SyntaxClient : public QObject {
    Q_OBJECT

public:
    explicit SyntaxClient(ITransport* transport = nullptr, QObject* parent = nullptr);
    ~SyntaxClient() override = default;

    // Transport management
    void setTransport(ITransport* transport);
    ITransport* transport() const { return m_transport; }
    bool isConnected() const;

    // Command execution
    bool executeSyntax(const QString& syntax);
    bool executeMacro(uint8_t macroId);
    bool executeCommand(const QString& command);

    // Stream and packet parsing
    void processIncomingPacket(const InstrumentPacketHeader& hdr, const QByteArray& payload);
    void processIncomingData(const QByteArray& data);

signals:
    void syntaxExecuted(uint8_t status, const QString& result);
    void syntaxError(uint8_t errorCode, const QString& reason);

private:
    bool sendPacket(InstrumentPacketType type, const void* payload = nullptr, uint16_t len = 0);

    ITransport* m_transport{nullptr};
    uint16_t    m_sequence{0};
    QByteArray  m_rxBuffer;
};

} // namespace UdooKey
