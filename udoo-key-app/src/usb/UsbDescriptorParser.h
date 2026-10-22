#pragma once

#include <cstdint>
#include <QString>
#include <QByteArray>
#include <QList>
#include <QMap>
#include "UsbTypes.h"

namespace UdooKey {

#pragma pack(push, 1)
struct DeviceDescriptor {
    uint8_t  bLength = 18;
    uint8_t  bDescriptorType = 1;
    uint16_t bcdUSB = 0x0200;
    uint8_t  bDeviceClass = 0;
    uint8_t  bDeviceSubClass = 0;
    uint8_t  bDeviceProtocol = 0;
    uint8_t  bMaxPacketSize0 = 64;
    uint16_t idVendor = 0;
    uint16_t idProduct = 0;
    uint16_t bcdDevice = 0;
    uint8_t  iManufacturer = 0;
    uint8_t  iProduct = 0;
    uint8_t  iSerialNumber = 0;
    uint8_t  bNumConfigurations = 1;

    QByteArray toByteArray() const {
        return QByteArray(reinterpret_cast<const char*>(this), sizeof(DeviceDescriptor));
    }
};

struct EndpointDescriptor {
    uint8_t  bLength = 7;
    uint8_t  bDescriptorType = 5;
    uint8_t  bEndpointAddress = 0;
    uint8_t  bmAttributes = 0;
    uint16_t wMaxPacketSize = 64;
    uint8_t  bInterval = 0;

    uint8_t endpointNumber() const { return bEndpointAddress & 0x0F; }
    bool isIn() const { return (bEndpointAddress & 0x80) != 0; }
    UsbTransferType transferType() const { return static_cast<UsbTransferType>(bmAttributes & 0x03); }
    QString transferTypeString() const;
    QString directionString() const { return isIn() ? "IN" : "OUT"; }

    QByteArray toByteArray() const {
        return QByteArray(reinterpret_cast<const char*>(this), sizeof(EndpointDescriptor));
    }
};

struct RawInterfaceHeader {
    uint8_t  bLength = 9;
    uint8_t  bDescriptorType = 4;
    uint8_t  bInterfaceNumber = 0;
    uint8_t  bAlternateSetting = 0;
    uint8_t  bNumEndpoints = 0;
    uint8_t  bInterfaceClass = 0;
    uint8_t  bInterfaceSubClass = 0;
    uint8_t  bInterfaceProtocol = 0;
    uint8_t  iInterface = 0;
};

struct RawConfigurationHeader {
    uint8_t  bLength = 9;
    uint8_t  bDescriptorType = 2;
    uint16_t wTotalLength = 9;
    uint8_t  bNumInterfaces = 0;
    uint8_t  bConfigurationValue = 1;
    uint8_t  iConfiguration = 0;
    uint8_t  bmAttributes = 0x80;
    uint8_t  bMaxPower = 50;
};
#pragma pack(pop)

struct InterfaceDescriptor {
    uint8_t  bLength = 9;
    uint8_t  bDescriptorType = 4;
    uint8_t  bInterfaceNumber = 0;
    uint8_t  bAlternateSetting = 0;
    uint8_t  bNumEndpoints = 0;
    uint8_t  bInterfaceClass = 0;
    uint8_t  bInterfaceSubClass = 0;
    uint8_t  bInterfaceProtocol = 0;
    uint8_t  iInterface = 0;
    QList<EndpointDescriptor> endpoints;

    QString className() const;
    QByteArray toByteArray() const;
};

struct ConfigurationDescriptor {
    uint8_t  bLength = 9;
    uint8_t  bDescriptorType = 2;
    uint16_t wTotalLength = 9;
    uint8_t  bNumInterfaces = 0;
    uint8_t  bConfigurationValue = 1;
    uint8_t  iConfiguration = 0;
    uint8_t  bmAttributes = 0x80;
    uint8_t  bMaxPower = 50; // In 2mA units (100mA)
    QList<InterfaceDescriptor> interfaces;

    bool isSelfPowered() const { return (bmAttributes & 0x40) != 0; }
    bool supportsRemoteWakeup() const { return (bmAttributes & 0x20) != 0; }
    int maxPowerMilliAmps() const { return bMaxPower * 2; }

    QByteArray toByteArray() const;
};

struct UsbDeviceTree {
    DeviceDescriptor deviceDesc;
    QList<ConfigurationDescriptor> configs;
    QMap<uint8_t, QString> strings;

    QString dumpText() const;
};

class UsbDescriptorParser {
public:
    static bool parseDeviceDescriptor(const QByteArray& raw, DeviceDescriptor& outDesc);
    static bool parseConfigurationTree(const QByteArray& raw, ConfigurationDescriptor& outConfig);
    static QString parseStringDescriptor(const QByteArray& raw);
    static QString resolveClassName(uint8_t classCode);
};

} // namespace UdooKey
