#include "CaptureBuffer.h"

namespace UdooKey {

CaptureBuffer::CaptureBuffer(const QByteArray& rawData, uint32_t sampleRateHz)
    : m_rawData(rawData)
    , m_sampleRateHz(sampleRateHz) {
}

void CaptureBuffer::setData(const QByteArray& rawData, uint32_t sampleRateHz) {
    m_rawData = rawData;
    m_sampleRateHz = sampleRateHz > 0 ? sampleRateHz : 10'000'000;
}

void CaptureBuffer::clear() {
    m_rawData.clear();
}

double CaptureBuffer::durationSec() const {
    if (m_sampleRateHz == 0) return 0.0;
    return static_cast<double>(m_rawData.size()) / static_cast<double>(m_sampleRateHz);
}

} // namespace UdooKey
