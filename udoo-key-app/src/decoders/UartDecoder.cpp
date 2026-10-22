#include "UartDecoder.h"
#include <cmath>

namespace UdooKey {

UartDecoder::UartDecoder() {
}

QMap<QString, QVariant> UartDecoder::parameters() const {
    QMap<QString, QVariant> params;
    params["channel"] = m_channel;
    params["baudRate"] = m_baudRate;
    params["dataBits"] = m_dataBits;
    params["parity"] = m_parity;
    params["stopBits"] = m_stopBits;
    return params;
}

void UartDecoder::setParameter(const QString& key, const QVariant& value) {
    if (key == "channel") m_channel = static_cast<uint8_t>(value.toUInt());
    else if (key == "baudRate") m_baudRate = value.toUInt();
    else if (key == "dataBits") m_dataBits = static_cast<uint8_t>(value.toUInt());
    else if (key == "parity") m_parity = value.toString();
    else if (key == "stopBits") m_stopBits = static_cast<uint8_t>(value.toUInt());
}

QVector<DecodedPacket> UartDecoder::decode(const CaptureBuffer& buffer) {
    QVector<DecodedPacket> packets;
    if (buffer.isEmpty() || buffer.sampleRateHz() == 0 || m_baudRate == 0) {
        return packets;
    }

    uint32_t sampleRate = buffer.sampleRateHz();
    double samplesPerBit = static_cast<double>(sampleRate) / m_baudRate;
    if (samplesPerBit < 2.0) {
        // Sample rate too low for this baud rate
        return packets;
    }

    uint32_t totalSamples = buffer.sampleCount();
    uint32_t idx = 0;

    while (idx + static_cast<uint32_t>(samplesPerBit * (1 + m_dataBits + m_stopBits)) < totalSamples) {
        // Look for start bit (high to low transition, since idle is high)
        if (!buffer.channelState(m_channel, idx) && (idx == 0 || buffer.channelState(m_channel, idx - 1))) {
            uint32_t startSample = idx;
            // Verify center of start bit is still low
            uint32_t startCenter = idx + static_cast<uint32_t>(samplesPerBit * 0.5);
            if (startCenter >= totalSamples || buffer.channelState(m_channel, startCenter)) {
                idx++;
                continue;
            }

            // Sample data bits at centers
            uint8_t byteVal = 0;
            bool valid = true;
            for (uint8_t b = 0; b < m_dataBits; ++b) {
                uint32_t bitCenter = idx + static_cast<uint32_t>(samplesPerBit * (1.5 + b));
                if (bitCenter >= totalSamples) {
                    valid = false;
                    break;
                }
                if (buffer.channelState(m_channel, bitCenter)) {
                    byteVal |= (1 << b);
                }
            }

            if (!valid) {
                idx += static_cast<uint32_t>(samplesPerBit);
                continue;
            }

            // Stop bit check
            uint32_t stopCenter = idx + static_cast<uint32_t>(samplesPerBit * (1.5 + m_dataBits));
            uint32_t endSample = idx + static_cast<uint32_t>(samplesPerBit * (1 + m_dataBits + m_stopBits));
            bool framingError = (stopCenter < totalSamples && !buffer.channelState(m_channel, stopCenter));

            DecodedPacket pkt;
            pkt.startSample = startSample;
            pkt.endSample = endSample;
            pkt.title = QString("0x%1").arg(byteVal, 2, 16, QChar('0')).toUpper();

            DecodedToken tok;
            tok.startSample = startSample;
            tok.endSample = endSample;
            tok.startTimeSec = buffer.sampleToTimeSec(startSample);
            tok.endTimeSec = buffer.sampleToTimeSec(endSample);
            tok.color = framingError ? 0xFF5555 : 0x00FFCC;
            tok.isWarning = framingError;

            char ascii = (byteVal >= 32 && byteVal <= 126) ? static_cast<char>(byteVal) : '.';
            tok.text = QString("0x%1 '%2'%3")
                           .arg(byteVal, 2, 16, QChar('0')).toUpper()
                           .arg(ascii)
                           .arg(framingError ? " [FRAME ERR]" : "");

            pkt.tokens.append(tok);
            packets.append(pkt);

            idx = endSample;
        } else {
            idx++;
        }
    }

    return packets;
}

} // namespace UdooKey
