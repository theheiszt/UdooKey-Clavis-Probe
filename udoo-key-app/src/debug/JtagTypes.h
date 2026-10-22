#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <QString>
#include <QByteArray>

namespace udoo::debug {

/**
 * @brief IEEE 1149.1 Standard TAP (Test Access Port) State Machine states.
 * Exactly 16 states specified by the standard.
 */
enum class JtagTapState : uint8_t {
    TestLogicReset = 0,
    RunTestIdle    = 1,
    SelectDrScan   = 2,
    CaptureDr      = 3,
    ShiftDr        = 4,
    Exit1Dr        = 5,
    PauseDr        = 6,
    Exit2Dr        = 7,
    UpdateDr       = 8,
    SelectIrScan   = 9,
    CaptureIr      = 10,
    ShiftIr        = 11,
    Exit1Ir        = 12,
    PauseIr        = 13,
    Exit2Ir        = 14,
    UpdateIr       = 15
};

const char* tapStateToString(JtagTapState state);
JtagTapState tapStateFromString(const QString& name);

/**
 * @brief Bit-precise scan container for JTAG/SWD shifts.
 * Supports non-byte-aligned streams (1 bit up to 4096 bits).
 * Bits are stored in LSB-first order (bit 0 is shifted first).
 */
class JtagScanData {
public:
    JtagScanData() : m_bitCount(0) {}
    explicit JtagScanData(size_t bitCount) : m_bitCount(bitCount) {
        m_bytes.resize((bitCount + 7) / 8, 0);
    }
    JtagScanData(const std::vector<uint8_t>& bytes, size_t bitCount)
        : m_bytes(bytes), m_bitCount(bitCount) {
        m_bytes.resize((bitCount + 7) / 8, 0);
    }

    static JtagScanData fromUint32(uint32_t val, size_t bitCount = 32);
    static JtagScanData fromUint64(uint64_t val, size_t bitCount = 64);
    static JtagScanData fromHex(const QString& hexStr, size_t bitCount);
    static JtagScanData fromBits(const std::vector<bool>& bits);

    size_t bitCount() const { return m_bitCount; }
    size_t byteCount() const { return m_bytes.size(); }
    const std::vector<uint8_t>& bytes() const { return m_bytes; }
    bool isEmpty() const { return m_bitCount == 0; }

    bool bit(size_t index) const {
        if (index >= m_bitCount) return false;
        return (m_bytes[index / 8] & (1u << (index % 8))) != 0;
    }

    void setBit(size_t index, bool value) {
        if (index >= m_bitCount) return;
        if (value) {
            m_bytes[index / 8] |= static_cast<uint8_t>(1u << (index % 8));
        } else {
            m_bytes[index / 8] &= static_cast<uint8_t>(~(1u << (index % 8)));
        }
    }

    void appendBit(bool value) {
        size_t index = m_bitCount++;
        if (index / 8 >= m_bytes.size()) {
            m_bytes.push_back(0);
        }
        setBit(index, value);
    }

    uint32_t toUint32() const;
    uint64_t toUint64() const;
    QString toHexString() const;
    QString toBinString() const;
    std::vector<bool> toBitVector() const;

    bool operator==(const JtagScanData& other) const {
        if (m_bitCount != other.m_bitCount) return false;
        for (size_t i = 0; i < m_bitCount; ++i) {
            if (bit(i) != other.bit(i)) return false;
        }
        return true;
    }

private:
    std::vector<uint8_t> m_bytes;
    size_t m_bitCount = 0;
};

/**
 * @brief JEP106 Manufacturer identification decoding.
 */
struct Jep106Entry {
    uint8_t continuation; // Number of 0x7E/0x7F continuation bytes
    uint8_t id;           // 7-bit identity code
    const char* name;
};

const char* lookupJep106Manufacturer(uint16_t jep106Raw);

/**
 * @brief Parsed 32-bit IEEE 1149.1 JTAG IDCODE.
 * Format:
 * [31:28] Version (4 bits)
 * [27:12] Part Number (16 bits)
 * [11:1]  Manufacturer ID (11 bits JEP-106: [11:8] bank count, [7:1] manufacturer identity)
 * [0]     Always '1' (Fixed LSB per IEEE 1149.1)
 */
class JtagIdCode {
public:
    JtagIdCode() : m_raw(0) {}
    explicit JtagIdCode(uint32_t raw) : m_raw(raw) {}

    uint32_t raw() const { return m_raw; }
    bool isValid() const { return (m_raw & 0x01) == 0x01 && m_raw != 0xFFFFFFFF; }

    uint8_t version() const { return static_cast<uint8_t>((m_raw >> 28) & 0x0F); }
    uint16_t partNumber() const { return static_cast<uint16_t>((m_raw >> 12) & 0xFFFF); }
    uint16_t manufacturerId() const { return static_cast<uint16_t>((m_raw >> 1) & 0x07FF); }
    uint8_t continuationCode() const { return static_cast<uint8_t>((manufacturerId() >> 7) & 0x0F); }
    uint8_t identityCode() const { return static_cast<uint8_t>(manufacturerId() & 0x7F); }

    QString manufacturerName() const;
    QString formatted() const;
    QString description() const;

private:
    uint32_t m_raw = 0;
};

/**
 * @brief Information about a detected device in a JTAG chain.
 */
struct JtagDevice {
    size_t chainIndex = 0;
    JtagIdCode idcode;
    size_t irLength = 0;     // Instruction register length in bits
    QString name;            // Deduced device or core name
    QString manufacturer;
};

/**
 * @brief Types of JTAG operations.
 */
enum class JtagScanType {
    ShiftIR,
    ShiftDR,
    TmsSequence,
    Batch
};

/**
 * @brief Standard IEEE 1149.1 instructions.
 */
enum class JtagStandardInstruction : uint32_t {
    Extest        = 0x00,
    SamplePreload = 0x01,
    IdCode        = 0x02,
    Clamp         = 0x04,
    HighZ         = 0x08,
    Bypass        = 0xFFFFFFFF
};

const char* standardInstructionToString(JtagStandardInstruction inst);

/**
 * @brief Discrete operations supported in deterministic batch transactions.
 */
enum class JtagOpType : uint8_t {
    TapReset   = 0x01,
    GotoState  = 0x02,
    ClockTms   = 0x03,
    ShiftIr    = 0x04,
    ShiftDr    = 0x05,
    ClockIdle  = 0x06
};

/**
 * @brief Single operation within a batch transaction.
 */
struct JtagBatchOperation {
    JtagOpType type = JtagOpType::ShiftDr;
    JtagTapState targetState = JtagTapState::RunTestIdle;
    JtagScanData data;
    uint32_t clockCycles = 0;
};

/**
 * @brief Result returned from an executed batch transaction.
 */
struct JtagBatchResult {
    bool success = false;
    std::vector<JtagScanData> shiftResults;
    JtagTapState finalState = JtagTapState::RunTestIdle;
    QString errorMessage;
};

/**
 * @brief Single JTAG transaction request.
 */
struct JtagTransaction {
    JtagScanType type = JtagScanType::ShiftDR;
    JtagScanData tdiData;
    JtagTapState exitState = JtagTapState::RunTestIdle;
};

/**
 * @brief Single JTAG transaction result returned from hardware.
 */
struct JtagTransactionResult {
    bool success = false;
    JtagScanData tdoData;
    JtagTapState finalState = JtagTapState::RunTestIdle;
    QString errorMessage;
};

/**
 * @brief Multi-device JTAG daisy-chain topology and shift calculator.
 * Manages instruction/data routing, BYPASS padding for untargeted devices,
 * and IEEE 1149.1 Capture-IR parsing.
 *
 * Topology: [Host TDI] -> [Device 0] -> [Device 1] -> ... -> [Device N-1] -> [Host TDO]
 */
class JtagChain {
public:
    JtagChain() = default;
    explicit JtagChain(const std::vector<JtagDevice>& devices) : m_devices(devices) {}

    void addDevice(const JtagDevice& dev) { m_devices.push_back(dev); }
    void setDevices(const std::vector<JtagDevice>& devs) { m_devices = devs; }
    void clear() { m_devices.clear(); }

    size_t deviceCount() const { return m_devices.size(); }
    bool isEmpty() const { return m_devices.empty(); }
    const std::vector<JtagDevice>& devices() const { return m_devices; }
    const JtagDevice* deviceAt(size_t index) const {
        if (index < m_devices.size()) return &m_devices[index];
        return nullptr;
    }

    /// Total Instruction Register length across all devices in the chain
    size_t totalIrLength() const;

    /// Generate an all-1s BYPASS instruction matching device's IR length
    JtagScanData bypassInstruction(size_t devIndex) const;

    /// Generate standard instruction for target device
    JtagScanData standardInstruction(size_t devIndex, JtagStandardInstruction inst) const;

    /// Prepare a full-chain Shift-IR bitstream targeting devIndex with targetIr while placing all other devices in BYPASS
    JtagScanData prepareTargetedIr(size_t devIndex, const JtagScanData& targetIr) const;

    /// Extract target device's IR bits from the full chain's TDO scan result
    JtagScanData extractTargetedIr(size_t devIndex, const JtagScanData& chainTdo) const;

    /// Prepare a full-chain Shift-DR bitstream targeting devIndex with targetDr while padding 1-bit BYPASS for other devices
    JtagScanData prepareTargetedDr(size_t devIndex, const JtagScanData& targetDr) const;

    /// Extract target device's DR bits from full chain's TDO scan result given target's expected bit count
    JtagScanData extractTargetedDr(size_t devIndex, const JtagScanData& chainTdo, size_t targetDrBits) const;

    /// Deduce or validate per-device IR lengths from IEEE 1149.1 Capture-IR scan ('...01' patterns)
    static bool parseCaptureIrPattern(const JtagScanData& capturedIr, size_t totalIrLen, size_t expectedDevices, std::vector<size_t>& outIrLengths);

private:
    std::vector<JtagDevice> m_devices;
};

} // namespace udoo::debug

