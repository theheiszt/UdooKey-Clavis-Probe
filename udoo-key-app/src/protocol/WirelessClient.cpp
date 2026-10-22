#include "WirelessClient.h"
#include <cstring>
#include <algorithm>
#include <QDateTime>

namespace UdooKey {

WirelessClient::WirelessClient(ITransport* transport, QObject* parent)
    : QObject(parent) {
    if (transport) {
        setTransport(transport);
    }
}

void WirelessClient::setTransport(ITransport* transport) {
    if (m_transport == transport) return;

    if (m_transport) {
        disconnect(m_transport, &ITransport::dataReady, this, &WirelessClient::processIncomingData);
    }

    m_transport = transport;

    if (m_transport) {
        connect(m_transport, &ITransport::dataReady, this, &WirelessClient::processIncomingData);
    }
}

bool WirelessClient::isConnected() const {
    return m_transport && m_transport->isOpen();
}

// ─── Infrared (IR) Operations ────────────────────────────────────────────────
bool WirelessClient::configureIr(uint8_t txPin, uint8_t rxPin, uint16_t carrierFreqHz, uint8_t dutyPercent) {
    InstrumentIrConfigPayload p;
    p.tx_pin           = txPin;
    p.rx_pin           = rxPin;
    p.carrier_freq_hz  = carrierFreqHz;
    p.duty_percent     = dutyPercent;
    return sendPacket(PKT_TYPE_IR_CONFIG, &p, sizeof(p));
}

bool WirelessClient::transmitIrNec(uint16_t address, uint32_t command, uint8_t repeatCount) {
    InstrumentIrTxPayload p;
    std::memset(&p, 0, sizeof(p));
    p.protocol     = IR_PROTO_NEC;
    p.address      = address;
    p.command      = command;
    p.repeat_count = repeatCount;
    p.raw_len      = 0;
    return sendPacket(PKT_TYPE_IR_TX, &p, sizeof(p));
}

bool WirelessClient::transmitIrRaw(const QVector<uint16_t>& timingsUs, uint8_t repeatCount) {
    if (timingsUs.isEmpty() || timingsUs.size() > 128) return false;
    InstrumentIrTxPayload p;
    std::memset(&p, 0, sizeof(p));
    p.protocol     = IR_PROTO_RAW;
    p.repeat_count = repeatCount;
    p.raw_len      = static_cast<uint16_t>(timingsUs.size());
    for (int i = 0; i < timingsUs.size(); ++i) {
        p.raw_timings_us[i] = timingsUs[i];
    }
    return sendPacket(PKT_TYPE_IR_TX, &p, sizeof(p));
}

bool WirelessClient::configureIrRx(bool enabled, bool filterUnknown) {
    InstrumentIrRxConfigPayload p;
    p.enabled        = enabled ? 1 : 0;
    p.filter_unknown = filterUnknown ? 1 : 0;
    return sendPacket(PKT_TYPE_IR_RX_CONFIG, &p, sizeof(p));
}

// ─── nRF24L01 Radio Operations ───────────────────────────────────────────────
bool WirelessClient::configureNrf24(uint8_t channel,
                                    InstrumentNrf24Datarate datarate,
                                    InstrumentNrf24Power powerLevel,
                                    InstrumentNrf24Crc crcMode,
                                    uint8_t addrWidth,
                                    bool autoAck) {
    InstrumentNrf24ConfigPayload p;
    p.channel     = channel;
    p.datarate    = static_cast<uint8_t>(datarate);
    p.power_level = static_cast<uint8_t>(powerLevel);
    p.crc_mode    = static_cast<uint8_t>(crcMode);
    p.addr_width  = addrWidth;
    p.auto_ack    = autoAck ? 1 : 0;
    return sendPacket(PKT_TYPE_NRF24_CONFIG, &p, sizeof(p));
}

bool WirelessClient::transmitNrf24(const QByteArray& targetAddr, const QByteArray& data) {
    if (data.isEmpty() || data.size() > 32) return false;
    InstrumentNrf24TxPayload p;
    std::memset(&p, 0, sizeof(p));
    int addrLen = std::min(5, static_cast<int>(targetAddr.size()));
    if (addrLen > 0) {
        std::memcpy(p.target_addr, targetAddr.constData(), addrLen);
    }
    p.length = static_cast<uint8_t>(data.size());
    std::memcpy(p.data, data.constData(), data.size());
    return sendPacket(PKT_TYPE_NRF24_TX, &p, sizeof(p));
}

bool WirelessClient::configureNrf24RxPipe(uint8_t pipeIndex, bool enabled, const QByteArray& pipeAddr, uint8_t payloadSize) {
    if (pipeIndex > 5) return false;
    InstrumentNrf24RxConfigPayload p;
    std::memset(&p, 0, sizeof(p));
    p.pipe_index    = pipeIndex;
    p.enabled       = enabled ? 1 : 0;
    int addrLen = std::min(5, static_cast<int>(pipeAddr.size()));
    if (addrLen > 0) {
        std::memcpy(p.pipe_addr, pipeAddr.constData(), addrLen);
    }
    p.payload_size  = payloadSize;
    return sendPacket(PKT_TYPE_NRF24_RX_CONFIG, &p, sizeof(p));
}

bool WirelessClient::queryNrf24Status() {
    return sendPacket(PKT_TYPE_NRF24_STATUS, nullptr, 0);
}

// ─── Wi-Fi, BLE & Time Sync Operations ───────────────────────────────────────
bool WirelessClient::startBleScan(uint8_t duration_s, bool filtered) {
    InstrumentRadioCmdPayload cmd;
    std::memset(&cmd, 0, sizeof(cmd));
    cmd.cmd_type = 1; // Start BLE Scan
    cmd.scan_duration_s = duration_s;
    cmd.filter_mode = filtered ? 1 : 0;
    return sendPacket(PKT_TYPE_RADIO_CMD, &cmd, sizeof(cmd));
}

bool WirelessClient::stopBleScan() {
    InstrumentRadioCmdPayload cmd;
    std::memset(&cmd, 0, sizeof(cmd));
    cmd.cmd_type = 2; // Stop BLE Scan
    return sendPacket(PKT_TYPE_RADIO_CMD, &cmd, sizeof(cmd));
}

bool WirelessClient::requestWifiRecon() {
    InstrumentRadioCmdPayload cmd;
    std::memset(&cmd, 0, sizeof(cmd));
    cmd.cmd_type = 3; // Wi-Fi Recon
    return sendPacket(PKT_TYPE_RADIO_CMD, &cmd, sizeof(cmd));
}

bool WirelessClient::requestWifiTelemetry() {
    InstrumentRadioCmdPayload cmd;
    std::memset(&cmd, 0, sizeof(cmd));
    cmd.cmd_type = 5; // Stream Wi-Fi Telemetry
    return sendPacket(PKT_TYPE_RADIO_CMD, &cmd, sizeof(cmd));
}

bool WirelessClient::sendNetworkProbe(const QString& targetIp) {
    InstrumentRadioCmdPayload cmd;
    std::memset(&cmd, 0, sizeof(cmd));
    cmd.cmd_type = 4; // Ping / Network Probe
    QStringList parts = targetIp.split('.');
    for (int i = 0; i < std::min(4, static_cast<int>(parts.size())); ++i) {
        cmd.target_ip[i] = static_cast<uint8_t>(parts[i].toUInt());
    }
    return sendPacket(PKT_TYPE_RADIO_CMD, &cmd, sizeof(cmd));
}

bool WirelessClient::performTimeSync() {
    InstrumentTimeSyncReqPayload req;
    req.host_request_id = ++m_sequence;
    req.host_timestamp_us = static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch()) * 1000ULL;
    return sendPacket(PKT_TYPE_TIME_SYNC_REQ, &req, sizeof(req));
}

// ─── Wi-Fi Promiscuous Sniffer & PCAP Operations ────────────────────────────
bool WirelessClient::configureWifiSniffer(bool enabled, uint8_t channel, uint8_t filterType) {
    InstrumentWifiSniffConfigPayload p;
    p.enabled     = enabled ? 1 : 0;
    p.channel     = channel;
    p.filter_type = filterType;
    p.reserved    = 0;
    return sendPacket(PKT_TYPE_WIFI_SNIFF_CONFIG, &p, sizeof(p));
}

bool WirelessClient::queryWifiSnifferStatus() {
    return sendPacket(PKT_TYPE_WIFI_SNIFF_STATUS, nullptr, 0);
}

bool WirelessClient::requestPcapDump(bool streamBinary, bool clearAfter, uint16_t maxPackets) {
    InstrumentPcapDumpReqPayload p;
    p.stream_binary = streamBinary ? 1 : 0;
    p.clear_after   = clearAfter ? 1 : 0;
    p.max_packets   = maxPackets;
    return sendPacket(PKT_TYPE_PCAP_DUMP_REQ, &p, sizeof(p));
}

// ─── Packet Sending Helper ───────────────────────────────────────────────────
bool WirelessClient::sendPacket(InstrumentPacketType type, const void* payload, uint16_t len) {
    if (!m_transport || !m_transport->isOpen()) return false;

    InstrumentPacketHeader hdr;
    hdr.magic       = INSTRUMENT_PROTO_MAGIC;
    hdr.version     = INSTRUMENT_PROTO_VERSION;
    hdr.type        = static_cast<uint8_t>(type);
    hdr.sequence    = ++m_sequence;
    hdr.payload_len = len;
    hdr.crc16       = (len > 0 && payload) ? instrument_crc16(reinterpret_cast<const uint8_t*>(payload), len) : 0;

    QByteArray buf;
    buf.reserve(sizeof(hdr) + len);
    buf.append(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    if (len > 0 && payload) {
        buf.append(reinterpret_cast<const char*>(payload), len);
    }

    qint64 written = m_transport->write(buf);
    return written == buf.size();
}

// ─── Incoming Data & Stream Processing ───────────────────────────────────────
void WirelessClient::processIncomingData(const QByteArray& data) {
    m_rxBuffer.append(data);

    while (m_rxBuffer.size() >= static_cast<int>(sizeof(InstrumentPacketHeader))) {
        int magicIdx = -1;
        for (int i = 0; i <= m_rxBuffer.size() - static_cast<int>(sizeof(uint16_t)); ++i) {
            uint16_t val;
            std::memcpy(&val, m_rxBuffer.constData() + i, sizeof(uint16_t));
            if (val == INSTRUMENT_PROTO_MAGIC) {
                magicIdx = i;
                break;
            }
        }

        if (magicIdx < 0) {
            m_rxBuffer.clear();
            return;
        }

        if (magicIdx > 0) {
            m_rxBuffer.remove(0, magicIdx);
        }

        if (m_rxBuffer.size() < static_cast<int>(sizeof(InstrumentPacketHeader))) {
            return;
        }

        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, m_rxBuffer.constData(), sizeof(hdr));

        int totalLen = sizeof(InstrumentPacketHeader) + hdr.payload_len;
        if (m_rxBuffer.size() < totalLen) {
            return; // Wait for full frame
        }

        QByteArray payload = m_rxBuffer.mid(sizeof(InstrumentPacketHeader), hdr.payload_len);
        m_rxBuffer.remove(0, totalLen);

        if (hdr.payload_len > 0) {
            uint16_t calcCrc = instrument_crc16(reinterpret_cast<const uint8_t*>(payload.constData()), hdr.payload_len);
            if (calcCrc != hdr.crc16) {
                continue; // Drop corrupted packet
            }
        }

        processIncomingPacket(hdr, payload);
    }
}

void WirelessClient::processIncomingPacket(const InstrumentPacketHeader& hdr, const QByteArray& payload) {
    switch (hdr.type) {
        case PKT_TYPE_CONFIG_ACK:
            emit configAcknowledged();
            break;

        case PKT_TYPE_IR_RX_EVENT: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentIrRxEventPayload))) {
                InstrumentIrRxEventPayload ev;
                std::memcpy(&ev, payload.constData(), sizeof(ev));
                emit irRxEventReceived(ev);
            }
            break;
        }

        case PKT_TYPE_NRF24_TX_STATUS: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentNrf24TxStatusPayload))) {
                InstrumentNrf24TxStatusPayload st;
                std::memcpy(&st, payload.constData(), sizeof(st));
                emit nrf24TxStatusReceived(st);
            }
            break;
        }

        case PKT_TYPE_NRF24_RX_EVENT: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentNrf24RxEventPayload))) {
                InstrumentNrf24RxEventPayload ev;
                std::memcpy(&ev, payload.constData(), sizeof(ev));
                emit nrf24RxEventReceived(ev);
            }
            break;
        }

        case PKT_TYPE_NRF24_STATUS: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentNrf24StatusPayload))) {
                InstrumentNrf24StatusPayload st;
                std::memcpy(&st, payload.constData(), sizeof(st));
                emit nrf24StatusReceived(st);
            }
            break;
        }

        case PKT_TYPE_RADIO_WIFI_DIAG: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentRadioWifiDiagPayload))) {
                const auto* diag = reinterpret_cast<const InstrumentRadioWifiDiagPayload*>(payload.constData());
                QString ssid = QString::fromUtf8(diag->ssid);
                emit wifiTelemetryReceived(ssid, diag->rssi, diag->dns_latency_us, diag->gateway_rtt_us);
            }
            break;
        }

        case PKT_TYPE_RADIO_EVENT: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentRadioEventPayload))) {
                InstrumentRadioEventPayload ev;
                std::memcpy(&ev, payload.constData(), sizeof(ev));
                emit radioEventReceived(ev);
            }
            break;
        }

        case PKT_TYPE_TIME_SYNC_RESP: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentTimeSyncRespPayload))) {
                const auto* resp = reinterpret_cast<const InstrumentTimeSyncRespPayload*>(payload.constData());
                uint64_t now_us = static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch()) * 1000ULL;
                int64_t rtt = static_cast<int64_t>(now_us - resp->host_timestamp_us);
                int64_t offset = static_cast<int64_t>(resp->device_timestamp_us) - static_cast<int64_t>(resp->host_timestamp_us + rtt / 2);
                emit timeSyncCompleted(offset, static_cast<uint32_t>(rtt));
            }
            break;
        }

        case PKT_TYPE_WIFI_SNIFF_STATUS: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentWifiSniffStatusPayload))) {
                InstrumentWifiSniffStatusPayload st;
                std::memcpy(&st, payload.constData(), sizeof(st));
                emit wifiSnifferStatusReceived(st);
            }
            break;
        }

        case PKT_TYPE_PACKET_RECORD: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentPacketRecordPayload))) {
                InstrumentPacketRecordPayload rec;
                std::memcpy(&rec, payload.constData(), sizeof(rec));
                emit packetRecordReceived(rec);
            }
            break;
        }

        case PKT_TYPE_ERROR: {
            if (payload.size() >= static_cast<int>(sizeof(InstrumentErrorPayload))) {
                const auto* err = reinterpret_cast<const InstrumentErrorPayload*>(payload.constData());
                emit packetErrorReceived(err->error_code, QString::fromUtf8(err->message));
            }
            break;
        }

        default:
            break;
    }
}

} // namespace UdooKey
