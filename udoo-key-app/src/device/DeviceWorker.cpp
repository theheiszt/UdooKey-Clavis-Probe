#include "DeviceWorker.h"
#include <QMutex>

namespace UdooKey {

DeviceWorker::DeviceWorker(ITransport* transport, QObject* parent)
    : QThread(parent)
    , m_transport(transport) {
    if (m_transport) {
        connect(m_transport, &ITransport::dataReady, this, &DeviceWorker::dataReceived, Qt::DirectConnection);
    }
}

DeviceWorker::~DeviceWorker() {
    stop();
}

void DeviceWorker::postWrite(const QByteArray& data) {
    if (m_transport && m_transport->isOpen()) {
        qint64 written = m_transport->write(data);
        emit writeCompleted(written);
    }
}

void DeviceWorker::stop() {
    m_running = false;
    quit();
    wait();
}

void DeviceWorker::run() {
    exec(); // Run thread event loop
}

} // namespace UdooKey
