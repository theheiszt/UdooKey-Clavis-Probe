#include "UsbDeviceEmulator.h"
#include "Logger.h"

namespace UdooKey {

UsbDeviceEmulator::UsbDeviceEmulator(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this)) {
    connect(m_timer, &QTimer::timeout, this, &UsbDeviceEmulator::onTick);
}

UsbDeviceEmulator::~UsbDeviceEmulator() {
    stop();
}

void UsbDeviceEmulator::start(UsbEmulatorMode mode, int intervalMs) {
    stop();
    m_mode = mode;
    m_totalBytes = 0;
    m_packetCounter = 0;
    m_inQueue.clear();

    if (mode != UsbEmulatorMode::Stopped) {
        m_timer->start(intervalMs);
        LOG_INFO(QString("USB Device Emulator started in mode %1").arg(static_cast<int>(mode)));
    }
}

void UsbDeviceEmulator::stop() {
    if (m_timer->isActive()) {
        m_timer->stop();
    }
    m_mode = UsbEmulatorMode::Stopped;
    LOG_INFO("USB Device Emulator stopped");
}

void UsbDeviceEmulator::writeOutData(uint8_t /*endpoint*/, const QByteArray& data) {
    QMutexLocker locker(&m_mutex);
    m_totalBytes += data.size();

    if (m_mode == UsbEmulatorMode::CdcAcmLoopback) {
        // Echo back data
        m_inQueue.append(data);
    }
}

QByteArray UsbDeviceEmulator::readInData(uint8_t /*endpoint*/, int maxBytes) {
    QMutexLocker locker(&m_mutex);
    if (m_inQueue.isEmpty()) {
        return QByteArray();
    }

    int count = std::min(maxBytes, static_cast<int>(m_inQueue.size()));
    QByteArray out = m_inQueue.left(count);
    m_inQueue = m_inQueue.mid(count);
    return out;
}

void UsbDeviceEmulator::onTick() {
    QMutexLocker locker(&m_mutex);
    m_packetCounter++;

    if (m_mode == UsbEmulatorMode::HidTelemetryGenerator) {
        // Generate simulated HID Input Report (8 bytes: Modifier, Reserved, Keycodes...)
        QByteArray report(8, 0);
        report[0] = static_cast<char>(m_packetCounter & 0x01 ? 0x02 : 0x00); // Shift key toggle
        report[2] = static_cast<char>(0x04 + (m_packetCounter % 26));       // Letters A-Z
        m_inQueue.append(report);
        m_totalBytes += report.size();
        emit telemetryReportGenerated(report);
    } else if (m_mode == UsbEmulatorMode::ThroughputStressTest) {
        // Generate 64-byte bulk burst
        QByteArray burst(64, 0);
        for (int i = 0; i < 64; ++i) {
            burst[i] = static_cast<char>((m_packetCounter + i) & 0xFF);
        }
        m_inQueue.append(burst);
        m_totalBytes += burst.size();
    }

    // Estimate throughput
    double throughputKBs = (m_totalBytes / 1024.0);
    emit statisticsUpdated(m_totalBytes, throughputKBs);
}

} // namespace UdooKey
