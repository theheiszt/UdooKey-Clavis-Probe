#pragma once

#include "ITransport.h"
#include <QByteArray>
#include <QTimer>

namespace UdooKey {

class MockTransport : public ITransport {
    Q_OBJECT

public:
    explicit MockTransport(QObject* parent = nullptr);
    ~MockTransport() override = default;

    bool open() override;
    void close() override;
    bool isOpen() const override;

    qint64 write(const QByteArray& data) override;
    qint64 write(const char* data, qint64 maxSize) override;
    QByteArray readAll() override;
    qint64 bytesAvailable() const override;

    QString portName() const override { return m_portName; }
    void setPortName(const QString& name) override { m_portName = name; }

    // Helpers to inject synthetic responses or trigger mock capture
    void injectIncomingData(const QByteArray& data);
    void generateSyntheticCapture(uint32_t sampleCount = 65536);

private:
    void handleCommand(const QByteArray& cmd);

    bool m_open = false;
    QString m_portName = "MOCK_UDOO_KEY";
    QByteArray m_rxBuffer;
};

} // namespace UdooKey
