#include "StreamDemux.h"
#include "Logger.h"
#include <cstring>
#include <algorithm>

namespace UdooKey {

// 0x5A Protocol commands (compatible with Python companion driver)
static constexpr uint8_t PROTOCOL_MAGIC_0x5A = 0x5A;
static constexpr uint8_t CMD_GET_INFO        = 0x01;
static constexpr uint8_t CMD_SET_CONFIG      = 0x02;
static constexpr uint8_t CMD_START_CAPTURE   = 0x03;
static constexpr uint8_t CMD_STOP_CAPTURE    = 0x04;
static constexpr uint8_t CMD_STREAM_DATA     = 0x05;

StreamDemux::StreamDemux(QObject* parent)
    : QObject(parent) {
}

void StreamDemux::reset() {
    m_state = DemuxState::Idle;
    m_buffer.clear();
    m_expected0x5ALength = 0;
    m_frame0x5ABuffer.clear();
    m_expectedSumpBytes = 0;
    m_sumpBuffer.clear();
    std::memset(&m_pending0xAA55Header, 0, sizeof(m_pending0xAA55Header));
    m_pending0xAA55Payload.clear();
}

void StreamDemux::expectSumpCapture(uint32_t expectedBytes) {
    m_state = DemuxState::CollectingSumpData;
    m_expectedSumpBytes = expectedBytes;
    m_sumpBuffer.clear();
}

void StreamDemux::processIncomingData(const QByteArray& data) {
    if (data.isEmpty()) return;
    m_totalBytesProcessed += data.size();

    // 1. Collecting SUMP capture data
    if (m_state == DemuxState::CollectingSumpData) {
        m_sumpBuffer.append(data);
        if (static_cast<uint32_t>(m_sumpBuffer.size()) >= m_expectedSumpBytes) {
            QByteArray capture = m_sumpBuffer.left(m_expectedSumpBytes);
            QByteArray remaining = m_sumpBuffer.mid(m_expectedSumpBytes);
            m_sumpBuffer.clear();
            m_state = DemuxState::Idle;
            m_expectedSumpBytes = 0;
            emit sumpDataReceived(capture);
            if (!remaining.isEmpty()) {
                processIncomingData(remaining);
            }
        }
        return;
    }

    // 2. Collecting 0xAA55 InstrumentProtocol payload
    if (m_state == DemuxState::CollectingInstrument0xAA55) {
        m_pending0xAA55Payload.append(data);
        uint32_t expLen = m_pending0xAA55Header.payload_len;
        if (static_cast<uint32_t>(m_pending0xAA55Payload.size()) >= expLen) {
            QByteArray payload = m_pending0xAA55Payload.left(expLen);
            QByteArray remaining = m_pending0xAA55Payload.mid(expLen);
            m_pending0xAA55Payload.clear();
            m_state = DemuxState::Idle;
            m_packets0xAA55Count++;
            emit instrumentPacketReceived(m_pending0xAA55Header, payload);
            if (!remaining.isEmpty()) {
                processIncomingData(remaining);
            }
        }
        return;
    }

    // 3. Collecting 0x5A binary frame (Logic Capture Buffer)
    if (m_state == DemuxState::CollectingBinary0x5A) {
        m_frame0x5ABuffer.append(data);
        if (static_cast<uint32_t>(m_frame0x5ABuffer.size()) >= m_expected0x5ALength) {
            QByteArray payload = m_frame0x5ABuffer.left(m_expected0x5ALength);
            QByteArray remaining = m_frame0x5ABuffer.mid(m_expected0x5ALength);
            m_frame0x5ABuffer.clear();
            m_state = DemuxState::Idle;
            m_expected0x5ALength = 0;
            m_frames0x5ACount++;
            emit binary0x5AFrameReceived(payload);
            if (!remaining.isEmpty()) {
                processIncomingData(remaining);
            }
        }
        return;
    }

    // 4. Idle state: Scan for candidate markers
    m_buffer.append(data);
    parseIdleBuffer();
}

void StreamDemux::parseIdleBuffer() {
    while (!m_buffer.isEmpty()) {
        int idx1ALS = m_buffer.indexOf("1ALS");
        int idx0xAA55 = m_buffer.indexOf(QByteArray("\x55\xAA", 2));
        int idx0x5A = -1;

        // Search for 0x5A ONLY if followed by valid command 1..5 or legacy 0x00
        for (int i = 0; i < m_buffer.size(); ++i) {
            if (static_cast<uint8_t>(m_buffer.at(i)) == PROTOCOL_MAGIC_0x5A) {
                if (i + 1 < m_buffer.size()) {
                    uint8_t cmd = static_cast<uint8_t>(m_buffer.at(i + 1));
                    if ((cmd >= CMD_GET_INFO && cmd <= CMD_STREAM_DATA) || cmd == 0x00) {
                        idx0x5A = i;
                        break;
                    }
                } else {
                    // 0x5A at end of buffer: wait for next byte
                    idx0x5A = i;
                    break;
                }
            }
        }

        // Find the earliest valid marker
        int minIdx = -1;
        enum MarkerType { None, Sump1ALS, Proto0xAA55, Proto0x5A } marker = None;

        if (idx1ALS != -1 && (minIdx == -1 || idx1ALS < minIdx)) {
            minIdx = idx1ALS;
            marker = Sump1ALS;
        }
        if (idx0xAA55 != -1 && (minIdx == -1 || idx0xAA55 < minIdx)) {
            // Verify if candidate 0xAA55 has valid version byte (0x01)
            if (idx0xAA55 + 2 < m_buffer.size()) {
                uint8_t ver = static_cast<uint8_t>(m_buffer.at(idx0xAA55 + 2));
                if (ver == INSTRUMENT_PROTO_VERSION) {
                    minIdx = idx0xAA55;
                    marker = Proto0xAA55;
                }
            } else {
                // Not enough bytes to confirm version yet, wait
                minIdx = idx0xAA55;
                marker = Proto0xAA55;
            }
        }
        if (idx0x5A != -1 && (minIdx == -1 || idx0x5A < minIdx)) {
            minIdx = idx0x5A;
            marker = Proto0x5A;
        }

        // If no binary markers found, the entire buffer is terminal text
        if (marker == None) {
            QString text = QString::fromUtf8(m_buffer);
            m_totalTerminalBytes += m_buffer.size();
            emit terminalTextReceived(text);
            m_buffer.clear();
            return;
        }

        // Emit any terminal text that precedes the marker
        if (minIdx > 0) {
            QString text = QString::fromUtf8(m_buffer.left(minIdx));
            m_totalTerminalBytes += minIdx;
            emit terminalTextReceived(text);
            m_buffer.remove(0, minIdx);
        }

        // Now m_buffer starts at the marker
        if (marker == Sump1ALS) {
            emit sumpDataReceived(QByteArray("1ALS"));
            m_buffer.remove(0, 4);
            continue;
        }

        if (marker == Proto0xAA55) {
            if (m_buffer.size() < static_cast<int>(sizeof(InstrumentPacketHeader))) {
                // Wait for complete 10-byte header
                return;
            }

            InstrumentPacketHeader hdr;
            std::memcpy(&hdr, m_buffer.constData(), sizeof(InstrumentPacketHeader));

            if (hdr.magic != INSTRUMENT_PROTO_MAGIC || hdr.version != INSTRUMENT_PROTO_VERSION || hdr.payload_len > 2048) {
                // Invalid header (false positive match on \x55\xAA in text)
                QString text = QString::fromUtf8(m_buffer.left(2));
                m_totalTerminalBytes += 2;
                emit terminalTextReceived(text);
                m_buffer.remove(0, 2);
                continue;
            }

            // Valid 0xAA55 header
            m_buffer.remove(0, sizeof(InstrumentPacketHeader));
            if (hdr.payload_len == 0) {
                m_packets0xAA55Count++;
                emit instrumentPacketReceived(hdr, QByteArray());
                continue;
            }

            m_pending0xAA55Header = hdr;
            m_pending0xAA55Payload.clear();
            m_state = DemuxState::CollectingInstrument0xAA55;

            int available = std::min(static_cast<uint32_t>(m_buffer.size()), static_cast<uint32_t>(hdr.payload_len));
            m_pending0xAA55Payload.append(m_buffer.left(available));
            m_buffer.remove(0, available);

            if (static_cast<uint32_t>(m_pending0xAA55Payload.size()) >= hdr.payload_len) {
                QByteArray payload = m_pending0xAA55Payload;
                m_pending0xAA55Payload.clear();
                m_state = DemuxState::Idle;
                m_packets0xAA55Count++;
                emit instrumentPacketReceived(hdr, payload);
                continue;
            }
            return;
        }

        if (marker == Proto0x5A) {
            if (m_buffer.size() < 2) {
                // Need at least magic and command
                return;
            }

            uint8_t cmd = static_cast<uint8_t>(m_buffer.at(1));
            if (cmd != 0x00 && (cmd < CMD_GET_INFO || cmd > CMD_STREAM_DATA)) {
                // Not a valid 0x5A command: just character 'Z' in terminal output
                emit terminalTextReceived("Z");
                m_totalTerminalBytes += 1;
                m_buffer.remove(0, 1);
                continue;
            }

            // Legacy 4-byte header: [0x5A][0x00][L1][L0]
            if (cmd == 0x00) {
                if (m_buffer.size() < 4) return;
                uint8_t b1 = static_cast<uint8_t>(m_buffer.at(1));
                uint8_t b2 = static_cast<uint8_t>(m_buffer.at(2));
                uint8_t b3 = static_cast<uint8_t>(m_buffer.at(3));
                uint32_t len = (static_cast<uint32_t>(b1) << 16) | (static_cast<uint32_t>(b2) << 8) | static_cast<uint32_t>(b3);

                if (len > 0 && len <= 1024 * 1024) {
                    m_expected0x5ALength = len;
                    m_state = DemuxState::CollectingBinary0x5A;
                    m_frame0x5ABuffer.clear();
                    m_buffer.remove(0, 4);

                    int available = std::min(static_cast<uint32_t>(m_buffer.size()), m_expected0x5ALength);
                    m_frame0x5ABuffer.append(m_buffer.left(available));
                    m_buffer.remove(0, available);

                    if (static_cast<uint32_t>(m_frame0x5ABuffer.size()) >= m_expected0x5ALength) {
                        QByteArray payload = m_frame0x5ABuffer;
                        m_frame0x5ABuffer.clear();
                        m_state = DemuxState::Idle;
                        m_expected0x5ALength = 0;
                        m_frames0x5ACount++;
                        emit binary0x5AFrameReceived(payload);
                        continue;
                    }
                    return;
                } else {
                    emit terminalTextReceived("Z");
                    m_totalTerminalBytes += 1;
                    m_buffer.remove(0, 1);
                    continue;
                }
            }

            if (cmd == CMD_STREAM_DATA) {
                // Needs 4-byte SerialHeader + 8-byte metadata (buf_len + sample_rate) = 12 bytes
                if (m_buffer.size() < 12) {
                    return;
                }

                uint32_t bufLen = 0;
                uint32_t sampleRate = 0;
                std::memcpy(&bufLen, m_buffer.constData() + 4, sizeof(uint32_t));
                std::memcpy(&sampleRate, m_buffer.constData() + 8, sizeof(uint32_t));

                if (bufLen > 0 && bufLen <= 1024 * 1024) {
                    m_expected0x5ALength = bufLen;
                    m_state = DemuxState::CollectingBinary0x5A;
                    m_frame0x5ABuffer.clear();

                    // Consume the 12-byte header
                    m_buffer.remove(0, 12);

                    int available = std::min(static_cast<uint32_t>(m_buffer.size()), m_expected0x5ALength);
                    m_frame0x5ABuffer.append(m_buffer.left(available));
                    m_buffer.remove(0, available);

                    if (static_cast<uint32_t>(m_frame0x5ABuffer.size()) >= m_expected0x5ALength) {
                        QByteArray payload = m_frame0x5ABuffer;
                        m_frame0x5ABuffer.clear();
                        m_state = DemuxState::Idle;
                        m_expected0x5ALength = 0;
                        m_frames0x5ACount++;
                        emit binary0x5AFrameReceived(payload);
                        continue;
                    }
                    return;
                } else {
                    // Corrupted length, consume magic and treat as text
                    emit terminalTextReceived("Z");
                    m_totalTerminalBytes += 1;
                    m_buffer.remove(0, 1);
                    continue;
                }
            } else {
                // Other 0x5A commands (CMD_GET_INFO, etc.): 4-byte header
                if (m_buffer.size() < 4) return;
                uint16_t plen = 0;
                std::memcpy(&plen, m_buffer.constData() + 2, sizeof(uint16_t));
                if (m_buffer.size() < 4 + plen) return;

                QByteArray frame = m_buffer.mid(4, plen);
                m_buffer.remove(0, 4 + plen);
                m_frames0x5ACount++;
                emit binary0x5AFrameReceived(frame);
                continue;
            }
        }
    }
}

} // namespace UdooKey
