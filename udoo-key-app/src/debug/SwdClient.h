#pragma once

#include <QObject>
#include "IDebugTransport.h"
#include "SwdTypes.h"

namespace udoo::debug {

class SwdClient : public QObject {
    Q_OBJECT

public:
    explicit SwdClient(IDebugTransport* transport, QObject* parent = nullptr);
    ~SwdClient() override = default;

    /// Send 50+ clock SWD line reset sequence
    bool resetLine();

    /// Switch JTAG to SWD (0xE79E), perform reset, read DPIDR
    bool connectTarget(SwdDpidr& outDpidr);

    /// Set SWD clock frequency (Hz)
    bool setClock(uint32_t frequencyHz);

    /// Read a DP register (A[3:2])
    SwdTransferResult readDp(uint8_t addr);

    /// Write a DP register (A[3:2])
    SwdTransferResult writeDp(uint8_t addr, uint32_t value);

    /// Read an AP register
    SwdTransferResult readAp(uint8_t addr);

    /// Write an AP register
    SwdTransferResult writeAp(uint8_t addr, uint32_t value);

    /// Read 32-bit word from memory using MEM-AP
    SwdTransferResult readMem32(uint32_t address);

    /// Write 32-bit word to memory using MEM-AP
    SwdTransferResult writeMem32(uint32_t address, uint32_t value);

    bool isTargetConnected() const { return m_connected; }
    const SwdDpidr& targetDpidr() const { return m_dpidr; }

signals:
    void targetConnected(const SwdDpidr& dpidr);
    void targetDisconnected();
    void logMessage(const QString& message);
    void errorOccurred(const QString& message);

private:
    IDebugTransport* m_transport = nullptr;
    bool m_connected = false;
    SwdDpidr m_dpidr;
    uint8_t m_seq = 0;
};

} // namespace udoo::debug
