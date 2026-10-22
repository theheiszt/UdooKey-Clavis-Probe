#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include "instrument_protocol.h"

namespace UdooKey {

enum class DemuxState {
    Idle,
    CollectingBinary0x5A,
    CollectingSumpData,
    CollectingInstrument0xAA55
};

class StreamDemux : public QObject {
    Q_OBJECT

public:
    explicit StreamDemux(QObject* parent = nullptr);
    ~StreamDemux() override = default;

    void processIncomingData(const QByteArray& data);
    void reset();

    void expectSumpCapture(uint32_t expectedBytes);

    DemuxState currentState() const { return m_state; }
    quint64 totalBytesProcessed() const { return m_totalBytesProcessed; }
    quint64 totalTerminalBytes() const { return m_totalTerminalBytes; }
    quint32 packets0xAA55Parsed() const { return m_packets0xAA55Count; }
    quint32 frames0x5AParsed() const { return m_frames0x5ACount; }

signals:
    void binary0x5AFrameReceived(const QByteArray& payload);
    void sumpDataReceived(const QByteArray& data);
    void terminalTextReceived(const QString& text);
    void instrumentPacketReceived(const InstrumentPacketHeader& hdr, const QByteArray& payload);

private:
    void parseIdleBuffer();

    DemuxState m_state = DemuxState::Idle;
    QByteArray m_buffer;

    // 0x5A binary frame tracking
    uint32_t m_expected0x5ALength = 0;
    QByteArray m_frame0x5ABuffer;

    // SUMP capture tracking
    uint32_t m_expectedSumpBytes = 0;
    QByteArray m_sumpBuffer;

    // 0xAA55 InstrumentProtocol tracking
    InstrumentPacketHeader m_pending0xAA55Header{};
    QByteArray m_pending0xAA55Payload;

    // Diagnostics
    quint64 m_totalBytesProcessed = 0;
    quint64 m_totalTerminalBytes = 0;
    quint32 m_packets0xAA55Count = 0;
    quint32 m_frames0x5ACount = 0;
};

} // namespace UdooKey
