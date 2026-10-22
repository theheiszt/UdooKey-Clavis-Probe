#pragma once

#include "ITransport.h"
#include <QSerialPort>
#include <memory>

namespace UdooKey {

class SerialTransport : public ITransport {
    Q_OBJECT

public:
    explicit SerialTransport(QObject* parent = nullptr);
    ~SerialTransport() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;

    qint64 write(const QByteArray& data) override;
    qint64 write(const char* data, qint64 maxSize) override;
    QByteArray readAll() override;
    qint64 bytesAvailable() const override;

    QString portName() const override;
    void setPortName(const QString& name) override;

    int baudRate() const;
    void setBaudRate(int baud);

    // Diagnostics / Wire statistics
    qint64 totalBytesWritten() const { return m_totalTxBytes; }
    qint64 totalBytesRead() const { return m_totalRxBytes; }

signals:
    void rawBytesTransmitted(const QByteArray& data);
    void rawBytesReceived(const QByteArray& data);

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    std::unique_ptr<QSerialPort> m_serialPort;
    QString m_portName;
    int m_baudRate = 115200;
    qint64 m_totalTxBytes = 0;
    qint64 m_totalRxBytes = 0;
};

} // namespace UdooKey
