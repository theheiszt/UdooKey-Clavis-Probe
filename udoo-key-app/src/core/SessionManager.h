#pragma once

#include <QString>
#include <QJsonObject>
#include <QByteArray>
#include "CaptureBuffer.h"

namespace UdooKey {

struct SessionData {
    QString hardwareModel = "UDOO KEY";
    QString firmwareVersion = "1.2.0";
    QString sessionDescription;
    QDateTime timestamp;

    // Acquisition & Waveform
    uint32_t sampleRateHz = 10'000'000;
    uint32_t sampleCount = 0;
    QByteArray rawCaptureBytes;

    // Measurement Cursors
    double cursorASec = 0.001;
    double cursorBSec = 0.003;
    double cursorCSec = 0.004;
    double cursorDSec = 0.005;

    // Active Decoder Info
    QString activeProtocol = "None";
    int decoderChannelA = 0;
    int decoderChannelB = 1;
    uint32_t decoderBaudRate = 115200;
};

class SessionManager {
public:
    static bool saveSession(const QString& filePath, const SessionData& session);
    static bool loadSession(const QString& filePath, SessionData& session);

    static QJsonObject serializeToJson(const SessionData& session);
    static bool deserializeFromJson(const QJsonObject& obj, SessionData& session);
};

} // namespace UdooKey
