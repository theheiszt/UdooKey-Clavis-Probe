#include "SerialTransport.h"
#include "Logger.h"

namespace UdooKey {

SerialTransport::SerialTransport(QObject* parent)
    : ITransport(parent)
    , m_serialPort(std::make_unique<QSerialPort>()) {
    
    connect(m_serialPort.get(), &QSerialPort::readyRead, this, &SerialTransport::onReadyRead);
    connect(m_serialPort.get(), &QSerialPort::errorOccurred, this, &SerialTransport::onErrorOccurred);
}

SerialTransport::~SerialTransport() {
    close();
}

bool SerialTransport::open() {
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }

    m_serialPort->setPortName(m_portName);
    m_serialPort->setBaudRate(m_baudRate);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serialPort->open(QIODevice::ReadWrite)) {
        // Assert DTR and RTS for USB CDC ACM line state
        m_serialPort->setDataTerminalReady(true);
        m_serialPort->setRequestToSend(true);

        LOG_INFO(QString("[Serial] Open %1 @ %2 baud (8N1, DTR=1, RTS=1)").arg(m_portName).arg(m_baudRate));
        emit connected();
        return true;
    }

    QString err = m_serialPort->errorString();
#if defined(Q_OS_LINUX)
    if (m_serialPort->error() == QSerialPort::PermissionError || err.contains("Permission denied", Qt::CaseInsensitive)) {
        err += "\n\nLinux Permission Diagnostics:\n"
               "Current user does not have permission to access '" + m_portName + "'.\n"
               "To resolve this, add your user to the 'dialout' group:\n"
               "  sudo usermod -a -G dialout $USER\n"
               "And install the udev rules:\n"
               "  sudo cp dist/linux/99-udoo-key.rules /etc/udev/rules.d/\n"
               "  sudo udevadm control --reload-rules && sudo udevadm trigger\n"
               "Then log out and log back in.";
    }
#endif
    LOG_ERROR(QString("[Serial] ERROR: Failed to open %1: %2").arg(m_portName, err));
    emit errorOccurred(err);
    return false;
}

void SerialTransport::close() {
    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->setDataTerminalReady(false);
        m_serialPort->setRequestToSend(false);
        m_serialPort->close();
        LOG_INFO(QString("[Serial] Closed %1").arg(m_portName));
        emit disconnected();
    }
}

bool SerialTransport::isOpen() const {
    return m_serialPort && m_serialPort->isOpen();
}

qint64 SerialTransport::write(const QByteArray& data) {
    if (!isOpen() || data.isEmpty()) return -1;

    qint64 written = m_serialPort->write(data);
    if (written > 0) {
        m_totalTxBytes += written;
        emit rawBytesTransmitted(data.left(written));

        // Raw byte hex logging
        QString hexStr = data.left(written).toHex(' ').toUpper();
        if (hexStr.size() > 96) hexStr = hexStr.left(96) + "...";
        LOG_DEBUG(QString("[Serial] TX %1 bytes: %2").arg(written).arg(hexStr));
    }
    return written;
}

qint64 SerialTransport::write(const char* data, qint64 maxSize) {
    if (!isOpen() || !data || maxSize <= 0) return -1;
    return write(QByteArray(data, static_cast<int>(maxSize)));
}

QByteArray SerialTransport::readAll() {
    if (!isOpen()) return QByteArray();
    QByteArray data = m_serialPort->readAll();
    if (!data.isEmpty()) {
        m_totalRxBytes += data.size();
        emit rawBytesReceived(data);

        QString hexStr = data.toHex(' ').toUpper();
        if (hexStr.size() > 96) hexStr = hexStr.left(96) + "...";
        LOG_DEBUG(QString("[Serial] RX %1 bytes: %2").arg(data.size()).arg(hexStr));
    }
    return data;
}

qint64 SerialTransport::bytesAvailable() const {
    if (!isOpen()) return 0;
    return m_serialPort->bytesAvailable();
}

QString SerialTransport::portName() const {
    return m_portName;
}

void SerialTransport::setPortName(const QString& name) {
    m_portName = name;
}

int SerialTransport::baudRate() const {
    return m_baudRate;
}

void SerialTransport::setBaudRate(int baud) {
    m_baudRate = baud;
    if (isOpen()) {
        m_serialPort->setBaudRate(baud);
    }
}

void SerialTransport::onReadyRead() {
    QByteArray data = m_serialPort->readAll();
    if (!data.isEmpty()) {
        m_totalRxBytes += data.size();
        emit rawBytesReceived(data);

        QString hexStr = data.toHex(' ').toUpper();
        if (hexStr.size() > 96) hexStr = hexStr.left(96) + "...";
        LOG_DEBUG(QString("[Serial] RX %1 bytes: %2").arg(data.size()).arg(hexStr));

        emit dataReady(data);
    }
}

void SerialTransport::onErrorOccurred(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::NoError) return;
    
    QString errStr = m_serialPort->errorString();
    LOG_WARN(QString("Serial port error (%1): %2").arg(static_cast<int>(error)).arg(errStr));
    emit errorOccurred(errStr);
    
    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError) {
        close();
    }
}

} // namespace UdooKey
