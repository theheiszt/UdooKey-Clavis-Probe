#pragma once

#include <cstdint>
#include <cstddef>
#include <QByteArray>
#include <QString>
#include <vector>

namespace udoo::debug {

constexpr uint8_t DEBUG_SYNC_BYTE = 0x5B; // '['

enum class DebugCommand : uint8_t {
    // System / Target Management
    GET_STATUS        = 0x01,
    SET_TARGET_POWER  = 0x02,
    RESET_TARGET      = 0x03,

    // JTAG Commands
    JTAG_RESET        = 0x10,
    JTAG_SET_CLOCK    = 0x11,
    JTAG_TMS_SEQ      = 0x12,
    JTAG_SHIFT_IR     = 0x13,
    JTAG_SHIFT_DR     = 0x14,
    JTAG_TRANSACTION  = 0x15,
    JTAG_SCAN_CHAIN   = 0x16,
    JTAG_DISCOVER_IR  = 0x17,

    // SWD Commands
    SWD_RESET         = 0x20,
    SWD_CONNECT       = 0x21,
    SWD_SET_CLOCK     = 0x22,
    SWD_READ_DP       = 0x23,
    SWD_WRITE_DP      = 0x24,
    SWD_READ_AP       = 0x25,
    SWD_WRITE_AP      = 0x26,
    SWD_MEM_READ32    = 0x27,
    SWD_MEM_WRITE32   = 0x28,

    // Mask for response indicator
    RESPONSE_FLAG     = 0x80
};

enum class DebugStatus : uint8_t {
    OK                 = 0x00,
    ERR_TIMEOUT        = 0x01,
    ERR_INVALID_PARAM  = 0x02,
    ERR_CRC_MISMATCH   = 0x03,
    ERR_TARGET_FAULT   = 0x04,
    ERR_SWD_WAIT       = 0x05,
    ERR_PARITY         = 0x06,
    ERR_UNSUPPORTED    = 0x07,
    ERR_NOT_CONNECTED  = 0x08,
    ERR_FRAMING        = 0x09
};

const char* debugCommandToString(DebugCommand cmd);
const char* debugStatusToString(DebugStatus status);

/**
 * @brief Structured binary packet for Debug transport.
 * Layout:
 * [SYNC: 0x5B] [CMD: 1B] [SEQ: 1B] [STATUS: 1B] [LEN_MSB: 1B] [LEN_LSB: 1B] [PAYLOAD: N bytes] [CRC16_MSB: 1B] [CRC16_LSB: 1B]
 */
struct DebugPacket {
    DebugCommand command = DebugCommand::GET_STATUS;
    uint8_t seq = 0;
    DebugStatus status = DebugStatus::OK;
    QByteArray payload;

    bool isResponse() const {
        return (static_cast<uint8_t>(command) & static_cast<uint8_t>(DebugCommand::RESPONSE_FLAG)) != 0;
    }

    DebugCommand baseCommand() const {
        return static_cast<DebugCommand>(static_cast<uint8_t>(command) & ~static_cast<uint8_t>(DebugCommand::RESPONSE_FLAG));
    }
};

class DebugProtocol {
public:
    /// Calculate CRC-16-CCITT (poly 0x1021, init 0xFFFF)
    static uint16_t calculateCrc16(const uint8_t* data, size_t length);
    static uint16_t calculateCrc16(const QByteArray& data);

    /// Serialize a DebugPacket into raw wire bytes
    static QByteArray serializePacket(const DebugPacket& packet);

    /// Attempt to parse a DebugPacket from raw wire bytes
    static bool parsePacket(const QByteArray& rawBytes, DebugPacket& outPacket, QString* outError = nullptr);

    /// Helper to construct a request packet
    static DebugPacket createRequest(DebugCommand cmd, uint8_t seq, const QByteArray& payload = QByteArray());

    /// Helper to construct a response packet
    static DebugPacket createResponse(DebugCommand cmd, uint8_t seq, DebugStatus status, const QByteArray& payload = QByteArray());
};

} // namespace udoo::debug
