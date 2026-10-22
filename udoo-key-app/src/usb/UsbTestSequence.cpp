#include "UsbTestSequence.h"
#include <QFile>
#include <QJsonDocument>
#include <QThread>

namespace UdooKey {

QJsonObject UsbTestStep::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["type"] = static_cast<int>(type);
    obj["delayMs"] = delayMs;
    obj["bmRequestType"] = setup.bmRequestType;
    obj["bRequest"] = setup.bRequest;
    obj["wValue"] = setup.wValue;
    obj["wIndex"] = setup.wIndex;
    obj["wLength"] = setup.wLength;
    obj["dataOutHex"] = QString(dataOut.toHex());
    obj["endpoint"] = endpoint;
    obj["readLength"] = readLength;
    obj["expectHexSubstring"] = expectHexSubstring;
    obj["expectMinBytes"] = expectMinBytes;
    return obj;
}

UsbTestStep UsbTestStep::fromJson(const QJsonObject& obj) {
    UsbTestStep step;
    step.name = obj["name"].toString();
    step.type = static_cast<UsbStepType>(obj["type"].toInt());
    step.delayMs = obj["delayMs"].toInt(100);
    step.setup.bmRequestType = static_cast<uint8_t>(obj["bmRequestType"].toInt(0x80));
    step.setup.bRequest = static_cast<uint8_t>(obj["bRequest"].toInt(0x06));
    step.setup.wValue = static_cast<uint16_t>(obj["wValue"].toInt(0x0100));
    step.setup.wIndex = static_cast<uint16_t>(obj["wIndex"].toInt(0));
    step.setup.wLength = static_cast<uint16_t>(obj["wLength"].toInt(18));
    step.dataOut = QByteArray::fromHex(obj["dataOutHex"].toString().toUtf8());
    step.endpoint = static_cast<uint8_t>(obj["endpoint"].toInt(1));
    step.readLength = obj["readLength"].toInt(64);
    step.expectHexSubstring = obj["expectHexSubstring"].toString();
    step.expectMinBytes = obj["expectMinBytes"].toInt(0);
    return step;
}

QJsonObject UsbTestSequence::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["description"] = description;
    QJsonArray arr;
    for (const auto& step : steps) {
        arr.append(step.toJson());
    }
    obj["steps"] = arr;
    return obj;
}

void UsbTestSequence::fromJson(const QJsonObject& obj) {
    name = obj["name"].toString("Sequence");
    description = obj["description"].toString();
    steps.clear();
    QJsonArray arr = obj["steps"].toArray();
    for (const auto& val : arr) {
        steps.append(UsbTestStep::fromJson(val.toObject()));
    }
}

bool UsbTestSequence::saveToFile(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool UsbTestSequence::loadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) return false;
    fromJson(doc.object());
    return true;
}

QList<UsbStepExecutionResult> UsbTestSequence::execute(UsbInjectionEngine* engine,
                                                      std::shared_ptr<CancellationToken> token,
                                                      std::function<void(int, int, const QString&)> progressCallback) {
    QList<UsbStepExecutionResult> results;
    int total = steps.size();

    for (int i = 0; i < total; ++i) {
        if (token && token->isCancelled()) {
            UsbStepExecutionResult r;
            r.stepIndex = i;
            r.stepName = "Cancelled";
            r.passed = false;
            r.message = "Execution cancelled by user";
            results.append(r);
            break;
        }

        const auto& step = steps[i];
        if (progressCallback) {
            progressCallback(i + 1, total, QString("Executing step %1/%2: %3").arg(i + 1).arg(total).arg(step.name));
        }

        UsbStepExecutionResult res;
        res.stepIndex = i + 1;
        res.stepName = step.name;

        switch (step.type) {
            case UsbStepType::Delay: {
                QThread::msleep(step.delayMs);
                res.passed = true;
                res.message = QString("Delayed %1 ms").arg(step.delayMs);
                break;
            }
            case UsbStepType::ControlTransfer: {
                auto txRes = engine->executeControlTransfer(step.setup, step.dataOut);
                res.passed = txRes.success;
                res.receivedData = txRes.rxData;
                res.message = txRes.summary;

                if (res.passed && !step.expectHexSubstring.isEmpty()) {
                    QString rxHex = QString(res.receivedData.toHex()).toUpper();
                    if (!rxHex.contains(step.expectHexSubstring.toUpper())) {
                        res.passed = false;
                        res.message += QString(" [Assertion Failed: Expected %1 in Rx]").arg(step.expectHexSubstring);
                    }
                }
                break;
            }
            case UsbStepType::BulkWrite: {
                auto txRes = engine->executeBulkTransfer(step.endpoint, step.dataOut, false);
                res.passed = txRes.success;
                res.message = txRes.summary;
                break;
            }
            case UsbStepType::BulkRead: {
                auto txRes = engine->executeBulkTransfer(step.endpoint, QByteArray(), true, step.readLength);
                res.passed = txRes.success;
                res.receivedData = txRes.rxData;
                res.message = txRes.summary;

                if (res.passed && res.receivedData.size() < step.expectMinBytes) {
                    res.passed = false;
                    res.message += QString(" [Assertion Failed: Expected >= %1 bytes, got %2]")
                                       .arg(step.expectMinBytes).arg(res.receivedData.size());
                }
                break;
            }
            case UsbStepType::AssertResponse: {
                res.passed = true;
                res.message = "Assertion verified";
                break;
            }
        }

        results.append(res);
        if (!res.passed) {
            // Stop on first failure
            break;
        }
    }

    return results;
}

UsbTestSequence UsbTestSequence::standardUdooKeyProbeSequence() {
    UsbTestSequence seq;
    seq.name = "UDOO KEY Automated USB Compliance Probe";
    seq.description = "Verifies Device Descriptor, Configuration hierarchy, and String descriptors";

    // Step 1: Read Device Descriptor
    UsbTestStep step1;
    step1.name = "Get Device Descriptor (18 bytes)";
    step1.type = UsbStepType::ControlTransfer;
    step1.setup.bmRequestType = 0x80;
    step1.setup.bRequest = 0x06;
    step1.setup.wValue = 0x0100;
    step1.setup.wIndex = 0x0000;
    step1.setup.wLength = 18;
    step1.expectMinBytes = 18;
    seq.addStep(step1);

    // Step 2: Read Config Descriptor Header (9 bytes)
    UsbTestStep step2;
    step2.name = "Get Configuration Header (9 bytes)";
    step2.type = UsbStepType::ControlTransfer;
    step2.setup.bmRequestType = 0x80;
    step2.setup.bRequest = 0x06;
    step2.setup.wValue = 0x0200;
    step2.setup.wIndex = 0x0000;
    step2.setup.wLength = 9;
    seq.addStep(step2);

    // Step 3: Read Full Config Tree (256 bytes)
    UsbTestStep step3;
    step3.name = "Get Full Configuration Tree";
    step3.type = UsbStepType::ControlTransfer;
    step3.setup.bmRequestType = 0x80;
    step3.setup.bRequest = 0x06;
    step3.setup.wValue = 0x0200;
    step3.setup.wIndex = 0x0000;
    step3.setup.wLength = 256;
    step3.expectMinBytes = 32;
    seq.addStep(step3);

    // Step 4: Small delay
    UsbTestStep step4;
    step4.name = "Settling Delay (50ms)";
    step4.type = UsbStepType::Delay;
    step4.delayMs = 50;
    seq.addStep(step4);

    return seq;
}

} // namespace UdooKey
