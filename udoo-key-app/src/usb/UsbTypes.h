#pragma once

#include <cstdint>
#include <QString>
#include <QByteArray>

namespace UdooKey {

// USB Standard Request Codes (Table 9-4)
enum class UsbRequestCode : uint8_t {
    GetStatus        = 0x00,
    ClearFeature     = 0x01,
    SetFeature       = 0x03,
    SetAddress       = 0x05,
    GetDescriptor    = 0x06,
    SetDescriptor    = 0x07,
    GetConfiguration = 0x08,
    SetConfiguration = 0x09,
    GetInterface     = 0x0A,
    SetInterface     = 0x0B,
    SynchFrame       = 0x0C
};

// USB Descriptor Types (Table 9-5)
enum class UsbDescriptorType : uint8_t {
    Device                    = 0x01,
    Configuration             = 0x02,
    String                    = 0x03,
    Interface                 = 0x04,
    Endpoint                  = 0x05,
    DeviceQualifier           = 0x06,
    OtherSpeedConfiguration   = 0x07,
    InterfacePower            = 0x08,
    Otg                       = 0x09,
    Debug                     = 0x0A,
    InterfaceAssociation      = 0x0B,
    BOS                       = 0x0F,
    DeviceCapability          = 0x10,
    Hid                       = 0x21,
    HidReport                 = 0x22,
    HidPhysical               = 0x23,
    CsInterface               = 0x24,
    CsEndpoint                = 0x25
};

// Endpoint Transfer Type
enum class UsbTransferType : uint8_t {
    Control     = 0,
    Isochronous = 1,
    Bulk        = 2,
    Interrupt   = 3
};

// Direction
enum class UsbDirection : uint8_t {
    Out = 0x00, // Host-to-Device
    In  = 0x80  // Device-to-Host
};

// Recipient
enum class UsbRecipient : uint8_t {
    Device    = 0,
    Interface = 1,
    Endpoint  = 2,
    Other     = 3
};

// Request Type
enum class UsbControlRequestType : uint8_t {
    Standard = 0,
    Class    = 1,
    Vendor   = 2,
    Reserved = 3
};

// USB 8-byte Setup Packet
#pragma pack(push, 1)
struct UsbSetupPacket {
    uint8_t  bmRequestType = 0x80; // Default: In, Standard, Device
    uint8_t  bRequest      = 0x06; // Default: GET_DESCRIPTOR
    uint16_t wValue        = 0x0100; // Device descriptor index 0
    uint16_t wIndex        = 0x0000;
    uint16_t wLength       = 0x0012; // 18 bytes

    QByteArray toByteArray() const {
        return QByteArray(reinterpret_cast<const char*>(this), sizeof(UsbSetupPacket));
    }

    static UsbSetupPacket fromByteArray(const QByteArray& data) {
        UsbSetupPacket pkt;
        if (data.size() >= static_cast<int>(sizeof(UsbSetupPacket))) {
            memcpy(&pkt, data.constData(), sizeof(UsbSetupPacket));
        }
        return pkt;
    }

    UsbDirection direction() const {
        return (bmRequestType & 0x80) ? UsbDirection::In : UsbDirection::Out;
    }

    UsbControlRequestType requestType() const {
        return static_cast<UsbControlRequestType>((bmRequestType >> 5) & 0x03);
    }

    UsbRecipient recipient() const {
        return static_cast<UsbRecipient>(bmRequestType & 0x1F);
    }

    QString directionString() const {
        return direction() == UsbDirection::In ? "IN (Dev→Host)" : "OUT (Host→Dev)";
    }

    QString requestTypeString() const {
        switch (requestType()) {
            case UsbControlRequestType::Standard: return "Standard";
            case UsbControlRequestType::Class:    return "Class";
            case UsbControlRequestType::Vendor:   return "Vendor";
            default:                              return "Reserved";
        }
    }

    QString recipientString() const {
        switch (recipient()) {
            case UsbRecipient::Device:    return "Device";
            case UsbRecipient::Interface: return "Interface";
            case UsbRecipient::Endpoint:  return "Endpoint";
            default:                      return "Other";
        }
    }

    QString requestNameString() const {
        if (requestType() == UsbControlRequestType::Standard) {
            switch (static_cast<UsbRequestCode>(bRequest)) {
                case UsbRequestCode::GetStatus:        return "GET_STATUS";
                case UsbRequestCode::ClearFeature:     return "CLEAR_FEATURE";
                case UsbRequestCode::SetFeature:       return "SET_FEATURE";
                case UsbRequestCode::SetAddress:       return "SET_ADDRESS";
                case UsbRequestCode::GetDescriptor:    return "GET_DESCRIPTOR";
                case UsbRequestCode::SetDescriptor:    return "SET_DESCRIPTOR";
                case UsbRequestCode::GetConfiguration: return "GET_CONFIGURATION";
                case UsbRequestCode::SetConfiguration: return "SET_CONFIGURATION";
                case UsbRequestCode::GetInterface:     return "GET_INTERFACE";
                case UsbRequestCode::SetInterface:     return "SET_INTERFACE";
                case UsbRequestCode::SynchFrame:       return "SYNCH_FRAME";
            }
        }
        return QString("REQ_0x%1").arg(bRequest, 2, 16, QChar('0')).toUpper();
    }
};
#pragma pack(pop)

// USB Device Info for Host Enumeration
struct UsbDeviceInfo {
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    uint16_t bcdDevice = 0;
    uint8_t busNumber = 0;
    uint8_t deviceAddress = 0;
    uint8_t deviceClass = 0;
    uint8_t deviceSubClass = 0;
    uint8_t deviceProtocol = 0;
    QString manufacturer;
    QString product;
    QString serialNumber;
    QString path;
    bool isUdooKey = false;

    QString summary() const {
        return QString("[%1:%2] %3 %4 (Addr: %5)")
            .arg(vendorId, 4, 16, QChar('0'))
            .arg(productId, 4, 16, QChar('0'))
            .arg(manufacturer.isEmpty() ? "Unknown" : manufacturer)
            .arg(product.isEmpty() ? "Device" : product)
            .arg(deviceAddress);
    }
};

} // namespace UdooKey
