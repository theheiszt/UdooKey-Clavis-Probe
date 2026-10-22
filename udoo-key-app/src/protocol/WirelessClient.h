#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QVector>
#include <cstdint>
#include "instrument_protocol.h"
#include "ITransport.h"

namespace UdooKey {

/**
 * @brief Wireless & Coprocessor Integration Client for UDOO KEY
 *
 * Provides client-side control and event telemetry for:
 *   1. Infrared (IR) Engine (opcodes 0x30 - 0x33)
 *   2. nRF24L01 2.4 GHz RF Transceiver (opcodes 0x40 - 0x45)
 *   3. Wi-Fi & Bluetooth LE Telemetry (opcodes 0x24, 0x25, 0x2A, 0x2B, 0x2C)
 *
 * Fully integrated into canonical InstrumentProtocol v1.0 (0xAA55, CCITT-FALSE CRC-16).
 */
class WirelessClient : public QObject {
    Q_OBJECT

public:
    explicit WirelessClient(ITransport* transport = nullptr, QObject* parent = nullptr);
    ~WirelessClient() override = default;

    // Transport management
    void setTransport(ITransport* transport);
    ITransport* transport() const { return m_transport; }
    bool isConnected() const;

    // ─── Infrared (IR) Operations (0x30 - 0x33) ──────────────────────────────
    bool configureIr(uint8_t txPin = 25, uint8_t rxPin = 26, uint16_t carrierFreqHz = 38000, uint8_t dutyPercent = 33);
    bool transmitIrNec(uint16_t address, uint32_t command, uint8_t repeatCount = 0);
    bool transmitIrRaw(const QVector<uint16_t>& timingsUs, uint8_t repeatCount = 0);
    bool configureIrRx(bool enabled, bool filterUnknown = false);

    // ─── nRF24L01 Radio Operations (0x40 - 0x45) ─────────────────────────────
    bool configureNrf24(uint8_t channel = 76,
                        InstrumentNrf24Datarate datarate = NRF24_RATE_1MBPS,
                        InstrumentNrf24Power powerLevel = NRF24_PA_MAX,
                        InstrumentNrf24Crc crcMode = NRF24_CRC_16BIT,
                        uint8_t addrWidth = 5,
                        bool autoAck = true);
    bool transmitNrf24(const QByteArray& targetAddr, const QByteArray& data);
    bool configureNrf24RxPipe(uint8_t pipeIndex, bool enabled, const QByteArray& pipeAddr, uint8_t payloadSize = 32);
    bool queryNrf24Status();

    // ─── Wi-Fi, BLE & Time Sync Operations (0x24, 0x25, 0x2A - 0x2C) ─────────
    bool startBleScan(uint8_t duration_s = 10, bool filtered = false);
    bool stopBleScan();
    bool requestWifiRecon();
    bool requestWifiTelemetry();
    bool sendNetworkProbe(const QString& targetIp);
    bool performTimeSync();

    // ─── Wi-Fi Promiscuous Sniffer & PCAP Operations (0x2D, 0x2E, 0x50, 0x51) ──
    bool configureWifiSniffer(bool enabled, uint8_t channel = 1, uint8_t filterType = 0);
    bool queryWifiSnifferStatus();
    bool requestPcapDump(bool streamBinary = true, bool clearAfter = false, uint16_t maxPackets = 0);

    // ─── Raw packet and byte stream processing ───────────────────────────────
    void processIncomingPacket(const InstrumentPacketHeader& hdr, const QByteArray& payload);
    void processIncomingData(const QByteArray& data);

signals:
    void configAcknowledged();
    void irRxEventReceived(const InstrumentIrRxEventPayload& event);
    void nrf24TxStatusReceived(const InstrumentNrf24TxStatusPayload& status);
    void nrf24RxEventReceived(const InstrumentNrf24RxEventPayload& event);
    void nrf24StatusReceived(const InstrumentNrf24StatusPayload& status);
    void wifiTelemetryReceived(const QString& ssid, int8_t rssi, uint32_t dnsLatencyUs, uint32_t rttUs);
    void radioEventReceived(const InstrumentRadioEventPayload& event);
    void timeSyncCompleted(int64_t offsetUs, uint32_t rttUs);
    void wifiSnifferStatusReceived(const InstrumentWifiSniffStatusPayload& status);
    void packetRecordReceived(const InstrumentPacketRecordPayload& record);
    void pcapDataReceived(const QByteArray& pcapChunk);
    void packetErrorReceived(uint8_t errorCode, const QString& message);

private:
    bool sendPacket(InstrumentPacketType type, const void* payload = nullptr, uint16_t len = 0);

    ITransport* m_transport{nullptr};
    uint16_t m_sequence{0};
    QByteArray m_rxBuffer;
};

} // namespace UdooKey
