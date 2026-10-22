#pragma once

#include <QString>
#include <QJsonObject>
#include <QVector>
#include <QList>
#include <cstdint>
#include "Types.h"
#include "CaptureProcessor.h"
#include "TransactionModel.h"

namespace UdooKey {

struct InstrumentReportData {
    QString title = "UDOO KEY Instrument Diagnostic & Capture Report";
    QString generatedAt;
    
    // Hardware & Device
    QString hardwareModel = "UDOO KEY";
    QString firmwareVersion = "1.2.0";
    QString mcuPrimary = "RP2040 (133 MHz Cortex-M0+)";
    QString mcuCoprocessor = "ESP32 (240 MHz Xtensa LX6)";
    QString connectionPort = "USB CDC";

    // Capture Acquisition
    uint32_t sampleRateHz = 10'000'000;
    uint32_t sampleCount = 65536;
    double durationSec = 0.0065536;
    QString triggerSummary = "Immediate (CH0 Rising Edge)";

    // Channel Metrics
    ChannelMetrics metrics[8];

    // Cursors & Measurements
    double cursorASec = 0.001;
    double cursorBSec = 0.003;
    double cursorCSec = 0.004;
    double cursorDSec = 0.005;

    // Decoded Protocol Packets
    QString activeProtocol = "None";
    QVector<DecodedPacket> decodedPackets;

    // Recent Transactions
    QList<TransactionResult> recentTransactions;
};

class InstrumentReportGenerator {
public:
    static QString generateHtmlReport(const InstrumentReportData& data);
    static QJsonObject generateJsonReport(const InstrumentReportData& data);

    static bool saveHtmlReport(const QString& filePath, const InstrumentReportData& data, QString* errorMsg = nullptr);
    static bool saveJsonReport(const QString& filePath, const InstrumentReportData& data, QString* errorMsg = nullptr);
};

} // namespace UdooKey
