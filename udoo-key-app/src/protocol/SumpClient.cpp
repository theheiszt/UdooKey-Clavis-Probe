#include "SumpClient.h"
#include "Logger.h"

namespace UdooKey {

SumpClient::SumpClient(ITransport* transport, QObject* parent)
    : QObject(parent)
    , m_transport(transport) {
}

void SumpClient::setTransport(ITransport* transport) {
    m_transport = transport;
}

void SumpClient::sendShortCommand(uint8_t cmd) {
    if (!m_transport || !m_transport->isOpen()) return;
    char b = static_cast<char>(cmd);
    m_transport->write(&b, 1);
}

void SumpClient::sendLongCommand(uint8_t cmd, uint32_t data) {
    if (!m_transport || !m_transport->isOpen()) return;
    QByteArray pkt;
    pkt.append(static_cast<char>(cmd));
    // SUMP long command data is 4 bytes little endian
    pkt.append(static_cast<char>(data & 0xFF));
    pkt.append(static_cast<char>((data >> 8) & 0xFF));
    pkt.append(static_cast<char>((data >> 16) & 0xFF));
    pkt.append(static_cast<char>((data >> 24) & 0xFF));
    m_transport->write(pkt);
}

void SumpClient::reset() {
    if (!m_transport || !m_transport->isOpen()) return;
    LOG_INFO("SUMP: Sending 5x reset bytes");
    QByteArray resetSeq("\x00\x00\x00\x00\x00", 5);
    m_transport->write(resetSeq);
}

void SumpClient::queryId() {
    LOG_INFO("SUMP: Querying device ID (0x02)");
    m_expectingId = true;
    sendShortCommand(0x02);
}

void SumpClient::queryMetadata() {
    LOG_INFO("SUMP: Querying device metadata (0x04)");
    m_expectingMeta = true;
    sendShortCommand(0x04);
}

void SumpClient::setDivider(uint32_t divider) {
    LOG_INFO(QString("SUMP: Setting clock divider: %1").arg(divider));
    sendLongCommand(0x80, divider);
}

void SumpClient::setReadAndDelayCount(uint16_t readCount, uint16_t delayCount) {
    uint32_t data = (static_cast<uint32_t>(delayCount) << 16) | readCount;
    LOG_INFO(QString("SUMP: Setting read count: %1, delay: %2").arg(readCount).arg(delayCount));
    sendLongCommand(0x81, data);
}

void SumpClient::setFlags(uint16_t flags) {
    sendLongCommand(0x82, flags);
}

void SumpClient::armRun() {
    LOG_INFO("SUMP: Arming acquisition run (0x01)");
    sendShortCommand(0x01);
}

void SumpClient::onSumpDataReceived(const QByteArray& data) {
    if (m_expectingId && data.contains("1ALS")) {
        m_expectingId = false;
        LOG_INFO("SUMP: ID match: 1ALS");
        emit deviceIdReceived("1ALS (SUMP Logic Analyzer)");
        return;
    }

    if (m_expectingMeta) {
        m_expectingMeta = false;
        QMap<int, QVariant> meta;
        int i = 0;
        while (i < data.size()) {
            uint8_t key = static_cast<uint8_t>(data.at(i++));
            if (key == 0x00) break; // End of metadata

            if (key == 0x01) { // Device name string
                QByteArray str;
                while (i < data.size() && data.at(i) != 0x00) {
                    str.append(data.at(i++));
                }
                if (i < data.size()) i++; // skip null
                meta[key] = QString::fromUtf8(str);
            } else if (key >= 0x20 && key <= 0x3F) { // 32-bit integer
                if (i + 4 <= data.size()) {
                    uint32_t val = (static_cast<uint8_t>(data.at(i)) << 24) |
                                   (static_cast<uint8_t>(data.at(i + 1)) << 16) |
                                   (static_cast<uint8_t>(data.at(i + 2)) << 8) |
                                   static_cast<uint8_t>(data.at(i + 3));
                    meta[key] = val;
                    i += 4;
                }
            } else if (key >= 0x40 && key <= 0x5F) { // 8-bit integer
                if (i < data.size()) {
                    meta[key] = static_cast<uint8_t>(data.at(i++));
                }
            }
        }
        LOG_INFO(QString("SUMP: Parsed %1 metadata keys").arg(meta.size()));
        emit metadataReceived(meta);
        return;
    }

    emit captureReceived(data);
}

} // namespace UdooKey
