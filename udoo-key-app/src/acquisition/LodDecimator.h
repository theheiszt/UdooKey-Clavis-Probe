#pragma once

#include <vector>
#include <cstdint>
#include <QByteArray>

namespace UdooKey {

struct LodBucket {
    uint8_t minVals;
    uint8_t maxVals;
    uint8_t firstVal;
    uint8_t lastVal;
};

class LodDecimator {
public:
    LodDecimator() = default;

    void buildPyramid(const QByteArray& rawData);
    void clear();

    bool hasData() const { return !m_level1.empty(); }
    size_t rawSampleCount() const { return m_rawCount; }

    // Fetch min/max bounds for channel over sample range [startSample, endSample]
    // bucketCount specifies desired resolution (usually width in pixels)
    struct RenderPoint {
        float x;
        uint8_t minV;
        uint8_t maxV;
        uint8_t firstV;
        uint8_t lastV;
    };

    std::vector<RenderPoint> queryRange(uint32_t startSample, uint32_t endSample, uint32_t bucketCount, uint8_t channel) const;

private:
    size_t m_rawCount = 0;
    const char* m_rawPtr = nullptr;
    QByteArray m_cachedRaw;

    std::vector<LodBucket> m_level1; // Factor 16
    std::vector<LodBucket> m_level2; // Factor 256
    std::vector<LodBucket> m_level3; // Factor 4096
};

} // namespace UdooKey
