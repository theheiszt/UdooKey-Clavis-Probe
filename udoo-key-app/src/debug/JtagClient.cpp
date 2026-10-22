#include "JtagClient.h"

namespace udoo::debug {

JtagClient::JtagClient(IDebugTransport* transport, QObject* parent)
    : QObject(parent), m_transport(transport), m_tap(JtagTapState::TestLogicReset) {
}

bool JtagClient::resetTap() {
    if (!m_transport) return false;

    DebugPacket req = DebugProtocol::createRequest(DebugCommand::JTAG_RESET, ++m_seq);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status == DebugStatus::OK) {
        m_tap.forceState(JtagTapState::TestLogicReset);
        emit tapStateChanged(JtagTapState::TestLogicReset);
        emit logMessage("JTAG TAP Reset: Test-Logic-Reset");
        return true;
    }

    emit errorOccurred(QString("TAP Reset failed: %1").arg(debugStatusToString(resp.status)));
    return false;
}

bool JtagClient::setClock(uint32_t frequencyHz) {
    if (!m_transport) return false;

    QByteArray payload;
    payload.append(static_cast<char>((frequencyHz >> 24) & 0xFF));
    payload.append(static_cast<char>((frequencyHz >> 16) & 0xFF));
    payload.append(static_cast<char>((frequencyHz >> 8) & 0xFF));
    payload.append(static_cast<char>(frequencyHz & 0xFF));

    DebugPacket req = DebugProtocol::createRequest(DebugCommand::JTAG_SET_CLOCK, ++m_seq, payload);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status == DebugStatus::OK) {
        emit logMessage(QString("JTAG Clock configured: %1 kHz").arg(frequencyHz / 1000.0, 0, 'f', 1));
        return true;
    }

    emit errorOccurred(QString("Set clock failed: %1").arg(debugStatusToString(resp.status)));
    return false;
}

bool JtagClient::navigateTap(JtagTapState targetState) {
    std::vector<bool> path = m_tap.pathTo(targetState);
    if (path.empty() && m_tap.currentState() == targetState) {
        return true;
    }
    return sendTmsSequence(path);
}

bool JtagClient::sendTmsSequence(const std::vector<bool>& tmsBits) {
    if (!m_transport || tmsBits.empty()) return false;

    QByteArray payload;
    payload.append(static_cast<char>(tmsBits.size() & 0xFF));

    size_t byteCount = (tmsBits.size() + 7) / 8;
    QByteArray bytes(byteCount, 0);
    for (size_t i = 0; i < tmsBits.size(); ++i) {
        if (tmsBits[i]) {
            bytes[i / 8] = static_cast<char>(bytes[i / 8] | (1 << (i % 8)));
        }
    }
    payload.append(bytes);

    DebugPacket req = DebugProtocol::createRequest(DebugCommand::JTAG_TMS_SEQ, ++m_seq, payload);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status == DebugStatus::OK) {
        m_tap.clockSequence(tmsBits);
        if (!resp.payload.isEmpty()) {
            m_tap.forceState(static_cast<JtagTapState>(resp.payload[0]));
        }
        emit tapStateChanged(m_tap.currentState());
        return true;
    }

    emit errorOccurred(QString("TMS sequence failed: %1").arg(debugStatusToString(resp.status)));
    return false;
}

std::vector<JtagDevice> JtagClient::scanChain() {
    m_devices.clear();
    if (!m_transport) return m_devices;

    emit logMessage("Starting JTAG chain enumeration...");
    DebugPacket req = DebugProtocol::createRequest(DebugCommand::JTAG_SCAN_CHAIN, ++m_seq);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status != DebugStatus::OK || resp.payload.isEmpty()) {
        emit errorOccurred(QString("Chain scan failed: %1").arg(debugStatusToString(resp.status)));
        return m_devices;
    }

    uint8_t count = static_cast<uint8_t>(resp.payload[0]);
    size_t offset = 1;

    for (uint8_t i = 0; i < count && (offset + 5 <= static_cast<size_t>(resp.payload.size())); ++i) {
        uint32_t rawId = static_cast<uint8_t>(resp.payload[offset]) |
                        (static_cast<uint8_t>(resp.payload[offset + 1]) << 8) |
                        (static_cast<uint8_t>(resp.payload[offset + 2]) << 16) |
                        (static_cast<uint8_t>(resp.payload[offset + 3]) << 24);
        uint8_t irLen = static_cast<uint8_t>(resp.payload[offset + 4]);
        offset += 5;

        JtagDevice dev;
        dev.chainIndex = i;
        dev.idcode = JtagIdCode(rawId);
        dev.irLength = irLen;
        dev.manufacturer = dev.idcode.manufacturerName();

        if (rawId == 0x0BC12477) {
            dev.name = "RP2040 CoreSight SWJ-DP";
        } else if (rawId == 0x020B10DD) {
            dev.name = "iCE40 FPGA";
        } else {
            dev.name = QString("Device #%1 (%2)").arg(i + 1).arg(dev.idcode.formatted());
        }

        m_devices.push_back(dev);
        emit logMessage(QString("Device [%1]: IDCODE=%2 IR_LEN=%3 MFR=%4 (%5)")
            .arg(i)
            .arg(dev.idcode.formatted())
            .arg(dev.irLength)
            .arg(dev.manufacturer)
            .arg(dev.name));
    }

    m_chain.setDevices(m_devices);
    discoverIrLengths();

    emit chainScanned(m_devices);
    return m_devices;
}

bool JtagClient::discoverIrLengths() {
    if (!m_transport || m_devices.empty()) return false;

    DebugPacket req = DebugProtocol::createRequest(DebugCommand::JTAG_DISCOVER_IR, ++m_seq);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status != DebugStatus::OK || resp.payload.size() < 3) {
        return false;
    }

    uint16_t totalIrBits = (static_cast<uint8_t>(resp.payload[0]) << 8) | static_cast<uint8_t>(resp.payload[1]);
    QByteArray rawCaptured = resp.payload.mid(2);
    std::vector<uint8_t> byteVec(rawCaptured.begin(), rawCaptured.end());
    JtagScanData capturedData(byteVec, totalIrBits);

    std::vector<size_t> irLengths;
    if (JtagChain::parseCaptureIrPattern(capturedData, totalIrBits, m_devices.size(), irLengths)) {
        for (size_t i = 0; i < m_devices.size() && i < irLengths.size(); ++i) {
            m_devices[i].irLength = irLengths[i];
        }
        m_chain.setDevices(m_devices);
        emit logMessage(QString("IEEE 1149.1 IR scan verified: Total IR length = %1 bits").arg(totalIrBits));
        return true;
    }

    return false;
}

JtagTransactionResult JtagClient::shiftDeviceIr(size_t devIndex, const JtagScanData& irData, JtagTapState exitState) {
    if (devIndex >= m_devices.size()) {
        JtagTransactionResult res;
        res.errorMessage = QString("Device index %1 out of range (chain has %2 devices)").arg(devIndex).arg(m_devices.size());
        emit errorOccurred(res.errorMessage);
        return res;
    }

    JtagScanData chainTdi = m_chain.prepareTargetedIr(devIndex, irData);
    JtagTransactionResult rawResult = shiftIr(chainTdi, exitState);
    if (rawResult.success) {
        rawResult.tdoData = m_chain.extractTargetedIr(devIndex, rawResult.tdoData);
    }
    return rawResult;
}

JtagTransactionResult JtagClient::shiftDeviceDr(size_t devIndex, const JtagScanData& drData, size_t responseBits, JtagTapState exitState) {
    if (devIndex >= m_devices.size()) {
        JtagTransactionResult res;
        res.errorMessage = QString("Device index %1 out of range (chain has %2 devices)").arg(devIndex).arg(m_devices.size());
        emit errorOccurred(res.errorMessage);
        return res;
    }

    JtagScanData chainTdi = m_chain.prepareTargetedDr(devIndex, drData);
    JtagTransactionResult rawResult = shiftDr(chainTdi, exitState);
    if (rawResult.success) {
        rawResult.tdoData = m_chain.extractTargetedDr(devIndex, rawResult.tdoData, responseBits);
    }
    return rawResult;
}

JtagTransactionResult JtagClient::executeStandardInstruction(size_t devIndex, JtagStandardInstruction instruction) {
    if (devIndex >= m_devices.size()) {
        JtagTransactionResult res;
        res.errorMessage = QString("Device index %1 out of range").arg(devIndex);
        emit errorOccurred(res.errorMessage);
        return res;
    }

    JtagScanData opcode = m_chain.standardInstruction(devIndex, instruction);
    emit logMessage(QString("Device [%1] Executing Standard Instruction: %2 (Opcode: %3)")
        .arg(devIndex).arg(standardInstructionToString(instruction)).arg(opcode.toHexString()));
    return shiftDeviceIr(devIndex, opcode);
}

JtagBatchResult JtagClient::executeBatch(const std::vector<JtagBatchOperation>& operations) {
    JtagBatchResult result;
    if (!m_transport) {
        result.errorMessage = "Transport not initialized";
        emit errorOccurred(result.errorMessage);
        return result;
    }

    QByteArray payload;
    payload.append(static_cast<char>(2)); // 2 = Batch Mode
    payload.append(static_cast<char>(operations.size() & 0xFF));

    for (const auto& op : operations) {
        payload.append(static_cast<char>(op.type));
        switch (op.type) {
            case JtagOpType::TapReset:
                // No extra parameters
                break;
            case JtagOpType::GotoState:
                payload.append(static_cast<char>(op.targetState));
                break;
            case JtagOpType::ClockTms: {
                uint8_t bits = static_cast<uint8_t>(op.data.bitCount() & 0xFF);
                payload.append(static_cast<char>(bits));
                for (uint8_t b : op.data.bytes()) {
                    payload.append(static_cast<char>(b));
                }
                break;
            }
            case JtagOpType::ShiftIr:
            case JtagOpType::ShiftDr: {
                uint16_t bits = static_cast<uint16_t>(op.data.bitCount());
                payload.append(static_cast<char>((bits >> 8) & 0xFF));
                payload.append(static_cast<char>(bits & 0xFF));
                payload.append(static_cast<char>(op.targetState));
                for (uint8_t b : op.data.bytes()) {
                    payload.append(static_cast<char>(b));
                }
                break;
            }
            case JtagOpType::ClockIdle: {
                payload.append(static_cast<char>((op.clockCycles >> 8) & 0xFF));
                payload.append(static_cast<char>(op.clockCycles & 0xFF));
                break;
            }
        }
    }

    DebugPacket req = DebugProtocol::createRequest(DebugCommand::JTAG_TRANSACTION, ++m_seq, payload);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status != DebugStatus::OK || resp.payload.size() < 2) {
        result.errorMessage = QString("Batch transaction failed: %1").arg(debugStatusToString(resp.status));
        emit errorOccurred(result.errorMessage);
        return result;
    }

    result.success = true;
    result.finalState = static_cast<JtagTapState>(resp.payload[0]);
    m_tap.forceState(result.finalState);

    uint8_t resultCount = static_cast<uint8_t>(resp.payload[1]);
    size_t offset = 2;
    for (uint8_t i = 0; i < resultCount && offset + 2 <= static_cast<size_t>(resp.payload.size()); ++i) {
        uint16_t bits = (static_cast<uint8_t>(resp.payload[offset]) << 8) | static_cast<uint8_t>(resp.payload[offset + 1]);
        offset += 2;
        size_t byteCount = (bits + 7) / 8;

        JtagScanData scan(bits);
        for (size_t b = 0; b < bits; ++b) {
            size_t bIdx = offset + (b / 8);
            if (bIdx < static_cast<size_t>(resp.payload.size())) {
                bool val = (resp.payload[bIdx] & (1 << (b % 8))) != 0;
                scan.setBit(b, val);
            }
        }
        offset += byteCount;
        result.shiftResults.push_back(scan);
    }

    emit tapStateChanged(m_tap.currentState());
    emit batchCompleted(result);
    return result;
}

JtagTransactionResult JtagClient::shiftIr(const JtagScanData& tdiData, JtagTapState exitState) {
    JtagTransactionResult result;
    if (!m_transport) {
        result.errorMessage = "Transport not initialized";
        return result;
    }

    QByteArray payload;
    payload.append(static_cast<char>(0)); // 0 = ShiftIR
    uint16_t bits = static_cast<uint16_t>(tdiData.bitCount());
    payload.append(static_cast<char>((bits >> 8) & 0xFF));
    payload.append(static_cast<char>(bits & 0xFF));
    payload.append(static_cast<char>(exitState));
    for (uint8_t b : tdiData.bytes()) {
        payload.append(static_cast<char>(b));
    }

    DebugPacket req = DebugProtocol::createRequest(DebugCommand::JTAG_TRANSACTION, ++m_seq, payload);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status != DebugStatus::OK || resp.payload.size() < 3) {
        result.errorMessage = QString("Shift-IR failed: %1").arg(debugStatusToString(resp.status));
        emit errorOccurred(result.errorMessage);
        return result;
    }

    result.success = true;
    result.finalState = static_cast<JtagTapState>(resp.payload[0]);
    m_tap.forceState(result.finalState);

    uint16_t sampledBits = (static_cast<uint8_t>(resp.payload[1]) << 8) | static_cast<uint8_t>(resp.payload[2]);
    result.tdoData = JtagScanData(sampledBits);
    for (size_t i = 0; i < sampledBits; ++i) {
        size_t byteIdx = 3 + (i / 8);
        if (byteIdx < static_cast<size_t>(resp.payload.size())) {
            bool val = (resp.payload[byteIdx] & (1 << (i % 8))) != 0;
            result.tdoData.setBit(i, val);
        }
    }

    emit tapStateChanged(m_tap.currentState());
    emit transactionCompleted(result);
    return result;
}

JtagTransactionResult JtagClient::shiftDr(const JtagScanData& tdiData, JtagTapState exitState) {
    JtagTransactionResult result;
    if (!m_transport) {
        result.errorMessage = "Transport not initialized";
        return result;
    }

    QByteArray payload;
    payload.append(static_cast<char>(1)); // 1 = ShiftDR
    uint16_t bits = static_cast<uint16_t>(tdiData.bitCount());
    payload.append(static_cast<char>((bits >> 8) & 0xFF));
    payload.append(static_cast<char>(bits & 0xFF));
    payload.append(static_cast<char>(exitState));
    for (uint8_t b : tdiData.bytes()) {
        payload.append(static_cast<char>(b));
    }

    DebugPacket req = DebugProtocol::createRequest(DebugCommand::JTAG_TRANSACTION, ++m_seq, payload);
    DebugPacket resp = m_transport->sendTransaction(req);

    if (resp.status != DebugStatus::OK || resp.payload.size() < 3) {
        result.errorMessage = QString("Shift-DR failed: %1").arg(debugStatusToString(resp.status));
        emit errorOccurred(result.errorMessage);
        return result;
    }

    result.success = true;
    result.finalState = static_cast<JtagTapState>(resp.payload[0]);
    m_tap.forceState(result.finalState);

    uint16_t sampledBits = (static_cast<uint8_t>(resp.payload[1]) << 8) | static_cast<uint8_t>(resp.payload[2]);
    result.tdoData = JtagScanData(sampledBits);
    for (size_t i = 0; i < sampledBits; ++i) {
        size_t byteIdx = 3 + (i / 8);
        if (byteIdx < static_cast<size_t>(resp.payload.size())) {
            bool val = (resp.payload[byteIdx] & (1 << (i % 8))) != 0;
            result.tdoData.setBit(i, val);
        }
    }

    emit tapStateChanged(m_tap.currentState());
    emit transactionCompleted(result);
    return result;
}

} // namespace udoo::debug
