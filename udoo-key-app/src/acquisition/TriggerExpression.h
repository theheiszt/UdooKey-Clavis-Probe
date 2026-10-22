#pragma once

#include <cstdint>
#include <QString>
#include <QJsonObject>
#include "Types.h"
#include "CaptureBuffer.h"

namespace UdooKey {

enum class ChannelCondition : uint8_t {
    DontCare    = 0, // 'X' - ignore channel
    Low         = 1, // '0' - must be LOW
    High        = 2, // '1' - must be HIGH
    RisingEdge  = 3, // 'R' - transition from 0 to 1
    FallingEdge = 4, // 'F' - transition from 1 to 0
    AnyEdge     = 5  // 'E' - transition 0->1 or 1->0
};

class TriggerExpression {
public:
    TriggerExpression();

    void setCondition(uint8_t channel, ChannelCondition condition);
    ChannelCondition condition(uint8_t channel) const;

    // Direct single-channel trigger convenience
    static TriggerExpression edgeTrigger(uint8_t channel, TriggerMode mode);

    // Parallel bus trigger (value masked by mask)
    void setBusPattern(uint8_t value, uint8_t mask = 0xFF);
    uint8_t busValue() const { return m_busValue; }
    uint8_t busMask() const { return m_busMask; }
    bool isBusTriggerEnabled() const { return m_busTriggerEnabled; }
    void setBusTriggerEnabled(bool enabled) { m_busTriggerEnabled = enabled; }

    // Evaluates condition between two consecutive 8-bit sample bytes
    bool evaluate(uint8_t currentSample, uint8_t previousSample) const;

    // Search buffer for first matching sample index, starting at startIndex
    // Returns -1 if no trigger match is found
    int64_t findTriggerIndex(const CaptureBuffer& buffer, uint32_t startIndex = 0) const;

    // 8-character string representation (e.g. "X01RX1FE" for CH7..CH0)
    QString toPatternString() const;
    bool fromPatternString(const QString& pattern);

    // Human-readable summary
    QString summary() const;

    // Persistence
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);

private:
    ChannelCondition m_conditions[8];
    bool m_busTriggerEnabled = false;
    uint8_t m_busValue = 0x00;
    uint8_t m_busMask = 0xFF;
};

} // namespace UdooKey
