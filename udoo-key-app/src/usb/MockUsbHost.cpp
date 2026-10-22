#include "MockUsbHost.h"
#include "Logger.h"

namespace UdooKey {

MockUsbHost::MockUsbHost() {
    setupSimulatedDevices();
}

bool MockUsbHost::initialize() {
    m_initialized = true;
    LOG_INFO("Mock USB Host initialized");
    return true;
}

void MockUsbHost::shutdown() {
    closeDevice();
    m_initialized = false;
    LOG_INFO("Mock USB Host shut down");
}

void MockUsbHost::setupSimulatedDevices() {
    m_devices.clear();

    // 1. UDOO KEY Composite Instrument
    {
        SimulatedDevice dev;
        dev.info.vendorId = 0x2E8A;
        dev.info.productId = 0x105A;
        dev.info.bcdDevice = 0x0120;
        dev.info.busNumber = 1;
        dev.info.deviceAddress = 4;
        dev.info.deviceClass = 0xEF; // Miscellaneous
        dev.info.deviceSubClass = 0x02;
        dev.info.deviceProtocol = 0x01;
        dev.info.manufacturer = "SECO / UDOO";
        dev.info.product = "UDOO KEY Multi-Instrument";
        dev.info.serialNumber = "UK2026-004289";
        dev.info.path = "1-4";
        dev.info.isUdooKey = true;

        dev.devDesc.bLength = 18;
        dev.devDesc.bDescriptorType = 1;
        dev.devDesc.bcdUSB = 0x0200;
        dev.devDesc.bDeviceClass = 0xEF;
        dev.devDesc.bDeviceSubClass = 0x02;
        dev.devDesc.bDeviceProtocol = 0x01;
        dev.devDesc.bMaxPacketSize0 = 64;
        dev.devDesc.idVendor = 0x2E8A;
        dev.devDesc.idProduct = 0x105A;
        dev.devDesc.bcdDevice = 0x0120;
        dev.devDesc.iManufacturer = 1;
        dev.devDesc.iProduct = 2;
        dev.devDesc.iSerialNumber = 3;
        dev.devDesc.bNumConfigurations = 1;

        dev.configDesc.bLength = 9;
        dev.configDesc.bDescriptorType = 2;
        dev.configDesc.bNumInterfaces = 4;
        dev.configDesc.bConfigurationValue = 1;
        dev.configDesc.iConfiguration = 0;
        dev.configDesc.bmAttributes = 0xC0; // Self-powered
        dev.configDesc.bMaxPower = 100; // 200mA

        // Interface 0: CDC Comm
        InterfaceDescriptor if0;
        if0.bLength = 9;
        if0.bDescriptorType = 4;
        if0.bInterfaceNumber = 0;
        if0.bAlternateSetting = 0;
        if0.bNumEndpoints = 1;
        if0.bInterfaceClass = 0x02; // CDC
        if0.bInterfaceSubClass = 0x02;
        if0.bInterfaceProtocol = 0x01;
        if0.iInterface = 0;
        EndpointDescriptor ep0_in;
        ep0_in.bEndpointAddress = 0x81;
        ep0_in.bmAttributes = 0x03; // Interrupt
        ep0_in.wMaxPacketSize = 16;
        ep0_in.bInterval = 16;
        if0.endpoints.append(ep0_in);
        dev.configDesc.interfaces.append(if0);

        // Interface 1: CDC Data
        InterfaceDescriptor if1;
        if1.bLength = 9;
        if1.bDescriptorType = 4;
        if1.bInterfaceNumber = 1;
        if1.bAlternateSetting = 0;
        if1.bNumEndpoints = 2;
        if1.bInterfaceClass = 0x0A; // CDC Data
        if1.bInterfaceSubClass = 0x00;
        if1.bInterfaceProtocol = 0x00;
        if1.iInterface = 0;
        EndpointDescriptor ep1_out;
        ep1_out.bEndpointAddress = 0x01;
        ep1_out.bmAttributes = 0x02; // Bulk
        ep1_out.wMaxPacketSize = 64;
        ep1_out.bInterval = 0;
        EndpointDescriptor ep1_in;
        ep1_in.bEndpointAddress = 0x82;
        ep1_in.bmAttributes = 0x02; // Bulk
        ep1_in.wMaxPacketSize = 64;
        ep1_in.bInterval = 0;
        if1.endpoints.append(ep1_out);
        if1.endpoints.append(ep1_in);
        dev.configDesc.interfaces.append(if1);

        // Interface 2: Vendor Instrument / Logic Analyzer
        InterfaceDescriptor if2;
        if2.bLength = 9;
        if2.bDescriptorType = 4;
        if2.bInterfaceNumber = 2;
        if2.bAlternateSetting = 0;
        if2.bNumEndpoints = 2;
        if2.bInterfaceClass = 0xFF; // Vendor
        if2.bInterfaceSubClass = 0x5A;
        if2.bInterfaceProtocol = 0x01;
        if2.iInterface = 0;
        EndpointDescriptor ep2_out;
        ep2_out.bEndpointAddress = 0x02;
        ep2_out.bmAttributes = 0x02; // Bulk
        ep2_out.wMaxPacketSize = 64;
        EndpointDescriptor ep2_in;
        ep2_in.bEndpointAddress = 0x83;
        ep2_in.bmAttributes = 0x02; // Bulk
        ep2_in.wMaxPacketSize = 64;
        if2.endpoints.append(ep2_out);
        if2.endpoints.append(ep2_in);
        dev.configDesc.interfaces.append(if2);

        // Interface 3: Hardware Debug (JTAG / SWD Engine)
        InterfaceDescriptor if3;
        if3.bLength = 9;
        if3.bDescriptorType = 4;
        if3.bInterfaceNumber = 3;
        if3.bAlternateSetting = 0;
        if3.bNumEndpoints = 2;
        if3.bInterfaceClass = 0xFF; // Vendor
        if3.bInterfaceSubClass = 0xDB;
        if3.bInterfaceProtocol = 0x02;
        if3.iInterface = 0;
        EndpointDescriptor ep3_out;
        ep3_out.bEndpointAddress = 0x03;
        ep3_out.bmAttributes = 0x02; // Bulk
        ep3_out.wMaxPacketSize = 64;
        EndpointDescriptor ep3_in;
        ep3_in.bEndpointAddress = 0x84;
        ep3_in.bmAttributes = 0x02; // Bulk
        ep3_in.wMaxPacketSize = 64;
        if3.endpoints.append(ep3_out);
        if3.endpoints.append(ep3_in);
        dev.configDesc.interfaces.append(if3);

        dev.strings[1] = "SECO / UDOO";
        dev.strings[2] = "UDOO KEY Multi-Instrument";
        dev.strings[3] = "UK2026-004289";

        m_devices[(0x2E8A << 16) | 0x105A] = dev;
    }

    // 2. High-Speed Flash Drive
    {
        SimulatedDevice dev;
        dev.info.vendorId = 0x0781;
        dev.info.productId = 0x5581;
        dev.info.bcdDevice = 0x0100;
        dev.info.busNumber = 1;
        dev.info.deviceAddress = 2;
        dev.info.deviceClass = 0x00;
        dev.info.manufacturer = "SanDisk";
        dev.info.product = "Ultra USB 3.0";
        dev.info.serialNumber = "0401b4430f89";
        dev.info.path = "1-2";

        dev.devDesc.bLength = 18;
        dev.devDesc.bDescriptorType = 1;
        dev.devDesc.bcdUSB = 0x0210;
        dev.devDesc.idVendor = 0x0781;
        dev.devDesc.idProduct = 0x5581;
        dev.devDesc.bMaxPacketSize0 = 64;
        dev.devDesc.bNumConfigurations = 1;

        dev.configDesc.bLength = 9;
        dev.configDesc.bDescriptorType = 2;
        dev.configDesc.bNumInterfaces = 1;
        dev.configDesc.bConfigurationValue = 1;
        dev.configDesc.bmAttributes = 0x80;
        dev.configDesc.bMaxPower = 112; // 224mA

        InterfaceDescriptor if0;
        if0.bLength = 9;
        if0.bDescriptorType = 4;
        if0.bInterfaceNumber = 0;
        if0.bInterfaceClass = 0x08; // Mass Storage
        if0.bInterfaceSubClass = 0x06; // SCSI
        if0.bInterfaceProtocol = 0x50; // Bulk-Only
        if0.bNumEndpoints = 2;

        EndpointDescriptor ep_in;
        ep_in.bEndpointAddress = 0x81;
        ep_in.bmAttributes = 0x02; // Bulk
        ep_in.wMaxPacketSize = 512;
        EndpointDescriptor ep_out;
        ep_out.bEndpointAddress = 0x02;
        ep_out.bmAttributes = 0x02; // Bulk
        ep_out.wMaxPacketSize = 512;
        if0.endpoints.append(ep_in);
        if0.endpoints.append(ep_out);
        dev.configDesc.interfaces.append(if0);

        dev.strings[1] = "SanDisk";
        dev.strings[2] = "Ultra USB 3.0";
        dev.strings[3] = "0401b4430f89";

        m_devices[(0x0781 << 16) | 0x5581] = dev;
    }

    // 3. Test USB Keyboard
    {
        SimulatedDevice dev;
        dev.info.vendorId = 0x046D;
        dev.info.productId = 0xC31C;
        dev.info.bcdDevice = 0x6400;
        dev.info.busNumber = 1;
        dev.info.deviceAddress = 3;
        dev.info.deviceClass = 0x00;
        dev.info.manufacturer = "Logitech";
        dev.info.product = "Keyboard K120";
        dev.info.serialNumber = "";
        dev.info.path = "1-3";

        dev.devDesc.bLength = 18;
        dev.devDesc.bDescriptorType = 1;
        dev.devDesc.bcdUSB = 0x0110;
        dev.devDesc.idVendor = 0x046D;
        dev.devDesc.idProduct = 0xC31C;
        dev.devDesc.bMaxPacketSize0 = 8;
        dev.devDesc.bNumConfigurations = 1;

        dev.configDesc.bLength = 9;
        dev.configDesc.bDescriptorType = 2;
        dev.configDesc.bNumInterfaces = 1;
        dev.configDesc.bConfigurationValue = 1;
        dev.configDesc.bmAttributes = 0xA0; // Bus powered + Remote Wakeup
        dev.configDesc.bMaxPower = 50; // 100mA

        InterfaceDescriptor if0;
        if0.bLength = 9;
        if0.bDescriptorType = 4;
        if0.bInterfaceNumber = 0;
        if0.bInterfaceClass = 0x03; // HID
        if0.bInterfaceSubClass = 0x01; // Boot
        if0.bInterfaceProtocol = 0x01; // Keyboard
        if0.bNumEndpoints = 1;

        EndpointDescriptor ep_in;
        ep_in.bEndpointAddress = 0x81;
        ep_in.bmAttributes = 0x03; // Interrupt
        ep_in.wMaxPacketSize = 8;
        ep_in.bInterval = 10;
        if0.endpoints.append(ep_in);
        dev.configDesc.interfaces.append(if0);

        dev.strings[1] = "Logitech";
        dev.strings[2] = "Keyboard K120";

        m_devices[(0x046D << 16) | 0xC31C] = dev;
    }
}

QList<UsbDeviceInfo> MockUsbHost::enumerateDevices() {
    QList<UsbDeviceInfo> list;
    for (const auto& pair : m_devices) {
        list.append(pair.second.info);
    }
    return list;
}

bool MockUsbHost::openDevice(uint16_t vid, uint16_t pid) {
    uint32_t key = (static_cast<uint32_t>(vid) << 16) | pid;
    if (m_devices.find(key) != m_devices.end()) {
        m_openDeviceKey = key;
        LOG_INFO(QString("Opened Mock USB device 0x%1:0x%2").arg(vid, 4, 16, QChar('0')).arg(pid, 4, 16, QChar('0')));
        return true;
    }
    LOG_WARN(QString("Mock USB device 0x%1:0x%2 not found").arg(vid, 4, 16, QChar('0')).arg(pid, 4, 16, QChar('0')));
    return false;
}

void MockUsbHost::closeDevice() {
    m_openDeviceKey = 0;
}

bool MockUsbHost::isDeviceOpen() const {
    return m_openDeviceKey != 0;
}

UsbDeviceInfo MockUsbHost::currentDeviceInfo() const {
    auto it = m_devices.find(m_openDeviceKey);
    if (it != m_devices.end()) {
        return it->second.info;
    }
    return UsbDeviceInfo{};
}

QByteArray MockUsbHost::makeStringDescriptor(const QString& str) const {
    int byteLen = 2 + (str.length() * 2);
    QByteArray bytes(byteLen, 0);
    bytes[0] = static_cast<char>(byteLen);
    bytes[1] = 0x03; // STRING descriptor type
    const char16_t* u16 = reinterpret_cast<const char16_t*>(str.utf16());
    memcpy(bytes.data() + 2, u16, str.length() * 2);
    return bytes;
}

QByteArray MockUsbHost::getDescriptor(uint8_t descType, uint8_t descIndex, uint16_t /*langId*/, int length) {
    auto it = m_devices.find(m_openDeviceKey);
    if (it == m_devices.end()) return QByteArray();

    const auto& dev = it->second;
    QByteArray result;

    if (descType == static_cast<uint8_t>(UsbDescriptorType::Device)) {
        result = dev.devDesc.toByteArray();
    } else if (descType == static_cast<uint8_t>(UsbDescriptorType::Configuration)) {
        result = dev.configDesc.toByteArray();
    } else if (descType == static_cast<uint8_t>(UsbDescriptorType::String)) {
        if (descIndex == 0) {
            // LangID table: English US (0x0409)
            result = QByteArray("\x04\x03\x09\x04", 4);
        } else if (dev.strings.contains(descIndex)) {
            result = makeStringDescriptor(dev.strings[descIndex]);
        }
    }

    if (result.size() > length) {
        result = result.left(length);
    }
    return result;
}

bool MockUsbHost::controlTransfer(const UsbSetupPacket& setup, const QByteArray& dataOut, QByteArray& dataIn, int /*timeoutMs*/) {
    auto it = m_devices.find(m_openDeviceKey);
    if (it == m_devices.end()) return false;

    if (setup.direction() == UsbDirection::In) {
        if (setup.bRequest == static_cast<uint8_t>(UsbRequestCode::GetDescriptor)) {
            uint8_t descType = (setup.wValue >> 8) & 0xFF;
            uint8_t descIndex = setup.wValue & 0xFF;
            dataIn = getDescriptor(descType, descIndex, setup.wIndex, setup.wLength);
            emit transferCompleted(setup, true, dataIn.size());
            return true;
        } else if (setup.bRequest == static_cast<uint8_t>(UsbRequestCode::GetConfiguration)) {
            dataIn = QByteArray(1, 0x01);
            emit transferCompleted(setup, true, 1);
            return true;
        } else if (setup.bRequest == static_cast<uint8_t>(UsbRequestCode::GetStatus)) {
            dataIn = QByteArray(2, 0x00);
            emit transferCompleted(setup, true, 2);
            return true;
        }
    } else {
        // OUT control request
        if (setup.bRequest == static_cast<uint8_t>(UsbRequestCode::SetConfiguration) ||
            setup.bRequest == static_cast<uint8_t>(UsbRequestCode::SetInterface)) {
            emit transferCompleted(setup, true, 0);
            return true;
        }
    }

    emit transferCompleted(setup, true, dataOut.size());
    return true;
}

bool MockUsbHost::bulkWrite(uint8_t endpoint, const QByteArray& data, int /*timeoutMs*/) {
    auto it = m_devices.find(m_openDeviceKey);
    if (it == m_devices.end()) return false;

    it->second.endpointBuffers[endpoint & 0x7F].append(data);
    return true;
}

bool MockUsbHost::bulkRead(uint8_t endpoint, QByteArray& data, int length, int /*timeoutMs*/) {
    auto it = m_devices.find(m_openDeviceKey);
    if (it == m_devices.end()) return false;

    uint8_t epNum = endpoint & 0x7F;
    auto& buf = it->second.endpointBuffers[epNum];
    if (!buf.isEmpty()) {
        int take = std::min(length, static_cast<int>(buf.size()));
        data = buf.left(take);
        buf = buf.mid(take);
    } else {
        // Return test pattern bytes
        data.resize(length);
        for (int i = 0; i < length; ++i) {
            data[i] = static_cast<char>((0x5A + i) & 0xFF);
        }
    }
    return true;
}

bool MockUsbHost::interruptWrite(uint8_t endpoint, const QByteArray& data, int timeoutMs) {
    return bulkWrite(endpoint, data, timeoutMs);
}

bool MockUsbHost::interruptRead(uint8_t endpoint, QByteArray& data, int length, int timeoutMs) {
    return bulkRead(endpoint, data, length, timeoutMs);
}

} // namespace UdooKey
