#pragma once

#include <cstdint>
#include <cstddef>
#include <QString>

namespace udoo::debug {

/**
 * @brief ARM Serial Wire Debug (SWD) Acknowledgement Status.
 * Returned in 3 bits (LSB-first): OK=0b001 (1), WAIT=0b010 (2), FAULT=0b100 (4).
 */
enum class SwdAck : uint8_t {
    Ok            = 0x01, // 0b001
    Wait          = 0x02, // 0b010
    Fault         = 0x04, // 0b100
    ProtocolError = 0x07  // Timeout, invalid framing, or no response (0b111)
};

const char* swdAckToString(SwdAck ack);

/**
 * @brief Standard ARM Debug Port (DP) Register Addresses (A[3:2]).
 */
namespace DpRegister {
    constexpr uint8_t IDCODE     = 0x00; // Read: DPIDR (Debug Port Identification Register)
    constexpr uint8_t ABORT      = 0x00; // Write: ABORT (Clear sticky flags, abort AP trans)
    constexpr uint8_t CTRL_STAT  = 0x04; // Read/Write: Control/Status
    constexpr uint8_t RESEND     = 0x08; // Read: Read last transfer data again
    constexpr uint8_t SELECT     = 0x08; // Write: APSEL & APBANKSEL
    constexpr uint8_t RDBUFF     = 0x0C; // Read: Read Buffer (captures previous read)
    constexpr uint8_t TARGETSEL  = 0x0C; // Write: Target Select (DPv2 multi-drop)
}

/**
 * @brief Standard ARM Access Port (MEM-AP) Register Addresses.
 */
namespace ApRegister {
    constexpr uint8_t CSW        = 0x00; // Control/Status Word
    constexpr uint8_t TAR        = 0x04; // Transfer Address Register
    constexpr uint8_t DRW        = 0x0C; // Data Read/Write Register
    constexpr uint8_t BD0        = 0x10; // Banked Data 0
    constexpr uint8_t BD1        = 0x14; // Banked Data 1
    constexpr uint8_t BD2        = 0x18; // Banked Data 2
    constexpr uint8_t BD3        = 0x1C; // Banked Data 3
    constexpr uint8_t CFG        = 0xF4; // Configuration Register
    constexpr uint8_t BASE       = 0xF8; // Debug Base Address
    constexpr uint8_t IDR        = 0xFC; // Access Port Identification Register
}

/**
 * @brief Standard SWD Switching sequences (LSB-first).
 */
namespace SwdSequences {
    constexpr uint16_t JTAG_TO_SWD = 0xE79E; // 16-bit JTAG-to-SWD sequence
    constexpr uint16_t SWD_TO_JTAG = 0xE73C; // 16-bit SWD-to-JTAG sequence
}

/**
 * @brief SWD Request Header Builder & Parity Calculator.
 */
class SwdPacket {
public:
    /// Construct standard 8-bit SWD request header byte
    static uint8_t makeHeader(bool isAP, bool isRead, uint8_t regAddr);

    /// Compute even parity of a 32-bit word (returns true if odd count of 1s)
    static bool calculateParity32(uint32_t val);

    /// Verify 32-bit data word against parity bit
    static bool verifyParity32(uint32_t val, bool parityBit);
};

/**
 * @brief Parsed ARM CoreSight DPIDR (Debug Port Identification Register).
 */
class SwdDpidr {
public:
    SwdDpidr() : m_raw(0) {}
    explicit SwdDpidr(uint32_t raw) : m_raw(raw) {}

    uint32_t raw() const { return m_raw; }
    bool isValid() const { return (m_raw & 0x01) == 0x01 && m_raw != 0xFFFFFFFF; }

    uint16_t designerId() const { return static_cast<uint16_t>((m_raw >> 1) & 0x07FF); }
    uint8_t version() const { return static_cast<uint8_t>((m_raw >> 12) & 0x0F); }
    bool minDp() const { return (m_raw & (1u << 16)) != 0; }
    uint8_t partNo() const { return static_cast<uint8_t>((m_raw >> 20) & 0xFF); }
    uint8_t revision() const { return static_cast<uint8_t>((m_raw >> 28) & 0x0F); }

    QString designerName() const;
    QString formatted() const;
    QString description() const;

private:
    uint32_t m_raw = 0;
};

/**
 * @brief Transfer result for an SWD register read/write.
 */
struct SwdTransferResult {
    SwdAck ack = SwdAck::ProtocolError;
    uint32_t data = 0;
    bool parityOk = false;
    QString errorMessage;

    bool isOk() const { return ack == SwdAck::Ok && parityOk; }
};

} // namespace udoo::debug
