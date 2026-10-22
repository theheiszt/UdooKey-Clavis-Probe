#include "GpioClient.h"
#include <cstring>

namespace UdooKey {

GpioClient::GpioClient(ITransport* transport, QObject* parent)
    : QObject(parent) {
    if (transport) {
        setTransport(transport);
    }
}

void GpioClient::setTransport(ITransport* transport) {
    if (m_transport == transport) return;

    if (m_transport) {
        disconnect(m_transport, &ITransport::dataReady, this, &GpioClient::processIncomingData);
    }

    m_transport = transport;

    if (m_transport) {
        connect(m_transport, &ITransport::dataReady, this, &GpioClient::processIncomingData);
    }
}

bool GpioClient::isConnected() const {
    return m_transport && m_transport->isOpen();
}

bool GpioClient::configurePin(uint8_t pinOrIdx, bool isOutput, bool initialLevel, GpioPullMode pull) {
    InstrumentGpioConfigPayload payload;
    payload.pin_or_idx    = pinOrIdx;
    payload.is_output     = isOutput ? 1 : 0;
    payload.initial_level = initialLevel ? 1 : 0;
    payload.pull_mode     = static_cast<uint8_t>(pull);

    return sendPacket(PKT_TYPE_GPIO_CONFIG, &payload, sizeof(payload));
}

bool GpioClient::writePin(uint8_t pinOrIdx, bool level) {
    InstrumentGpioWritePayload payload;
    payload.pin_or_idx = pinOrIdx;
    payload.level      = level ? 1 : 0;

    return sendPacket(PKT_TYPE_GPIO_WRITE, &payload, sizeof(payload));
}

bool GpioClient::readPin(uint8_t pinOrIdx) {
    InstrumentGpioReadPayload payload;
    payload.pin_or_idx = pinOrIdx;

    return sendPacket(PKT_TYPE_GPIO_READ, &payload, sizeof(payload));
}

bool GpioClient::writeMask(uint8_t dirMask, uint8_t valueMask) {
    InstrumentGpioWriteMaskPayload payload;
    payload.dir_mask   = dirMask;
    payload.value_mask = valueMask;

    return sendPacket(PKT_TYPE_GPIO_WRITE_MASK, &payload, sizeof(payload));
}

bool GpioClient::readAll() {
    return sendPacket(PKT_TYPE_GPIO_READ_ALL, nullptr, 0);
}

bool GpioClient::sendPacket(InstrumentPacketType type, const void* payload, uint16_t len) {
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

void GpioClient::processIncomingPacket(const InstrumentPacketHeader& hdr, const QByteArray& payload) {
    switch (static_cast<InstrumentPacketType>(hdr.type)) {
        case PKT_TYPE_GPIO_STATUS: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentGpioStatusPayload))) {
                InstrumentGpioStatusPayload st;
                std::memcpy(&st, payload.constData(), sizeof(st));
                uint8_t idx = st.pin_or_idx;
                if (idx >= 2 && idx <= 9) idx -= 2;
                if (idx < 8) {
                    m_pinStatusHistory[idx] = st;
                    if (st.is_output) {
                        m_lastPinDirections |= (1 << idx);
                    } else {
                        m_lastPinDirections &= ~(1 << idx);
                    }
                }
                m_lastPinValues = st.all_pins_state;
                emit pinStatusReceived(st);
            }
            break;
        }

        case PKT_TYPE_GPIO_READ_ALL: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentGpioReadAllPayload))) {
                InstrumentGpioReadAllPayload all;
                std::memcpy(&all, payload.constData(), sizeof(all));
                m_lastPinValues     = all.values_mask;
                m_lastPinDirections = all.directions_mask;
                emit allPinsReceived(all);
            }
            break;
        }

        case PKT_TYPE_ERROR: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentErrorPayload))) {
                InstrumentErrorPayload err;
                std::memcpy(&err, payload.constData(), sizeof(err));
                emit errorReceived(err.error_code, QString::fromUtf8(err.message));
            }
            break;
        }

        default:
            break;
    }
}

void GpioClient::processIncomingData(const QByteArray& data) {
    m_rxBuffer.append(data);

    while (m_rxBuffer.size() >= static_cast<int>(sizeof(InstrumentPacketHeader))) {
        // Find framing magic 0xAA55 (little-endian byte 0: 0x55, byte 1: 0xAA)
        int magicIdx = -1;
        for (int i = 0; i <= m_rxBuffer.size() - 2; ++i) {
            uint16_t magic = static_cast<uint8_t>(m_rxBuffer[i]) | (static_cast<uint8_t>(m_rxBuffer[i + 1]) << 8);
            if (magic == INSTRUMENT_PROTO_MAGIC) {
                magicIdx = i;
                break;
            }
        }

        if (magicIdx < 0) {
            // No magic found, preserve last byte in case it's the start of 0xAA55
            if (m_rxBuffer.size() > 1) {
                m_rxBuffer = m_rxBuffer.right(1);
            }
            break;
        }

        if (magicIdx > 0) {
            m_rxBuffer.remove(0, magicIdx);
        }

        if (m_rxBuffer.size() < static_cast<int>(sizeof(InstrumentPacketHeader))) {
            break;
        }

        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, m_rxBuffer.constData(), sizeof(hdr));
        int totalLen = sizeof(hdr) + hdr.payload_len;

        if (m_rxBuffer.size() < totalLen) {
            break; // Await full frame
        }

        QByteArray payload = m_rxBuffer.mid(sizeof(hdr), hdr.payload_len);
        m_rxBuffer.remove(0, totalLen);

        if (hdr.payload_len > 0) {
            uint16_t calcCrc = instrument_crc16(reinterpret_cast<const uint8_t*>(payload.constData()), payload.size());
            if (calcCrc != hdr.crc16) {
                continue; // CRC mismatch, drop corrupt frame
            }
        }

        processIncomingPacket(hdr, payload);
    }
}

InstrumentGpioStatusPayload GpioClient::lastPinStatus(uint8_t pinOrIdx) const {
    uint8_t idx = pinOrIdx;
    if (idx >= 2 && idx <= 9) idx -= 2;
    if (idx < 8) {
        return m_pinStatusHistory[idx];
    }
    return InstrumentGpioStatusPayload{};
}

} // namespace UdooKey
