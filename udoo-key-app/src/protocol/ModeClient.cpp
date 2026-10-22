#include "ModeClient.h"
#include <cstring>

namespace UdooKey {

ModeClient::ModeClient(ITransport* transport, QObject* parent)
    : QObject(parent) {
    m_pinFunctions.fill(PIN_FN_DEFAULT);
    if (transport) {
        setTransport(transport);
    }
}

void ModeClient::setTransport(ITransport* transport) {
    if (m_transport == transport) return;

    if (m_transport) {
        disconnect(m_transport, &ITransport::dataReady, this, &ModeClient::processIncomingData);
    }

    m_transport = transport;

    if (m_transport) {
        connect(m_transport, &ITransport::dataReady, this, &ModeClient::processIncomingData);
    }
}

bool ModeClient::isConnected() const {
    return m_transport && m_transport->isOpen();
}

bool ModeClient::setMode(InstrumentModeId mode, uint32_t speedHz) {
    InstrumentModeSetPayload payload;
    payload.mode     = static_cast<uint8_t>(mode);
    payload.speed_hz = speedHz;

    return sendPacket(PKT_TYPE_MODE_SET, &payload, sizeof(payload));
}

bool ModeClient::requestStatus() {
    return sendPacket(PKT_TYPE_MODE_STATUS, nullptr, 0);
}

bool ModeClient::releaseToHiZ() {
    return setMode(INST_MODE_IDLE, 0);
}

bool ModeClient::sendPacket(InstrumentPacketType type, const void* payload, uint16_t len) {
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

void ModeClient::processIncomingPacket(const InstrumentPacketHeader& hdr, const QByteArray& payload) {
    switch (static_cast<InstrumentPacketType>(hdr.type)) {
        case PKT_TYPE_MODE_STATUS: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentModeStatusPayload))) {
                InstrumentModeStatusPayload st;
                std::memcpy(&st, payload.constData(), sizeof(st));

                InstrumentModeId prevMode = m_currentMode;
                m_currentMode      = static_cast<InstrumentModeId>(st.active_mode);
                m_currentModeName  = QString::fromUtf8(st.mode_name);
                m_activeSpeedHz    = st.active_speed_hz;
                m_probeSpiActive   = (st.probe_spi_active != 0);
                m_safeHiZVerified  = (st.safe_hiz_verified != 0);

                for (size_t i = 0; i < 8; ++i) {
                    m_pinFunctions[i] = static_cast<PinFunction>(st.pin_functions[i]);
                }

                emit pinFunctionsUpdated(m_pinFunctions);
                emit modeStatusReceived(st);
                if (m_currentMode != prevMode) {
                    emit modeChanged(m_currentMode, prevMode, m_currentModeName);
                }
            }
            break;
        }

        case PKT_TYPE_ERROR: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentErrorPayload))) {
                InstrumentErrorPayload err;
                std::memcpy(&err, payload.constData(), sizeof(err));
                emit modeErrorReceived(err.error_code, QString::fromUtf8(err.message));
            }
            break;
        }

        default:
            break;
    }
}

void ModeClient::processIncomingData(const QByteArray& data) {
    m_rxBuffer.append(data);

    while (m_rxBuffer.size() >= static_cast<int>(sizeof(InstrumentPacketHeader))) {
        // Find framing magic 0xAA55
        int magicIdx = -1;
        for (int i = 0; i <= m_rxBuffer.size() - 2; ++i) {
            uint16_t magic = static_cast<uint8_t>(m_rxBuffer[i]) | (static_cast<uint8_t>(m_rxBuffer[i + 1]) << 8);
            if (magic == INSTRUMENT_PROTO_MAGIC) {
                magicIdx = i;
                break;
            }
        }

        if (magicIdx < 0) {
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

PinFunction ModeClient::pinFunction(uint8_t pin) const {
    if (pin >= 2 && pin <= 9) {
        return m_pinFunctions[pin - 2];
    }
    return PIN_FN_DEFAULT;
}

QString ModeClient::pinFunctionDescription(uint8_t pin) const {
    if (pin < 2 || pin > 9) return "SYSTEM";
    PinFunction fn = pinFunction(pin);
    switch (fn) {
        case PIN_FN_DEFAULT:     return "Safe Hi-Z Input";
        case PIN_FN_LOGIC_PROBE: return QString("LA Channel %1").arg(pin - 2);
        case PIN_FN_GPIO_IN:     return QString("DIO In (P1.%1)").arg(pin);
        case PIN_FN_GPIO_OUT:    return QString("DIO Out (P1.%1)").arg(pin);
        case PIN_FN_I2C_SDA:     return "I2C SDA";
        case PIN_FN_I2C_SCL:     return "I2C SCL";
        case PIN_FN_SPI_SCK:     return "SPI SCK";
        case PIN_FN_SPI_MOSI:    return "SPI MOSI";
        case PIN_FN_SPI_MISO:    return "SPI MISO";
        case PIN_FN_SPI_CS:      return "SPI CS#";
        case PIN_FN_UART_TX:     return "UART TX";
        case PIN_FN_UART_RX:     return "UART RX";
        case PIN_FN_ONEWIRE_DQ:  return "1-Wire DQ";
        case PIN_FN_PWM_OUT:     return "PWM Output";
        case PIN_FN_SWD_SWCLK:   return "SWD SWCLK";
        case PIN_FN_SWD_SWDIO:   return "SWD SWDIO";
        case PIN_FN_JTAG_TCK:    return "JTAG TCK";
        case PIN_FN_JTAG_TMS:    return "JTAG TMS";
        case PIN_FN_JTAG_TDI:    return "JTAG TDI";
        case PIN_FN_JTAG_TDO:    return "JTAG TDO";
        default:                 return "Unallocated";
    }
}

QString ModeClient::modeNameString(InstrumentModeId mode) {
    switch (mode) {
        case INST_MODE_IDLE:           return "IDLE (Safe Hi-Z)";
        case INST_MODE_LOGIC_ANALYZER: return "LOGIC_ANALYZER";
        case INST_MODE_SPI:            return "SPI";
        case INST_MODE_I2C:            return "I2C";
        case INST_MODE_UART:           return "UART";
        case INST_MODE_ONE_WIRE:       return "1-WIRE";
        case INST_MODE_SWD:            return "SWD";
        case INST_MODE_PWM_GENERATOR:  return "PWM_GENERATOR";
        case INST_MODE_GLITCH:         return "GLITCH";
        case INST_MODE_EEPROM:         return "EEPROM";
        case INST_MODE_FLASH:          return "FLASH";
        case INST_MODE_INTERCHIP_SPI:  return "INTERCHIP_SPI";
        case INST_MODE_DIO:            return "DIO";
        case INST_MODE_USB_SNIFFER:    return "USB_SNIFFER";
        case INST_MODE_SENSOR_TEST:    return "SENSOR_TEST";
        default:                       return "UNKNOWN";
    }
}

} // namespace UdooKey
