#include "SyntaxClient.h"
#include <cstring>

namespace UdooKey {

SyntaxClient::SyntaxClient(ITransport* transport, QObject* parent)
    : QObject(parent) {
    if (transport) {
        setTransport(transport);
    }
}

void SyntaxClient::setTransport(ITransport* transport) {
    if (m_transport == transport) return;

    if (m_transport) {
        disconnect(m_transport, &ITransport::dataReady, this, &SyntaxClient::processIncomingData);
    }

    m_transport = transport;

    if (m_transport) {
        connect(m_transport, &ITransport::dataReady, this, &SyntaxClient::processIncomingData);
    }
}

bool SyntaxClient::isConnected() const {
    return m_transport && m_transport->isOpen();
}

bool SyntaxClient::executeSyntax(const QString& syntax) {
    QByteArray utf8 = syntax.toUtf8();
    if (utf8.size() >= 256) return false;

    InstrumentBpSyntaxExecPayload payload;
    std::memset(&payload, 0, sizeof(payload));
    std::memcpy(payload.syntax, utf8.constData(), utf8.size());
    payload.syntax[utf8.size()] = '\0';

    return sendPacket(PKT_TYPE_BP_SYNTAX_EXEC, &payload, static_cast<uint16_t>(utf8.size() + 1));
}

bool SyntaxClient::executeMacro(uint8_t macroId) {
    return executeSyntax(QStringLiteral("macro %1").arg(macroId));
}

bool SyntaxClient::executeCommand(const QString& command) {
    return executeSyntax(command);
}

bool SyntaxClient::sendPacket(InstrumentPacketType type, const void* payload, uint16_t len) {
    if (!m_transport || !m_transport->isOpen()) return false;

    InstrumentPacketHeader hdr;
    hdr.magic       = INSTRUMENT_PROTO_MAGIC;
    hdr.version     = INSTRUMENT_PROTO_VERSION;
    hdr.type        = static_cast<uint8_t>(type);
    hdr.sequence    = ++m_sequence;
    hdr.payload_len = len;
    hdr.crc16       = (payload && len > 0) ? instrument_crc16(reinterpret_cast<const uint8_t*>(payload), len) : 0;

    QByteArray packet;
    packet.resize(sizeof(hdr) + len);
    std::memcpy(packet.data(), &hdr, sizeof(hdr));
    if (payload && len > 0) {
        std::memcpy(packet.data() + sizeof(hdr), payload, len);
    }

    return m_transport->write(packet) == packet.size();
}

void SyntaxClient::processIncomingData(const QByteArray& data) {
    m_rxBuffer.append(data);

    while (m_rxBuffer.size() >= static_cast<int>(sizeof(InstrumentPacketHeader))) {
        int magicIndex = -1;
        for (int i = 0; i <= m_rxBuffer.size() - 2; ++i) {
            uint16_t magic = static_cast<uint8_t>(m_rxBuffer[i]) |
                             (static_cast<uint8_t>(m_rxBuffer[i + 1]) << 8);
            if (magic == INSTRUMENT_PROTO_MAGIC) {
                magicIndex = i;
                break;
            }
        }

        if (magicIndex < 0) {
            m_rxBuffer.clear();
            return;
        }

        if (magicIndex > 0) {
            m_rxBuffer.remove(0, magicIndex);
        }

        if (m_rxBuffer.size() < static_cast<int>(sizeof(InstrumentPacketHeader))) {
            return;
        }

        const auto* hdr = reinterpret_cast<const InstrumentPacketHeader*>(m_rxBuffer.constData());
        int totalExpected = sizeof(InstrumentPacketHeader) + hdr->payload_len;

        if (m_rxBuffer.size() < totalExpected) {
            return;
        }

        QByteArray payload;
        if (hdr->payload_len > 0) {
            payload = m_rxBuffer.mid(sizeof(InstrumentPacketHeader), hdr->payload_len);
            uint16_t calculatedCrc = instrument_crc16(
                reinterpret_cast<const uint8_t*>(payload.constData()), hdr->payload_len);
            if (calculatedCrc != hdr->crc16) {
                m_rxBuffer.remove(0, 2);
                continue;
            }
        }

        InstrumentPacketHeader parsedHdr = *hdr;
        m_rxBuffer.remove(0, totalExpected);

        processIncomingPacket(parsedHdr, payload);
    }
}

void SyntaxClient::processIncomingPacket(const InstrumentPacketHeader& hdr, const QByteArray& payload) {
    switch (hdr.type) {
        case PKT_TYPE_BP_SYNTAX_RESP: {
            if (payload.size() >= static_cast<int>(sizeof(uint8_t) + sizeof(uint16_t))) {
                const auto* p = reinterpret_cast<const InstrumentBpSyntaxRespPayload*>(payload.constData());
                uint8_t status = p->status;
                uint16_t resultLen = p->result_len;
                if (resultLen > 512) resultLen = 512;

                QString result;
                if (resultLen > 0 && payload.size() >= static_cast<int>(sizeof(uint8_t) + sizeof(uint16_t) + resultLen)) {
                    result = QString::fromUtf8(p->result, resultLen);
                }
                emit syntaxExecuted(status, result);
            }
            break;
        }

        case PKT_TYPE_ERROR: {
            uint8_t errCode = payload.isEmpty() ? 0xFF : static_cast<uint8_t>(payload[0]);
            QString reason = (payload.size() > 1) ? QString::fromUtf8(payload.mid(1)) : QStringLiteral("Unknown Error");
            emit syntaxError(errCode, reason);
            break;
        }

        default:
            break;
    }
}

} // namespace UdooKey
