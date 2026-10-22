#include <iostream>
#include <cassert>
#include <cstring>
#include <QByteArray>
#include <QList>
#include "WirelessClient.h"
#include "ITransport.h"
#include "instrument_protocol.h"

using namespace UdooKey;

class MockWirelessTransport : public ITransport {
public:
    explicit MockWirelessTransport(QObject* parent = nullptr) : ITransport(parent) {}

    bool open() override { m_open = true; emit connected(); return true; }
    void close() override { m_open = false; emit disconnected(); }
    bool isOpen() const override { return m_open; }

    qint64 write(const QByteArray& data) override {
        m_writtenPackets.append(data);
        return data.size();
    }
    qint64 write(const char* data, qint64 maxSize) override {
        m_writtenPackets.append(QByteArray(data, static_cast<int>(maxSize)));
        return maxSize;
    }
    QByteArray readAll() override { return {}; }
    qint64 bytesAvailable() const override { return 0; }
    QString portName() const override { return "MockWirelessTransport"; }
    void setPortName(const QString&) override {}

    void injectData(const QByteArray& data) {
        emit dataReady(data);
    }

    QList<QByteArray> m_writtenPackets;
    bool m_open = true;
};

static void injectPacket(MockWirelessTransport& transport, uint8_t type, uint16_t seq,
                         const void* payload, uint16_t plen) {
    InstrumentPacketHeader hdr;
    hdr.magic       = INSTRUMENT_PROTO_MAGIC;
    hdr.version     = INSTRUMENT_PROTO_VERSION;
    hdr.type        = type;
    hdr.sequence    = seq;
    hdr.payload_len = plen;
    hdr.crc16       = plen ? instrument_crc16(reinterpret_cast<const uint8_t*>(payload), plen) : 0;

    QByteArray raw;
    raw.resize(sizeof(hdr) + plen);
    std::memcpy(raw.data(), &hdr, sizeof(hdr));
    if (plen && payload) {
        std::memcpy(raw.data() + sizeof(hdr), payload, plen);
    }

    transport.injectData(raw);
}

bool runWirelessClientTests() {
    std::cout << "--- Running WirelessClient (Phase 5 IR Mode & nRF24L01 Module) Tests ---" << std::endl;

    MockWirelessTransport transport;
    transport.open();
    WirelessClient client(&transport);
    assert(client.isConnected());

    // ─────────────────────────────────────────────────────────────────────────
    // Test 1: IR Subsystem Configuration Framing (PKT_TYPE_IR_CONFIG 0x30)
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        bool ok = client.configureIr(25, 26, 38000, 33);
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());

        const QByteArray& pkt = transport.m_writtenPackets.last();
        assert(pkt.size() == sizeof(InstrumentPacketHeader) + sizeof(InstrumentIrConfigPayload));

        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.magic == INSTRUMENT_PROTO_MAGIC);
        assert(hdr.type == PKT_TYPE_IR_CONFIG);
        assert(hdr.payload_len == sizeof(InstrumentIrConfigPayload));

        InstrumentIrConfigPayload payload;
        std::memcpy(&payload, pkt.constData() + sizeof(hdr), sizeof(payload));
        assert(payload.tx_pin == 25);
        assert(payload.rx_pin == 26);
        assert(payload.carrier_freq_hz == 38000);
        assert(payload.duty_percent == 33);

        uint16_t crc = instrument_crc16(reinterpret_cast<const uint8_t*>(&payload), sizeof(payload));
        assert(hdr.crc16 == crc);

        bool ackReceived = false;
        QMetaObject::Connection conn = QObject::connect(&client, &WirelessClient::configAcknowledged, [&]() {
            ackReceived = true;
        });
        injectPacket(transport, PKT_TYPE_CONFIG_ACK, hdr.sequence, nullptr, 0);
        assert(ackReceived);
        QObject::disconnect(conn);
        std::cout << "  [PASS] Test 1: IR Subsystem Configuration Framing & ACK Verified" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 2: IR NEC Protocol Transmission Framing (PKT_TYPE_IR_TX 0x31)
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        bool ok = client.transmitIrNec(0x00FF, 0x12, 1);
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());

        const QByteArray& pkt = transport.m_writtenPackets.last();
        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.magic == INSTRUMENT_PROTO_MAGIC);
        assert(hdr.type == PKT_TYPE_IR_TX);

        InstrumentIrTxPayload payload;
        std::memcpy(&payload, pkt.constData() + sizeof(hdr), sizeof(payload));
        assert(payload.protocol == IR_PROTO_NEC);
        assert(payload.address == 0x00FF);
        assert(payload.command == 0x12);
        assert(payload.repeat_count == 1);
        assert(payload.raw_len == 0);
        std::cout << "  [PASS] Test 2: IR NEC Protocol Transmission Framing Verified" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 3: IR Raw Pulses Transmission Framing (PKT_TYPE_IR_TX 0x31)
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        QVector<uint16_t> timings = {9000, 4500, 560, 560};
        bool ok = client.transmitIrRaw(timings, 0);
        assert(ok);
        assert(!transport.m_writtenPackets.isEmpty());

        const QByteArray& pkt = transport.m_writtenPackets.last();
        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.type == PKT_TYPE_IR_TX);

        InstrumentIrTxPayload payload;
        std::memcpy(&payload, pkt.constData() + sizeof(hdr), sizeof(payload));
        assert(payload.protocol == IR_PROTO_RAW);
        assert(payload.raw_len == 4);
        assert(payload.raw_timings_us[0] == 9000);
        assert(payload.raw_timings_us[1] == 4500);
        assert(payload.raw_timings_us[2] == 560);
        assert(payload.raw_timings_us[3] == 560);
        std::cout << "  [PASS] Test 3: IR Raw Timing Pulses Transmission Framing Verified" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 4: IR Receiver Configuration & Demodulated Packet Ingestion (0x33 & 0x32)
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        bool ok = client.configureIrRx(true, false);
        assert(ok);

        const QByteArray& pkt = transport.m_writtenPackets.last();
        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.type == PKT_TYPE_IR_RX_CONFIG);

        InstrumentIrRxConfigPayload cfg;
        std::memcpy(&cfg, pkt.constData() + sizeof(hdr), sizeof(cfg));
        assert(cfg.enabled == 1);
        assert(cfg.filter_unknown == 0);

        InstrumentIrRxEventPayload rxEv;
        std::memset(&rxEv, 0, sizeof(rxEv));
        rxEv.protocol     = IR_PROTO_NEC;
        rxEv.address      = 0x00FF;
        rxEv.command      = 0x12;
        rxEv.repeat       = 0;
        rxEv.timestamp_us = 12345678ULL;

        bool evReceived = false;
        InstrumentIrRxEventPayload receivedPayload;
        QMetaObject::Connection conn = QObject::connect(&client, &WirelessClient::irRxEventReceived,
            [&](const InstrumentIrRxEventPayload& ev) {
                evReceived = true;
                receivedPayload = ev;
            });

        injectPacket(transport, PKT_TYPE_IR_RX_EVENT, 1, &rxEv, sizeof(rxEv));
        assert(evReceived);
        assert(receivedPayload.protocol == IR_PROTO_NEC);
        assert(receivedPayload.address == 0x00FF);
        assert(receivedPayload.command == 0x12);
        assert(receivedPayload.timestamp_us == 12345678ULL);
        QObject::disconnect(conn);
        std::cout << "  [PASS] Test 4: IR Demodulated RX Event Ingestion Verified" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 5: nRF24L01 Configuration Framing (PKT_TYPE_NRF24_CONFIG 0x40)
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        bool ok = client.configureNrf24(76, NRF24_RATE_2MBPS, NRF24_PA_MAX, NRF24_CRC_16BIT, 5, true);
        assert(ok);

        const QByteArray& pkt = transport.m_writtenPackets.last();
        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.magic == INSTRUMENT_PROTO_MAGIC);
        assert(hdr.type == PKT_TYPE_NRF24_CONFIG);

        InstrumentNrf24ConfigPayload payload;
        std::memcpy(&payload, pkt.constData() + sizeof(hdr), sizeof(payload));
        assert(payload.channel == 76);
        assert(payload.datarate == NRF24_RATE_2MBPS);
        assert(payload.power_level == NRF24_PA_MAX);
        assert(payload.crc_mode == NRF24_CRC_16BIT);
        assert(payload.addr_width == 5);
        assert(payload.auto_ack == 1);
        std::cout << "  [PASS] Test 5: nRF24L01 Subsystem Configuration Framing Verified" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 6: nRF24L01 Transmission & TX Status Ingestion (0x41 & 0x42)
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        QByteArray targetAddr = QByteArray::fromHex("E7E7E7E701");
        QByteArray txData = "HELLO_NRF24";
        bool ok = client.transmitNrf24(targetAddr, txData);
        assert(ok);

        const QByteArray& pkt = transport.m_writtenPackets.last();
        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.type == PKT_TYPE_NRF24_TX);

        InstrumentNrf24TxPayload payload;
        std::memcpy(&payload, pkt.constData() + sizeof(hdr), sizeof(payload));
        assert(std::memcmp(payload.target_addr, targetAddr.constData(), 5) == 0);
        assert(payload.length == txData.size());
        assert(std::memcmp(payload.data, txData.constData(), txData.size()) == 0);

        InstrumentNrf24TxStatusPayload txSt;
        txSt.status = 0; // Success
        txSt.retries_count = 2;

        bool txStReceived = false;
        InstrumentNrf24TxStatusPayload receivedSt;
        QMetaObject::Connection conn = QObject::connect(&client, &WirelessClient::nrf24TxStatusReceived,
            [&](const InstrumentNrf24TxStatusPayload& st) {
                txStReceived = true;
                receivedSt = st;
            });

        injectPacket(transport, PKT_TYPE_NRF24_TX_STATUS, hdr.sequence, &txSt, sizeof(txSt));
        assert(txStReceived);
        assert(receivedSt.status == 0);
        assert(receivedSt.retries_count == 2);
        QObject::disconnect(conn);
        std::cout << "  [PASS] Test 6: nRF24L01 Transmission & TX Status Ingestion Verified" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 7: nRF24L01 RX Pipe Configuration & Packet Ingestion (0x43 & 0x44)
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        QByteArray pipeAddr = QByteArray::fromHex("E7E7E7E702");
        bool ok = client.configureNrf24RxPipe(1, true, pipeAddr, 32);
        assert(ok);

        const QByteArray& pkt = transport.m_writtenPackets.last();
        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.type == PKT_TYPE_NRF24_RX_CONFIG);

        InstrumentNrf24RxConfigPayload payload;
        std::memcpy(&payload, pkt.constData() + sizeof(hdr), sizeof(payload));
        assert(payload.pipe_index == 1);
        assert(payload.enabled == 1);
        assert(std::memcmp(payload.pipe_addr, pipeAddr.constData(), 5) == 0);
        assert(payload.payload_size == 32);

        InstrumentNrf24RxEventPayload rxEv;
        std::memset(&rxEv, 0, sizeof(rxEv));
        rxEv.pipe_index    = 1;
        rxEv.length        = 4;
        std::memcpy(rxEv.data, "PONG", 4);
        rxEv.timestamp_us  = 98765432ULL;

        bool rxReceived = false;
        InstrumentNrf24RxEventPayload receivedEv;
        QMetaObject::Connection conn = QObject::connect(&client, &WirelessClient::nrf24RxEventReceived,
            [&](const InstrumentNrf24RxEventPayload& ev) {
                rxReceived = true;
                receivedEv = ev;
            });

        injectPacket(transport, PKT_TYPE_NRF24_RX_EVENT, 2, &rxEv, sizeof(rxEv));
        assert(rxReceived);
        assert(receivedEv.pipe_index == 1);
        assert(receivedEv.length == 4);
        assert(std::memcmp(receivedEv.data, "PONG", 4) == 0);
        assert(receivedEv.timestamp_us == 98765432ULL);
        QObject::disconnect(conn);
        std::cout << "  [PASS] Test 7: nRF24L01 RX Pipe Configuration & Ingestion Verified" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 8: nRF24L01 Status Query & Ingestion (PKT_TYPE_NRF24_STATUS 0x45)
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        bool ok = client.queryNrf24Status();
        assert(ok);

        const QByteArray& pkt = transport.m_writtenPackets.last();
        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.type == PKT_TYPE_NRF24_STATUS);
        assert(hdr.payload_len == 0);

        InstrumentNrf24StatusPayload statusPayload;
        std::memset(&statusPayload, 0, sizeof(statusPayload));
        statusPayload.is_connected     = 1;
        statusPayload.carrier_detected = 0;
        statusPayload.rx_fifo_empty    = 1;
        statusPayload.tx_fifo_full     = 0;
        statusPayload.active_channel   = 76;

        bool statusReceived = false;
        InstrumentNrf24StatusPayload receivedStat;
        QMetaObject::Connection conn = QObject::connect(&client, &WirelessClient::nrf24StatusReceived,
            [&](const InstrumentNrf24StatusPayload& st) {
                statusReceived = true;
                receivedStat = st;
            });

        injectPacket(transport, PKT_TYPE_NRF24_STATUS, hdr.sequence, &statusPayload, sizeof(statusPayload));
        assert(statusReceived);
        assert(receivedStat.is_connected == 1);
        assert(receivedStat.active_channel == 76);
        QObject::disconnect(conn);
        std::cout << "  [PASS] Test 8: nRF24L01 Status Query & Response Ingestion Verified" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 9: BLE Scan Command & Radio Event Telemetry (0x2B / 0x2A)
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        bool ok = client.startBleScan(15, false);
        assert(ok);

        const QByteArray& pkt = transport.m_writtenPackets.last();
        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.type == PKT_TYPE_RADIO_CMD);
        assert(hdr.payload_len == sizeof(InstrumentRadioCmdPayload));

        InstrumentRadioCmdPayload cmd;
        std::memcpy(&cmd, pkt.constData() + sizeof(hdr), sizeof(cmd));
        assert(cmd.cmd_type == 1);
        assert(cmd.scan_duration_s == 15);
        assert(cmd.filter_mode == 0);

        InstrumentRadioEventPayload radioEv;
        std::memset(&radioEv, 0, sizeof(radioEv));
        radioEv.radio_type = 1; // BLE
        radioEv.rssi = -65;
        radioEv.channel = 37;
        radioEv.adv_type = 0x02;
        radioEv.mac[0] = 0xAA; radioEv.mac[1] = 0xBB; radioEv.mac[2] = 0xCC;
        radioEv.mac[3] = 0xDD; radioEv.mac[4] = 0xEE; radioEv.mac[5] = 0xFF;
        radioEv.data_len = 10;
        std::memcpy(radioEv.data, "LAB_BEACON", 10);

        bool evReceived = false;
        InstrumentRadioEventPayload receivedEv;
        QMetaObject::Connection conn = QObject::connect(&client, &WirelessClient::radioEventReceived,
            [&](const InstrumentRadioEventPayload& ev) {
                evReceived = true;
                receivedEv = ev;
            });

        injectPacket(transport, PKT_TYPE_RADIO_EVENT, 901, &radioEv, sizeof(radioEv));
        assert(evReceived);
        assert(receivedEv.radio_type == 1);
        assert(receivedEv.rssi == -65);
        assert(receivedEv.channel == 37);
        assert(std::memcmp(receivedEv.data, "LAB_BEACON", 10) == 0);
        QObject::disconnect(conn);
        std::cout << "  [PASS] Test 9: BLE Scan Command & Radio Event Ingestion Verified" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 10: Wi-Fi Telemetry Ingestion (0x2C)
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        bool ok = client.requestWifiTelemetry();
        assert(ok);

        InstrumentRadioWifiDiagPayload wifiDiag;
        std::memset(&wifiDiag, 0, sizeof(wifiDiag));
        wifiDiag.rssi = -52;
        wifiDiag.channel = 6;
        wifiDiag.status_flags = 0x04; // DNS OK
        wifiDiag.dns_latency_us = 12500;
        wifiDiag.gateway_rtt_us = 18000;
        std::strncpy(wifiDiag.ssid, "UDOO_LAB_NET", sizeof(wifiDiag.ssid) - 1);

        bool wifiReceived = false;
        QString receivedSsid;
        int8_t receivedRssi = 0;
        uint32_t receivedDns = 0;
        QMetaObject::Connection conn = QObject::connect(&client, &WirelessClient::wifiTelemetryReceived,
            [&](const QString& ssid, int8_t rssi, uint32_t dnsLatencyUs, uint32_t /*rttUs*/) {
                wifiReceived = true;
                receivedSsid = ssid;
                receivedRssi = rssi;
                receivedDns = dnsLatencyUs;
            });

        injectPacket(transport, PKT_TYPE_RADIO_WIFI_DIAG, 902, &wifiDiag, sizeof(wifiDiag));
        assert(wifiReceived);
        assert(receivedSsid == "UDOO_LAB_NET");
        assert(receivedRssi == -52);
        assert(receivedDns == 12500);
        QObject::disconnect(conn);
        std::cout << "  [PASS] Test 10: Wi-Fi Telemetry Request & Signal Dispatch Verified" << std::endl;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Test 11: High-Precision Time Synchronization (0x24 / 0x25)
    // ─────────────────────────────────────────────────────────────────────────
    {
        transport.m_writtenPackets.clear();
        bool ok = client.performTimeSync();
        assert(ok);

        const QByteArray& pkt = transport.m_writtenPackets.last();
        InstrumentPacketHeader hdr;
        std::memcpy(&hdr, pkt.constData(), sizeof(hdr));
        assert(hdr.type == PKT_TYPE_TIME_SYNC_REQ);
        assert(hdr.payload_len == sizeof(InstrumentTimeSyncReqPayload));

        InstrumentTimeSyncReqPayload req;
        std::memcpy(&req, pkt.constData() + sizeof(hdr), sizeof(req));

        InstrumentTimeSyncRespPayload resp;
        resp.host_request_id = req.host_request_id;
        resp.host_timestamp_us = req.host_timestamp_us;
        resp.device_timestamp_us = req.host_timestamp_us + 500; // 500 us device clock difference
        resp.clock_drift_ppm = 2;

        bool syncReceived = false;
        QMetaObject::Connection conn = QObject::connect(&client, &WirelessClient::timeSyncCompleted,
            [&](int64_t /*offsetUs*/, uint32_t /*rttUs*/) {
                syncReceived = true;
            });

        injectPacket(transport, PKT_TYPE_TIME_SYNC_RESP, 903, &resp, sizeof(resp));
        assert(syncReceived);
        QObject::disconnect(conn);
        std::cout << "  [PASS] Test 11: High-Precision Dual-MCU Time Sync Framing & Dispatch Verified" << std::endl;
    }

    std::cout << "All WirelessClient tests passed successfully!" << std::endl;
    return true;
}
