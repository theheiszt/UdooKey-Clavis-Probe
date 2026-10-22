#pragma once

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include "UsbTypes.h"
#include "UsbInjectionEngine.h"
#include "TaskManager.h"

namespace UdooKey {

enum class UsbStepType {
    Delay,
    ControlTransfer,
    BulkWrite,
    BulkRead,
    AssertResponse
};

struct UsbTestStep {
    UsbStepType type = UsbStepType::ControlTransfer;
    QString name;
    int delayMs = 100;

    // Control transfer fields
    UsbSetupPacket setup;
    QByteArray dataOut;

    // Bulk / Endpoint fields
    uint8_t endpoint = 0x01;
    int readLength = 64;

    // Assertion fields
    QString expectHexSubstring;
    int expectMinBytes = 0;

    QJsonObject toJson() const;
    static UsbTestStep fromJson(const QJsonObject& obj);
};

struct UsbStepExecutionResult {
    int stepIndex = 0;
    QString stepName;
    bool passed = true;
    QString message;
    QByteArray receivedData;
};

class UsbTestSequence {
public:
    UsbTestSequence() = default;

    QString name = "Default Test Sequence";
    QString description;
    QList<UsbTestStep> steps;

    void addStep(const UsbTestStep& step) { steps.append(step); }
    void clear() { steps.clear(); }

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);

    bool saveToFile(const QString& filePath) const;
    bool loadFromFile(const QString& filePath);

    QList<UsbStepExecutionResult> execute(UsbInjectionEngine* engine,
                                         std::shared_ptr<CancellationToken> token,
                                         std::function<void(int, int, const QString&)> progressCallback);

    static UsbTestSequence standardUdooKeyProbeSequence();
};

} // namespace UdooKey
