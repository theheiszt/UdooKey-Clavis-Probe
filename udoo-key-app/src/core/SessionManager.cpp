#include "SessionManager.h"
#include <QFile>
#include <QJsonDocument>

namespace UdooKey {

QJsonObject SessionManager::serializeToJson(const SessionData& s) {
    QJsonObject obj;
    obj["magic"] = "UDOOSESS";
    obj["version"] = 1;
    obj["timestamp"] = s.timestamp.isValid() ? s.timestamp.toString(Qt::ISODateWithMs) : QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    obj["hardwareModel"] = s.hardwareModel;
    obj["firmwareVersion"] = s.firmwareVersion;
    obj["sessionDescription"] = s.sessionDescription;

    // Acquisition
    QJsonObject acq;
    acq["sampleRateHz"] = static_cast<double>(s.sampleRateHz);
    acq["sampleCount"] = static_cast<int>(s.sampleCount);
    acq["rawCaptureBase64"] = QString(s.rawCaptureBytes.toBase64());
    obj["acquisition"] = acq;

    // Cursors
    QJsonObject cur;
    cur["cursorA"] = s.cursorASec;
    cur["cursorB"] = s.cursorBSec;
    cur["cursorC"] = s.cursorCSec;
    cur["cursorD"] = s.cursorDSec;
    obj["cursors"] = cur;

    // Decoder
    QJsonObject dec;
    dec["activeProtocol"] = s.activeProtocol;
    dec["channelA"] = s.decoderChannelA;
    dec["channelB"] = s.decoderChannelB;
    dec["baudRate"] = static_cast<int>(s.decoderBaudRate);
    obj["decoder"] = dec;

    return obj;
}

bool SessionManager::deserializeFromJson(const QJsonObject& obj, SessionData& s) {
    if (obj["magic"].toString() != "UDOOSESS") return false;

    s.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODateWithMs);
    s.hardwareModel = obj["hardwareModel"].toString("UDOO KEY");
    s.firmwareVersion = obj["firmwareVersion"].toString("1.0.0");
    s.sessionDescription = obj["sessionDescription"].toString();

    if (obj.contains("acquisition")) {
        QJsonObject acq = obj["acquisition"].toObject();
        s.sampleRateHz = static_cast<uint32_t>(acq["sampleRateHz"].toDouble(10'000'000));
        s.sampleCount = static_cast<uint32_t>(acq["sampleCount"].toInt(0));
        s.rawCaptureBytes = QByteArray::fromBase64(acq["rawCaptureBase64"].toString().toUtf8());
    }

    if (obj.contains("cursors")) {
        QJsonObject cur = obj["cursors"].toObject();
        s.cursorASec = cur["cursorA"].toDouble(0.001);
        s.cursorBSec = cur["cursorB"].toDouble(0.003);
        s.cursorCSec = cur["cursorC"].toDouble(0.004);
        s.cursorDSec = cur["cursorD"].toDouble(0.005);
    }

    if (obj.contains("decoder")) {
        QJsonObject dec = obj["decoder"].toObject();
        s.activeProtocol = dec["activeProtocol"].toString("None");
        s.decoderChannelA = dec["channelA"].toInt(0);
        s.decoderChannelB = dec["channelB"].toInt(1);
        s.decoderBaudRate = static_cast<uint32_t>(dec["baudRate"].toInt(115200));
    }

    return true;
}

bool SessionManager::saveSession(const QString& filePath, const SessionData& session) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QJsonObject obj = serializeToJson(session);
    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool SessionManager::loadSession(const QString& filePath, SessionData& session) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) return false;

    return deserializeFromJson(doc.object(), session);
}

} // namespace UdooKey
