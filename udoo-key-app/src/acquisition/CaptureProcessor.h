#pragma once

#include <QObject>
#include <QByteArray>
#include <memory>
#include "CaptureBuffer.h"
#include "LodDecimator.h"

namespace UdooKey {

struct ChannelMetrics {
    uint8_t channel = 0;
    uint32_t risingEdges = 0;
    uint32_t fallingEdges = 0;
    double estimatedFreqHz = 0.0;
    double dutyCyclePercent = 0.0;
};

class CaptureProcessor : public QObject {
    Q_OBJECT

public:
    explicit CaptureProcessor(QObject* parent = nullptr);
    ~CaptureProcessor() override = default;

    void processCapture(const QByteArray& rawData, uint32_t sampleRateHz);

    const CaptureBuffer& captureBuffer() const { return m_captureBuffer; }
    const LodDecimator& decimator() const { return m_decimator; }

    std::shared_ptr<const CaptureBuffer> sharedBuffer() const { return m_sharedBuffer; }
    std::shared_ptr<const LodDecimator> sharedDecimator() const { return m_sharedDecimator; }

    ChannelMetrics metrics(uint8_t channel) const;

signals:
    void processingStarted();
    void processingComplete();

private:
    void computeMetrics();

    CaptureBuffer m_captureBuffer;
    LodDecimator m_decimator;
    std::shared_ptr<CaptureBuffer> m_sharedBuffer;
    std::shared_ptr<LodDecimator> m_sharedDecimator;
    ChannelMetrics m_metrics[8];
};

} // namespace UdooKey
