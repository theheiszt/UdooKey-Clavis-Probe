#pragma once

#include <QObject>
#include <memory>
#include "IDebugTransport.h"
#include "JtagTypes.h"
#include "JtagTap.h"

namespace udoo::debug {

class JtagClient : public QObject {
    Q_OBJECT

public:
    explicit JtagClient(IDebugTransport* transport, QObject* parent = nullptr);
    ~JtagClient() override = default;

    /// Reset TAP controller to Test-Logic-Reset state
    bool resetTap();

    /// Set JTAG clock frequency (Hz)
    bool setClock(uint32_t frequencyHz);

    /// Navigate TAP to specified target state via shortest TMS sequence
    bool navigateTap(JtagTapState targetState);

    /// Clock a raw sequence of TMS bits
    bool sendTmsSequence(const std::vector<bool>& tmsBits);

    /// Scan and enumerate devices in the JTAG chain
    std::vector<JtagDevice> scanChain();

    /// Access JTAG chain model
    const JtagChain& chain() const { return m_chain; }
    JtagChain& chain() { return m_chain; }

    /// Discover total IR length and per-device IR lengths via IEEE 1149.1 Capture-IR scan
    bool discoverIrLengths();

    /// Perform targeted Shift-IR for a specific device, putting all other devices in BYPASS
    JtagTransactionResult shiftDeviceIr(size_t devIndex, const JtagScanData& irData, JtagTapState exitState = JtagTapState::RunTestIdle);

    /// Perform targeted Shift-DR for a specific device with automatic 1-bit BYPASS padding
    JtagTransactionResult shiftDeviceDr(size_t devIndex, const JtagScanData& drData, size_t responseBits, JtagTapState exitState = JtagTapState::RunTestIdle);

    /// Execute a standard IEEE 1149.1 instruction (BYPASS, IDCODE, EXTEST, etc.) on target device
    JtagTransactionResult executeStandardInstruction(size_t devIndex, JtagStandardInstruction instruction);

    /// Execute a deterministic batch of operations in a single round-trip
    JtagBatchResult executeBatch(const std::vector<JtagBatchOperation>& operations);

    /// Perform a Shift-IR transaction
    JtagTransactionResult shiftIr(const JtagScanData& tdiData, JtagTapState exitState = JtagTapState::RunTestIdle);

    /// Perform a Shift-DR transaction
    JtagTransactionResult shiftDr(const JtagScanData& tdiData, JtagTapState exitState = JtagTapState::RunTestIdle);

    /// Current tracked TAP state
    JtagTapState currentTapState() const { return m_tap.currentState(); }

    /// List of currently discovered devices
    const std::vector<JtagDevice>& devices() const { return m_devices; }

signals:
    void tapStateChanged(JtagTapState newState);
    void chainScanned(const std::vector<JtagDevice>& devices);
    void transactionCompleted(const JtagTransactionResult& result);
    void batchCompleted(const JtagBatchResult& result);
    void logMessage(const QString& message);
    void errorOccurred(const QString& message);

private:
    IDebugTransport* m_transport = nullptr;
    JtagTap m_tap;
    std::vector<JtagDevice> m_devices;
    JtagChain m_chain;
    uint8_t m_seq = 0;
};

} // namespace udoo::debug
