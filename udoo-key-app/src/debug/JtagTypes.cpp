#include "JtagTypes.h"
#include <QStringList>
#include <sstream>
#include <iomanip>

namespace udoo::debug {

const char* tapStateToString(JtagTapState state) {
    switch (state) {
        case JtagTapState::TestLogicReset: return "Test-Logic-Reset";
        case JtagTapState::RunTestIdle:    return "Run-Test/Idle";
        case JtagTapState::SelectDrScan:   return "Select-DR-Scan";
        case JtagTapState::CaptureDr:      return "Capture-DR";
        case JtagTapState::ShiftDr:        return "Shift-DR";
        case JtagTapState::Exit1Dr:        return "Exit1-DR";
        case JtagTapState::PauseDr:        return "Pause-DR";
        case JtagTapState::Exit2Dr:        return "Exit2-DR";
        case JtagTapState::UpdateDr:       return "Update-DR";
        case JtagTapState::SelectIrScan:   return "Select-IR-Scan";
        case JtagTapState::CaptureIr:      return "Capture-IR";
        case JtagTapState::ShiftIr:        return "Shift-IR";
        case JtagTapState::Exit1Ir:        return "Exit1-IR";
        case JtagTapState::PauseIr:        return "Pause-IR";
        case JtagTapState::Exit2Ir:        return "Exit2-IR";
        case JtagTapState::UpdateIr:       return "Update-IR";
        default: return "Unknown";
    }
}

JtagTapState tapStateFromString(const QString& name) {
    QString n = name.toLower().trimmed();
    if (n.contains("reset")) return JtagTapState::TestLogicReset;
    if (n.contains("idle")) return JtagTapState::RunTestIdle;
    if (n.contains("shift-dr") || n == "shiftdr") return JtagTapState::ShiftDr;
    if (n.contains("shift-ir") || n == "shiftir") return JtagTapState::ShiftIr;
    if (n.contains("capture-dr")) return JtagTapState::CaptureDr;
    if (n.contains("capture-ir")) return JtagTapState::CaptureIr;
    if (n.contains("update-dr")) return JtagTapState::UpdateDr;
    if (n.contains("update-ir")) return JtagTapState::UpdateIr;
    if (n.contains("pause-dr")) return JtagTapState::PauseDr;
    if (n.contains("pause-ir")) return JtagTapState::PauseIr;
    return JtagTapState::TestLogicReset;
}

// ---------------- JtagScanData Implementations ----------------

JtagScanData JtagScanData::fromUint32(uint32_t val, size_t bitCount) {
    JtagScanData data(bitCount);
    for (size_t i = 0; i < bitCount && i < 32; ++i) {
        data.setBit(i, (val & (1u << i)) != 0);
    }
    return data;
}

JtagScanData JtagScanData::fromUint64(uint64_t val, size_t bitCount) {
    JtagScanData data(bitCount);
    for (size_t i = 0; i < bitCount && i < 64; ++i) {
        data.setBit(i, (val & (1ULL << i)) != 0);
    }
    return data;
}

JtagScanData JtagScanData::fromHex(const QString& hexStr, size_t bitCount) {
    QString clean = hexStr.trimmed();
    if (clean.startsWith("0x", Qt::CaseInsensitive)) {
        clean = clean.mid(2);
    }
    JtagScanData data(bitCount);
    // Parse hex string: last char is lowest nybble (LSB)
    size_t bitIdx = 0;
    for (int i = clean.length() - 1; i >= 0 && bitIdx < bitCount; --i) {
        bool ok = false;
        uint8_t nybble = static_cast<uint8_t>(QString(clean[i]).toUInt(&ok, 16));
        if (ok) {
            for (int b = 0; b < 4 && bitIdx < bitCount; ++b) {
                data.setBit(bitIdx++, (nybble & (1u << b)) != 0);
            }
        }
    }
    return data;
}

JtagScanData JtagScanData::fromBits(const std::vector<bool>& bits) {
    JtagScanData data(bits.size());
    for (size_t i = 0; i < bits.size(); ++i) {
        data.setBit(i, bits[i]);
    }
    return data;
}

uint32_t JtagScanData::toUint32() const {
    uint32_t val = 0;
    size_t limit = std::min<size_t>(m_bitCount, 32);
    for (size_t i = 0; i < limit; ++i) {
        if (bit(i)) {
            val |= (1u << i);
        }
    }
    return val;
}

uint64_t JtagScanData::toUint64() const {
    uint64_t val = 0;
    size_t limit = std::min<size_t>(m_bitCount, 64);
    for (size_t i = 0; i < limit; ++i) {
        if (bit(i)) {
            val |= (1ULL << i);
        }
    }
    return val;
}

QString JtagScanData::toHexString() const {
    if (m_bitCount == 0) return "0x0";
    if (m_bitCount <= 32) {
        return QString("0x%1").arg(toUint32(), static_cast<int>((m_bitCount + 3) / 4), 16, QChar('0')).toUpper();
    }
    if (m_bitCount <= 64) {
        return QString("0x%1").arg(toUint64(), static_cast<int>((m_bitCount + 3) / 4), 16, QChar('0')).toUpper();
    }
    // For arbitrary length, print MSB to LSB
    QString hex;
    for (int byteIdx = static_cast<int>(m_bytes.size()) - 1; byteIdx >= 0; --byteIdx) {
        hex += QString("%1").arg(m_bytes[byteIdx], 2, 16, QChar('0')).toUpper();
    }
    return "0x" + hex;
}

QString JtagScanData::toBinString() const {
    if (m_bitCount == 0) return "0b0";
    QString bin = "0b";
    // Print MSB down to LSB
    for (int i = static_cast<int>(m_bitCount) - 1; i >= 0; --i) {
        bin += bit(i) ? '1' : '0';
    }
    return bin;
}

std::vector<bool> JtagScanData::toBitVector() const {
    std::vector<bool> vec(m_bitCount);
    for (size_t i = 0; i < m_bitCount; ++i) {
        vec[i] = bit(i);
    }
    return vec;
}

// ---------------- JEP-106 Lookup Table ----------------

static const struct {
    uint16_t code; // [11:8] continuation (bank), [7:1] ID
    const char* name;
} s_jep106Table[] = {
    // Bank 0 (continuation = 0)
    { 0x0001, "AMD" },
    { 0x0002, "AMI" },
    { 0x0009, "Intel" },
    { 0x000F, "National Semiconductor" },
    { 0x0015, "Philips / NXP" },
    { 0x0017, "Texas Instruments" },
    { 0x001F, "Atmel / Microchip" },
    { 0x0020, "STMicroelectronics" },
    { 0x0024, "Lattice Semiconductor" },
    { 0x0026, "Microchip Technology" },
    { 0x003A, "Cypress Semiconductor" },
    { 0x0049, "Xilinx / AMD" },
    { 0x0070, "Qualcomm" },
    // Bank 1 (continuation = 1) -> 0x0080 | ID
    { 0x00A7, "Realtek" },
    { 0x00C1, "Altera / Intel" },
    { 0x00E0, "Infineon Technologies" },
    // Bank 2 (continuation = 2) -> 0x0100 | ID
    // Bank 4 (continuation = 4) -> 0x0200 | ID
    { 0x023B, "ARM Ltd." },
    // Bank 7 (continuation = 7) -> 0x0380 | ID
    // Bank 8 (continuation = 8) -> 0x0400 | ID
    { 0x0492, "Espressif Systems" },
    // Bank 9 (continuation = 9) -> 0x0480 | ID
    { 0x04F5, "Raspberry Pi Ltd." },
    { 0x0000, nullptr }
};

const char* lookupJep106Manufacturer(uint16_t jep106Raw) {
    // Check known IDs
    for (size_t i = 0; s_jep106Table[i].name != nullptr; ++i) {
        if (s_jep106Table[i].code == jep106Raw) {
            return s_jep106Table[i].name;
        }
    }
    // Also check standard ARM (0x23B is continuation=4, id=0x3B)
    if (jep106Raw == 0x023B || (jep106Raw & 0x07F) == 0x3B) {
        return "ARM Ltd.";
    }
    // STMicroelectronics: 0x0020
    if ((jep106Raw & 0x07F) == 0x20) {
        return "STMicroelectronics";
    }
    // TI: 0x0017
    if ((jep106Raw & 0x07F) == 0x17) {
        return "Texas Instruments";
    }
    // Microchip: 0x0029 or 0x001F
    if ((jep106Raw & 0x07F) == 0x29 || (jep106Raw & 0x07F) == 0x1F) {
        return "Microchip Technology";
    }
    return "Unknown / Generic";
}

// ---------------- JtagIdCode Implementations ----------------

QString JtagIdCode::manufacturerName() const {
    return QString::fromUtf8(lookupJep106Manufacturer(manufacturerId()));
}

QString JtagIdCode::formatted() const {
    return QString("0x%1").arg(m_raw, 8, 16, QChar('0')).toUpper();
}

QString JtagIdCode::description() const {
    if (!isValid()) {
        return "Invalid IDCODE (0x00000000 or 0xFFFFFFFF)";
    }
    return QString("%1 [Part: 0x%2, Rev: %3, MFR: 0x%4 (%5)]")
        .arg(formatted())
        .arg(partNumber(), 4, 16, QChar('0')).toUpper()
        .arg(version())
        .arg(manufacturerId(), 3, 16, QChar('0')).toUpper()
        .arg(manufacturerName());
}

const char* standardInstructionToString(JtagStandardInstruction inst) {
    switch (inst) {
        case JtagStandardInstruction::Extest:        return "EXTEST";
        case JtagStandardInstruction::SamplePreload: return "SAMPLE/PRELOAD";
        case JtagStandardInstruction::IdCode:        return "IDCODE";
        case JtagStandardInstruction::Clamp:         return "CLAMP";
        case JtagStandardInstruction::HighZ:         return "HIGHZ";
        case JtagStandardInstruction::Bypass:        return "BYPASS";
        default:                                     return "CUSTOM";
    }
}

// ---------------- JtagChain Implementations ----------------

size_t JtagChain::totalIrLength() const {
    size_t total = 0;
    for (const auto& dev : m_devices) {
        total += dev.irLength;
    }
    return total;
}

JtagScanData JtagChain::bypassInstruction(size_t devIndex) const {
    if (devIndex >= m_devices.size()) return JtagScanData();
    size_t len = m_devices[devIndex].irLength > 0 ? m_devices[devIndex].irLength : 4;
    JtagScanData data(len);
    for (size_t i = 0; i < len; ++i) {
        data.setBit(i, true); // BYPASS is all 1s per IEEE 1149.1
    }
    return data;
}

JtagScanData JtagChain::standardInstruction(size_t devIndex, JtagStandardInstruction inst) const {
    if (devIndex >= m_devices.size()) return JtagScanData();
    size_t len = m_devices[devIndex].irLength > 0 ? m_devices[devIndex].irLength : 4;

    if (inst == JtagStandardInstruction::Bypass) {
        return bypassInstruction(devIndex);
    }
    if (inst == JtagStandardInstruction::Extest) {
        return JtagScanData(len); // All 0s
    }

    uint32_t rawVal = static_cast<uint32_t>(inst);
    // Mask to device's IR length
    uint32_t mask = (len >= 32) ? 0xFFFFFFFF : ((1u << len) - 1);
    return JtagScanData::fromUint32(rawVal & mask, len);
}

JtagScanData JtagChain::prepareTargetedIr(size_t devIndex, const JtagScanData& targetIr) const {
    if (devIndex >= m_devices.size()) return targetIr;

    std::vector<bool> fullBits;
    // Devices closer to TDO (indices > devIndex) must be shifted first
    if (m_devices.size() > 0) {
        for (int i = static_cast<int>(m_devices.size()) - 1; i > static_cast<int>(devIndex); --i) {
            size_t irLen = m_devices[i].irLength > 0 ? m_devices[i].irLength : 4;
            for (size_t b = 0; b < irLen; ++b) {
                fullBits.push_back(true); // BYPASS: 1
            }
        }
    }

    // Target device IR
    for (size_t b = 0; b < targetIr.bitCount(); ++b) {
        fullBits.push_back(targetIr.bit(b));
    }

    // Devices closer to TDI (indices < devIndex) are shifted last
    for (int i = static_cast<int>(devIndex) - 1; i >= 0; --i) {
        size_t irLen = m_devices[i].irLength > 0 ? m_devices[i].irLength : 4;
        for (size_t b = 0; b < irLen; ++b) {
            fullBits.push_back(true); // BYPASS: 1
        }
    }

    return JtagScanData::fromBits(fullBits);
}

JtagScanData JtagChain::extractTargetedIr(size_t devIndex, const JtagScanData& chainTdo) const {
    if (devIndex >= m_devices.size()) return chainTdo;

    size_t offset = 0;
    // Devices closer to TDO emerge first
    for (size_t i = m_devices.size() - 1; i > devIndex; --i) {
        offset += (m_devices[i].irLength > 0 ? m_devices[i].irLength : 4);
    }

    size_t targetLen = m_devices[devIndex].irLength > 0 ? m_devices[devIndex].irLength : 4;
    JtagScanData result(targetLen);
    for (size_t b = 0; b < targetLen && (offset + b) < chainTdo.bitCount(); ++b) {
        result.setBit(b, chainTdo.bit(offset + b));
    }

    return result;
}

JtagScanData JtagChain::prepareTargetedDr(size_t devIndex, const JtagScanData& targetDr) const {
    if (devIndex >= m_devices.size()) return targetDr;

    std::vector<bool> fullBits;
    // Devices closer to TDO (each in 1-bit BYPASS) shifted first
    if (m_devices.size() > 0) {
        for (int i = static_cast<int>(m_devices.size()) - 1; i > static_cast<int>(devIndex); --i) {
            fullBits.push_back(false); // 1-bit bypass pad
        }
    }

    // Target device DR bits
    for (size_t b = 0; b < targetDr.bitCount(); ++b) {
        fullBits.push_back(targetDr.bit(b));
    }

    // Devices closer to TDI (each in 1-bit BYPASS) shifted last
    for (int i = static_cast<int>(devIndex) - 1; i >= 0; --i) {
        fullBits.push_back(false); // 1-bit bypass pad
    }

    return JtagScanData::fromBits(fullBits);
}

JtagScanData JtagChain::extractTargetedDr(size_t devIndex, const JtagScanData& chainTdo, size_t targetDrBits) const {
    if (devIndex >= m_devices.size()) return chainTdo;

    size_t offset = 0;
    // Devices closer to TDO (indices > devIndex) have 1 bypass bit each exiting TDO first
    for (size_t i = m_devices.size() - 1; i > devIndex; --i) {
        offset += 1;
    }

    JtagScanData result(targetDrBits);
    for (size_t b = 0; b < targetDrBits && (offset + b) < chainTdo.bitCount(); ++b) {
        result.setBit(b, chainTdo.bit(offset + b));
    }

    return result;
}

bool JtagChain::parseCaptureIrPattern(const JtagScanData& capturedIr, size_t totalIrLen, size_t expectedDevices, std::vector<size_t>& outIrLengths) {
    outIrLengths.clear();
    if (expectedDevices == 0 || totalIrLen == 0 || capturedIr.bitCount() < totalIrLen) {
        return false;
    }

    std::vector<size_t> tdoOrderLengths;
    auto searchPartitions = [&](auto& self, size_t devIdxRemaining, size_t bitOffset) -> bool {
        if (devIdxRemaining == 0) {
            return bitOffset == totalIrLen;
        }

        // Each device must have at least 2 bits in its IR and must start with bit 0 = 1, bit 1 = 0
        if (bitOffset + 2 > totalIrLen) return false;
        if (!capturedIr.bit(bitOffset) || capturedIr.bit(bitOffset + 1)) {
            // Did not match IEEE 1149.1 Capture-IR signature (bit0=1, bit1=0)
            return false;
        }

        if (devIdxRemaining == 1) {
            // Last device gets remaining bits
            size_t len = totalIrLen - bitOffset;
            if (len >= 2) {
                tdoOrderLengths.push_back(len);
                return true;
            }
            return false;
        }

        // Try plausible IR lengths for this device (typically 2 to 32 bits, e.g. 4, 5, 6, 8)
        for (size_t tryLen = 2; tryLen <= 32 && (bitOffset + tryLen + 2 <= totalIrLen); ++tryLen) {
            // Next device must start with bit=1, bit=0
            if (capturedIr.bit(bitOffset + tryLen) && !capturedIr.bit(bitOffset + tryLen + 1)) {
                tdoOrderLengths.push_back(tryLen);
                if (self(self, devIdxRemaining - 1, bitOffset + tryLen)) {
                    return true;
                }
                tdoOrderLengths.pop_back();
            }
        }

        return false;
    };

    if (searchPartitions(searchPartitions, expectedDevices, 0)) {
        // tdoOrderLengths contains lengths for Device N-1 down to Device 0.
        // Reverse so outIrLengths is indexed by [Device 0 .. Device N-1]
        outIrLengths.assign(tdoOrderLengths.rbegin(), tdoOrderLengths.rend());
        return true;
    }

    // Fallback: If cannot partition unambiguously, allocate equal or default 4 bits
    outIrLengths.resize(expectedDevices, 4);
    return false;
}

} // namespace udoo::debug

