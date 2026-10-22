#include "TriggerExpression.h"
#include <QStringList>

namespace UdooKey {

TriggerExpression::TriggerExpression() {
    for (int i = 0; i < 8; ++i) {
        m_conditions[i] = ChannelCondition::DontCare;
    }
}

void TriggerExpression::setCondition(uint8_t channel, ChannelCondition condition) {
    if (channel < 8) {
        m_conditions[channel] = condition;
    }
}

ChannelCondition TriggerExpression::condition(uint8_t channel) const {
    if (channel < 8) {
        return m_conditions[channel];
    }
    return ChannelCondition::DontCare;
}

TriggerExpression TriggerExpression::edgeTrigger(uint8_t channel, TriggerMode mode) {
    TriggerExpression expr;
    if (channel < 8) {
        switch (mode) {
        case TriggerMode::EdgeRising:
            expr.setCondition(channel, ChannelCondition::RisingEdge);
            break;
        case TriggerMode::EdgeFalling:
            expr.setCondition(channel, ChannelCondition::FallingEdge);
            break;
        case TriggerMode::LevelHigh:
            expr.setCondition(channel, ChannelCondition::High);
            break;
        case TriggerMode::LevelLow:
            expr.setCondition(channel, ChannelCondition::Low);
            break;
        case TriggerMode::Immediate:
        default:
            expr.setCondition(channel, ChannelCondition::DontCare);
            break;
        }
    }
    return expr;
}

void TriggerExpression::setBusPattern(uint8_t value, uint8_t mask) {
    m_busValue = value;
    m_busMask = mask;
    m_busTriggerEnabled = true;
}

bool TriggerExpression::evaluate(uint8_t currentSample, uint8_t previousSample) const {
    if (m_busTriggerEnabled) {
        if ((currentSample & m_busMask) != (m_busValue & m_busMask)) {
            return false;
        }
    }

    for (uint8_t ch = 0; ch < 8; ++ch) {
        ChannelCondition cond = m_conditions[ch];
        if (cond == ChannelCondition::DontCare) continue;

        uint8_t mask = 1 << ch;
        bool cur = (currentSample & mask) != 0;
        bool prev = (previousSample & mask) != 0;

        switch (cond) {
        case ChannelCondition::Low:
            if (cur) return false;
            break;
        case ChannelCondition::High:
            if (!cur) return false;
            break;
        case ChannelCondition::RisingEdge:
            if (!(prev == false && cur == true)) return false;
            break;
        case ChannelCondition::FallingEdge:
            if (!(prev == true && cur == false)) return false;
            break;
        case ChannelCondition::AnyEdge:
            if (prev == cur) return false;
            break;
        default:
            break;
        }
    }

    return true;
}

int64_t TriggerExpression::findTriggerIndex(const CaptureBuffer& buffer, uint32_t startIndex) const {
    uint32_t count = buffer.sampleCount();
    if (count < 2 || startIndex >= count) return -1;

    uint32_t start = (startIndex == 0) ? 1 : startIndex;
    for (uint32_t i = start; i < count; ++i) {
        uint8_t prev = buffer.sampleAt(i - 1);
        uint8_t cur = buffer.sampleAt(i);
        if (evaluate(cur, prev)) {
            return static_cast<int64_t>(i);
        }
    }
    return -1;
}

QString TriggerExpression::toPatternString() const {
    // String ordered CH7..CH0
    QString s;
    for (int ch = 7; ch >= 0; --ch) {
        switch (m_conditions[ch]) {
        case ChannelCondition::Low:         s.append('0'); break;
        case ChannelCondition::High:        s.append('1'); break;
        case ChannelCondition::RisingEdge:  s.append('R'); break;
        case ChannelCondition::FallingEdge: s.append('F'); break;
        case ChannelCondition::AnyEdge:     s.append('E'); break;
        case ChannelCondition::DontCare:
        default:                            s.append('X'); break;
        }
    }
    return s;
}

bool TriggerExpression::fromPatternString(const QString& pattern) {
    QString p = pattern.trimmed().toUpper();
    if (p.length() != 8) return false;

    for (int i = 0; i < 8; ++i) {
        int ch = 7 - i;
        QChar c = p.at(i);
        if (c == '0') m_conditions[ch] = ChannelCondition::Low;
        else if (c == '1') m_conditions[ch] = ChannelCondition::High;
        else if (c == 'R') m_conditions[ch] = ChannelCondition::RisingEdge;
        else if (c == 'F') m_conditions[ch] = ChannelCondition::FallingEdge;
        else if (c == 'E') m_conditions[ch] = ChannelCondition::AnyEdge;
        else m_conditions[ch] = ChannelCondition::DontCare;
    }
    return true;
}

QString TriggerExpression::summary() const {
    if (m_busTriggerEnabled) {
        return QString("Bus Pattern: 0x%1 (Mask: 0x%2)").arg(m_busValue, 2, 16, QChar('0')).arg(m_busMask, 2, 16, QChar('0')).toUpper();
    }

    QStringList parts;
    for (int ch = 0; ch < 8; ++ch) {
        switch (m_conditions[ch]) {
        case ChannelCondition::Low:         parts << QString("CH%1=0").arg(ch); break;
        case ChannelCondition::High:        parts << QString("CH%1=1").arg(ch); break;
        case ChannelCondition::RisingEdge:  parts << QString("CH%1=↑").arg(ch); break;
        case ChannelCondition::FallingEdge: parts << QString("CH%1=↓").arg(ch); break;
        case ChannelCondition::AnyEdge:     parts << QString("CH%1=↕").arg(ch); break;
        default: break;
        }
    }

    if (parts.isEmpty()) {
        return "Immediate (No Trigger)";
    }
    return parts.join(" && ");
}

QJsonObject TriggerExpression::toJson() const {
    QJsonObject obj;
    obj["pattern"] = toPatternString();
    obj["busEnabled"] = m_busTriggerEnabled;
    obj["busValue"] = m_busValue;
    obj["busMask"] = m_busMask;
    return obj;
}

void TriggerExpression::fromJson(const QJsonObject& json) {
    if (json.contains("pattern")) {
        fromPatternString(json["pattern"].toString());
    }
    m_busTriggerEnabled = json["busEnabled"].toBool(false);
    m_busValue = static_cast<uint8_t>(json["busValue"].toInt(0));
    m_busMask = static_cast<uint8_t>(json["busMask"].toInt(0xFF));
}

} // namespace UdooKey
