#include "CommunicationDiagnosticsView.h"
#include "Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QDateTime>

namespace UdooKey {

CommunicationDiagnosticsView::CommunicationDiagnosticsView(StreamDemux* demux, WirelessClient* wirelessClient, QWidget* parent)
    : QWidget(parent)
    , m_demux(demux)
    , m_wirelessClient(wirelessClient) {
    setupUi();

    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &CommunicationDiagnosticsView::updateDiagnostics);
    m_refreshTimer->start(500); // 2 Hz live telemetry refresh

    if (m_wirelessClient) {
        connect(m_wirelessClient, &WirelessClient::wifiTelemetryReceived,
                this, &CommunicationDiagnosticsView::onWifiTelemetryReceived);
    }

    updateDiagnostics();
}

void CommunicationDiagnosticsView::setStreamDemux(StreamDemux* demux) {
    m_demux = demux;
    updateDiagnostics();
}

void CommunicationDiagnosticsView::setWirelessClient(WirelessClient* client) {
    if (m_wirelessClient == client) return;
    if (m_wirelessClient) {
        disconnect(m_wirelessClient, &WirelessClient::wifiTelemetryReceived,
                   this, &CommunicationDiagnosticsView::onWifiTelemetryReceived);
    }
    m_wirelessClient = client;
    if (m_wirelessClient) {
        connect(m_wirelessClient, &WirelessClient::wifiTelemetryReceived,
                this, &CommunicationDiagnosticsView::onWifiTelemetryReceived);
    }
}

void CommunicationDiagnosticsView::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // Header Title
    auto lblTitle = new QLabel("<h2>📡 UDOO KEY Communication & Wireless Diagnostics</h2>", this);
    lblTitle->setStyleSheet("color: #00FFCC; margin-bottom: 2px;");
    mainLayout->addWidget(lblTitle);

    auto topGrid = new QGridLayout();
    topGrid->setSpacing(10);

    // ─── 1. Serial Transport Panel ───────────────────────────────────────────
    auto grpSerial = new QGroupBox("1. Physical USB Serial Link", this);
    grpSerial->setStyleSheet("QGroupBox { font-weight: bold; color: #58A6FF; border: 1px solid #30363D; border-radius: 6px; margin-top: 8px; padding-top: 14px; }");
    auto gridSerial = new QGridLayout(grpSerial);
    gridSerial->setContentsMargins(10, 10, 10, 10);

    gridSerial->addWidget(new QLabel("Port Name:", grpSerial), 0, 0);
    m_lblPortName = new QLabel("Offline", grpSerial);
    m_lblPortName->setStyleSheet("font-weight: bold; color: #E6EDF3;");
    gridSerial->addWidget(m_lblPortName, 0, 1);

    gridSerial->addWidget(new QLabel("Baud Rate:", grpSerial), 1, 0);
    m_lblBaudRate = new QLabel("115200 bps", grpSerial);
    m_lblBaudRate->setStyleSheet("color: #E6EDF3;");
    gridSerial->addWidget(m_lblBaudRate, 1, 1);

    gridSerial->addWidget(new QLabel("Line Status:", grpSerial), 2, 0);
    m_lblConnStatus = new QLabel("Disconnected", grpSerial);
    m_lblConnStatus->setStyleSheet("color: #F85149; font-weight: bold;");
    gridSerial->addWidget(m_lblConnStatus, 2, 1);

    gridSerial->addWidget(new QLabel("Modem Signals:", grpSerial), 3, 0);
    m_lblDtrRts = new QLabel("DTR=1, RTS=1", grpSerial);
    m_lblDtrRts->setStyleSheet("color: #3FB950; font-family: Consolas;");
    gridSerial->addWidget(m_lblDtrRts, 3, 1);

    gridSerial->addWidget(new QLabel("Total Transmitted:", grpSerial), 4, 0);
    m_lblTxBytes = new QLabel("0 B", grpSerial);
    m_lblTxBytes->setStyleSheet("color: #79C0FF; font-family: Consolas;");
    gridSerial->addWidget(m_lblTxBytes, 4, 1);

    gridSerial->addWidget(new QLabel("Total Received:", grpSerial), 5, 0);
    m_lblRxBytes = new QLabel("0 B", grpSerial);
    m_lblRxBytes->setStyleSheet("color: #7EE787; font-family: Consolas;");
    gridSerial->addWidget(m_lblRxBytes, 5, 1);

    topGrid->addWidget(grpSerial, 0, 0);

    // ─── 2. Stream Demux & Framing Panel ─────────────────────────────────────
    auto grpDemux = new QGroupBox("2. Stream Demultiplexer & Frame Decoders", this);
    grpDemux->setStyleSheet("QGroupBox { font-weight: bold; color: #58A6FF; border: 1px solid #30363D; border-radius: 6px; margin-top: 8px; padding-top: 14px; }");
    auto gridDemux = new QGridLayout(grpDemux);
    gridDemux->setContentsMargins(10, 10, 10, 10);

    gridDemux->addWidget(new QLabel("Active State:", grpDemux), 0, 0);
    m_lblDemuxState = new QLabel("Idle (Awaiting sync)", grpDemux);
    m_lblDemuxState->setStyleSheet("color: #3FB950; font-weight: bold;");
    gridDemux->addWidget(m_lblDemuxState, 0, 1);

    gridDemux->addWidget(new QLabel("Total Processed:", grpDemux), 1, 0);
    m_lblTotalProcessed = new QLabel("0 bytes", grpDemux);
    m_lblTotalProcessed->setStyleSheet("color: #E6EDF3; font-family: Consolas;");
    gridDemux->addWidget(m_lblTotalProcessed, 1, 1);

    gridDemux->addWidget(new QLabel("Terminal Chars Routed:", grpDemux), 2, 0);
    m_lblTerminalRouted = new QLabel("0 chars", grpDemux);
    m_lblTerminalRouted->setStyleSheet("color: #56D364; font-family: Consolas; font-weight: bold;");
    gridDemux->addWidget(m_lblTerminalRouted, 2, 1);

    gridDemux->addWidget(new QLabel("0xAA55 Instrument Packets:", grpDemux), 3, 0);
    m_lbl0xAA55Packets = new QLabel("0 valid frames", grpDemux);
    m_lbl0xAA55Packets->setStyleSheet("color: #FFA657; font-family: Consolas;");
    gridDemux->addWidget(m_lbl0xAA55Packets, 3, 1);

    gridDemux->addWidget(new QLabel("0x5A Logic Stream Frames:", grpDemux), 4, 0);
    m_lbl0x5AFrames = new QLabel("0 buffers", grpDemux);
    m_lbl0x5AFrames->setStyleSheet("color: #D2A8FF; font-family: Consolas;");
    gridDemux->addWidget(m_lbl0x5AFrames, 4, 1);

    gridDemux->addWidget(new QLabel("Framing Invariant:", grpDemux), 5, 0);
    auto lblInv = new QLabel("Terminal text safe from 0x5A hijacking", grpDemux);
    lblInv->setStyleSheet("color: #8B949E; font-size: 11px;");
    gridDemux->addWidget(lblInv, 5, 1);

    topGrid->addWidget(grpDemux, 0, 1);

    // ─── 3. RP2040 Microcontroller Status ────────────────────────────────────
    auto grpRp2040 = new QGroupBox("3. RP2040 Real-Time Master Status", this);
    grpRp2040->setStyleSheet("QGroupBox { font-weight: bold; color: #58A6FF; border: 1px solid #30363D; border-radius: 6px; margin-top: 8px; padding-top: 14px; }");
    auto gridRp2040 = new QGridLayout(grpRp2040);
    gridRp2040->setContentsMargins(10, 10, 10, 10);

    gridRp2040->addWidget(new QLabel("Hardware Core:", grpRp2040), 0, 0);
    m_lblRp2040Model = new QLabel("RP2040 Dual ARM Cortex-M0+ @ 133 MHz", grpRp2040);
    m_lblRp2040Model->setStyleSheet("color: #E6EDF3; font-weight: bold;");
    gridRp2040->addWidget(m_lblRp2040Model, 0, 1);

    gridRp2040->addWidget(new QLabel("Active Profile:", grpRp2040), 1, 0);
    m_lblRp2040Profile = new QLabel("Universal Electronics Instrument Workbench", grpRp2040);
    m_lblRp2040Profile->setStyleSheet("color: #79C0FF;");
    gridRp2040->addWidget(m_lblRp2040Profile, 1, 1);

    gridRp2040->addWidget(new QLabel("Sample Buffer:", grpRp2040), 2, 0);
    m_lblRp2040Clock = new QLabel("64 KiB Circular DMA / 100 MSPS PIO", grpRp2040);
    m_lblRp2040Clock->setStyleSheet("color: #E6EDF3;");
    gridRp2040->addWidget(m_lblRp2040Clock, 2, 1);

    gridRp2040->addWidget(new QLabel("Inter-MCU UART:", grpRp2040), 3, 0);
    auto lblUart = new QLabel("GP0 TX / GP1 RX @ 1,000,000 baud 8N1", grpRp2040);
    lblUart->setStyleSheet("color: #3FB950; font-family: Consolas;");
    gridRp2040->addWidget(lblUart, 3, 1);

    topGrid->addWidget(grpRp2040, 1, 0);

    // ─── 4. ESP32 Coprocessor & Wi-Fi Panel ──────────────────────────────────
    auto grpEsp32 = new QGroupBox("4. ESP32 Wireless & Telemetry Subsystem", this);
    grpEsp32->setStyleSheet("QGroupBox { font-weight: bold; color: #58A6FF; border: 1px solid #30363D; border-radius: 6px; margin-top: 8px; padding-top: 14px; }");
    auto gridEsp32 = new QGridLayout(grpEsp32);
    gridEsp32->setContentsMargins(10, 10, 10, 10);

    gridEsp32->addWidget(new QLabel("Coprocessor Core:", grpEsp32), 0, 0);
    m_lblEsp32Model = new QLabel("ESP32-WROVER-E (Dual Xtensa LX6 @ 240 MHz)", grpEsp32);
    m_lblEsp32Model->setStyleSheet("color: #E6EDF3; font-weight: bold;");
    gridEsp32->addWidget(m_lblEsp32Model, 0, 1);

    gridEsp32->addWidget(new QLabel("Wi-Fi Mode & Role:", grpEsp32), 1, 0);
    m_lblEsp32WifiMode = new QLabel("AP + Station Dual-Role (Channel 1)", grpEsp32);
    m_lblEsp32WifiMode->setStyleSheet("color: #3FB950; font-weight: bold;");
    gridEsp32->addWidget(m_lblEsp32WifiMode, 1, 1);

    gridEsp32->addWidget(new QLabel("Access Point SSID:", grpEsp32), 2, 0);
    m_lblEsp32WifiSsid = new QLabel("UDOO-BIT-PIRATE-SCOPE", grpEsp32);
    m_lblEsp32WifiSsid->setStyleSheet("color: #79C0FF; font-weight: bold; font-family: Consolas;");
    gridEsp32->addWidget(m_lblEsp32WifiSsid, 2, 1);

    gridEsp32->addWidget(new QLabel("SoftAP IP Address:", grpEsp32), 3, 0);
    m_lblEsp32WifiIp = new QLabel("192.168.4.1 (Web Scope & WS)", grpEsp32);
    m_lblEsp32WifiIp->setStyleSheet("color: #56D364; font-weight: bold; font-family: Consolas;");
    gridEsp32->addWidget(m_lblEsp32WifiIp, 3, 1);

    gridEsp32->addWidget(new QLabel("Signal RSSI / Latency:", grpEsp32), 4, 0);
    m_lblEsp32WifiRssi = new QLabel("Direct AP Link | RTT: 1.2 ms | DNS: OK", grpEsp32);
    m_lblEsp32WifiRssi->setStyleSheet("color: #FFA657; font-family: Consolas;");
    gridEsp32->addWidget(m_lblEsp32WifiRssi, 4, 1);

    topGrid->addWidget(grpEsp32, 1, 1);

    mainLayout->addLayout(topGrid);

    // ─── Actions & Diagnostic Control Bar ────────────────────────────────────
    auto barLayout = new QHBoxLayout();
    m_btnQueryStatus = new QPushButton("🔄 Query System Status", this);
    m_btnQueryStatus->setStyleSheet("font-weight: bold; background-color: #238636; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(m_btnQueryStatus, &QPushButton::clicked, this, &CommunicationDiagnosticsView::onBtnQueryStatusClicked);
    barLayout->addWidget(m_btnQueryStatus);

    m_btnQueryWifi = new QPushButton("📡 Query ESP32 Wi-Fi", this);
    m_btnQueryWifi->setStyleSheet("background-color: #1F6FEB; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(m_btnQueryWifi, &QPushButton::clicked, this, &CommunicationDiagnosticsView::onBtnQueryWifiClicked);
    barLayout->addWidget(m_btnQueryWifi);

    m_btnWifiRecon = new QPushButton("🔍 Wi-Fi Recon Scan", this);
    m_btnWifiRecon->setStyleSheet("background-color: #388BFD; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(m_btnWifiRecon, &QPushButton::clicked, this, &CommunicationDiagnosticsView::onBtnWifiReconClicked);
    barLayout->addWidget(m_btnWifiRecon);

    m_btnPing = new QPushButton("⏱ Ping-Pong Time Sync", this);
    m_btnPing->setStyleSheet("background-color: #8957E5; color: white; padding: 6px 12px; border-radius: 4px;");
    connect(m_btnPing, &QPushButton::clicked, this, &CommunicationDiagnosticsView::onBtnPingClicked);
    barLayout->addWidget(m_btnPing);

    barLayout->addStretch();

    m_btnClearCounters = new QPushButton("Clear Counters", this);
    m_btnClearCounters->setStyleSheet("background-color: #21262D; color: #8B949E; padding: 6px 12px; border-radius: 4px;");
    connect(m_btnClearCounters, &QPushButton::clicked, this, &CommunicationDiagnosticsView::onBtnClearCountersClicked);
    barLayout->addWidget(m_btnClearCounters);

    mainLayout->addLayout(barLayout);
    mainLayout->addStretch();
}

void CommunicationDiagnosticsView::updateDiagnostics() {
    auto* transport = DeviceManager::instance().transport();
    bool connected = DeviceManager::instance().isConnected();

    if (transport && connected) {
        m_lblPortName->setText(transport->portName());
        m_lblConnStatus->setText("ONLINE (Read/Write Active)");
        m_lblConnStatus->setStyleSheet("color: #3FB950; font-weight: bold;");

        auto* serial = dynamic_cast<SerialTransport*>(transport);
        if (serial) {
            m_lblBaudRate->setText(QString("%1 bps").arg(serial->baudRate()));
            m_lblDtrRts->setText("DTR=1, RTS=1 (Active)");
            m_lblTxBytes->setText(QString("%1 bytes (%2 KiB)")
                                  .arg(serial->totalBytesWritten())
                                  .arg(serial->totalBytesWritten() / 1024.0, 0, 'f', 1));
            m_lblRxBytes->setText(QString("%1 bytes (%2 KiB)")
                                  .arg(serial->totalBytesRead())
                                  .arg(serial->totalBytesRead() / 1024.0, 0, 'f', 1));
        } else {
            m_lblBaudRate->setText("Internal / Simulation");
            m_lblDtrRts->setText("Virtual Loopback");
        }
    } else {
        m_lblPortName->setText("Disconnected");
        m_lblConnStatus->setText("OFFLINE");
        m_lblConnStatus->setStyleSheet("color: #F85149; font-weight: bold;");
        m_lblDtrRts->setText("DTR=0, RTS=0");
    }

    if (m_demux) {
        QString stateStr = "Idle";
        switch (m_demux->currentState()) {
            case DemuxState::Idle:
                stateStr = "Idle (Streaming)";
                break;
            case DemuxState::CollectingBinary0x5A:
                stateStr = "Collecting 0x5A Logic Capture Buffer";
                break;
            case DemuxState::CollectingSumpData:
                stateStr = "Collecting SUMP Waveform Data";
                break;
            case DemuxState::CollectingInstrument0xAA55:
                stateStr = "Collecting 0xAA55 Instrument Packet";
                break;
        }
        m_lblDemuxState->setText(stateStr);
        m_lblTotalProcessed->setText(QString("%1 bytes").arg(m_demux->totalBytesProcessed()));
        m_lblTerminalRouted->setText(QString("%1 chars").arg(m_demux->totalTerminalBytes()));
        m_lbl0xAA55Packets->setText(QString("%1 packets").arg(m_demux->packets0xAA55Parsed()));
        m_lbl0x5AFrames->setText(QString("%1 frames").arg(m_demux->frames0x5AParsed()));
    }

    // Refresh Wi-Fi display
    m_lblEsp32WifiSsid->setText(m_wifiSsid.isEmpty() ? "UDOO-BIT-PIRATE-SCOPE" : m_wifiSsid);
    m_lblEsp32WifiIp->setText(m_wifiIp.isEmpty() ? "192.168.4.1 (SoftAP)" : m_wifiIp);
    if (m_wifiRssi != 0 || m_wifiDnsUs > 0) {
        m_lblEsp32WifiRssi->setText(QString("%1 dBm | DNS Latency: %2 µs | Gateway RTT: %3 µs")
                                    .arg(m_wifiRssi).arg(m_wifiDnsUs).arg(m_wifiRttUs));
    }
}

void CommunicationDiagnosticsView::onWifiTelemetryReceived(const QString& ssid, int8_t rssi, uint32_t dnsLatencyUs, uint32_t rttUs) {
    if (!ssid.isEmpty()) m_wifiSsid = ssid;
    m_wifiRssi = rssi;
    m_wifiDnsUs = dnsLatencyUs;
    m_wifiRttUs = rttUs;
    updateDiagnostics();
}

void CommunicationDiagnosticsView::onBtnQueryStatusClicked() {
    auto* transport = DeviceManager::instance().transport();
    if (transport && transport->isOpen()) {
        transport->write("status\r\n");
        LOG_INFO("Dispatched 'status' query to instrument");
    }
}

void CommunicationDiagnosticsView::onBtnQueryWifiClicked() {
    if (m_wirelessClient) {
        m_wirelessClient->requestWifiTelemetry();
    }
    auto* transport = DeviceManager::instance().transport();
    if (transport && transport->isOpen()) {
        transport->write("wifi status\r\n");
    }
    LOG_INFO("Dispatched Wi-Fi status query to ESP32 coprocessor");
}

void CommunicationDiagnosticsView::onBtnWifiReconClicked() {
    if (m_wirelessClient) {
        m_wirelessClient->requestWifiRecon();
    }
    LOG_INFO("Dispatched 802.11 2.4 GHz channel scan to ESP32 coprocessor");
}

void CommunicationDiagnosticsView::onBtnPingClicked() {
    if (m_wirelessClient) {
        m_wirelessClient->performTimeSync();
    }
    LOG_INFO("Dispatched ping-pong microsecond time synchronization request");
}

void CommunicationDiagnosticsView::onBtnClearCountersClicked() {
    if (m_demux) {
        m_demux->reset();
    }
    updateDiagnostics();
}

} // namespace UdooKey
