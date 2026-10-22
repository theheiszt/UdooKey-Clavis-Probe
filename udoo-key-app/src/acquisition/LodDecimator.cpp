#include "LodDecimator.h"
#include <algorithm>

namespace UdooKey {

void LodDecimator::clear() {
    m_rawCount = 0;
    m_rawPtr = nullptr;
    m_cachedRaw.clear();
    m_level1.clear();
    m_level2.clear();
    m_level3.clear();
}

void LodDecimator::buildPyramid(const QByteArray& rawData) {
    clear();
    m_cachedRaw = rawData;
    m_rawCount = rawData.size();
    m_rawPtr = m_cachedRaw.constData();

    if (m_rawCount == 0) return;

    // Level 1: 1:16
    size_t count1 = (m_rawCount + 15) / 16;
    m_level1.resize(count1);
    for (size_t i = 0; i < count1; ++i) {
        size_t start = i * 16;
        size_t end = std::min(start + 16, m_rawCount);

        uint8_t minV = 0xFF;
        uint8_t maxV = 0x00;
        uint8_t firstV = static_cast<uint8_t>(m_rawPtr[start]);
        uint8_t lastV = static_cast<uint8_t>(m_rawPtr[end - 1]);

        for (size_t s = start; s < end; ++s) {
            uint8_t v = static_cast<uint8_t>(m_rawPtr[s]);
            minV = std::min(minV, v);
            maxV = std::max(maxV, v);
        }

        m_level1[i] = {minV, maxV, firstV, lastV};
    }

    // Level 2: 1:256 (derived from Level 1, factor 16)
    size_t count2 = (count1 + 15) / 16;
    m_level2.resize(count2);
    for (size_t i = 0; i < count2; ++i) {
        size_t start = i * 16;
        size_t end = std::min(start + 16, count1);

        uint8_t minV = 0xFF;
        uint8_t maxV = 0x00;
        uint8_t firstV = m_level1[start].firstVal;
        uint8_t lastV = m_level1[end - 1].lastVal;

        for (size_t s = start; s < end; ++s) {
            minV = std::min(minV, m_level1[s].minVals);
            maxV = std::max(maxV, m_level1[s].maxVals);
        }

        m_level2[i] = {minV, maxV, firstV, lastV};
    }

    // Level 3: 1:4096 (derived from Level 2, factor 16)
    size_t count3 = (count2 + 15) / 16;
    m_level3.resize(count3);
    for (size_t i = 0; i < count3; ++i) {
        size_t start = i * 16;
        size_t end = std::min(start + 16, count2);

        uint8_t minV = 0xFF;
        uint8_t maxV = 0x00;
        uint8_t firstV = m_level2[start].firstVal;
        uint8_t lastV = m_level2[end - 1].lastVal;

        for (size_t s = start; s < end; ++s) {
            minV = std::min(minV, m_level2[s].minVals);
            maxV = std::max(maxV, m_level2[s].maxVals);
        }

        m_level3[i] = {minV, maxV, firstV, lastV};
    }
}

std::vector<LodDecimator::RenderPoint> LodDecimator::queryRange(
    uint32_t startSample, uint32_t endSample, uint32_t bucketCount, uint8_t channel) const {
    
    std::vector<RenderPoint> result;
    if (m_rawCount == 0 || bucketCount == 0 || startSample >= endSample || channel > 7) {
        return result;
    }

    startSample = std::min(startSample, static_cast<uint32_t>(m_rawCount - 1));
    endSample = std::min(endSample, static_cast<uint32_t>(m_rawCount));
    uint32_t rangeSpan = endSample - startSample;
    if (rangeSpan == 0) return result;

    result.reserve(bucketCount);
    double samplesPerBucket = static_cast<double>(rangeSpan) / bucketCount;
    uint8_t mask = 1 << channel;

    // Direct sample extraction if zoom is close (samplesPerBucket <= 2)
    if (samplesPerBucket <= 2.0) {
        for (uint32_t b = 0; b < bucketCount; ++b) {
            uint32_t s0 = startSample + static_cast<uint32_t>(b * samplesPerBucket);
            uint32_t s1 = std::min(startSample + static_cast<uint32_t>((b + 1) * samplesPerBucket), endSample);
            if (s0 >= endSample) break;
            if (s1 <= s0) s1 = s0 + 1;

            uint8_t minBit = 1;
            uint8_t maxBit = 0;
            uint8_t firstBit = (static_cast<uint8_t>(m_rawPtr[s0]) & mask) ? 1 : 0;
            uint8_t lastBit = (static_cast<uint8_t>(m_rawPtr[s1 - 1]) & mask) ? 1 : 0;

            for (uint32_t s = s0; s < s1; ++s) {
                uint8_t bit = (static_cast<uint8_t>(m_rawPtr[s]) & mask) ? 1 : 0;
                minBit = std::min(minBit, bit);
                maxBit = std::max(maxBit, bit);
            }

            result.push_back({static_cast<float>(b), minBit, maxBit, firstBit, lastBit});
        }
    } else {
        // Use decimated steps
        for (uint32_t b = 0; b < bucketCount; ++b) {
            uint32_t s0 = startSample + static_cast<uint32_t>(b * samplesPerBucket);
            uint32_t s1 = std::min(startSample + static_cast<uint32_t>((b + 1) * samplesPerBucket), endSample);
            if (s0 >= endSample) break;
            if (s1 <= s0) s1 = s0 + 1;

            // Sample raw or level1
            uint8_t minBit = 1;
            uint8_t maxBit = 0;
            uint8_t firstBit = (static_cast<uint8_t>(m_rawPtr[s0]) & mask) ? 1 : 0;
            uint8_t lastBit = (static_cast<uint8_t>(m_rawPtr[s1 - 1]) & mask) ? 1 : 0;

            // Scan with step
            uint32_t step = std::max<uint32_t>(1, (s1 - s0) / 16);
            for (uint32_t s = s0; s < s1; s += step) {
                uint8_t bit = (static_cast<uint8_t>(m_rawPtr[s]) & mask) ? 1 : 0;
                minBit = std::min(minBit, bit);
                maxBit = std::max(maxBit, bit);
                if (minBit == 0 && maxBit == 1) break; // early exit: both low and high exist
            }

            result.push_back({static_cast<float>(b), minBit, maxBit, firstBit, lastBit});
        }
    }

    return result;
}

} // namespace UdooKey
