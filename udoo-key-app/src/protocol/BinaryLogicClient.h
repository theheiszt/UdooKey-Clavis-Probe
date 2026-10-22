#pragma once

#include <QObject>
#include <QByteArray>
#include "Types.h"
#include "ITransport.h"

namespace UdooKey {

class BinaryLogicClient : public QObject {
    Q_OBJECT

public:
    explicit BinaryLogicClient(ITransport* transport, QObject* parent = nullptr);
    ~BinaryLogicClient() override = default;

    void setTransport(ITransport* transport);

    bool armCapture(const AcquisitionConfig& config);
    void triggerCapture();
    void abortCapture();

    void onBinaryFrameReceived(const QByteArray& payload);

signals:
    void captureStarted();
    void captureComplete(const QByteArray& samples, uint32_t sampleRateHz);
    void captureError(const QString& error);

private:
    ITransport* m_transport;
    AcquisitionConfig m_currentConfig;
    bool m_armed = false;
};

} // namespace UdooKey
