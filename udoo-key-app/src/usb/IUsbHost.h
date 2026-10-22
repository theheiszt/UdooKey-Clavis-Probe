#pragma once

#include <QObject>
#include <QList>
#include <QByteArray>
#include "UsbTypes.h"
#include "UsbDescriptorParser.h"

namespace UdooKey {

class IUsbHost : public QObject {
    Q_OBJECT

public:
    ~IUsbHost() override = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual QList<UsbDeviceInfo> enumerateDevices() = 0;
    virtual bool openDevice(uint16_t vid, uint16_t pid) = 0;
    virtual void closeDevice() = 0;
    virtual bool isDeviceOpen() const = 0;
    virtual UsbDeviceInfo currentDeviceInfo() const = 0;

    virtual QByteArray getDescriptor(uint8_t descType, uint8_t descIndex, uint16_t langId = 0, int length = 256) = 0;
    virtual bool controlTransfer(const UsbSetupPacket& setup, const QByteArray& dataOut, QByteArray& dataIn, int timeoutMs = 1000) = 0;
    virtual bool bulkWrite(uint8_t endpoint, const QByteArray& data, int timeoutMs = 1000) = 0;
    virtual bool bulkRead(uint8_t endpoint, QByteArray& data, int length, int timeoutMs = 1000) = 0;
    virtual bool interruptWrite(uint8_t endpoint, const QByteArray& data, int timeoutMs = 1000) = 0;
    virtual bool interruptRead(uint8_t endpoint, QByteArray& data, int length, int timeoutMs = 1000) = 0;

signals:
    void deviceAttached(const UsbDeviceInfo& dev);
    void deviceDetached(uint16_t vid, uint16_t pid);
    void transferCompleted(const UsbSetupPacket& setup, bool success, int bytesTransferred);
    void errorOccurred(const QString& error);
};

} // namespace UdooKey
