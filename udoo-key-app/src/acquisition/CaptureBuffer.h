#pragma once

#include <QByteArray>
#include <QVector>
#include <cstdint>
#include "Types.h"

namespace UdooKey {

class CaptureBuffer {
public:
    CaptureBuffer() = default;
    CaptureBuffer(const QByteArray& rawData, uint32_t sampleRateHz);

    void setData(const QByteArray& rawData, uint32_t sampleRateHz);
    void clear();

    bool isEmpty() const { return m_rawData.isEmpty(); }
    uint32_t sampleCount() const { return static_cast<uint32_t>(m_rawData.size()); }
    uint32_t sampleRateHz() const { return m_sampleRateHz; }
    double durationSec() const;

    const char* rawData() const { return m_rawData.constData(); }
    const QByteArray& rawByteArray() const { return m_rawData; }

    uint8_t sampleAt(uint32_t index) const {
        if (index >= static_cast<uint32_t>(m_rawData.size())) return 0;
        return static_cast<uint8_t>(m_rawData.at(index));
    }

    bool channelState(uint8_t channel, uint32_t index) const {
        if (channel > 7 || index >= static_cast<uint32_t>(m_rawData.size())) return false;
        return (static_cast<uint8_t>(m_rawData.at(index)) & (1 << channel)) != 0;
    }

    double sampleToTimeSec(uint64_t sampleIndex) const {
        if (m_sampleRateHz == 0) return 0.0;
        return static_cast<double>(sampleIndex) / static_cast<double>(m_sampleRateHz);
    }

    uint64_t timeToSample(double timeSec) const {
        if (timeSec <= 0.0) return 0;
        return static_cast<uint64_t>(timeSec * m_sampleRateHz);
    }

private:
    QByteArray m_rawData;
    uint32_t m_sampleRateHz = 10'000'000;
};

} // namespace UdooKey
