#include "InstrumentReportGenerator.h"
#include "Theme.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>
#include <algorithm>

namespace UdooKey {

QString InstrumentReportGenerator::generateHtmlReport(const InstrumentReportData& data) {
    QString genTime = data.generatedAt.isEmpty() ? QDateTime::currentDateTime().toString(Qt::ISODate) : data.generatedAt;

    double deltaAB = std::abs(data.cursorBSec - data.cursorASec);
    double freqAB = (deltaAB > 0.0) ? (1.0 / deltaAB) : 0.0;
    double deltaCD = std::abs(data.cursorDSec - data.cursorCSec);
    double freqCD = (deltaCD > 0.0) ? (1.0 / deltaCD) : 0.0;

    QString html;
    QTextStream out(&html);

    out << "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n";
    out << "<title>" << data.title << "</title>\n";
    out << "<style>\n";
    out << "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background-color: #121216; color: #E0E0E6; margin: 0; padding: 24px; }\n";
    out << ".container { max-width: 1040px; margin: 0 auto; }\n";
    out << ".header { border-bottom: 2px solid #00D2A0; padding-bottom: 12px; margin-bottom: 24px; display: flex; justify-content: space-between; align-items: flex-end; }\n";
    out << "h1 { margin: 0; color: #00FFCC; font-size: 24px; }\n";
    out << ".timestamp { color: #8E8E9F; font-size: 13px; }\n";
    out << ".card { background-color: #1A1A22; border: 1px solid #2C2C3A; border-radius: 8px; padding: 18px; margin-bottom: 20px; }\n";
    out << "h2 { color: #00D2A0; font-size: 16px; margin-top: 0; margin-bottom: 14px; border-bottom: 1px solid #2C2C3A; padding-bottom: 6px; }\n";
    out << "table { width: 100%; border-collapse: collapse; font-size: 13px; }\n";
    out << "th { text-align: left; background-color: #22222E; color: #A0A0B0; padding: 8px 10px; border-bottom: 1px solid #333344; }\n";
    out << "td { padding: 8px 10px; border-bottom: 1px solid #22222E; font-family: Consolas, monospace; }\n";
    out << "tr:hover { background-color: #20202A; }\n";
    out << ".badge { display: inline-block; padding: 3px 8px; border-radius: 4px; font-weight: bold; font-size: 11px; }\n";
    out << ".badge-ok { background-color: #00503A; color: #00FFCC; }\n";
    out << ".badge-warn { background-color: #602020; color: #FFA0A0; }\n";
    out << ".grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }\n";
    out << ".metric-val { font-size: 20px; font-weight: bold; color: #00FFCC; font-family: Consolas, monospace; }\n";
    out << ".metric-lbl { font-size: 12px; color: #8E8E9F; }\n";
    out << "</style>\n</head>\n<body>\n";

    out << "<div class=\"container\">\n";
    out << "  <div class=\"header\">\n";
    out << "    <div>\n";
    out << "      <h1>⚡ " << data.title << "</h1>\n";
    out << "      <div style=\"color:#A0A0B0; font-size:13px; margin-top:4px;\">Hardware: " << data.hardwareModel << " | FW: " << data.firmwareVersion << "</div>\n";
    out << "    </div>\n";
    out << "    <div class=\"timestamp\">Generated: " << genTime << "</div>\n";
    out << "  </div>\n";

    // 1. Session Overview Card
    out << "  <div class=\"card\">\n";
    out << "    <h2>📊 System & Acquisition Overview</h2>\n";
    out << "    <div class=\"grid\">\n";
    out << "      <div>\n";
    out << "        <table>\n";
    out << "          <tr><th>Property</th><th>Value</th></tr>\n";
    out << "          <tr><td>Hardware Model</td><td>" << data.hardwareModel << "</td></tr>\n";
    out << "          <tr><td>Primary MCU</td><td>" << data.mcuPrimary << "</td></tr>\n";
    out << "          <tr><td>Coprocessor</td><td>" << data.mcuCoprocessor << "</td></tr>\n";
    out << "          <tr><td>Port / Transport</td><td>" << data.connectionPort << "</td></tr>\n";
    out << "        </table>\n";
    out << "      </div>\n";
    out << "      <div>\n";
    out << "        <table>\n";
    out << "          <tr><th>Parameter</th><th>Value</th></tr>\n";
    out << "          <tr><td>Sample Rate</td><td>" << (data.sampleRateHz / 1e6) << " MSPS (" << data.sampleRateHz << " Hz)</td></tr>\n";
    out << "          <tr><td>Sample Depth</td><td>" << data.sampleCount << " samples</td></tr>\n";
    out << "          <tr><td>Capture Duration</td><td>" << (data.durationSec * 1e3) << " ms</td></tr>\n";
    out << "          <tr><td>Trigger Expression</td><td>" << data.triggerSummary << "</td></tr>\n";
    out << "        </table>\n";
    out << "      </div>\n";
    out << "    </div>\n";
    out << "  </div>\n";

    // 2. Channel Metrics Card
    out << "  <div class=\"card\">\n";
    out << "    <h2>📈 Channel Signal Metrics</h2>\n";
    out << "    <table>\n";
    out << "      <tr><th>Channel</th><th>Rising Edges</th><th>Falling Edges</th><th>Duty Cycle</th><th>Estimated Frequency</th></tr>\n";
    for (int ch = 0; ch < 8; ++ch) {
        const auto& m = data.metrics[ch];
        QString freqStr;
        if (m.estimatedFreqHz >= 1e6) freqStr = QString("%1 MHz").arg(m.estimatedFreqHz / 1e6, 0, 'f', 2);
        else if (m.estimatedFreqHz >= 1e3) freqStr = QString("%1 kHz").arg(m.estimatedFreqHz / 1e3, 0, 'f', 1);
        else if (m.estimatedFreqHz > 0) freqStr = QString("%1 Hz").arg(m.estimatedFreqHz, 0, 'f', 0);
        else freqStr = "--";

        out << "      <tr>\n";
        out << "        <td><span style=\"color:#00FFCC; font-weight:bold;\">CH" << ch << "</span></td>\n";
        out << "        <td>" << m.risingEdges << "</td>\n";
        out << "        <td>" << m.fallingEdges << "</td>\n";
        out << "        <td>" << QString::number(m.dutyCyclePercent, 'f', 1) << "%</td>\n";
        out << "        <td>" << freqStr << "</td>\n";
        out << "      </tr>\n";
    }
    out << "    </table>\n";
    out << "  </div>\n";

    // 3. Measurement Markers Card
    out << "  <div class=\"card\">\n";
    out << "    <h2>📐 Measurement Markers & Timing Delta</h2>\n";
    out << "    <div class=\"grid\">\n";
    out << "      <div>\n";
    out << "        <table>\n";
    out << "          <tr><th>Marker Pair</th><th>Timing Delta (ΔT)</th><th>Equivalent Frequency</th></tr>\n";
    out << "          <tr><td><b>Markers A ↔ B</b></td><td>" << (deltaAB * 1e3) << " ms (" << (deltaAB * 1e6) << " µs)</td><td>" << (freqAB >= 1e3 ? QString("%1 kHz").arg(freqAB / 1e3, 0, 'f', 2) : QString("%1 Hz").arg(freqAB, 0, 'f', 1)) << "</td></tr>\n";
    out << "          <tr><td><b>Markers C ↔ D</b></td><td>" << (deltaCD * 1e3) << " ms (" << (deltaCD * 1e6) << " µs)</td><td>" << (freqCD >= 1e3 ? QString("%1 kHz").arg(freqCD / 1e3, 0, 'f', 2) : QString("%1 Hz").arg(freqCD, 0, 'f', 1)) << "</td></tr>\n";
    out << "        </table>\n";
    out << "      </div>\n";
    out << "      <div>\n";
    out << "        <table>\n";
    out << "          <tr><th>Marker</th><th>Absolute Position</th></tr>\n";
    out << "          <tr><td>Marker A</td><td>" << (data.cursorASec * 1e3) << " ms</td></tr>\n";
    out << "          <tr><td>Marker B</td><td>" << (data.cursorBSec * 1e3) << " ms</td></tr>\n";
    out << "          <tr><td>Marker C</td><td>" << (data.cursorCSec * 1e3) << " ms</td></tr>\n";
    out << "          <tr><td>Marker D</td><td>" << (data.cursorDSec * 1e3) << " ms</td></tr>\n";
    out << "        </table>\n";
    out << "      </div>\n";
    out << "    </div>\n";
    out << "  </div>\n";

    // 4. Protocol Decoders Card
    out << "  <div class=\"card\">\n";
    out << "    <h2>🔍 Protocol Decoder Analysis (" << data.activeProtocol << ")</h2>\n";
    if (data.decodedPackets.isEmpty()) {
        out << "    <p style=\"color:#8E8E9F;\">No decoded packets in this session.</p>\n";
    } else {
        out << "    <p>Total Decoded Packets: <b>" << data.decodedPackets.size() << "</b></p>\n";
        out << "    <table>\n";
        out << "      <tr><th>Time (s)</th><th>Packet Title</th><th>Tokens / Payload</th><th>Status</th></tr>\n";
        int maxRows = std::min<int>(50, static_cast<int>(data.decodedPackets.size()));
        for (int i = 0; i < maxRows; ++i) {
            const auto& pkt = data.decodedPackets[i];
            QString tokStr;
            bool hasWarn = false;
            for (const auto& tok : pkt.tokens) {
                tokStr += tok.text + " ";
                if (tok.isWarning) hasWarn = true;
            }
            double tStart = (data.sampleRateHz > 0) ? (static_cast<double>(pkt.startSample) / data.sampleRateHz) : 0.0;
            out << "      <tr>\n";
            out << "        <td>" << QString::number(tStart, 'f', 6) << "</td>\n";
            out << "        <td><b>" << pkt.title << "</b></td>\n";
            out << "        <td>" << tokStr.trimmed() << "</td>\n";
            out << "        <td>" << (hasWarn ? "<span class=\"badge badge-warn\">WARN</span>" : "<span class=\"badge badge-ok\">OK</span>") << "</td>\n";
            out << "      </tr>\n";
        }
        out << "    </table>\n";
        if (data.decodedPackets.size() > 50) {
            out << "    <p style=\"color:#8E8E9F; font-size:12px;\">Showing first 50 of " << data.decodedPackets.size() << " packets.</p>\n";
        }
    }
    out << "  </div>\n";

    // 5. Global Transactions Card
    if (!data.recentTransactions.isEmpty()) {
        out << "  <div class=\"card\">\n";
        out << "    <h2>⏱ Recent Hardware Transactions</h2>\n";
        out << "    <table>\n";
        out << "      <tr><th>ID</th><th>Subsystem</th><th>Summary</th><th>Duration</th><th>Status</th></tr>\n";
        for (const auto& tx : data.recentTransactions) {
            out << "      <tr>\n";
            out << "        <td>#" << tx.id << "</td>\n";
            out << "        <td>" << tx.subsystem << "</td>\n";
            out << "        <td>" << tx.summary << "</td>\n";
            out << "        <td>" << (tx.durationNs / 1e6) << " ms</td>\n";
            out << "        <td>" << (tx.success ? "<span class=\"badge badge-ok\">SUCCESS</span>" : "<span class=\"badge badge-warn\">FAILED</span>") << "</td>\n";
            out << "      </tr>\n";
        }
        out << "    </table>\n";
        out << "  </div>\n";
    }

    out << "</div>\n</body>\n</html>\n";
    return html;
}

QJsonObject InstrumentReportGenerator::generateJsonReport(const InstrumentReportData& data) {
    QJsonObject root;
    root["title"] = data.title;
    root["generatedAt"] = data.generatedAt.isEmpty() ? QDateTime::currentDateTime().toString(Qt::ISODate) : data.generatedAt;

    QJsonObject hw;
    hw["model"] = data.hardwareModel;
    hw["firmwareVersion"] = data.firmwareVersion;
    hw["mcuPrimary"] = data.mcuPrimary;
    hw["mcuCoprocessor"] = data.mcuCoprocessor;
    hw["port"] = data.connectionPort;
    root["hardware"] = hw;

    QJsonObject acq;
    acq["sampleRateHz"] = static_cast<qint64>(data.sampleRateHz);
    acq["sampleCount"] = static_cast<qint64>(data.sampleCount);
    acq["durationSec"] = data.durationSec;
    acq["trigger"] = data.triggerSummary;
    root["acquisition"] = acq;

    QJsonArray metricsArr;
    for (int ch = 0; ch < 8; ++ch) {
        QJsonObject mObj;
        mObj["channel"] = ch;
        mObj["risingEdges"] = static_cast<qint64>(data.metrics[ch].risingEdges);
        mObj["fallingEdges"] = static_cast<qint64>(data.metrics[ch].fallingEdges);
        mObj["dutyCyclePercent"] = data.metrics[ch].dutyCyclePercent;
        mObj["estimatedFreqHz"] = data.metrics[ch].estimatedFreqHz;
        metricsArr.append(mObj);
    }
    root["channelMetrics"] = metricsArr;

    QJsonObject cursors;
    cursors["cursorASec"] = data.cursorASec;
    cursors["cursorBSec"] = data.cursorBSec;
    cursors["cursorCSec"] = data.cursorCSec;
    cursors["cursorDSec"] = data.cursorDSec;
    cursors["deltaABSec"] = std::abs(data.cursorBSec - data.cursorASec);
    cursors["deltaCDSec"] = std::abs(data.cursorDSec - data.cursorCSec);
    root["cursors"] = cursors;

    root["activeProtocol"] = data.activeProtocol;
    QJsonArray packetsArr;
    for (const auto& pkt : data.decodedPackets) {
        QJsonObject pObj;
        pObj["title"] = pkt.title;
        pObj["startSample"] = static_cast<qint64>(pkt.startSample);
        pObj["endSample"] = static_cast<qint64>(pkt.endSample);
        QJsonArray tokArr;
        for (const auto& tok : pkt.tokens) {
            QJsonObject tObj;
            tObj["text"] = tok.text;
            tObj["timeSec"] = tok.startTimeSec;
            tObj["isWarning"] = tok.isWarning;
            tokArr.append(tObj);
        }
        pObj["tokens"] = tokArr;
        packetsArr.append(pObj);
    }
    root["decodedPackets"] = packetsArr;

    return root;
}

bool InstrumentReportGenerator::saveHtmlReport(const QString& filePath, const InstrumentReportData& data, QString* errorMsg) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMsg) *errorMsg = file.errorString();
        return false;
    }
    QTextStream out(&file);
    out << generateHtmlReport(data);
    file.close();
    return true;
}

bool InstrumentReportGenerator::saveJsonReport(const QString& filePath, const InstrumentReportData& data, QString* errorMsg) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMsg) *errorMsg = file.errorString();
        return false;
    }
    QJsonDocument doc(generateJsonReport(data));
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

} // namespace UdooKey
