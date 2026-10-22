#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "wifi_config.h"
#include "../packet/packet_record.h"

namespace Esp32Wireless {

enum class WifiState {
    Disabled,
    Initializing,
    APStarting,
    APActive,
    APError,
    STAConnecting,
    STAConnected,
    AP_STAActive,
    Scanning,
    Sniffing
};

const char* wifiStateToString(WifiState state);

enum class SniffFilter : uint8_t {
    ALL = 0,
    MGMT,
    CTRL,
    DATA
};

struct WifiStatusReport {
    bool        ap_active;
    bool        sta_connected;
    uint8_t     station_count;
    uint8_t     channel;
    char        ap_ssid[33];
    char        sta_ssid[33];
    uint32_t    ap_ip;
    uint32_t    sta_ip;
    int32_t     rssi;
    uint32_t    uptime_ms;
    WifiState   state;
};

class WifiService {
public:
    static WifiService& instance();

    // Deterministic single-owner Wi-Fi initialization
    bool initialize();
    bool init() { return initialize(); }

    // Runtime Health Check / Watchdog
    void healthCheck();

    // State Inspection
    WifiState state() const { return m_state; }
    const char* stateString() const { return wifiStateToString(m_state); }
    bool apActive() const { return m_state == WifiState::APActive || m_state == WifiState::AP_STAActive; }
    bool staConnected() const { return m_state == WifiState::STAConnected || m_state == WifiState::AP_STAActive; }
    uint8_t connectedStations() const;
    WifiStatusReport getStatusReport() const;

    // AP Control
    bool startAP();
    bool stopAP();

    // Diagnostic & Reports
    void printStatus(Print& out);
    void printFailureDiagnostics(Print& out);
    bool runSelfTest(Print& out);

    // Scanning
    void scanNetworks(bool passive, Print& out);
    String runWifiReconJson();

    // Station (Client) Mode
    bool connectStation(const char* ssid, const char* pass, Print& out);
    void disconnectStation(Print& out);

    // Network Utilities
    void pingHost(const char* host, Print& out);
    void resolveDns(const char* host, Print& out);
    void listChannels(Print& out);
    bool setChannel(uint8_t channel, Print& out);

    // Promiscuous Sniffer Subsystem
    bool startSniffer(uint8_t channel = 1, SniffFilter filter = SniffFilter::ALL, Print* out = nullptr);
    void stopSniffer(Print* out = nullptr);
    bool isSnifferActive() const { return m_state == WifiState::Sniffing; }
    void printSnifferStatus(Print& out);
    void setSnifferFilter(SniffFilter filter, Print& out);
    void processSnifferQueue();

    // CLI Dispatch
    void executeCommand(const char* args, Print& out);

private:
    WifiService();
    ~WifiService() = default;

    static void wifiPromiscuousRxCb(void* buf, wifi_promiscuous_pkt_type_t type);

    WifiState   m_state{WifiState::Disabled};
    WifiState   m_previousState{WifiState::Disabled};
    bool        m_initialized{false};
    uint8_t     m_sniffChannel{1};
    SniffFilter m_filter{SniffFilter::ALL};

    uint32_t    m_capturedPackets{0};
    uint32_t    m_droppedPackets{0};

    uint32_t    m_lastHealthCheckMs{0};
    uint8_t     m_recoveryCount{0};
};

} // namespace Esp32Wireless
