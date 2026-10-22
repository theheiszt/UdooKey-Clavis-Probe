#pragma once

#include "IUsbHost.h"
#include <map>
#include <QByteArray>

namespace UdooKey {

class MockUsbHost : public IUsbHost {
    Q_OBJECT

public:
    MockUsbHost();
    ~MockUsbHost() override = default;

    bool initialize() override;
    void shutdown() override;

    QList<UsbDeviceInfo> enumerateDevices() override;
    bool openDevice(uint16_t vid, uint16_t pid) override;
    void closeDevice() override;
    bool isDeviceOpen() const override;
    UsbDeviceInfo currentDeviceInfo() const override;

    QByteArray getDescriptor(uint8_t descType, uint8_t descIndex, uint16_t langId = 0, int length = 256) override;
    bool controlTransfer(const UsbSetupPacket& setup, const QByteArray& dataOut, QByteArray& dataIn, int timeoutMs = 1000) override;
    bool bulkWrite(uint8_t endpoint, const QByteArray& data, int timeoutMs = 1000) override;
    bool bulkRead(uint8_t endpoint, QByteArray& data, int length, int timeoutMs = 1000) override;
    bool interruptWrite(uint8_t endpoint, const QByteArray& data, int timeoutMs = 1000) override;
    bool interruptRead(uint8_t endpoint, QByteArray& data, int length, int timeoutMs = 1000) override;

private:
    struct SimulatedDevice {
        UsbDeviceInfo info;
        DeviceDescriptor devDesc;
        ConfigurationDescriptor configDesc;
        QMap<uint8_t, QString> strings;
        std::map<uint8_t, QByteArray> endpointBuffers;
    };

    void setupSimulatedDevices();
    QByteArray makeStringDescriptor(const QString& str) const;

    bool m_initialized = false;
    uint32_t m_openDeviceKey = 0; // (vid << 16) | pid
    std::map<uint32_t, SimulatedDevice> m_devices;
};

} // namespace UdooKey
