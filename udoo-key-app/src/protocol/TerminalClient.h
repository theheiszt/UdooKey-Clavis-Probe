#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include "ITransport.h"
#include "Types.h"

namespace UdooKey {

class TerminalClient : public QObject {
    Q_OBJECT

public:
    explicit TerminalClient(ITransport* transport, QObject* parent = nullptr);
    ~TerminalClient() override = default;

    void setTransport(ITransport* transport);

    void sendCommand(const QString& command);
    void readAdc();
    void readContinuity();
    void scanI2c();
    void writeSpiHex(const QString& hexBytes);

    void onTerminalTextReceived(const QString& text);

signals:
    void textReceived(const QString& text);
    void promptReceived(const QString& prompt);
    void adcReadingReceived(const MeterReading& reading);
    void continuityResultReceived(bool connected, const QString& detail);
    void i2cScanResultsReceived(const QList<uint8_t>& addresses);

private:
    void parseTerminalOutput(const QString& text);

    ITransport* m_transport;
    QString m_lineBuffer;
};

} // namespace UdooKey
