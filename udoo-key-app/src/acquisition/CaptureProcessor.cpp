#include "CaptureProcessor.h"
#include "Logger.h"

namespace UdooKey {

CaptureProcessor::CaptureProcessor(QObject* parent)
    : QObject(parent) {
    for (uint8_t ch = 0; ch < 8; ++ch) {
        m_metrics[ch].channel = ch;
    }
}

void CaptureProcessor::processCapture(const QByteArray& rawData, uint32_t sampleRateHz) {
    emit processingStarted();

    m_captureBuffer.setData(rawData, sampleRateHz);
    m_decimator.buildPyramid(rawData);
    m_sharedBuffer = std::make_shared<CaptureBuffer>(m_captureBuffer);
    m_sharedDecimator = std::make_shared<LodDecimator>(m_decimator);
    computeMetrics();

    LOG_INFO(QString("CaptureProcessor: Processed %1 samples at %2 Hz").arg(rawData.size()).arg(sampleRateHz));
    emit processingComplete();
}

ChannelMetrics CaptureProcessor::metrics(uint8_t channel) const {
    if (channel < 8) return m_metrics[channel];
    return ChannelMetrics();
}

void CaptureProcessor::computeMetrics() {
    uint32_t count = m_captureBuffer.sampleCount();
    uint32_t rate = m_captureBuffer.sampleRateHz();
    const char* data = m_captureBuffer.rawData();

    if (count < 2 || rate == 0) return;

    for (uint8_t ch = 0; ch < 8; ++ch) {
        uint8_t mask = 1 << ch;
        uint32_t rising = 0;
        uint32_t falling = 0;
        uint32_t highCount = 0;
        bool lastState = (static_cast<uint8_t>(data[0]) & mask) != 0;
        if (lastState) highCount++;

        for (uint32_t i = 1; i < count; ++i) {
            bool state = (static_cast<uint8_t>(data[i]) & mask) != 0;
            if (state) highCount++;

            if (!lastState && state) {
                rising++;
            } else if (lastState && !state) {
                falling++;
            }
            lastState = state;
        }

        m_metrics[ch].channel = ch;
        m_metrics[ch].risingEdges = rising;
        m_metrics[ch].fallingEdges = falling;
        m_metrics[ch].dutyCyclePercent = (static_cast<double>(highCount) / count) * 100.0;

        double totalTimeSec = static_cast<double>(count) / rate;
        if (totalTimeSec > 0.0 && rising > 0) {
            m_metrics[ch].estimatedFreqHz = static_cast<double>(rising) / totalTimeSec;
        } else {
            m_metrics[ch].estimatedFreqHz = 0.0;
        }
    }
}

} // namespace UdooKey
