#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVector>
#include "CaptureBuffer.h"
#include "LodDecimator.h"
#include "Types.h"

namespace UdooKey {

class WaveformWidget : public QWidget {
    Q_OBJECT

public:
    explicit WaveformWidget(QWidget* parent = nullptr);
    ~WaveformWidget() override = default;

    void setCaptureData(const CaptureBuffer& buffer, const LodDecimator& decimator);
    void setCaptureData(std::shared_ptr<const CaptureBuffer> buffer, std::shared_ptr<const LodDecimator> decimator);
    void setDecodedPackets(const QVector<DecodedPacket>& packets);
    void setChannelEnabled(uint8_t channel, bool enabled);

    void zoomIn();
    void zoomOut();
    void zoomToFit();

    double cursorAPosSec() const { return m_cursorASec; }
    double cursorBPosSec() const { return m_cursorBSec; }
    double cursorCPosSec() const { return m_cursorCSec; }
    double cursorDPosSec() const { return m_cursorDSec; }
    double cursorDeltaABSec() const { return std::abs(m_cursorBSec - m_cursorASec); }
    double cursorDeltaCDSec() const { return std::abs(m_cursorDSec - m_cursorCSec); }

    void setEnableMarkersCD(bool enabled) { m_enableMarkersCD = enabled; update(); }
    bool markersCDEnabled() const { return m_enableMarkersCD; }

signals:
    void cursorsChanged(double cursorASec, double cursorBSec, double deltaSec, double freqHz);
    void cursorsCDChanged(double cursorCSec, double cursorDSec, double deltaSec, double freqHz);
    void viewSpanChanged(double startSec, double endSec);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    double xToTime(int x) const;
    int timeToX(double timeSec) const;
    uint32_t timeToSample(double timeSec) const;

    CaptureBuffer m_buffer;
    LodDecimator m_decimator;
    QVector<DecodedPacket> m_packets;

    bool m_channelsEnabled[8] = {true, true, true, true, true, true, true, true};

    // Viewport span in seconds
    double m_viewStartSec = 0.0;
    double m_viewEndSec = 0.0065; // ~6.5ms default (65k samples @ 10MHz)

    // Cursors A, B, C, D
    double m_cursorASec = 0.001;
    double m_cursorBSec = 0.003;
    double m_cursorCSec = 0.004;
    double m_cursorDSec = 0.005;
    bool m_enableMarkersCD = true;

    enum class DragTarget { None, Pan, CursorA, CursorB, CursorC, CursorD };
    DragTarget m_dragTarget = DragTarget::None;
    int m_dragStartX = 0;
    double m_dragStartViewStartSec = 0.0;

    int m_channelTrackHeight = 44;
    int m_channelLabelWidth = 70;
    int m_timelineHeight = 24;
};

} // namespace UdooKey
