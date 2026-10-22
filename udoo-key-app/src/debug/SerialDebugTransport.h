#pragma once

#include "IDebugTransport.h"
#include "ITransport.h"
#include <QMutex>
#include <QWaitCondition>

namespace udoo::debug {

class SerialDebugTransport : public IDebugTransport {
    Q_OBJECT

public:
    explicit SerialDebugTransport(UdooKey::ITransport* transport = nullptr, QObject* parent = nullptr);
    ~SerialDebugTransport() override = default;

    void setTransport(UdooKey::ITransport* transport);

    bool isConnected() const override;
    bool open() override;
    void close() override;

    bool sendPacket(const DebugPacket& packet) override;
    DebugPacket sendTransaction(const DebugPacket& request, int timeoutMs = 1000) override;

public slots:
    void onRawDataReceived(const QByteArray& data);

private:
    UdooKey::ITransport* m_transport = nullptr;
    QByteArray m_rxBuffer;
    QMutex m_mutex;
    QWaitCondition m_waitCond;
    DebugPacket m_lastResponse;
    bool m_hasResponse = false;
    uint8_t m_waitingSeq = 0;
};

} // namespace udoo::debug
