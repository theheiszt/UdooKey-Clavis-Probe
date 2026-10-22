#pragma once

#include <QThread>
#include <QByteArray>
#include <QString>
#include "ITransport.h"

namespace UdooKey {

class DeviceWorker : public QThread {
    Q_OBJECT

public:
    explicit DeviceWorker(ITransport* transport, QObject* parent = nullptr);
    ~DeviceWorker() override;

    void postWrite(const QByteArray& data);
    void stop();

signals:
    void dataReceived(const QByteArray& data);
    void writeCompleted(qint64 bytesWritten);

protected:
    void run() override;

private:
    ITransport* m_transport;
    std::atomic<bool> m_running{true};
};

} // namespace UdooKey
