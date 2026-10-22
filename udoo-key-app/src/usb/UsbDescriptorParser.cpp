#include "UsbDescriptorParser.h"
#include <cstring>

namespace UdooKey {

QString EndpointDescriptor::transferTypeString() const {
    switch (transferType()) {
        case UsbTransferType::Control:     return "Control";
        case UsbTransferType::Isochronous: return "Isochronous";
        case UsbTransferType::Bulk:        return "Bulk";
        case UsbTransferType::Interrupt:   return "Interrupt";
    }
    return "Unknown";
}

QString InterfaceDescriptor::className() const {
    return UsbDescriptorParser::resolveClassName(bInterfaceClass);
}

QByteArray InterfaceDescriptor::toByteArray() const {
    RawInterfaceHeader hdr{bLength, bDescriptorType, bInterfaceNumber, bAlternateSetting,
                           bNumEndpoints, bInterfaceClass, bInterfaceSubClass, bInterfaceProtocol, iInterface};
    QByteArray bytes(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    for (const auto& ep : endpoints) {
        bytes.append(ep.toByteArray());
    }
    return bytes;
}

QByteArray ConfigurationDescriptor::toByteArray() const {
    RawConfigurationHeader hdr{bLength, bDescriptorType, wTotalLength, bNumInterfaces,
                               bConfigurationValue, iConfiguration, bmAttributes, bMaxPower};
    QByteArray bytes(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    for (const auto& iface : interfaces) {
        bytes.append(iface.toByteArray());
    }
    // Update wTotalLength
    if (bytes.size() >= 4) {
        uint16_t total = static_cast<uint16_t>(bytes.size());
        memcpy(bytes.data() + 2, &total, sizeof(uint16_t));
    }
    return bytes;
}

QString UsbDeviceTree::dumpText() const {
    QString out;
    out += QString("Device Descriptor:\n");
    out += QString("  • USB Version: %1.%2\n").arg((deviceDesc.bcdUSB >> 8) & 0xFF, 1, 16).arg(deviceDesc.bcdUSB & 0xFF, 2, 16, QChar('0'));
    out += QString("  • VID:PID: 0x%1:0x%2\n").arg(deviceDesc.idVendor, 4, 16, QChar('0')).arg(deviceDesc.idProduct, 4, 16, QChar('0')).toUpper();
    out += QString("  • Device Class: 0x%1 (%2)\n").arg(deviceDesc.bDeviceClass, 2, 16, QChar('0')).arg(UsbDescriptorParser::resolveClassName(deviceDesc.bDeviceClass));
    out += QString("  • Max Packet Size 0: %1 bytes\n").arg(deviceDesc.bMaxPacketSize0);
    out += QString("  • Number of Configurations: %1\n").arg(deviceDesc.bNumConfigurations);

    for (int c = 0; c < configs.size(); ++c) {
        const auto& cfg = configs[c];
        out += QString("\nConfiguration %1 (Value: %2, Power: %3 mA):\n")
                   .arg(c + 1).arg(cfg.bConfigurationValue).arg(cfg.maxPowerMilliAmps());

        for (int i = 0; i < cfg.interfaces.size(); ++i) {
            const auto& iface = cfg.interfaces[i];
            out += QString("  └─ Interface %1 (Alt: %2, Class: %3 - %4, Endpoints: %5):\n")
                       .arg(iface.bInterfaceNumber)
                       .arg(iface.bAlternateSetting)
                       .arg(iface.bInterfaceClass, 2, 16, QChar('0'))
                       .arg(iface.className())
                       .arg(iface.bNumEndpoints);

            for (const auto& ep : iface.endpoints) {
                out += QString("      └─ Endpoint 0x%1 (%2 %3, MaxPacket: %4, Interval: %5 ms)\n")
                           .arg(ep.bEndpointAddress, 2, 16, QChar('0')).toUpper()
                           .arg(ep.directionString())
                           .arg(ep.transferTypeString())
                           .arg(ep.wMaxPacketSize)
                           .arg(ep.bInterval);
            }
        }
    }
    return out;
}

bool UsbDescriptorParser::parseDeviceDescriptor(const QByteArray& raw, DeviceDescriptor& outDesc) {
    if (raw.size() < static_cast<int>(sizeof(DeviceDescriptor))) {
        return false;
    }
    memcpy(&outDesc, raw.constData(), sizeof(DeviceDescriptor));
    return outDesc.bLength == 18 && outDesc.bDescriptorType == 1;
}

bool UsbDescriptorParser::parseConfigurationTree(const QByteArray& raw, ConfigurationDescriptor& outConfig) {
    if (raw.size() < static_cast<int>(sizeof(RawConfigurationHeader))) {
        return false;
    }

    const uint8_t* p = reinterpret_cast<const uint8_t*>(raw.constData());
    int total = raw.size();

    // Check config header
    if (p[0] != 9 || p[1] != 2) {
        return false;
    }

    RawConfigurationHeader cfgHdr;
    memcpy(&cfgHdr, p, sizeof(cfgHdr));
    outConfig.bLength = cfgHdr.bLength;
    outConfig.bDescriptorType = cfgHdr.bDescriptorType;
    outConfig.wTotalLength = cfgHdr.wTotalLength;
    outConfig.bNumInterfaces = cfgHdr.bNumInterfaces;
    outConfig.bConfigurationValue = cfgHdr.bConfigurationValue;
    outConfig.iConfiguration = cfgHdr.iConfiguration;
    outConfig.bmAttributes = cfgHdr.bmAttributes;
    outConfig.bMaxPower = cfgHdr.bMaxPower;
    outConfig.interfaces.clear();

    int offset = sizeof(RawConfigurationHeader);
    InterfaceDescriptor currentIface;
    bool inIface = false;

    while (offset < total) {
        uint8_t len = p[offset];
        if (len == 0 || offset + len > total) {
            break; // malformed descriptor
        }
        uint8_t descType = p[offset + 1];

        if (descType == 4) { // Interface Descriptor
            if (inIface) {
                outConfig.interfaces.append(currentIface);
                currentIface = InterfaceDescriptor{};
            }
            if (len >= sizeof(RawInterfaceHeader)) {
                RawInterfaceHeader ifHdr;
                memcpy(&ifHdr, p + offset, sizeof(ifHdr));
                currentIface.bLength = ifHdr.bLength;
                currentIface.bDescriptorType = ifHdr.bDescriptorType;
                currentIface.bInterfaceNumber = ifHdr.bInterfaceNumber;
                currentIface.bAlternateSetting = ifHdr.bAlternateSetting;
                currentIface.bNumEndpoints = ifHdr.bNumEndpoints;
                currentIface.bInterfaceClass = ifHdr.bInterfaceClass;
                currentIface.bInterfaceSubClass = ifHdr.bInterfaceSubClass;
                currentIface.bInterfaceProtocol = ifHdr.bInterfaceProtocol;
                currentIface.iInterface = ifHdr.iInterface;
                currentIface.endpoints.clear();
                inIface = true;
            }
        } else if (descType == 5 && inIface) { // Endpoint Descriptor
            if (len >= static_cast<int>(sizeof(EndpointDescriptor))) {
                EndpointDescriptor ep;
                memcpy(&ep, p + offset, sizeof(EndpointDescriptor));
                currentIface.endpoints.append(ep);
            }
        }

        offset += len;
    }

    if (inIface) {
        outConfig.interfaces.append(currentIface);
    }

    return true;
}

QString UsbDescriptorParser::parseStringDescriptor(const QByteArray& raw) {
    if (raw.size() < 2) return QString();
    uint8_t len = static_cast<uint8_t>(raw[0]);
    uint8_t type = static_cast<uint8_t>(raw[1]);
    if (type != 3 || len > raw.size()) return QString();

    int chars = (len - 2) / 2;
    if (chars <= 0) return QString();

    const char16_t* u16 = reinterpret_cast<const char16_t*>(raw.constData() + 2);
    return QString::fromUtf16(u16, chars);
}

QString UsbDescriptorParser::resolveClassName(uint8_t classCode) {
    switch (classCode) {
        case 0x00: return "Device/Interface Specific";
        case 0x01: return "Audio";
        case 0x02: return "Communications & CDC Control";
        case 0x03: return "Human Interface Device (HID)";
        case 0x05: return "Physical";
        case 0x06: return "Image / Still Camera";
        case 0x07: return "Printer";
        case 0x08: return "Mass Storage";
        case 0x09: return "Hub";
        case 0x0A: return "CDC-Data";
        case 0x0B: return "Smart Card";
        case 0x0D: return "Content Security";
        case 0x0E: return "Video";
        case 0x0F: return "Personal Healthcare";
        case 0x10: return "Audio/Video Devices";
        case 0xDC: return "Diagnostic Device";
        case 0xE0: return "Wireless Controller (Bluetooth)";
        case 0xEF: return "Miscellaneous (Composite)";
        case 0xFE: return "Application Specific (DFU)";
        case 0xFF: return "Vendor Specific";
        default:   return QString("Class 0x%1").arg(classCode, 2, 16, QChar('0')).toUpper();
    }
}

} // namespace UdooKey
