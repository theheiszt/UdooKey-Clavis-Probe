#include "I2cDecoder.h"

namespace UdooKey {

I2cDecoder::I2cDecoder() {
}

QMap<QString, QVariant> I2cDecoder::parameters() const {
    QMap<QString, QVariant> params;
    params["sclChannel"] = m_sclChannel;
    params["sdaChannel"] = m_sdaChannel;
    return params;
}

void I2cDecoder::setParameter(const QString& key, const QVariant& value) {
    if (key == "sclChannel") m_sclChannel = static_cast<uint8_t>(value.toUInt());
    else if (key == "sdaChannel") m_sdaChannel = static_cast<uint8_t>(value.toUInt());
}

QVector<DecodedPacket> I2cDecoder::decode(const CaptureBuffer& buffer) {
    QVector<DecodedPacket> packets;
    if (buffer.isEmpty() || buffer.sampleCount() < 4) {
        return packets;
    }

    uint32_t totalSamples = buffer.sampleCount();
    bool inTransaction = false;
    bool isFirstByte = true;
    uint32_t transactionStart = 0;

    uint8_t bitCount = 0;
    uint8_t currentByte = 0;
    uint32_t byteStart = 0;

    bool lastScl = buffer.channelState(m_sclChannel, 0);
    bool lastSda = buffer.channelState(m_sdaChannel, 0);

    for (uint32_t i = 1; i < totalSamples; ++i) {
        bool scl = buffer.channelState(m_sclChannel, i);
        bool sda = buffer.channelState(m_sdaChannel, i);

        // START Condition: SDA falls while SCL is high
        if (scl && lastSda && !sda) {
            inTransaction = true;
            isFirstByte = true;
            transactionStart = i;
            bitCount = 0;
            currentByte = 0;
            byteStart = i;

            DecodedPacket pkt;
            pkt.startSample = i;
            pkt.endSample = i + 1;
            pkt.title = "START";

            DecodedToken tok;
            tok.startSample = i;
            tok.endSample = i + 1;
            tok.startTimeSec = buffer.sampleToTimeSec(i);
            tok.endTimeSec = buffer.sampleToTimeSec(i + 1);
            tok.text = "START";
            tok.color = 0x55FF55;
            pkt.tokens.append(tok);
            packets.append(pkt);

            lastScl = scl;
            lastSda = sda;
            continue;
        }

        // STOP Condition: SDA rises while SCL is high
        if (scl && !lastSda && sda) {
            if (inTransaction) {
                inTransaction = false;

                DecodedPacket pkt;
                pkt.startSample = i;
                pkt.endSample = i + 1;
                pkt.title = "STOP";

                DecodedToken tok;
                tok.startSample = i;
                tok.endSample = i + 1;
                tok.startTimeSec = buffer.sampleToTimeSec(i);
                tok.endTimeSec = buffer.sampleToTimeSec(i + 1);
                tok.text = "STOP";
                tok.color = 0xFF5555;
                pkt.tokens.append(tok);
                packets.append(pkt);
            }
            lastScl = scl;
            lastSda = sda;
            continue;
        }

        // Clock Edge: Sample data on SCL rising edge
        if (!lastScl && scl && inTransaction) {
            if (bitCount < 8) {
                currentByte = (currentByte << 1) | (sda ? 1 : 0);
                bitCount++;
            } else if (bitCount == 8) {
                // 9th bit: ACK / NACK
                bool ack = !sda; // Low is ACK, High is NACK
                DecodedPacket pkt;
                pkt.startSample = byteStart;
                pkt.endSample = i;

                DecodedToken tok;
                tok.startSample = byteStart;
                tok.endSample = i;
                tok.startTimeSec = buffer.sampleToTimeSec(byteStart);
                tok.endTimeSec = buffer.sampleToTimeSec(i);

                if (isFirstByte) {
                    uint8_t addr7 = currentByte >> 1;
                    bool isRead = (currentByte & 0x01) != 0;
                    pkt.title = QString("Addr 0x%1 %2").arg(addr7, 2, 16, QChar('0')).toUpper().arg(isRead ? "R" : "W");
                    tok.text = QString("Addr: 0x%1 [%2] %3")
                                   .arg(addr7, 2, 16, QChar('0')).toUpper()
                                   .arg(isRead ? "READ" : "WRITE")
                                   .arg(ack ? "ACK" : "NACK");
                    tok.color = 0x33CCFF;
                    isFirstByte = false;
                } else {
                    pkt.title = QString("Data 0x%1").arg(currentByte, 2, 16, QChar('0')).toUpper();
                    tok.text = QString("Data: 0x%1 %2")
                                   .arg(currentByte, 2, 16, QChar('0')).toUpper()
                                   .arg(ack ? "ACK" : "NACK");
                    tok.color = 0xFFCC00;
                }
                tok.isWarning = !ack;

                pkt.tokens.append(tok);
                packets.append(pkt);

                bitCount = 0;
                currentByte = 0;
                byteStart = i;
            }
        }

        lastScl = scl;
        lastSda = sda;
    }

    return packets;
}

} // namespace UdooKey
