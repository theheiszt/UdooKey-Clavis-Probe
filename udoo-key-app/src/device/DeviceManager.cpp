#include "DeviceManager.h"
#include "SerialTransport.h"
#include "MockTransport.h"
#include "Logger.h"
#include "SettingsManager.h"
#include <QTimer>
#include <QFileInfo>

namespace UdooKey {

DeviceManager::DeviceManager() {
    m_deviceCaps = DeviceCapabilities::defaultUdooKey();
}

DeviceManager::~DeviceManager() {
    disconnectDevice();
}

DeviceManager& DeviceManager::instance() {
    static DeviceManager s_instance;
    return s_instance;
}

QList<PortInfo> DeviceManager::availablePorts() const {
    QList<PortInfo> ports;
    const auto serialPorts = QSerialPortInfo::availablePorts();

    for (const auto& info : serialPorts) {
        PortInfo pi;
        pi.portName = info.portName();
        pi.systemLocation = info.systemLocation();
        pi.description = info.description();
        pi.manufacturer = info.manufacturer();
        pi.serialNumber = info.serialNumber();
        pi.vendorId = info.hasVendorIdentifier() ? info.vendorIdentifier() : 0;
        pi.productId = info.hasProductIdentifier() ? info.productIdentifier() : 0;
        pi.isBusy = false;

#if defined(Q_OS_LINUX)
        // Check read/write permission on Linux device node (e.g. /dev/ttyACM0)
        QFileInfo devFile(info.systemLocation().isEmpty() ? ("/dev/" + info.portName()) : info.systemLocation());
        if (devFile.exists()) {
            pi.isWritable = devFile.isReadable() && devFile.isWritable();
        } else {
            pi.isWritable = true;
        }
#else
        pi.isWritable = true;
#endif

        // RP2040 standard USB VID is 0x2E8A
        // UDOO KEY or Pico CDC
        if (pi.vendorId == 0x2E8A || 
            pi.description.contains("UDOO", Qt::CaseInsensitive) || 
            pi.description.contains("Pico", Qt::CaseInsensitive) ||
            pi.manufacturer.contains("Raspberry", Qt::CaseInsensitive)) {
            pi.isUdooKey = true;
        }

        ports.append(pi);
    }

    return ports;
}

bool DeviceManager::connectToPort(const QString& portName, int baudRate) {
    disconnectDevice();

    auto serial = std::make_unique<SerialTransport>();
    serial->setPortName(portName);
    serial->setBaudRate(baudRate);

    connect(serial.get(), &ITransport::connected, this, &DeviceManager::onTransportConnected);
    connect(serial.get(), &ITransport::disconnected, this, &DeviceManager::onTransportDisconnected);
    connect(serial.get(), &ITransport::errorOccurred, this, &DeviceManager::onTransportError);

    m_transport = std::move(serial);

    if (!m_transport->open()) {
        m_transport.reset();
        return false;
    }

    SettingsManager::instance().setLastPort(portName);
    SettingsManager::instance().setLastBaudRate(baudRate);
    probeCapabilities();
    return true;
}

bool DeviceManager::connectMock() {
    disconnectDevice();

    auto mock = std::make_unique<MockTransport>();
    connect(mock.get(), &ITransport::connected, this, &DeviceManager::onTransportConnected);
    connect(mock.get(), &ITransport::disconnected, this, &DeviceManager::onTransportDisconnected);
    connect(mock.get(), &ITransport::errorOccurred, this, &DeviceManager::onTransportError);

    m_transport = std::move(mock);

    if (!m_transport->open()) {
        m_transport.reset();
        return false;
    }

    // Mock has all capabilities
    m_deviceCaps = DeviceCapabilities::mockDevice();
    m_capabilities = m_deviceCaps.toLegacyMask();
    
    emit deviceConnected("MOCK_UDOO_KEY", m_capabilities);
    emit deviceConnectedWithCaps("MOCK_UDOO_KEY", m_deviceCaps);
    emit capabilitiesUpdated(m_capabilities);
    emit capabilitiesChanged(m_deviceCaps);
    return true;
}

void DeviceManager::disconnectDevice() {
    if (m_transport) {
        m_transport->close();
        m_transport.reset();
    }
    m_capabilities = CAP_NONE;
    m_deviceCaps = DeviceCapabilities();
    m_probing = false;
    m_probeBuffer.clear();
}

bool DeviceManager::isConnected() const {
    return m_transport && m_transport->isOpen();
}

ITransport* DeviceManager::transport() const {
    return m_transport.get();
}

uint32_t DeviceManager::capabilities() const {
    return m_capabilities;
}

const DeviceCapabilities& DeviceManager::currentCapabilities() const {
    return m_deviceCaps;
}

void DeviceManager::probeCapabilities() {
    m_probing = true;
    m_probeBuffer.clear();
    m_capabilities = CAP_NONE;

    if (!m_transport) return;

    connect(m_transport.get(), &ITransport::dataReady, this, &DeviceManager::onProbeDataReceived);

    // Send probe string: Send SUMP reset & ID query, plus carriage return for terminal
    // SUMP Reset: 0x00 0x00 0x00 0x00 0x00
    // SUMP ID: 0x02
    // Terminal probe: "\r\n"
    QByteArray probeSeq("\x00\x00\x00\x00\x00\x02\r\n", 8);
    m_transport->write(probeSeq);

    // Give 250ms for responses
    QTimer::singleShot(250, this, [this]() {
        if (!m_probing) return;
        m_probing = false;
        if (m_transport) {
            disconnect(m_transport.get(), &ITransport::dataReady, this, &DeviceManager::onProbeDataReceived);
        }

        m_deviceCaps = DeviceCapabilities::defaultUdooKey();

        // Evaluate probe buffer
        if (m_probeBuffer.contains("1ALS") || m_probeBuffer.contains("SUMP")) {
            m_capabilities |= CAP_LOGIC_ANALYZER | CAP_SUMP_PROTOCOL;
            m_deviceCaps.hasLogicAnalyzer = true;
            m_deviceCaps.hasSumpProtocol = true;
        }
        if (m_probeBuffer.contains("HiZ>") || m_probeBuffer.contains("UDOO KEY") || m_probeBuffer.contains(">")) {
            m_capabilities |= CAP_TERMINAL_SHELL | CAP_ADC_METER | CAP_SPI_TOOL | CAP_I2C_TOOL | CAP_UART_TOOL | CAP_CONTINUITY;
            m_deviceCaps.hasTerminalShell = true;
            m_deviceCaps.hasAdcMeter = true;
            m_deviceCaps.hasContinuity = true;
            m_deviceCaps.hasI2cTool = true;
            m_deviceCaps.hasSpiTool = true;
            m_deviceCaps.hasUartTool = true;
        }

        // If nothing responded specifically, default to basic logic analyzer + terminal capability
        if (m_capabilities == CAP_NONE) {
            m_capabilities = CAP_LOGIC_ANALYZER | CAP_TERMINAL_SHELL;
            m_deviceCaps.hasLogicAnalyzer = true;
            m_deviceCaps.hasTerminalShell = true;
        }

        m_capabilities = m_deviceCaps.toLegacyMask();

        LOG_INFO(QString("Capabilities detected: 0x%1 (%2)")
                 .arg(m_capabilities, 4, 16, QChar('0'))
                 .arg(m_deviceCaps.summary()));
        QString name = m_transport ? m_transport->portName() : "Unknown";
        emit deviceConnected(name, m_capabilities);
        emit deviceConnectedWithCaps(name, m_deviceCaps);
        emit capabilitiesUpdated(m_capabilities);
        emit capabilitiesChanged(m_deviceCaps);

        // If initial prompt text was received during probing, forward it to the active clients
        if (!m_probeBuffer.isEmpty() && m_transport) {
            emit m_transport->dataReady(m_probeBuffer);
        }
    });
}

void DeviceManager::onProbeDataReceived(const QByteArray& data) {
    if (m_probing) {
        m_probeBuffer.append(data);
    }
}

void DeviceManager::onTransportConnected() {
    LOG_INFO("Device transport connected");
}

void DeviceManager::onTransportDisconnected() {
    LOG_INFO("Device transport disconnected");
    emit deviceDisconnected();
}

void DeviceManager::onTransportError(const QString& err) {
    LOG_WARN(QString("Device transport error: %1").arg(err));
    emit connectionError(err);
}

} // namespace UdooKey
