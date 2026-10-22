#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QSerialPortInfo>
#include <memory>
#include "ITransport.h"
#include "Types.h"
#include "DeviceCapabilities.h"

namespace UdooKey {

struct PortInfo {
    QString portName;
    QString systemLocation;
    QString description;
    QString manufacturer;
    QString serialNumber;
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    bool isUdooKey = false;
    bool isBusy = false;
    bool isWritable = true;
};

class DeviceManager : public QObject {
    Q_OBJECT

public:
    static DeviceManager& instance();

    QList<PortInfo> availablePorts() const;
    bool connectToPort(const QString& portName, int baudRate = 115200);
    bool connectMock();
    void disconnectDevice();
    bool isConnected() const;

    ITransport* transport() const;
    uint32_t capabilities() const;
    const DeviceCapabilities& currentCapabilities() const;

signals:
    void deviceConnected(const QString& portName, uint32_t capabilities);
    void deviceConnectedWithCaps(const QString& portName, const DeviceCapabilities& capabilities);
    void deviceDisconnected();
    void connectionError(const QString& error);
    void capabilitiesUpdated(uint32_t capabilities);
    void capabilitiesChanged(const DeviceCapabilities& capabilities);

private slots:
    void onTransportConnected();
    void onTransportDisconnected();
    void onTransportError(const QString& err);
    void onProbeDataReceived(const QByteArray& data);

private:
    DeviceManager();
    ~DeviceManager() override;
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;

    void probeCapabilities();

    std::unique_ptr<ITransport> m_transport;
    uint32_t m_capabilities = CAP_NONE;
    DeviceCapabilities m_deviceCaps;
    bool m_probing = false;
    QByteArray m_probeBuffer;
};

} // namespace UdooKey
