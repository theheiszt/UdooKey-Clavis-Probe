#include "BinaryLogicClient.h"
#include "Logger.h"

namespace UdooKey {

BinaryLogicClient::BinaryLogicClient(ITransport* transport, QObject* parent)
    : QObject(parent)
    , m_transport(transport) {
}

void BinaryLogicClient::setTransport(ITransport* transport) {
    m_transport = transport;
}

bool BinaryLogicClient::armCapture(const AcquisitionConfig& config) {
    if (!m_transport || !m_transport->isOpen()) {
        emit captureError("Device transport is not open");
        return false;
    }

    m_currentConfig = config;
    m_armed = true;

    LOG_INFO(QString("Arming capture: Rate=%1 Hz, Depth=%2, TrigCh=%3, Mode=%4")
                 .arg(config.sampleRateHz)
                 .arg(config.sampleDepth)
                 .arg(config.triggerChannel)
                 .arg(static_cast<int>(config.triggerMode)));

    // Request capture from UDOO KEY: "capture\r\n"
    QByteArray cmd = "capture\r\n";
    m_transport->write(cmd);

    emit captureStarted();
    return true;
}

void BinaryLogicClient::triggerCapture() {
    if (!m_transport || !m_transport->isOpen()) return;
    LOG_INFO("Triggering immediate capture");
    QByteArray cmd = "capture\r\n";
    m_transport->write(cmd);
}

void BinaryLogicClient::abortCapture() {
    m_armed = false;
    if (m_transport && m_transport->isOpen()) {
        m_transport->write("\x03\r\n"); // Ctrl+C / abort
    }
}

void BinaryLogicClient::onBinaryFrameReceived(const QByteArray& payload) {
    if (!m_armed) {
        // Unexpected or unprompted binary frame; process anyway if non-empty
        LOG_INFO(QString("Received unprompted binary frame (%1 bytes)").arg(payload.size()));
    }
    m_armed = false;
    LOG_INFO(QString("Capture complete: received %1 samples").arg(payload.size()));
    emit captureComplete(payload, m_currentConfig.sampleRateHz);
}

} // namespace UdooKey
