#include "SwdClient.h"

namespace udoo::debug {

SwdClient::SwdClient(IDebugTransport* transport, QObject* parent)
    : QObject(parent), m_transport(transport) {
}

bool SwdClient::resetLine() {
    if (!m_transport) return false;

    DebugPacket req = DebugProtocol::createRequest(DebugCommand::SWD_RESET, ++m_seq);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status == DebugStatus::OK) {
        emit logMessage("SWD Line Reset successful (50+ clocks HIGH)");
        return true;
    }

    emit errorOccurred(QString("SWD Line Reset failed: %1").arg(debugStatusToString(resp.status)));
    return false;
}

bool SwdClient::connectTarget(SwdDpidr& outDpidr) {
    if (!m_transport) return false;

    emit logMessage("Attempting SWD connect (switching JTAG->SWD 0xE79E)...");
    DebugPacket req = DebugProtocol::createRequest(DebugCommand::SWD_CONNECT, ++m_seq);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status != DebugStatus::OK || resp.payload.size() < 4) {
        m_connected = false;
        emit targetDisconnected();
        emit errorOccurred(QString("SWD Connect failed: %1").arg(debugStatusToString(resp.status)));
        return false;
    }

    uint32_t rawDpidr = static_cast<uint8_t>(resp.payload[0]) |
                       (static_cast<uint8_t>(resp.payload[1]) << 8) |
                       (static_cast<uint8_t>(resp.payload[2]) << 16) |
                       (static_cast<uint8_t>(resp.payload[3]) << 24);

    m_dpidr = SwdDpidr(rawDpidr);
    m_connected = m_dpidr.isValid();
    outDpidr = m_dpidr;

    if (m_connected) {
        emit logMessage(QString("Target Connected: %1").arg(m_dpidr.description()));
        emit targetConnected(m_dpidr);
        return true;
    } else {
        emit errorOccurred(QString("Target returned invalid DPIDR: %1").arg(m_dpidr.formatted()));
        return false;
    }
}

bool SwdClient::setClock(uint32_t frequencyHz) {
    if (!m_transport) return false;

    QByteArray payload;
    payload.append(static_cast<char>((frequencyHz >> 24) & 0xFF));
    payload.append(static_cast<char>((frequencyHz >> 16) & 0xFF));
    payload.append(static_cast<char>((frequencyHz >> 8) & 0xFF));
    payload.append(static_cast<char>(frequencyHz & 0xFF));

    DebugPacket req = DebugProtocol::createRequest(DebugCommand::SWD_SET_CLOCK, ++m_seq, payload);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status == DebugStatus::OK) {
        emit logMessage(QString("SWD Clock configured: %1 kHz").arg(frequencyHz / 1000.0, 0, 'f', 1));
        return true;
    }

    emit errorOccurred(QString("SWD Set Clock failed: %1").arg(debugStatusToString(resp.status)));
    return false;
}

SwdTransferResult SwdClient::readDp(uint8_t addr) {
    SwdTransferResult result;
    if (!m_transport) {
        result.errorMessage = "Transport not initialized";
        return result;
    }

    QByteArray payload;
    payload.append(static_cast<char>(addr));

    DebugPacket req = DebugProtocol::createRequest(DebugCommand::SWD_READ_DP, ++m_seq, payload);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status != DebugStatus::OK || resp.payload.size() < 5) {
        result.ack = SwdAck::ProtocolError;
        result.errorMessage = QString("Read DP failed: %1").arg(debugStatusToString(resp.status));
        emit errorOccurred(result.errorMessage);
        return result;
    }

    result.data = static_cast<uint8_t>(resp.payload[0]) |
                 (static_cast<uint8_t>(resp.payload[1]) << 8) |
                 (static_cast<uint8_t>(resp.payload[2]) << 16) |
                 (static_cast<uint8_t>(resp.payload[3]) << 24);
    result.ack = static_cast<SwdAck>(static_cast<uint8_t>(resp.payload[4]));
    result.parityOk = true;

    return result;
}

SwdTransferResult SwdClient::writeDp(uint8_t addr, uint32_t value) {
    SwdTransferResult result;
    if (!m_transport) {
        result.errorMessage = "Transport not initialized";
        return result;
    }

    QByteArray payload;
    payload.append(static_cast<char>(addr));
    payload.append(static_cast<char>(value & 0xFF));
    payload.append(static_cast<char>((value >> 8) & 0xFF));
    payload.append(static_cast<char>((value >> 16) & 0xFF));
    payload.append(static_cast<char>((value >> 24) & 0xFF));

    DebugPacket req = DebugProtocol::createRequest(DebugCommand::SWD_WRITE_DP, ++m_seq, payload);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status != DebugStatus::OK || resp.payload.isEmpty()) {
        result.ack = SwdAck::ProtocolError;
        result.errorMessage = QString("Write DP failed: %1").arg(debugStatusToString(resp.status));
        emit errorOccurred(result.errorMessage);
        return result;
    }

    result.ack = static_cast<SwdAck>(static_cast<uint8_t>(resp.payload[0]));
    result.parityOk = true;

    return result;
}

SwdTransferResult SwdClient::readAp(uint8_t addr) {
    SwdTransferResult result;
    if (!m_transport) {
        result.errorMessage = "Transport not initialized";
        return result;
    }

    QByteArray payload;
    payload.append(static_cast<char>(addr));

    DebugPacket req = DebugProtocol::createRequest(DebugCommand::SWD_READ_AP, ++m_seq, payload);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status != DebugStatus::OK || resp.payload.size() < 5) {
        result.ack = SwdAck::ProtocolError;
        result.errorMessage = QString("Read AP failed: %1").arg(debugStatusToString(resp.status));
        emit errorOccurred(result.errorMessage);
        return result;
    }

    result.data = static_cast<uint8_t>(resp.payload[0]) |
                 (static_cast<uint8_t>(resp.payload[1]) << 8) |
                 (static_cast<uint8_t>(resp.payload[2]) << 16) |
                 (static_cast<uint8_t>(resp.payload[3]) << 24);
    result.ack = static_cast<SwdAck>(static_cast<uint8_t>(resp.payload[4]));
    result.parityOk = true;

    return result;
}

SwdTransferResult SwdClient::writeAp(uint8_t addr, uint32_t value) {
    SwdTransferResult result;
    if (!m_transport) {
        result.errorMessage = "Transport not initialized";
        return result;
    }

    QByteArray payload;
    payload.append(static_cast<char>(addr));
    payload.append(static_cast<char>(value & 0xFF));
    payload.append(static_cast<char>((value >> 8) & 0xFF));
    payload.append(static_cast<char>((value >> 16) & 0xFF));
    payload.append(static_cast<char>((value >> 24) & 0xFF));

    DebugPacket req = DebugProtocol::createRequest(DebugCommand::SWD_WRITE_AP, ++m_seq, payload);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status != DebugStatus::OK || resp.payload.isEmpty()) {
        result.ack = SwdAck::ProtocolError;
        result.errorMessage = QString("Write AP failed: %1").arg(debugStatusToString(resp.status));
        emit errorOccurred(result.errorMessage);
        return result;
    }

    result.ack = static_cast<SwdAck>(static_cast<uint8_t>(resp.payload[0]));
    result.parityOk = true;

    return result;
}

SwdTransferResult SwdClient::readMem32(uint32_t address) {
    // Standard ARM MEM-AP read: write TAR, read DRW, then read RDBUFF
    SwdTransferResult res = writeAp(ApRegister::TAR, address);
    if (!res.isOk()) return res;

    // Trigger read through DRW
    res = readAp(ApRegister::DRW);
    if (!res.isOk()) return res;

    // Capture result from DP RDBUFF
    return readDp(DpRegister::RDBUFF);
}

SwdTransferResult SwdClient::writeMem32(uint32_t address, uint32_t value) {
    // Standard ARM MEM-AP write: write TAR, write DRW
    SwdTransferResult res = writeAp(ApRegister::TAR, address);
    if (!res.isOk()) return res;

    return writeAp(ApRegister::DRW, value);
}

} // namespace udoo::debug
