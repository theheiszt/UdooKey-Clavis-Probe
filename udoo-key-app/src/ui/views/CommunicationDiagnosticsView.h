#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include "StreamDemux.h"
#include "WirelessClient.h"
#include "DeviceManager.h"
#include "SerialTransport.h"

namespace UdooKey {

class CommunicationDiagnosticsView : public QWidget {
    Q_OBJECT

public:
    explicit CommunicationDiagnosticsView(StreamDemux* demux, WirelessClient* wirelessClient = nullptr, QWidget* parent = nullptr);
    ~CommunicationDiagnosticsView() override = default;

    void setStreamDemux(StreamDemux* demux);
    void setWirelessClient(WirelessClient* client);

public slots:
    void updateDiagnostics();
    void onWifiTelemetryReceived(const QString& ssid, int8_t rssi, uint32_t dnsLatencyUs, uint32_t rttUs);

private slots:
    void onBtnQueryStatusClicked();
    void onBtnQueryWifiClicked();
    void onBtnWifiReconClicked();
    void onBtnPingClicked();
    void onBtnClearCountersClicked();

private:
    void setupUi();

    StreamDemux* m_demux{nullptr};
    WirelessClient* m_wirelessClient{nullptr};
    QTimer* m_refreshTimer{nullptr};

    // Serial Transport Labels
    QLabel* m_lblPortName{nullptr};
    QLabel* m_lblBaudRate{nullptr};
    QLabel* m_lblConnStatus{nullptr};
    QLabel* m_lblDtrRts{nullptr};
    QLabel* m_lblTxBytes{nullptr};
    QLabel* m_lblRxBytes{nullptr};

    // Stream Demux Labels
    QLabel* m_lblDemuxState{nullptr};
    QLabel* m_lblTotalProcessed{nullptr};
    QLabel* m_lblTerminalRouted{nullptr};
    QLabel* m_lbl0xAA55Packets{nullptr};
    QLabel* m_lbl0x5AFrames{nullptr};

    // RP2040 MCU Status Labels
    QLabel* m_lblRp2040Model{nullptr};
    QLabel* m_lblRp2040Clock{nullptr};
    QLabel* m_lblRp2040Profile{nullptr};

    // ESP32 Coprocessor & Wi-Fi Status Labels
    QLabel* m_lblEsp32Model{nullptr};
    QLabel* m_lblEsp32WifiMode{nullptr};
    QLabel* m_lblEsp32WifiSsid{nullptr};
    QLabel* m_lblEsp32WifiIp{nullptr};
    QLabel* m_lblEsp32WifiRssi{nullptr};

    // Buttons
    QPushButton* m_btnQueryStatus{nullptr};
    QPushButton* m_btnQueryWifi{nullptr};
    QPushButton* m_btnWifiRecon{nullptr};
    QPushButton* m_btnPing{nullptr};
    QPushButton* m_btnClearCounters{nullptr};

    QString m_wifiSsid;
    QString m_wifiIp;
    int8_t  m_wifiRssi{0};
    uint32_t m_wifiDnsUs{0};
    uint32_t m_wifiRttUs{0};
};

} // namespace UdooKey
