#pragma once

#include "IDebugTransport.h"
#include "JtagTap.h"
#include "SwdTypes.h"
#include <map>
#include <vector>

namespace udoo::debug {

/**
 * @brief High-fidelity headless simulation transport for JTAG and SWD targets.
 * Enables 100% automated test coverage without physical hardware.
 */
class MockDebugTransport : public IDebugTransport {
    Q_OBJECT

public:
    explicit MockDebugTransport(QObject* parent = nullptr);
    ~MockDebugTransport() override = default;

    bool isConnected() const override { return m_connected; }
    bool open() override;
    void close() override;

    bool sendPacket(const DebugPacket& packet) override;
    DebugPacket sendTransaction(const DebugPacket& request, int timeoutMs = 1000) override;

    // Simulation inspection / configuration hooks
    void setSimulateTargetConnected(bool connected) { m_targetConnected = connected; }
    void setSimulateParityError(bool fail) { m_failParity = fail; }
    void setSimulateTimeout(bool timeout) { m_simulateTimeout = timeout; }

    JtagTapState simulatedTapState() const { return m_tap.currentState(); }
    uint32_t simulatedDpRegister(uint8_t addr) const;
    uint32_t simulatedApRegister(uint8_t addr) const;

private:
    DebugPacket handleRequest(const DebugPacket& req);

    DebugPacket handleGetStatus(const DebugPacket& req);
    DebugPacket handleJtagReset(const DebugPacket& req);
    DebugPacket handleJtagSetClock(const DebugPacket& req);
    DebugPacket handleJtagTmsSeq(const DebugPacket& req);
    DebugPacket handleJtagTransaction(const DebugPacket& req);
    DebugPacket handleJtagBatch(const DebugPacket& req);
    DebugPacket handleJtagScanChain(const DebugPacket& req);
    DebugPacket handleJtagDiscoverIr(const DebugPacket& req);

    DebugPacket handleSwdReset(const DebugPacket& req);
    DebugPacket handleSwdConnect(const DebugPacket& req);
    DebugPacket handleSwdReadDp(const DebugPacket& req);
    DebugPacket handleSwdWriteDp(const DebugPacket& req);
    DebugPacket handleSwdReadAp(const DebugPacket& req);
    DebugPacket handleSwdWriteAp(const DebugPacket& req);

    bool m_connected = false;
    bool m_targetConnected = true;
    bool m_failParity = false;
    bool m_simulateTimeout = false;

    // JTAG Simulation State
    JtagTap m_tap;
    uint32_t m_jtagClockHz = 1000000;
    std::vector<JtagDevice> m_chainDevices;
    std::vector<uint32_t> m_deviceIrRegisters;

    // SWD Simulation State
    bool m_swdConnected = false;
    uint32_t m_swdClockHz = 1000000;
    std::map<uint8_t, uint32_t> m_dpRegisters;
    std::map<uint8_t, uint32_t> m_apRegisters;
    std::map<uint32_t, uint32_t> m_simulatedMemory;
};

} // namespace udoo::debug
