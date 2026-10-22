#include "SwdTypes.h"
#include "JtagTypes.h"

namespace udoo::debug {

const char* swdAckToString(SwdAck ack) {
    switch (ack) {
        case SwdAck::Ok:            return "OK (0b001)";
        case SwdAck::Wait:          return "WAIT (0b010)";
        case SwdAck::Fault:         return "FAULT (0b100)";
        case SwdAck::ProtocolError: return "PROTOCOL_ERROR";
        default:                    return "UNKNOWN_ACK";
    }
}

uint8_t SwdPacket::makeHeader(bool isAP, bool isRead, uint8_t regAddr) {
    uint8_t a2 = (regAddr & 0x04) ? 1 : 0;
    uint8_t a3 = (regAddr & 0x08) ? 1 : 0;
    uint8_t ap = isAP ? 1 : 0;
    uint8_t rnw = isRead ? 1 : 0;

    // Even parity over AP, RnW, A[2], A[3]
    uint8_t parity = (ap ^ rnw ^ a2 ^ a3) & 1;

    uint8_t header = 0;
    header |= (1 << 0);           // Start bit
    header |= (ap << 1);          // APnDP
    header |= (rnw << 2);         // RnW
    header |= (a2 << 3);          // A[2]
    header |= (a3 << 4);          // A[3]
    header |= (parity << 5);      // Parity
    header |= (0 << 6);           // Stop bit
    header |= (1 << 7);           // Park bit

    return header;
}

bool SwdPacket::calculateParity32(uint32_t val) {
    // Standard Brian Kernighan popcount parity
    val ^= val >> 16;
    val ^= val >> 8;
    val ^= val >> 4;
    val ^= val >> 2;
    val ^= val >> 1;
    return (val & 1) != 0;
}

bool SwdPacket::verifyParity32(uint32_t val, bool parityBit) {
    return calculateParity32(val) == parityBit;
}

QString SwdDpidr::designerName() const {
    return QString::fromUtf8(lookupJep106Manufacturer(designerId()));
}

QString SwdDpidr::formatted() const {
    return QString("0x%1").arg(m_raw, 8, 16, QChar('0')).toUpper();
}

QString SwdDpidr::description() const {
    if (!isValid()) {
        return "Invalid DPIDR (0x00000000 or 0xFFFFFFFF)";
    }

    QString coreType;
    if (m_raw == 0x0BC12477) {
        coreType = "ARM Cortex-M0+ (RP2040 standard CoreSight SW-DP)";
    } else if (m_raw == 0x0BB11477) {
        coreType = "ARM Cortex-M0 / Cortex-M3 (CoreSight SW-DP)";
    } else if (m_raw == 0x2BA01477) {
        coreType = "ARM Cortex-M4 / Cortex-M7 (CoreSight SW-DPv2)";
    } else {
        coreType = QString("Part: 0x%1, Designer: %2").arg(partNo(), 2, 16, QChar('0')).arg(designerName());
    }

    return QString("%1 [%2, Rev: %3, MIN-DP: %4]")
        .arg(formatted())
        .arg(coreType)
        .arg(revision())
        .arg(minDp() ? "Yes" : "No");
}

} // namespace udoo::debug
