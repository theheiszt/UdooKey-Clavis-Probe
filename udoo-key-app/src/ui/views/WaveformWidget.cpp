#include "WaveformWidget.h"
#include "Theme.h"
#include <algorithm>
#include <cmath>
#include <QPainterPath>

namespace UdooKey {

WaveformWidget::WaveformWidget(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
    setMinimumHeight(380);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void WaveformWidget::setCaptureData(const CaptureBuffer& buffer, const LodDecimator& decimator) {
    m_buffer = buffer;
    m_decimator = decimator;
    m_packets.clear();
    zoomToFit();
    update();
}

void WaveformWidget::setCaptureData(std::shared_ptr<const CaptureBuffer> buffer, std::shared_ptr<const LodDecimator> decimator) {
    if (buffer) {
        m_buffer = *buffer;
    } else {
        m_buffer.clear();
    }
    if (decimator) {
        m_decimator = *decimator;
    } else {
        m_decimator.clear();
    }
    m_packets.clear();
    zoomToFit();
    update();
}

void WaveformWidget::setDecodedPackets(const QVector<DecodedPacket>& packets) {
    m_packets = packets;
    update();
}

void WaveformWidget::setChannelEnabled(uint8_t channel, bool enabled) {
    if (channel < 8) {
        m_channelsEnabled[channel] = enabled;
        update();
    }
}

void WaveformWidget::zoomIn() {
    double center = (m_viewStartSec + m_viewEndSec) / 2.0;
    double halfSpan = (m_viewEndSec - m_viewStartSec) / 4.0;
    if (halfSpan > 1e-9) {
        m_viewStartSec = center - halfSpan;
        m_viewEndSec = center + halfSpan;
        update();
        emit viewSpanChanged(m_viewStartSec, m_viewEndSec);
    }
}

void WaveformWidget::zoomOut() {
    double center = (m_viewStartSec + m_viewEndSec) / 2.0;
    double halfSpan = (m_viewEndSec - m_viewStartSec);
    m_viewStartSec = std::max(0.0, center - halfSpan);
    m_viewEndSec = center + halfSpan;
    update();
    emit viewSpanChanged(m_viewStartSec, m_viewEndSec);
}

void WaveformWidget::zoomToFit() {
    m_viewStartSec = 0.0;
    if (m_buffer.durationSec() > 0.0) {
        m_viewEndSec = m_buffer.durationSec();
    } else {
        m_viewEndSec = 0.0065;
    }
    m_cursorASec = m_viewStartSec + (m_viewEndSec - m_viewStartSec) * 0.2;
    m_cursorBSec = m_viewStartSec + (m_viewEndSec - m_viewStartSec) * 0.6;
    update();
    emit viewSpanChanged(m_viewStartSec, m_viewEndSec);
    
    double delta = std::abs(m_cursorBSec - m_cursorASec);
    double freq = delta > 0.0 ? (1.0 / delta) : 0.0;
    emit cursorsChanged(m_cursorASec, m_cursorBSec, delta, freq);
}

double WaveformWidget::xToTime(int x) const {
    int w = width() - m_channelLabelWidth;
    if (w <= 0) return 0.0;
    double rel = static_cast<double>(x - m_channelLabelWidth) / w;
    return m_viewStartSec + rel * (m_viewEndSec - m_viewStartSec);
}

int WaveformWidget::timeToX(double timeSec) const {
    int w = width() - m_channelLabelWidth;
    if (w <= 0 || m_viewEndSec <= m_viewStartSec) return m_channelLabelWidth;
    double rel = (timeSec - m_viewStartSec) / (m_viewEndSec - m_viewStartSec);
    return m_channelLabelWidth + static_cast<int>(rel * w);
}

uint32_t WaveformWidget::timeToSample(double timeSec) const {
    return static_cast<uint32_t>(m_buffer.timeToSample(timeSec));
}

void WaveformWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Canvas background
    painter.fillRect(rect(), QColor(24, 24, 30));

    int waveWidth = width() - m_channelLabelWidth;
    int totalHeight = height();
    if (waveWidth <= 10) return;

    // 1. Draw Timeline Header at top
    painter.fillRect(0, 0, width(), m_timelineHeight, QColor(32, 32, 40));
    painter.setPen(QColor(80, 80, 95));
    painter.drawLine(0, m_timelineHeight, width(), m_timelineHeight);

    // Timeline tick marks
    painter.setPen(QColor(140, 140, 160));
    painter.setFont(QFont("Consolas", 9));
    int tickDivs = 10;
    for (int t = 0; t <= tickDivs; ++t) {
        int x = m_channelLabelWidth + (waveWidth * t) / tickDivs;
        double timeVal = m_viewStartSec + (m_viewEndSec - m_viewStartSec) * (static_cast<double>(t) / tickDivs);
        painter.drawLine(x, m_timelineHeight - 6, x, m_timelineHeight);

        QString timeStr;
        if (timeVal >= 1.0) timeStr = QString("%1s").arg(timeVal, 0, 'f', 3);
        else if (timeVal >= 1e-3) timeStr = QString("%1ms").arg(timeVal * 1e3, 0, 'f', 2);
        else if (timeVal >= 1e-6) timeStr = QString("%1µs").arg(timeVal * 1e6, 0, 'f', 1);
        else timeStr = QString("%1ns").arg(timeVal * 1e9, 0, 'f', 0);

        painter.drawText(x - 25, 2, 50, m_timelineHeight - 8, Qt::AlignCenter, timeStr);

        // Vertical faint grid line
        painter.setPen(QColor(36, 36, 46));
        painter.drawLine(x, m_timelineHeight, x, totalHeight);
        painter.setPen(QColor(140, 140, 160));
    }

    // 2. Draw Channels (CH0..CH7)
    uint32_t startSample = timeToSample(m_viewStartSec);
    uint32_t endSample = timeToSample(m_viewEndSec);

    for (uint8_t ch = 0; ch < 8; ++ch) {
        int trackY = m_timelineHeight + ch * m_channelTrackHeight;
        if (trackY + m_channelTrackHeight > totalHeight) break;

        // Label background & text
        painter.fillRect(0, trackY, m_channelLabelWidth, m_channelTrackHeight, QColor(28, 28, 36));
        painter.setPen(QColor(46, 46, 56));
        painter.drawLine(0, trackY + m_channelTrackHeight, width(), trackY + m_channelTrackHeight);
        painter.drawLine(m_channelLabelWidth, trackY, m_channelLabelWidth, trackY + m_channelTrackHeight);

        // Channel Name & Color pill
        QColor chColor = Theme::channelColor(ch);
        painter.fillRect(6, trackY + 12, 4, m_channelTrackHeight - 24, chColor);

        painter.setFont(QFont("Segoe UI", 9, QFont::Bold));
        painter.setPen(m_channelsEnabled[ch] ? QColor(220, 220, 230) : QColor(100, 100, 110));
        painter.drawText(16, trackY, m_channelLabelWidth - 20, m_channelTrackHeight, Qt::AlignVCenter | Qt::AlignLeft, QString("CH%1").arg(ch));

        if (!m_channelsEnabled[ch]) continue;

        // Waveform plot track bounds
        int highY = trackY + 8;
        int lowY = trackY + m_channelTrackHeight - 10;

        // Use LOD Decimator queryRange
        auto points = m_decimator.queryRange(startSample, endSample, waveWidth, ch);
        if (points.empty()) {
            // Draw idle line if no capture data
            painter.setPen(QPen(chColor, 1));
            painter.drawLine(m_channelLabelWidth, lowY, width(), lowY);
            continue;
        }

        // Draw waveform segments
        painter.setPen(QPen(chColor, 1.5));
        int lastX = m_channelLabelWidth;
        int lastY = (points[0].firstV ? highY : lowY);

        for (const auto& pt : points) {
            int curX = m_channelLabelWidth + static_cast<int>(pt.x);

            if (pt.minV == pt.maxV) {
                // Steady state
                int curY = pt.minV ? highY : lowY;
                painter.drawLine(lastX, lastY, curX, curY);
                lastY = curY;
            } else {
                // Transition / toggle inside bucket: draw high, low, and transition bars
                painter.drawLine(lastX, lastY, curX, lastY);
                painter.drawLine(curX, lowY, curX, highY);
                lastY = pt.lastV ? highY : lowY;
            }
            lastX = curX;
        }
    }

    // 3. Draw Protocol Annotations (if present)
    for (const auto& pkt : m_packets) {
        int x0 = timeToX(m_buffer.sampleToTimeSec(pkt.startSample));
        int x1 = timeToX(m_buffer.sampleToTimeSec(pkt.endSample));
        if (x1 < m_channelLabelWidth || x0 > width()) continue;

        x0 = std::max(x0, m_channelLabelWidth);
        int w = std::max(6, x1 - x0);

        int annoY = m_timelineHeight + 8 * m_channelTrackHeight + 4;
        if (annoY + 22 <= totalHeight) {
            for (const auto& tok : pkt.tokens) {
                painter.fillRect(x0, annoY, w, 18, QColor(tok.color).darker(200));
                painter.setPen(QColor(tok.color));
                painter.drawRect(x0, annoY, w, 18);

                painter.setFont(QFont("Consolas", 8));
                painter.setPen(Qt::white);
                painter.drawText(x0 + 2, annoY, w - 4, 18, Qt::AlignCenter, tok.text);
            }
        }
    }

    // 4. Draw Cursors A, B, C, D
    int curAX = timeToX(m_cursorASec);
    int curBX = timeToX(m_cursorBSec);

    // Cursor A (Red)
    if (curAX >= m_channelLabelWidth && curAX <= width()) {
        painter.setPen(QPen(QColor(Theme::COLOR_CURSOR_A), 1.5, Qt::DashLine));
        painter.drawLine(curAX, m_timelineHeight, curAX, totalHeight);

        painter.fillRect(curAX - 10, 2, 20, m_timelineHeight - 4, QColor(Theme::COLOR_CURSOR_A));
        painter.setPen(Qt::white);
        painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
        painter.drawText(curAX - 10, 2, 20, m_timelineHeight - 4, Qt::AlignCenter, "A");
    }

    // Cursor B (Blue)
    if (curBX >= m_channelLabelWidth && curBX <= width()) {
        painter.setPen(QPen(QColor(Theme::COLOR_CURSOR_B), 1.5, Qt::DashLine));
        painter.drawLine(curBX, m_timelineHeight, curBX, totalHeight);

        painter.fillRect(curBX - 10, 2, 20, m_timelineHeight - 4, QColor(Theme::COLOR_CURSOR_B));
        painter.setPen(Qt::white);
        painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
        painter.drawText(curBX - 10, 2, 20, m_timelineHeight - 4, Qt::AlignCenter, "B");
    }

    // Cursor C (Teal)
    if (m_enableMarkersCD) {
        int curCX = timeToX(m_cursorCSec);
        int curDX = timeToX(m_cursorDSec);

        if (curCX >= m_channelLabelWidth && curCX <= width()) {
            painter.setPen(QPen(QColor("#00FFCC"), 1.5, Qt::DashDotLine));
            painter.drawLine(curCX, m_timelineHeight, curCX, totalHeight);

            painter.fillRect(curCX - 10, 2, 20, m_timelineHeight - 4, QColor("#008870"));
            painter.setPen(Qt::white);
            painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
            painter.drawText(curCX - 10, 2, 20, m_timelineHeight - 4, Qt::AlignCenter, "C");
        }

        // Cursor D (Purple)
        if (curDX >= m_channelLabelWidth && curDX <= width()) {
            painter.setPen(QPen(QColor("#D175FF"), 1.5, Qt::DashDotLine));
            painter.drawLine(curDX, m_timelineHeight, curDX, totalHeight);

            painter.fillRect(curDX - 10, 2, 20, m_timelineHeight - 4, QColor("#8A3DA8"));
            painter.setPen(Qt::white);
            painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
            painter.drawText(curDX - 10, 2, 20, m_timelineHeight - 4, Qt::AlignCenter, "D");
        }
    }

    // 5. Delta Badge (top right overlay)
    double deltaSec = std::abs(m_cursorBSec - m_cursorASec);
    double freqHz = deltaSec > 0.0 ? (1.0 / deltaSec) : 0.0;

    QString deltaStr;
    if (deltaSec >= 1e-3) deltaStr = QString("ΔAB: %1 ms").arg(deltaSec * 1e3, 0, 'f', 3);
    else if (deltaSec >= 1e-6) deltaStr = QString("ΔAB: %1 µs").arg(deltaSec * 1e6, 0, 'f', 2);
    else deltaStr = QString("ΔAB: %1 ns").arg(deltaSec * 1e9, 0, 'f', 1);

    QString freqStr;
    if (freqHz >= 1e6) freqStr = QString("f: %1 MHz").arg(freqHz / 1e6, 0, 'f', 2);
    else if (freqHz >= 1e3) freqStr = QString("f: %1 kHz").arg(freqHz / 1e3, 0, 'f', 2);
    else freqStr = QString("f: %1 Hz").arg(freqHz, 0, 'f', 1);

    QString badgeText = QString("%1 | %2").arg(deltaStr, freqStr);
    if (m_enableMarkersCD) {
        double deltaCDSec = std::abs(m_cursorDSec - m_cursorCSec);
        QString deltaCDStr;
        if (deltaCDSec >= 1e-3) deltaCDStr = QString("ΔCD: %1 ms").arg(deltaCDSec * 1e3, 0, 'f', 3);
        else if (deltaCDSec >= 1e-6) deltaCDStr = QString("ΔCD: %1 µs").arg(deltaCDSec * 1e6, 0, 'f', 2);
        else deltaCDStr = QString("ΔCD: %1 ns").arg(deltaCDSec * 1e9, 0, 'f', 1);
        badgeText += QString(" | %1").arg(deltaCDStr);
    }

    int badgeW = m_enableMarkersCD ? 300 : 200;
    int badgeX = width() - badgeW - 12;
    painter.fillRect(badgeX, 4, badgeW, m_timelineHeight - 6, QColor(36, 36, 48, 220));
    painter.setPen(QColor(Theme::COLOR_ACCENT));
    painter.drawRect(badgeX, 4, badgeW, m_timelineHeight - 6);
    painter.setFont(QFont("Consolas", 8, QFont::Bold));
    painter.drawText(badgeX, 4, badgeW, m_timelineHeight - 6, Qt::AlignCenter, badgeText);
}

void WaveformWidget::mousePressEvent(QMouseEvent* event) {
    int x = event->pos().x();
    int curAX = timeToX(m_cursorASec);
    int curBX = timeToX(m_cursorBSec);
    int curCX = m_enableMarkersCD ? timeToX(m_cursorCSec) : -999;
    int curDX = m_enableMarkersCD ? timeToX(m_cursorDSec) : -999;

    if (std::abs(x - curAX) < 8) {
        m_dragTarget = DragTarget::CursorA;
    } else if (std::abs(x - curBX) < 8) {
        m_dragTarget = DragTarget::CursorB;
    } else if (m_enableMarkersCD && std::abs(x - curCX) < 8) {
        m_dragTarget = DragTarget::CursorC;
    } else if (m_enableMarkersCD && std::abs(x - curDX) < 8) {
        m_dragTarget = DragTarget::CursorD;
    } else if (event->button() == Qt::LeftButton) {
        m_dragTarget = DragTarget::Pan;
        m_dragStartX = x;
        m_dragStartViewStartSec = m_viewStartSec;
    }
}

void WaveformWidget::mouseMoveEvent(QMouseEvent* event) {
    int x = event->pos().x();

    if (m_dragTarget == DragTarget::CursorA) {
        m_cursorASec = std::max(0.0, xToTime(x));
        update();
        double delta = std::abs(m_cursorBSec - m_cursorASec);
        emit cursorsChanged(m_cursorASec, m_cursorBSec, delta, delta > 0.0 ? 1.0 / delta : 0.0);
    } else if (m_dragTarget == DragTarget::CursorB) {
        m_cursorBSec = std::max(0.0, xToTime(x));
        update();
        double delta = std::abs(m_cursorBSec - m_cursorASec);
        emit cursorsChanged(m_cursorASec, m_cursorBSec, delta, delta > 0.0 ? 1.0 / delta : 0.0);
    } else if (m_dragTarget == DragTarget::CursorC) {
        m_cursorCSec = std::max(0.0, xToTime(x));
        update();
        double delta = std::abs(m_cursorDSec - m_cursorCSec);
        emit cursorsCDChanged(m_cursorCSec, m_cursorDSec, delta, delta > 0.0 ? 1.0 / delta : 0.0);
    } else if (m_dragTarget == DragTarget::CursorD) {
        m_cursorDSec = std::max(0.0, xToTime(x));
        update();
        double delta = std::abs(m_cursorDSec - m_cursorCSec);
        emit cursorsCDChanged(m_cursorCSec, m_cursorDSec, delta, delta > 0.0 ? 1.0 / delta : 0.0);
    } else if (m_dragTarget == DragTarget::Pan) {
        int dx = x - m_dragStartX;
        double span = m_viewEndSec - m_viewStartSec;
        int w = width() - m_channelLabelWidth;
        if (w > 0) {
            double dt = (static_cast<double>(dx) / w) * span;
            m_viewStartSec = std::max(0.0, m_dragStartViewStartSec - dt);
            m_viewEndSec = m_viewStartSec + span;
            update();
            emit viewSpanChanged(m_viewStartSec, m_viewEndSec);
        }
    }
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent*) {
    m_dragTarget = DragTarget::None;
}

void WaveformWidget::wheelEvent(QWheelEvent* event) {
    double factor = event->angleDelta().y() > 0 ? 0.75 : 1.33;
    double mouseSec = xToTime(event->position().toPoint().x());
    double span = m_viewEndSec - m_viewStartSec;
    double newSpan = span * factor;

    if (newSpan > 1e-9 && newSpan < 100.0) {
        double mouseRel = (mouseSec - m_viewStartSec) / span;
        m_viewStartSec = std::max(0.0, mouseSec - mouseRel * newSpan);
        m_viewEndSec = m_viewStartSec + newSpan;
        update();
        emit viewSpanChanged(m_viewStartSec, m_viewEndSec);
    }
}

void WaveformWidget::resizeEvent(QResizeEvent*) {
    update();
}

} // namespace UdooKey
