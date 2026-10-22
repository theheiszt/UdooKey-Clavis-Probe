#include "DebugProtocol.h"

namespace udoo::debug {

const char* debugCommandToString(DebugCommand cmd) {
    uint8_t raw = static_cast<uint8_t>(cmd) & ~static_cast<uint8_t>(DebugCommand::RESPONSE_FLAG);
    switch (static_cast<DebugCommand>(raw)) {
        case DebugCommand::GET_STATUS:       return "GET_STATUS";
        case DebugCommand::SET_TARGET_POWER: return "SET_TARGET_POWER";
        case DebugCommand::RESET_TARGET:     return "RESET_TARGET";
        case DebugCommand::JTAG_RESET:       return "JTAG_RESET";
        case DebugCommand::JTAG_SET_CLOCK:   return "JTAG_SET_CLOCK";
        case DebugCommand::JTAG_TMS_SEQ:     return "JTAG_TMS_SEQ";
        case DebugCommand::JTAG_SHIFT_IR:    return "JTAG_SHIFT_IR";
        case DebugCommand::JTAG_SHIFT_DR:    return "JTAG_SHIFT_DR";
        case DebugCommand::JTAG_TRANSACTION: return "JTAG_TRANSACTION";
        case DebugCommand::JTAG_SCAN_CHAIN:  return "JTAG_SCAN_CHAIN";
        case DebugCommand::JTAG_DISCOVER_IR: return "JTAG_DISCOVER_IR";
        case DebugCommand::SWD_RESET:        return "SWD_RESET";
        case DebugCommand::SWD_CONNECT:      return "SWD_CONNECT";
        case DebugCommand::SWD_SET_CLOCK:    return "SWD_SET_CLOCK";
        case DebugCommand::SWD_READ_DP:      return "SWD_READ_DP";
        case DebugCommand::SWD_WRITE_DP:     return "SWD_WRITE_DP";
        case DebugCommand::SWD_READ_AP:      return "SWD_READ_AP";
        case DebugCommand::SWD_WRITE_AP:     return "SWD_WRITE_AP";
        case DebugCommand::SWD_MEM_READ32:   return "SWD_MEM_READ32";
        case DebugCommand::SWD_MEM_WRITE32:  return "SWD_MEM_WRITE32";
        default:                             return "UNKNOWN_CMD";
    }
}

const char* debugStatusToString(DebugStatus status) {
    switch (status) {
        case DebugStatus::OK:                return "OK";
        case DebugStatus::ERR_TIMEOUT:       return "ERR_TIMEOUT";
        case DebugStatus::ERR_INVALID_PARAM: return "ERR_INVALID_PARAM";
        case DebugStatus::ERR_CRC_MISMATCH:  return "ERR_CRC_MISMATCH";
        case DebugStatus::ERR_TARGET_FAULT:  return "ERR_TARGET_FAULT";
        case DebugStatus::ERR_SWD_WAIT:      return "ERR_SWD_WAIT";
        case DebugStatus::ERR_PARITY:        return "ERR_PARITY";
        case DebugStatus::ERR_UNSUPPORTED:   return "ERR_UNSUPPORTED";
        case DebugStatus::ERR_NOT_CONNECTED: return "ERR_NOT_CONNECTED";
        case DebugStatus::ERR_FRAMING:       return "ERR_FRAMING";
        default:                             return "ERR_UNKNOWN";
    }
}

uint16_t DebugProtocol::calculateCrc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

uint16_t DebugProtocol::calculateCrc16(const QByteArray& data) {
    return calculateCrc16(reinterpret_cast<const uint8_t*>(data.constData()), data.size());
}

QByteArray DebugProtocol::serializePacket(const DebugPacket& packet) {
    QByteArray raw;
    raw.reserve(packet.payload.size() + 8);

    raw.append(static_cast<char>(DEBUG_SYNC_BYTE));
    raw.append(static_cast<char>(packet.command));
    raw.append(static_cast<char>(packet.seq));
    raw.append(static_cast<char>(packet.status));

    uint16_t len = static_cast<uint16_t>(packet.payload.size());
    raw.append(static_cast<char>((len >> 8) & 0xFF));
    raw.append(static_cast<char>(len & 0xFF));

    if (!packet.payload.isEmpty()) {
        raw.append(packet.payload);
    }

    // CRC computed over [CMD, SEQ, STATUS, LEN_MSB, LEN_LSB, PAYLOAD...]
    // (i.e. from index 1 to end of raw)
    uint16_t crc = calculateCrc16(reinterpret_cast<const uint8_t*>(raw.constData() + 1), raw.size() - 1);
    raw.append(static_cast<char>((crc >> 8) & 0xFF));
    raw.append(static_cast<char>(crc & 0xFF));

    return raw;
}

bool DebugProtocol::parsePacket(const QByteArray& rawBytes, DebugPacket& outPacket, QString* outError) {
    if (rawBytes.size() < 8) {
        if (outError) *outError = "Packet length too short (minimum 8 bytes required)";
        return false;
    }

    if (static_cast<uint8_t>(rawBytes[0]) != DEBUG_SYNC_BYTE) {
        if (outError) *outError = QString("Invalid sync byte: 0x%1").arg(static_cast<uint8_t>(rawBytes[0]), 2, 16, QChar('0'));
        return false;
    }

    outPacket.command = static_cast<DebugCommand>(static_cast<uint8_t>(rawBytes[1]));
    outPacket.seq     = static_cast<uint8_t>(rawBytes[2]);
    outPacket.status  = static_cast<DebugStatus>(static_cast<uint8_t>(rawBytes[3]));

    uint16_t payloadLen = (static_cast<uint8_t>(rawBytes[4]) << 8) | static_cast<uint8_t>(rawBytes[5]);
    if (rawBytes.size() != payloadLen + 8) {
        if (outError) *outError = QString("Payload length mismatch: expected %1, total packet %2")
                                     .arg(payloadLen + 8).arg(rawBytes.size());
        return false;
    }

    // Extract payload
    if (payloadLen > 0) {
        outPacket.payload = rawBytes.mid(6, payloadLen);
    } else {
        outPacket.payload.clear();
    }

    // Extract and verify CRC
    uint16_t expectedCrc = (static_cast<uint8_t>(rawBytes[6 + payloadLen]) << 8) |
                            static_cast<uint8_t>(rawBytes[7 + payloadLen]);

    uint16_t actualCrc = calculateCrc16(reinterpret_cast<const uint8_t*>(rawBytes.constData() + 1), 5 + payloadLen);
    if (expectedCrc != actualCrc) {
        if (outError) *outError = QString("CRC mismatch: received 0x%1, calculated 0x%2")
                                     .arg(expectedCrc, 4, 16, QChar('0'))
                                     .arg(actualCrc, 4, 16, QChar('0'));
        return false;
    }

    return true;
}

DebugPacket DebugProtocol::createRequest(DebugCommand cmd, uint8_t seq, const QByteArray& payload) {
    DebugPacket p;
    p.command = cmd;
    p.seq = seq;
    p.status = DebugStatus::OK;
    p.payload = payload;
    return p;
}

DebugPacket DebugProtocol::createResponse(DebugCommand cmd, uint8_t seq, DebugStatus status, const QByteArray& payload) {
    DebugPacket p;
    p.command = static_cast<DebugCommand>(static_cast<uint8_t>(cmd) | static_cast<uint8_t>(DebugCommand::RESPONSE_FLAG));
    p.seq = seq;
    p.status = status;
    p.payload = payload;
    return p;
}

} // namespace udoo::debug
