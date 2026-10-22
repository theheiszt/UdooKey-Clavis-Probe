#include "MockDebugTransport.h"
#include <cstring>

namespace udoo::debug {

MockDebugTransport::MockDebugTransport(QObject* parent)
    : IDebugTransport(parent), m_tap(JtagTapState::TestLogicReset) {
    // Populate simulated JTAG chain
    JtagDevice dev0;
    dev0.chainIndex = 0;
    dev0.idcode = JtagIdCode(0x0BC12477); // RP2040 Cortex-M0+ SWJ-DP
    dev0.irLength = 4;
    dev0.name = "RP2040 CoreSight SWJ-DP";
    dev0.manufacturer = "ARM Ltd.";

    JtagDevice dev1;
    dev1.chainIndex = 1;
    dev1.idcode = JtagIdCode(0x020B10DD); // Lattice iCE40 CPLD/FPGA
    dev1.irLength = 6;
    dev1.name = "iCE40 FPGA";
    dev1.manufacturer = "Lattice Semiconductor";

    m_chainDevices.push_back(dev0);
    m_chainDevices.push_back(dev1);
    m_deviceIrRegisters = { 0x01, 0x01 };

    // Initial SWD DP registers
    m_dpRegisters[DpRegister::IDCODE] = 0x0BC12477; // RP2040 Cortex-M0+
    m_dpRegisters[DpRegister::CTRL_STAT] = 0xF0000000; // Powerup ACK flags
    m_dpRegisters[DpRegister::SELECT] = 0x00000000;
    m_dpRegisters[DpRegister::RDBUFF] = 0x00000000;

    // Initial SWD MEM-AP registers
    m_apRegisters[ApRegister::CSW] = 0x23000052; // 32-bit incrementing transfer
    m_apRegisters[ApRegister::TAR] = 0x20000000; // SRAM base
    m_apRegisters[ApRegister::DRW] = 0x00000000;
    m_apRegisters[ApRegister::IDR] = 0x24770011; // CoreSight MEM-AP
}

bool MockDebugTransport::open() {
    m_connected = true;
    emit connectionChanged(true);
    return true;
}

void MockDebugTransport::close() {
    m_connected = false;
    emit connectionChanged(false);
}

bool MockDebugTransport::sendPacket(const DebugPacket& packet) {
    if (!m_connected) return false;
    DebugPacket resp = handleRequest(packet);
    emit packetReceived(resp);
    return true;
}

DebugPacket MockDebugTransport::sendTransaction(const DebugPacket& request, int /*timeoutMs*/) {
    if (!m_connected) {
        return DebugProtocol::createResponse(request.command, request.seq, DebugStatus::ERR_NOT_CONNECTED);
    }
    if (m_simulateTimeout) {
        return DebugProtocol::createResponse(request.command, request.seq, DebugStatus::ERR_TIMEOUT);
    }
    return handleRequest(request);
}

uint32_t MockDebugTransport::simulatedDpRegister(uint8_t addr) const {
    auto it = m_dpRegisters.find(addr);
    return (it != m_dpRegisters.end()) ? it->second : 0;
}

uint32_t MockDebugTransport::simulatedApRegister(uint8_t addr) const {
    auto it = m_apRegisters.find(addr);
    return (it != m_apRegisters.end()) ? it->second : 0;
}

DebugPacket MockDebugTransport::handleRequest(const DebugPacket& req) {
    if (!m_targetConnected) {
        return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::ERR_TARGET_FAULT);
    }

    switch (req.command) {
        case DebugCommand::GET_STATUS:
            return handleGetStatus(req);
        case DebugCommand::JTAG_RESET:
            return handleJtagReset(req);
        case DebugCommand::JTAG_SET_CLOCK:
            return handleJtagSetClock(req);
        case DebugCommand::JTAG_TMS_SEQ:
            return handleJtagTmsSeq(req);
        case DebugCommand::JTAG_TRANSACTION:
            return handleJtagTransaction(req);
        case DebugCommand::JTAG_SCAN_CHAIN:
            return handleJtagScanChain(req);
        case DebugCommand::JTAG_DISCOVER_IR:
            return handleJtagDiscoverIr(req);
        case DebugCommand::SWD_RESET:
            return handleSwdReset(req);
        case DebugCommand::SWD_CONNECT:
            return handleSwdConnect(req);
        case DebugCommand::SWD_SET_CLOCK:
            return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK);
        case DebugCommand::SWD_READ_DP:
            return handleSwdReadDp(req);
        case DebugCommand::SWD_WRITE_DP:
            return handleSwdWriteDp(req);
        case DebugCommand::SWD_READ_AP:
            return handleSwdReadAp(req);
        case DebugCommand::SWD_WRITE_AP:
            return handleSwdWriteAp(req);
        default:
            return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::ERR_UNSUPPORTED);
    }
}

DebugPacket MockDebugTransport::handleGetStatus(const DebugPacket& req) {
    QByteArray payload;
    // Status byte: [b0: connected, b1: jtag_mode, b2: swd_mode]
    uint8_t status = 0x01 | (m_swdConnected ? 0x04 : 0x02);
    payload.append(static_cast<char>(status));
    payload.append(static_cast<char>(m_tap.currentState()));
    return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK, payload);
}

DebugPacket MockDebugTransport::handleJtagReset(const DebugPacket& req) {
    m_tap.navigateTo(JtagTapState::TestLogicReset);
    m_swdConnected = false;
    QByteArray payload;
    payload.append(static_cast<char>(m_tap.currentState()));
    return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK, payload);
}

DebugPacket MockDebugTransport::handleJtagSetClock(const DebugPacket& req) {
    if (req.payload.size() < 4) {
        return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::ERR_INVALID_PARAM);
    }
    uint32_t hz = (static_cast<uint8_t>(req.payload[0]) << 24) |
                  (static_cast<uint8_t>(req.payload[1]) << 16) |
                  (static_cast<uint8_t>(req.payload[2]) << 8)  |
                  static_cast<uint8_t>(req.payload[3]);
    m_jtagClockHz = hz;
    return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK);
}

DebugPacket MockDebugTransport::handleJtagTmsSeq(const DebugPacket& req) {
    if (req.payload.isEmpty()) {
        return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::ERR_INVALID_PARAM);
    }
    uint8_t bitCount = static_cast<uint8_t>(req.payload[0]);
    for (uint8_t i = 0; i < bitCount; ++i) {
        uint8_t byteIdx = 1 + (i / 8);
        if (byteIdx >= req.payload.size()) break;
        bool tms = (req.payload[byteIdx] & (1 << (i % 8))) != 0;
        m_tap.clock(tms);
    }
    QByteArray payload;
    payload.append(static_cast<char>(m_tap.currentState()));
    return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK, payload);
}

DebugPacket MockDebugTransport::handleJtagTransaction(const DebugPacket& req) {
    if (req.payload.size() < 4) {
        return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::ERR_INVALID_PARAM);
    }
    uint8_t scanType = static_cast<uint8_t>(req.payload[0]);
    if (scanType == 2) {
        return handleJtagBatch(req);
    }

    uint16_t bitCount = (static_cast<uint8_t>(req.payload[1]) << 8) | static_cast<uint8_t>(req.payload[2]);
    JtagTapState exitState = static_cast<JtagTapState>(req.payload[3]);

    QByteArray tdoBytes;
    tdoBytes.resize((bitCount + 7) / 8);
    tdoBytes.fill(0);

    if (scanType == 0) { // ShiftIR
        // Update device instruction registers if TDI provided
        if (req.payload.size() >= 4 + static_cast<int>((bitCount + 7) / 8)) {
            uint32_t rawTdi = 0;
            for (size_t b = 0; b < bitCount && b < 32; ++b) {
                if (req.payload[4 + (b / 8)] & (1 << (b % 8))) {
                    rawTdi |= (1u << b);
                }
            }
            if (bitCount >= 10) {
                m_deviceIrRegisters[1] = rawTdi & 0x3F;        // Dev 1 (Lattice, 6-bit IR)
                m_deviceIrRegisters[0] = (rawTdi >> 6) & 0x0F; // Dev 0 (RP2040, 4-bit IR)
            } else if (bitCount == 4) {
                m_deviceIrRegisters[0] = rawTdi & 0x0F;
            }
        }
        // Capture-IR pattern: Dev 1 captures 0b000001 (0x01), Dev 0 captures 0b0001 (0x01 << 6 = 0x40) -> 0x41
        if (!tdoBytes.isEmpty()) {
            tdoBytes[0] = static_cast<char>(0x41);
        }
    } else if (scanType == 1) { // ShiftDR
        bool dev1Bypass = (m_deviceIrRegisters.size() > 1 && (m_deviceIrRegisters[1] & 0x3F) == 0x3F);
        bool dev0Bypass = (m_deviceIrRegisters.size() > 0 && (m_deviceIrRegisters[0] & 0x0F) == 0x0F);

        if (dev1Bypass && !dev0Bypass) {
            // Dev 1 in BYPASS (1 bit: 0 at bit 0). Dev 0 in IDCODE (32 bits at bits 1..32)
            uint32_t id0 = m_chainDevices[0].idcode.raw();
            for (size_t b = 0; b < 32 && (b + 1) < bitCount; ++b) {
                if (id0 & (1u << b)) {
                    tdoBytes[(b + 1) / 8] |= static_cast<char>(1 << ((b + 1) % 8));
                }
            }
        } else if (dev0Bypass && !dev1Bypass) {
            // Dev 1 in IDCODE (32 bits at bits 0..31). Dev 0 in BYPASS (1 bit: 0 at bit 32)
            uint32_t id1 = m_chainDevices[1].idcode.raw();
            for (size_t b = 0; b < 32 && b < bitCount; ++b) {
                if (id1 & (1u << b)) {
                    tdoBytes[b / 8] |= static_cast<char>(1 << (b % 8));
                }
            }
        } else {
            // Default: Dev 0 IDCODE first (or single-device test shift)
            if (bitCount >= 32) {
                uint32_t id = m_chainDevices[0].idcode.raw();
                tdoBytes[0] = static_cast<char>(id & 0xFF);
                tdoBytes[1] = static_cast<char>((id >> 8) & 0xFF);
                tdoBytes[2] = static_cast<char>((id >> 16) & 0xFF);
                tdoBytes[3] = static_cast<char>((id >> 24) & 0xFF);
            }
        }
    }

    m_tap.navigateTo(exitState);

    QByteArray respPayload;
    respPayload.append(static_cast<char>(m_tap.currentState()));
    respPayload.append(static_cast<char>((bitCount >> 8) & 0xFF));
    respPayload.append(static_cast<char>(bitCount & 0xFF));
    respPayload.append(tdoBytes);

    return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK, respPayload);
}

DebugPacket MockDebugTransport::handleJtagBatch(const DebugPacket& req) {
    if (req.payload.size() < 2) {
        return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::ERR_INVALID_PARAM);
    }

    uint8_t opCount = static_cast<uint8_t>(req.payload[1]);
    size_t offset = 2;

    std::vector<QByteArray> shiftOutputs;
    std::vector<uint16_t> shiftBitCounts;

    for (uint8_t opIdx = 0; opIdx < opCount && offset < static_cast<size_t>(req.payload.size()); ++opIdx) {
        uint8_t opType = static_cast<uint8_t>(req.payload[offset++]);
        switch (opType) {
            case 0x01: { // TapReset
                m_tap.navigateTo(JtagTapState::TestLogicReset);
                m_deviceIrRegisters = { 0x01, 0x01 };
                break;
            }
            case 0x02: { // GotoState
                if (offset < static_cast<size_t>(req.payload.size())) {
                    JtagTapState targetState = static_cast<JtagTapState>(req.payload[offset++]);
                    m_tap.navigateTo(targetState);
                }
                break;
            }
            case 0x03: { // ClockTms
                if (offset < static_cast<size_t>(req.payload.size())) {
                    uint8_t bitCount = static_cast<uint8_t>(req.payload[offset++]);
                    size_t bytes = (bitCount + 7) / 8;
                    for (uint8_t b = 0; b < bitCount && (offset + (b / 8)) < static_cast<size_t>(req.payload.size()); ++b) {
                        bool tms = (req.payload[offset + (b / 8)] & (1 << (b % 8))) != 0;
                        m_tap.clock(tms);
                    }
                    offset += bytes;
                }
                break;
            }
            case 0x04:   // ShiftIr
            case 0x05: { // ShiftDr
                if (offset + 3 <= static_cast<size_t>(req.payload.size())) {
                    uint16_t bitCount = (static_cast<uint8_t>(req.payload[offset]) << 8) | static_cast<uint8_t>(req.payload[offset + 1]);
                    JtagTapState exitState = static_cast<JtagTapState>(req.payload[offset + 2]);
                    offset += 3;
                    size_t byteCount = (bitCount + 7) / 8;

                    if (offset + byteCount <= static_cast<size_t>(req.payload.size())) {
                        offset += byteCount;
                    }

                    QByteArray tdo(byteCount, 0);
                    if (opType == 0x04) {
                        if (!tdo.isEmpty()) tdo[0] = static_cast<char>(0x41);
                    } else {
                        if (bitCount >= 32) {
                            uint32_t id = m_chainDevices[0].idcode.raw();
                            tdo[0] = static_cast<char>(id & 0xFF);
                            tdo[1] = static_cast<char>((id >> 8) & 0xFF);
                            tdo[2] = static_cast<char>((id >> 16) & 0xFF);
                            tdo[3] = static_cast<char>((id >> 24) & 0xFF);
                        }
                    }

                    m_tap.navigateTo(exitState);
                    shiftOutputs.push_back(tdo);
                    shiftBitCounts.push_back(bitCount);
                }
                break;
            }
            case 0x06: { // ClockIdle
                if (offset + 2 <= static_cast<size_t>(req.payload.size())) {
                    offset += 2;
                    m_tap.navigateTo(JtagTapState::RunTestIdle);
                }
                break;
            }
            default:
                break;
        }
    }

    QByteArray respPayload;
    respPayload.append(static_cast<char>(m_tap.currentState()));
    respPayload.append(static_cast<char>(shiftOutputs.size()));
    for (size_t i = 0; i < shiftOutputs.size(); ++i) {
        respPayload.append(static_cast<char>((shiftBitCounts[i] >> 8) & 0xFF));
        respPayload.append(static_cast<char>(shiftBitCounts[i] & 0xFF));
        respPayload.append(shiftOutputs[i]);
    }

    return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK, respPayload);
}

DebugPacket MockDebugTransport::handleJtagDiscoverIr(const DebugPacket& req) {
    m_tap.navigateTo(JtagTapState::ShiftIr);
    uint16_t totalIrBits = 10; // Dev 1 (6) + Dev 0 (4)
    QByteArray payload;
    payload.append(static_cast<char>((totalIrBits >> 8) & 0xFF));
    payload.append(static_cast<char>(totalIrBits & 0xFF));
    payload.append(static_cast<char>(0x41)); // Captured IR bits: 0x41
    payload.append(static_cast<char>(0x00));
    m_tap.navigateTo(JtagTapState::RunTestIdle);
    return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK, payload);
}

DebugPacket MockDebugTransport::handleJtagScanChain(const DebugPacket& req) {
    QByteArray payload;
    uint8_t devCount = static_cast<uint8_t>(m_chainDevices.size());
    payload.append(static_cast<char>(devCount));

    for (const auto& dev : m_chainDevices) {
        uint32_t id = dev.idcode.raw();
        payload.append(static_cast<char>(id & 0xFF));
        payload.append(static_cast<char>((id >> 8) & 0xFF));
        payload.append(static_cast<char>((id >> 16) & 0xFF));
        payload.append(static_cast<char>((id >> 24) & 0xFF));
        payload.append(static_cast<char>(dev.irLength & 0xFF));
    }

    return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK, payload);
}

DebugPacket MockDebugTransport::handleSwdReset(const DebugPacket& req) {
    m_swdConnected = false;
    return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK);
}

DebugPacket MockDebugTransport::handleSwdConnect(const DebugPacket& req) {
    if (m_failParity) {
        return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::ERR_PARITY);
    }
    m_swdConnected = true;
    uint32_t dpidr = m_dpRegisters[DpRegister::IDCODE];
    QByteArray payload;
    payload.append(static_cast<char>(dpidr & 0xFF));
    payload.append(static_cast<char>((dpidr >> 8) & 0xFF));
    payload.append(static_cast<char>((dpidr >> 16) & 0xFF));
    payload.append(static_cast<char>((dpidr >> 24) & 0xFF));
    return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK, payload);
}

DebugPacket MockDebugTransport::handleSwdReadDp(const DebugPacket& req) {
    if (req.payload.isEmpty()) {
        return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::ERR_INVALID_PARAM);
    }
    uint8_t addr = static_cast<uint8_t>(req.payload[0]) & 0x0C;
    uint32_t val = m_dpRegisters[addr];

    QByteArray payload;
    payload.append(static_cast<char>(val & 0xFF));
    payload.append(static_cast<char>((val >> 8) & 0xFF));
    payload.append(static_cast<char>((val >> 16) & 0xFF));
    payload.append(static_cast<char>((val >> 24) & 0xFF));
    payload.append(static_cast<char>(SwdAck::Ok));

    return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK, payload);
}

DebugPacket MockDebugTransport::handleSwdWriteDp(const DebugPacket& req) {
    if (req.payload.size() < 5) {
        return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::ERR_INVALID_PARAM);
    }
    uint8_t addr = static_cast<uint8_t>(req.payload[0]) & 0x0C;
    uint32_t val = static_cast<uint8_t>(req.payload[1]) |
                  (static_cast<uint8_t>(req.payload[2]) << 8) |
                  (static_cast<uint8_t>(req.payload[3]) << 16) |
                  (static_cast<uint8_t>(req.payload[4]) << 24);

    m_dpRegisters[addr] = val;

    QByteArray payload;
    payload.append(static_cast<char>(SwdAck::Ok));
    return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK, payload);
}

DebugPacket MockDebugTransport::handleSwdReadAp(const DebugPacket& req) {
    if (req.payload.isEmpty()) {
        return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::ERR_INVALID_PARAM);
    }
    uint8_t addr = static_cast<uint8_t>(req.payload[0]);
    uint32_t val = m_apRegisters[addr];

    QByteArray payload;
    payload.append(static_cast<char>(val & 0xFF));
    payload.append(static_cast<char>((val >> 8) & 0xFF));
    payload.append(static_cast<char>((val >> 16) & 0xFF));
    payload.append(static_cast<char>((val >> 24) & 0xFF));
    payload.append(static_cast<char>(SwdAck::Ok));

    return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK, payload);
}

DebugPacket MockDebugTransport::handleSwdWriteAp(const DebugPacket& req) {
    if (req.payload.size() < 5) {
        return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::ERR_INVALID_PARAM);
    }
    uint8_t addr = static_cast<uint8_t>(req.payload[0]);
    uint32_t val = static_cast<uint8_t>(req.payload[1]) |
                  (static_cast<uint8_t>(req.payload[2]) << 8) |
                  (static_cast<uint8_t>(req.payload[3]) << 16) |
                  (static_cast<uint8_t>(req.payload[4]) << 24);

    m_apRegisters[addr] = val;

    QByteArray payload;
    payload.append(static_cast<char>(SwdAck::Ok));
    return DebugProtocol::createResponse(req.command, req.seq, DebugStatus::OK, payload);
}

} // namespace udoo::debug
