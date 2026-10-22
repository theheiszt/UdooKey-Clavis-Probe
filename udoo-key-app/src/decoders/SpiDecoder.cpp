#include "SpiDecoder.h"

namespace UdooKey {

SpiDecoder::SpiDecoder() {
}

QMap<QString, QVariant> SpiDecoder::parameters() const {
    QMap<QString, QVariant> params;
    params["sckChannel"] = m_sckChannel;
    params["mosiChannel"] = m_mosiChannel;
    params["misoChannel"] = m_misoChannel;
    params["csChannel"] = m_csChannel;
    params["mode"] = m_mode;
    params["csActiveLow"] = m_csActiveLow;
    return params;
}

void SpiDecoder::setParameter(const QString& key, const QVariant& value) {
    if (key == "sckChannel") m_sckChannel = static_cast<uint8_t>(value.toUInt());
    else if (key == "mosiChannel") m_mosiChannel = static_cast<uint8_t>(value.toUInt());
    else if (key == "misoChannel") m_misoChannel = static_cast<uint8_t>(value.toUInt());
    else if (key == "csChannel") m_csChannel = static_cast<uint8_t>(value.toUInt());
    else if (key == "mode") m_mode = static_cast<uint8_t>(value.toUInt());
    else if (key == "csActiveLow") m_csActiveLow = value.toBool();
}

QVector<DecodedPacket> SpiDecoder::decode(const CaptureBuffer& buffer) {
    QVector<DecodedPacket> packets;
    if (buffer.isEmpty() || buffer.sampleCount() < 8) {
        return packets;
    }

    // SPI Mode:
    // Mode 0: CPOL=0, CPHA=0 (Sample on SCK rising edge)
    // Mode 1: CPOL=0, CPHA=1 (Sample on SCK falling edge)
    // Mode 2: CPOL=1, CPHA=0 (Sample on SCK falling edge)
    // Mode 3: CPOL=1, CPHA=1 (Sample on SCK rising edge)
    bool sampleOnRising = (m_mode == 0 || m_mode == 3);

    uint32_t totalSamples = buffer.sampleCount();
    bool lastSck = buffer.channelState(m_sckChannel, 0);

    uint8_t bitCount = 0;
    uint8_t mosiByte = 0;
    uint8_t misoByte = 0;
    uint32_t byteStart = 0;

    for (uint32_t i = 1; i < totalSamples; ++i) {
        bool cs = buffer.channelState(m_csChannel, i);
        bool csActive = m_csActiveLow ? !cs : cs;

        if (!csActive) {
            // CS inactive: reset byte state
            bitCount = 0;
            mosiByte = 0;
            misoByte = 0;
            lastSck = buffer.channelState(m_sckChannel, i);
            continue;
        }

        bool sck = buffer.channelState(m_sckChannel, i);
        bool isSampleEdge = sampleOnRising ? (!lastSck && sck) : (lastSck && !sck);

        if (isSampleEdge) {
            if (bitCount == 0) {
                byteStart = i;
            }

            bool mosiBit = buffer.channelState(m_mosiChannel, i);
            bool misoBit = buffer.channelState(m_misoChannel, i);

            mosiByte = (mosiByte << 1) | (mosiBit ? 1 : 0);
            misoByte = (misoByte << 1) | (misoBit ? 1 : 0);
            bitCount++;

            if (bitCount == 8) {
                DecodedPacket pkt;
                pkt.startSample = byteStart;
                pkt.endSample = i;
                pkt.title = QString("MOSI: 0x%1 | MISO: 0x%2")
                                .arg(mosiByte, 2, 16, QChar('0')).toUpper()
                                .arg(misoByte, 2, 16, QChar('0')).toUpper();

                DecodedToken tok;
                tok.startSample = byteStart;
                tok.endSample = i;
                tok.startTimeSec = buffer.sampleToTimeSec(byteStart);
                tok.endTimeSec = buffer.sampleToTimeSec(i);
                tok.text = pkt.title;
                tok.color = 0xAA66FF;

                pkt.tokens.append(tok);
                packets.append(pkt);

                bitCount = 0;
                mosiByte = 0;
                misoByte = 0;
            }
        }

        lastSck = sck;
    }

    return packets;
}

} // namespace UdooKey
