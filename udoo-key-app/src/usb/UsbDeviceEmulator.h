#pragma once

#include <QObject>
#include <QByteArray>
#include <QTimer>
#include <QMutex>

namespace UdooKey {

enum class UsbEmulatorMode {
    Stopped,
    CdcAcmLoopback,
    HidTelemetryGenerator,
    ThroughputStressTest
};

class UsbDeviceEmulator : public QObject {
    Q_OBJECT

public:
    explicit UsbDeviceEmulator(QObject* parent = nullptr);
    ~UsbDeviceEmulator() override;

    void start(UsbEmulatorMode mode, int intervalMs = 20);
    void stop();
    bool isRunning() const { return m_mode != UsbEmulatorMode::Stopped; }
    UsbEmulatorMode mode() const { return m_mode; }

    // Host interface interactions
    void writeOutData(uint8_t endpoint, const QByteArray& data);
    QByteArray readInData(uint8_t endpoint, int maxBytes);

signals:
    void telemetryReportGenerated(const QByteArray& report);
    void statisticsUpdated(uint64_t totalBytes, double throughputKBs);

private slots:
    void onTick();

private:
    UsbEmulatorMode m_mode = UsbEmulatorMode::Stopped;
    QTimer* m_timer;
    mutable QMutex m_mutex;
    QByteArray m_inQueue;
    uint64_t m_totalBytes = 0;
    uint32_t m_packetCounter = 0;
};

} // namespace UdooKey
