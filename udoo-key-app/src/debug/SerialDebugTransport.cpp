#include "SerialDebugTransport.h"
#include <QElapsedTimer>

namespace udoo::debug {

SerialDebugTransport::SerialDebugTransport(UdooKey::ITransport* transport, QObject* parent)
    : IDebugTransport(parent), m_transport(transport) {
    if (m_transport) {
        connect(m_transport, &UdooKey::ITransport::dataReady, this, &SerialDebugTransport::onRawDataReceived);
    }
}

void SerialDebugTransport::setTransport(UdooKey::ITransport* transport) {
    if (m_transport) {
        disconnect(m_transport, &UdooKey::ITransport::dataReady, this, &SerialDebugTransport::onRawDataReceived);
    }
    m_transport = transport;
    if (m_transport) {
        connect(m_transport, &UdooKey::ITransport::dataReady, this, &SerialDebugTransport::onRawDataReceived);
    }
}

bool SerialDebugTransport::isConnected() const {
    return m_transport && m_transport->isOpen();
}

bool SerialDebugTransport::open() {
    return isConnected();
}

void SerialDebugTransport::close() {
    m_rxBuffer.clear();
}

bool SerialDebugTransport::sendPacket(const DebugPacket& packet) {
    if (!isConnected()) return false;
    QByteArray wire = DebugProtocol::serializePacket(packet);
    return m_transport->write(wire) == wire.size();
}

DebugPacket SerialDebugTransport::sendTransaction(const DebugPacket& request, int timeoutMs) {
    QMutexLocker locker(&m_mutex);
    if (!isConnected()) {
        return DebugProtocol::createResponse(request.command, request.seq, DebugStatus::ERR_NOT_CONNECTED);
    }

    m_waitingSeq = request.seq;
    m_hasResponse = false;

    QByteArray wire = DebugProtocol::serializePacket(request);
    if (m_transport->write(wire) != wire.size()) {
        return DebugProtocol::createResponse(request.command, request.seq, DebugStatus::ERR_TARGET_FAULT);
    }

    // Wait for response
    bool success = m_waitCond.wait(&m_mutex, timeoutMs);
    if (!success || !m_hasResponse) {
        return DebugProtocol::createResponse(request.command, request.seq, DebugStatus::ERR_TIMEOUT);
    }

    return m_lastResponse;
}

void SerialDebugTransport::onRawDataReceived(const QByteArray& data) {
    QMutexLocker locker(&m_mutex);
    m_rxBuffer.append(data);

    while (!m_rxBuffer.isEmpty()) {
        int syncIdx = m_rxBuffer.indexOf(static_cast<char>(DEBUG_SYNC_BYTE));
        if (syncIdx == -1) {
            m_rxBuffer.clear();
            return;
        }

        if (syncIdx > 0) {
            m_rxBuffer = m_rxBuffer.mid(syncIdx);
        }

        if (m_rxBuffer.size() < 8) {
            return; // Wait for minimum header size
        }

        uint16_t payloadLen = (static_cast<uint8_t>(m_rxBuffer.at(4)) << 8) | static_cast<uint8_t>(m_rxBuffer.at(5));
        int totalExpected = 8 + payloadLen;

        if (m_rxBuffer.size() < totalExpected) {
            return; // Wait for full packet
        }

        QByteArray packetBytes = m_rxBuffer.left(totalExpected);
        DebugPacket pkt;
        QString err;
        if (DebugProtocol::parsePacket(packetBytes, pkt, &err)) {
            m_rxBuffer = m_rxBuffer.mid(totalExpected);

            if (pkt.isResponse() && pkt.seq == m_waitingSeq) {
                m_lastResponse = pkt;
                m_hasResponse = true;
                m_waitCond.wakeAll();
            }

            emit packetReceived(pkt);
        } else {
            // Bad packet / framing error: advance by 1 byte to search next sync
            m_rxBuffer = m_rxBuffer.mid(1);
        }
    }
}

} // namespace udoo::debug
