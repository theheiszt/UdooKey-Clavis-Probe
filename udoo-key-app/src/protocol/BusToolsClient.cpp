#include "BusToolsClient.h"
#include <cstring>

namespace UdooKey {

BusToolsClient::BusToolsClient(ITransport* transport, QObject* parent)
    : QObject(parent) {
    if (transport) {
        setTransport(transport);
    }
}

void BusToolsClient::setTransport(ITransport* transport) {
    if (m_transport == transport) return;

    if (m_transport) {
        disconnect(m_transport, &ITransport::dataReady, this, &BusToolsClient::processIncomingData);
    }

    m_transport = transport;

    if (m_transport) {
        connect(m_transport, &ITransport::dataReady, this, &BusToolsClient::processIncomingData);
    }
}

bool BusToolsClient::isConnected() const {
    return m_transport && m_transport->isOpen();
}

// ─── UART Bridge Operations ──────────────────────────────────────────────────
bool BusToolsClient::configureUart(uint32_t baudrate, uint8_t dataBits,
                                   uint8_t parity, uint8_t stopBits, bool invert) {
    InstrumentUartConfigPayload p;
    p.baudrate  = baudrate;
    p.data_bits = dataBits;
    p.parity    = parity;
    p.stop_bits = stopBits;
    p.invert    = invert ? 1 : 0;
    return sendPacket(PKT_TYPE_UART_CONFIG, &p, sizeof(p));
}

bool BusToolsClient::writeUart(const QByteArray& data) {
    if (data.size() > 256) return false;
    InstrumentUartDataPayload p;
    p.len = static_cast<uint16_t>(data.size());
    std::memcpy(p.data, data.constData(), data.size());
    return sendPacket(PKT_TYPE_UART_DATA, &p, sizeof(uint16_t) + p.len);
}

bool BusToolsClient::queryUartStatus() {
    return sendPacket(PKT_TYPE_UART_STATUS, nullptr, 0);
}

// ─── I2C Engine Operations ───────────────────────────────────────────────────
bool BusToolsClient::configureI2c(uint32_t speedHz, bool enablePullups) {
    InstrumentI2cConfigPayload p;
    p.speed_hz       = speedHz;
    p.enable_pullups = enablePullups ? 1 : 0;
    return sendPacket(PKT_TYPE_I2C_CONFIG, &p, sizeof(p));
}

bool BusToolsClient::scanI2c() {
    return sendPacket(PKT_TYPE_I2C_SCAN_REQ, nullptr, 0);
}

bool BusToolsClient::transferI2c(uint8_t addr7Bit, const QByteArray& writeData, uint16_t readLen, bool sendStop) {
    if (writeData.size() > 256 || readLen > 256) return false;
    InstrumentI2cXferPayload p;
    std::memset(&p, 0, sizeof(p));
    p.addr_7bit = addr7Bit;
    p.send_stop = sendStop ? 1 : 0;
    p.write_len = static_cast<uint16_t>(writeData.size());
    p.read_len  = readLen;
    p.status    = 0;
    if (!writeData.isEmpty()) {
        std::memcpy(p.data, writeData.constData(), writeData.size());
    }
    return sendPacket(PKT_TYPE_I2C_XFER, &p, sizeof(p) - 256 + p.write_len);
}

// ─── SPI Engine & Flash Probe Operations ─────────────────────────────────────
bool BusToolsClient::configureSpi(uint32_t speedHz, uint8_t cpol, uint8_t cpha,
                                  bool lsbFirst, uint8_t csPin) {
    InstrumentSpiConfigPayload p;
    p.speed_hz  = speedHz;
    p.cpol      = cpol;
    p.cpha      = cpha;
    p.lsb_first = lsbFirst ? 1 : 0;
    p.cs_pin    = csPin;
    return sendPacket(PKT_TYPE_SPI_CONFIG, &p, sizeof(p));
}

bool BusToolsClient::transferSpi(const QByteArray& txData, bool assertCs) {
    if (txData.size() > 512) return false;
    InstrumentSpiXferPayload p;
    std::memset(&p, 0, sizeof(p));
    p.assert_cs = assertCs ? 1 : 0;
    p.data_len  = static_cast<uint16_t>(txData.size());
    if (!txData.isEmpty()) {
        std::memcpy(p.data, txData.constData(), txData.size());
    }
    return sendPacket(PKT_TYPE_SPI_XFER, &p, sizeof(uint8_t) + sizeof(uint16_t) + p.data_len);
}

bool BusToolsClient::probeSpiFlash(uint8_t csPin) {
    InstrumentSpiFlashProbePayload p;
    p.cs_pin = csPin;
    return sendPacket(PKT_TYPE_SPI_FLASH_PROBE, &p, sizeof(p));
}

// ─── 1-Wire Engine Operations ────────────────────────────────────────────────
bool BusToolsClient::configure1Wire(uint8_t pin, bool enablePullup) {
    Instrument1WireConfigPayload p;
    p.pin           = pin;
    p.enable_pullup = enablePullup ? 1 : 0;
    return sendPacket(PKT_TYPE_1WIRE_CONFIG, &p, sizeof(p));
}

bool BusToolsClient::search1Wire() {
    return sendPacket(PKT_TYPE_1WIRE_SEARCH_REQ, nullptr, 0);
}

bool BusToolsClient::transfer1Wire(bool issueReset, const QByteArray& writeData, uint16_t readLen) {
    if (writeData.size() > 64 || readLen > 64) return false;
    Instrument1WireXferPayload p;
    std::memset(&p, 0, sizeof(p));
    p.issue_reset = issueReset ? 1 : 0;
    p.presence_ok = 0;
    p.write_len   = static_cast<uint16_t>(writeData.size());
    p.read_len    = readLen;
    if (!writeData.isEmpty()) {
        std::memcpy(p.data, writeData.constData(), writeData.size());
    }
    return sendPacket(PKT_TYPE_1WIRE_XFER, &p, sizeof(Instrument1WireXferPayload) - 64 + p.write_len);
}

// ─── Packet Sending Helper ───────────────────────────────────────────────────
bool BusToolsClient::sendPacket(InstrumentPacketType type, const void* payload, uint16_t len) {
    if (!m_transport || !m_transport->isOpen()) return false;

    InstrumentPacketHeader hdr;
    hdr.magic       = INSTRUMENT_PROTO_MAGIC;
    hdr.version     = INSTRUMENT_PROTO_VERSION;
    hdr.type        = static_cast<uint8_t>(type);
    hdr.sequence    = ++m_sequence;
    hdr.payload_len = len;
    hdr.crc16       = (payload && len > 0) ? instrument_crc16(reinterpret_cast<const uint8_t*>(payload), len) : 0;

    QByteArray packet;
    packet.resize(sizeof(hdr) + len);
    std::memcpy(packet.data(), &hdr, sizeof(hdr));
    if (payload && len > 0) {
        std::memcpy(packet.data() + sizeof(hdr), payload, len);
    }

    return m_transport->write(packet) == packet.size();
}

// ─── Incoming Data & Packet Parsing ──────────────────────────────────────────
void BusToolsClient::processIncomingData(const QByteArray& data) {
    m_rxBuffer.append(data);

    while (m_rxBuffer.size() >= static_cast<int>(sizeof(InstrumentPacketHeader))) {
        int magicIndex = -1;
        for (int i = 0; i <= m_rxBuffer.size() - 2; ++i) {
            uint16_t magic = static_cast<uint8_t>(m_rxBuffer[i]) |
                             (static_cast<uint8_t>(m_rxBuffer[i + 1]) << 8);
            if (magic == INSTRUMENT_PROTO_MAGIC) {
                magicIndex = i;
                break;
            }
        }

        if (magicIndex < 0) {
            m_rxBuffer.clear();
            return;
        }

        if (magicIndex > 0) {
            m_rxBuffer.remove(0, magicIndex);
        }

        if (m_rxBuffer.size() < static_cast<int>(sizeof(InstrumentPacketHeader))) {
            return;
        }

        const auto* hdr = reinterpret_cast<const InstrumentPacketHeader*>(m_rxBuffer.constData());
        int totalExpected = sizeof(InstrumentPacketHeader) + hdr->payload_len;

        if (m_rxBuffer.size() < totalExpected) {
            return;
        }

        QByteArray payload;
        if (hdr->payload_len > 0) {
            payload = m_rxBuffer.mid(sizeof(InstrumentPacketHeader), hdr->payload_len);
            uint16_t calculatedCrc = instrument_crc16(
                reinterpret_cast<const uint8_t*>(payload.constData()), hdr->payload_len);
            if (calculatedCrc != hdr->crc16) {
                m_rxBuffer.remove(0, 2);
                continue;
            }
        }

        InstrumentPacketHeader parsedHdr = *hdr;
        m_rxBuffer.remove(0, totalExpected);

        processIncomingPacket(parsedHdr, payload);
    }
}

void BusToolsClient::processIncomingPacket(const InstrumentPacketHeader& hdr, const QByteArray& payload) {
    switch (hdr.type) {
        case PKT_TYPE_CONFIG_ACK: {
            emit commandAcknowledged(hdr.type);
            break;
        }

        case PKT_TYPE_UART_DATA: {
            if (payload.size() >= static_cast<int>(sizeof(uint16_t))) {
                const auto* p = reinterpret_cast<const InstrumentUartDataPayload*>(payload.constData());
                uint16_t len = p->len;
                if (payload.size() >= static_cast<int>(sizeof(uint16_t) + len)) {
                    QByteArray rxBytes(reinterpret_cast<const char*>(p->data), len);
                    emit uartDataReceived(rxBytes);
                }
            }
            break;
        }

        case PKT_TYPE_UART_STATUS: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentUartStatusPayload))) {
                InstrumentUartStatusPayload st;
                std::memcpy(&st, payload.constData(), sizeof(st));
                emit uartStatusReceived(st);
            }
            break;
        }

        case PKT_TYPE_I2C_SCAN_RESP: {
            if (payload.size() >= static_cast<int>(sizeof(uint8_t))) {
                const auto* p = reinterpret_cast<const InstrumentI2cScanRespPayload*>(payload.constData());
                uint8_t count = p->device_count;
                QVector<uint8_t> addrs;
                addrs.reserve(count);
                for (uint8_t i = 0; i < count && i < 32; ++i) {
                    addrs.append(p->addresses[i]);
                }
                emit i2cScanCompleted(addrs);
            }
            break;
        }

        case PKT_TYPE_I2C_XFER: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentI2cXferPayload) - 256)) {
                const auto* p = reinterpret_cast<const InstrumentI2cXferPayload*>(payload.constData());
                uint16_t readLen = p->read_len;
                QByteArray readBytes;
                if (readLen > 0 && payload.size() >= static_cast<int>(sizeof(InstrumentI2cXferPayload) - 256 + readLen)) {
                    readBytes = QByteArray(reinterpret_cast<const char*>(p->data), readLen);
                }
                emit i2cTransferCompleted(p->status, readBytes);
            }
            break;
        }

        case PKT_TYPE_SPI_XFER: {
            if (payload.size() >= static_cast<int>(sizeof(uint8_t) + sizeof(uint16_t))) {
                const auto* p = reinterpret_cast<const InstrumentSpiXferPayload*>(payload.constData());
                uint16_t dataLen = p->data_len;
                QByteArray rxBytes;
                if (dataLen > 0 && payload.size() >= static_cast<int>(sizeof(uint8_t) + sizeof(uint16_t) + dataLen)) {
                    rxBytes = QByteArray(reinterpret_cast<const char*>(p->data), dataLen);
                }
                emit spiTransferCompleted(rxBytes);
            }
            break;
        }

        case PKT_TYPE_SPI_FLASH_RESP: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentFlashInfoPayload))) {
                InstrumentFlashInfoPayload info;
                std::memcpy(&info, payload.constData(), sizeof(info));
                emit spiFlashInfoReceived(info);
            }
            break;
        }

        case PKT_TYPE_1WIRE_SEARCH_RESP: {
            if (payload.size() >= static_cast<int>(sizeof(uint8_t))) {
                const auto* p = reinterpret_cast<const Instrument1WireSearchRespPayload*>(payload.constData());
                uint8_t count = p->device_count;
                QVector<QByteArray> romList;
                romList.reserve(count);
                for (uint8_t i = 0; i < count && i < 8; ++i) {
                    romList.append(QByteArray(reinterpret_cast<const char*>(p->rom_ids[i]), 8));
                }
                emit oneWireSearchCompleted(romList);
            }
            break;
        }

        case PKT_TYPE_1WIRE_XFER: {
            if (payload.size() >= static_cast<int>(sizeof(Instrument1WireXferPayload) - 64)) {
                const auto* p = reinterpret_cast<const Instrument1WireXferPayload*>(payload.constData());
                uint16_t readLen = p->read_len;
                QByteArray readBytes;
                if (readLen > 0 && payload.size() >= static_cast<int>(sizeof(Instrument1WireXferPayload) - 64 + readLen)) {
                    readBytes = QByteArray(reinterpret_cast<const char*>(p->data), readLen);
                }
                emit oneWireTransferCompleted(p->presence_ok != 0, readBytes);
            }
            break;
        }

        case PKT_TYPE_ERROR: {
            uint8_t errCode = payload.isEmpty() ? 0xFF : static_cast<uint8_t>(payload[0]);
            QString reason = (payload.size() > 1) ? QString::fromUtf8(payload.mid(1)) : QStringLiteral("Unknown Error");
            emit errorReceived(errCode, reason);
            break;
        }

        default:
            break;
    }
}

} // namespace UdooKey
