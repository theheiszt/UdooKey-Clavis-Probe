#include "TerminalClient.h"
#include "Logger.h"
#include <QRegularExpression>
#include <QDateTime>

namespace UdooKey {

TerminalClient::TerminalClient(ITransport* transport, QObject* parent)
    : QObject(parent)
    , m_transport(transport) {
}

void TerminalClient::setTransport(ITransport* transport) {
    m_transport = transport;
}

void TerminalClient::sendCommand(const QString& command) {
    if (!m_transport || !m_transport->isOpen()) return;
    QString cmdWithNl = command.trimmed() + "\n";
    m_transport->write(cmdWithNl.toUtf8());
}

void TerminalClient::readAdc() {
    sendCommand("adc");
}

void TerminalClient::readContinuity() {
    sendCommand("cont");
}

void TerminalClient::scanI2c() {
    sendCommand("i2c scan");
}

void TerminalClient::writeSpiHex(const QString& hexBytes) {
    sendCommand(QString("spi write %1").arg(hexBytes));
}

void TerminalClient::onTerminalTextReceived(const QString& text) {
    emit textReceived(text);
    parseTerminalOutput(text);
}

void TerminalClient::parseTerminalOutput(const QString& text) {
    m_lineBuffer.append(text);

    // Prompt check
    if (m_lineBuffer.contains("HiZ>") || m_lineBuffer.contains("UDOO>")) {
        emit promptReceived("HiZ>");
    }

    // Process completed lines
    while (m_lineBuffer.contains('\n')) {
        int idx = m_lineBuffer.indexOf('\n');
        QString line = m_lineBuffer.left(idx).trimmed();
        m_lineBuffer = m_lineBuffer.mid(idx + 1);

        if (line.isEmpty()) continue;

        // 1. ADC Parsing: e.g. "ADC Ch0: 1.652 V (Raw: 2050)" or "Voltage: 3.30 V"
        static QRegularExpression reAdc(R"((?:ADC|Voltage).*?([0-9]+\.[0-9]+)\s*V.*?(?:Raw:\s*([0-9]+))?)", QRegularExpression::CaseInsensitiveOption);
        auto matchAdc = reAdc.match(line);
        if (matchAdc.hasMatch()) {
            MeterReading r;
            r.voltageV = matchAdc.captured(1).toDouble();
            if (matchAdc.captured(2).isEmpty()) {
                r.rawAdc = (r.voltageV / 3.3) * 4095.0;
            } else {
                r.rawAdc = matchAdc.captured(2).toDouble();
            }
            r.continuity = false;
            r.timestampMs = QDateTime::currentMSecsSinceEpoch();
            emit adcReadingReceived(r);
            continue;
        }

        // 2. Continuity Parsing: e.g. "Continuity: CLOSED" / "Continuity: OPEN"
        if (line.contains("Continuity:", Qt::CaseInsensitive)) {
            bool connected = line.contains("CLOSED", Qt::CaseInsensitive) || 
                             line.contains("CONNECTED", Qt::CaseInsensitive) ||
                             line.contains("SHORT", Qt::CaseInsensitive);
            emit continuityResultReceived(connected, line);
            continue;
        }

        // 3. I2C Scan Found Summary: e.g. "Found 1 device(s): [0x50]"
        static QRegularExpression reI2cFound(R"(Found\s+\d+\s+device\(s\):\s*\[(.*?)\])", QRegularExpression::CaseInsensitiveOption);
        auto matchI2c = reI2cFound.match(line);
        if (matchI2c.hasMatch()) {
            QString listStr = matchI2c.captured(1);
            QStringList tokens = listStr.split(QRegularExpression(R"([\s,]+)"), Qt::SkipEmptyParts);
            QList<uint8_t> addrs;
            for (const QString& tok : tokens) {
                bool ok = false;
                uint8_t a = static_cast<uint8_t>(tok.toUInt(&ok, 16));
                if (ok) addrs.append(a);
            }
            emit i2cScanResultsReceived(addrs);
            continue;
        }
    }
}

} // namespace UdooKey
