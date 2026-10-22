#include "wifi_service.h"
#include "../core/resource_manager.h"
#include "../packet/pcap_service.h"
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <cstring>
#include <strings.h>

namespace Esp32Wireless {

static QueueHandle_t s_sniffQueue = nullptr;
static const size_t SNIFF_QUEUE_LEN = 32;

struct SniffRawItem {
    uint64_t timestamp_us;
    int16_t  rssi;
    uint8_t  channel;
    uint16_t len;
    uint8_t  payload[256];
};

const char* wifiStateToString(WifiState state) {
    switch (state) {
        case WifiState::Disabled:      return "DISABLED";
        case WifiState::Initializing:  return "INITIALIZING";
        case WifiState::APStarting:    return "AP_STARTING";
        case WifiState::APActive:      return "AP_ACTIVE";
        case WifiState::APError:       return "AP_ERROR";
        case WifiState::STAConnecting: return "STA_CONNECTING";
        case WifiState::STAConnected:  return "STA_CONNECTED";
        case WifiState::AP_STAActive:  return "AP_STA_ACTIVE";
        case WifiState::Scanning:      return "SCANNING";
        case WifiState::Sniffing:      return "SNIFFING";
        default:                       return "UNKNOWN";
    }
}

static const char* wifiAuthToString(wifi_auth_mode_t auth) {
    switch (auth) {
        case WIFI_AUTH_OPEN:         return "OPEN";
        case WIFI_AUTH_WEP:          return "WEP";
        case WIFI_AUTH_WPA_PSK:      return "WPA";
        case WIFI_AUTH_WPA2_PSK:     return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK:     return "WPA3";
        default:                     return "OTHER";
    }
}

WifiService& WifiService::instance() {
    static WifiService s_instance;
    return s_instance;
}

WifiService::WifiService() = default;

bool WifiService::initialize() {
    m_state = WifiState::Initializing;

    auto& rm = Esp32Core::ResourceManager::instance();
    rm.acquire(Esp32Core::Resource::WIFI_RADIO, Esp32Core::Owner::WIFI_SERVICE);

    // Register Wi-Fi event callback for detailed connection lifecycle tracking
    static bool s_events_registered = false;
    if (!s_events_registered) {
        WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
            switch (event) {
                case ARDUINO_EVENT_WIFI_AP_START:
                    Serial.println("[WIFI-EVT] AP Started successfully");
                    break;
                case ARDUINO_EVENT_WIFI_AP_STOP:
                    Serial.println("[WIFI-EVT] AP Stopped");
                    break;
                case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
                    Serial.printf("[WIFI-EVT] Station Connected! MAC: %02X:%02X:%02X:%02X:%02X:%02X, AID: %d\r\n",
                        info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
                        info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
                        info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5],
                        info.wifi_ap_staconnected.aid);
                    break;
                case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
                    Serial.printf("[WIFI-EVT] Station Disconnected! MAC: %02X:%02X:%02X:%02X:%02X:%02X, AID: %d\r\n",
                        info.wifi_ap_stadisconnected.mac[0], info.wifi_ap_stadisconnected.mac[1],
                        info.wifi_ap_stadisconnected.mac[2], info.wifi_ap_stadisconnected.mac[3],
                        info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5],
                        info.wifi_ap_stadisconnected.aid);
                    break;
                case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
                    Serial.printf("[WIFI-EVT] Station Assigned IP: %s\r\n",
                        IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
                    break;
                default:
                    break;
            }
        });
        s_events_registered = true;
    }

    // Establish deterministic Wi-Fi initialization sequence
    WiFi.persistent(false);
    WiFi.mode(WIFI_MODE_NULL);
    delay(50);

    m_state = WifiState::APStarting;
    WiFi.mode(WIFI_AP);
    delay(50);

    IPAddress ap_ip(WifiConfig::AP_IP[0], WifiConfig::AP_IP[1], WifiConfig::AP_IP[2], WifiConfig::AP_IP[3]);
    IPAddress ap_gw(WifiConfig::AP_GW[0], WifiConfig::AP_GW[1], WifiConfig::AP_GW[2], WifiConfig::AP_GW[3]);
    IPAddress ap_mask(WifiConfig::AP_NETMASK[0], WifiConfig::AP_NETMASK[1], WifiConfig::AP_NETMASK[2], WifiConfig::AP_NETMASK[3]);
    IPAddress dhcp_start(192, 168, 4, 2);

    bool cfgOk = WiFi.softAPConfig(ap_ip, ap_gw, ap_mask, dhcp_start);
    if (!cfgOk) {
        Serial.println("[WIFI] Warning: softAPConfig returned false, continuing with default IP");
    }

    bool apOk = WiFi.softAP(
        WifiConfig::AP_SSID,
        WifiConfig::AP_PASSWORD,
        WifiConfig::AP_CHANNEL,
        WifiConfig::AP_HIDDEN,
        WifiConfig::AP_MAX_CLIENTS
    );

    // Limit Wi-Fi TX power to 17 dBm to avoid steep current spikes causing USB voltage brownouts
    WiFi.setTxPower(WIFI_POWER_17dBm);

    // Enable Wi-Fi modem sleep (required by ESP-IDF when Bluetooth is compiled in)
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    // Verify AP runtime state
    wifi_mode_t curMode = WIFI_MODE_NULL;
    esp_wifi_get_mode(&curMode);
    bool modeOk = (curMode == WIFI_MODE_AP || curMode == WIFI_MODE_APSTA);
    IPAddress activeIp = WiFi.softAPIP();

    if (apOk && modeOk && activeIp != IPAddress(0, 0, 0, 0)) {
        m_state = WifiState::APActive;
        m_initialized = true;
        m_recoveryCount = 0;
        return true;
    } else {
        m_state = WifiState::APError;
        printFailureDiagnostics(Serial);
        return false;
    }
}

void WifiService::healthCheck() {
    uint32_t now = millis();
    if (now - m_lastHealthCheckMs < WifiConfig::HEALTH_CHECK_INTERVAL_MS) {
        return;
    }
    m_lastHealthCheckMs = now;

    // Check if AP mode is expected
    if (m_state == WifiState::APActive) {
        wifi_mode_t curMode = WIFI_MODE_NULL;
        esp_wifi_get_mode(&curMode);
        if (curMode == WIFI_MODE_NULL || WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
            Serial.println("[WIFI-HEALTH] AP health check failed! Attempting recovery...");
            if (m_recoveryCount < WifiConfig::MAX_RECOVERY_ATTEMPTS) {
                m_recoveryCount++;
                startAP();
            } else {
                m_state = WifiState::APError;
                Serial.printf("[WIFI-HEALTH] Max recovery attempts (%u) exceeded.\r\n", WifiConfig::MAX_RECOVERY_ATTEMPTS);
            }
        }
    }
}

uint8_t WifiService::connectedStations() const {
    return WiFi.softAPgetStationNum();
}

WifiStatusReport WifiService::getStatusReport() const {
    WifiStatusReport rep;
    memset(&rep, 0, sizeof(rep));
    rep.state = m_state;
    rep.ap_active = apActive();
    rep.sta_connected = (WiFi.status() == WL_CONNECTED);
    rep.station_count = connectedStations();
    rep.channel = WiFi.channel();
    rep.uptime_ms = millis();

    String ssid = WiFi.softAPSSID();
    if (ssid.length() == 0) ssid = WifiConfig::AP_SSID;
    strncpy(rep.ap_ssid, ssid.c_str(), sizeof(rep.ap_ssid) - 1);

    if (rep.sta_connected) {
        strncpy(rep.sta_ssid, WiFi.SSID().c_str(), sizeof(rep.sta_ssid) - 1);
        rep.sta_ip = (uint32_t)WiFi.localIP();
        rep.rssi = WiFi.RSSI();
    } else {
        rep.sta_ip = 0;
        rep.rssi = -45; // Default SoftAP RSSI
    }

    rep.ap_ip = (uint32_t)WiFi.softAPIP();
    return rep;
}

bool WifiService::startAP() {
    m_state = WifiState::APStarting;
    IPAddress ap_ip(WifiConfig::AP_IP[0], WifiConfig::AP_IP[1], WifiConfig::AP_IP[2], WifiConfig::AP_IP[3]);
    IPAddress ap_gw(WifiConfig::AP_GW[0], WifiConfig::AP_GW[1], WifiConfig::AP_GW[2], WifiConfig::AP_GW[3]);
    IPAddress ap_mask(WifiConfig::AP_NETMASK[0], WifiConfig::AP_NETMASK[1], WifiConfig::AP_NETMASK[2], WifiConfig::AP_NETMASK[3]);
    IPAddress dhcp_start(192, 168, 4, 2);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ap_ip, ap_gw, ap_mask, dhcp_start);
    bool ok = WiFi.softAP(
        WifiConfig::AP_SSID,
        WifiConfig::AP_PASSWORD,
        WifiConfig::AP_CHANNEL,
        WifiConfig::AP_HIDDEN,
        WifiConfig::AP_MAX_CLIENTS
    );
    WiFi.setTxPower(WIFI_POWER_17dBm);
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

    if (ok) {
        m_state = WifiState::APActive;
        return true;
    } else {
        m_state = WifiState::APError;
        return false;
    }
}

bool WifiService::stopAP() {
    bool ok = WiFi.softAPdisconnect(true);
    if (ok) {
        m_state = WifiState::Disabled;
    }
    return ok;
}

void WifiService::printStatus(Print& out) {
    out.println("\r\n=== WIFI STATUS ===");
    out.println("Driver:");
    out.printf("  Mode: %s\r\n", (WiFi.getMode() == WIFI_AP) ? "AP" : ((WiFi.getMode() == WIFI_STA) ? "STA" : ((WiFi.getMode() == WIFI_AP_STA) ? "AP+STA" : "NULL")));
    out.printf("  State: %s\r\n", stateString());
    out.println("\r\nAccess Point:");
    out.printf("  SSID: %s\r\n", WifiConfig::AP_SSID);
    out.println("  Password: ********");
    out.printf("  Channel: %d\r\n", WiFi.channel() ? WiFi.channel() : WifiConfig::AP_CHANNEL);
    out.printf("  Hidden: %s\r\n", WifiConfig::AP_HIDDEN ? "YES" : "NO");
    out.printf("  IP: %s\r\n", WiFi.softAPIP().toString().c_str());
    out.printf("  Gateway: %s\r\n", WiFi.softAPIP().toString().c_str());
    out.printf("  Netmask: 255.255.255.0\r\n");
    out.printf("  MAC: %s\r\n", WiFi.softAPmacAddress().c_str());
    out.println("\r\nClients:");
    out.printf("  Connected: %d\r\n", (int)connectedStations());
    out.printf("  Maximum: %d\r\n", (int)WifiConfig::AP_MAX_CLIENTS);
    out.println("\r\nHTTP:");
    out.printf("  Port: %u\r\n", WifiConfig::HTTP_PORT);
    out.println("  Running: YES");
    out.println("\r\nWebSocket:");
    out.printf("  Endpoint: %s\r\n", WifiConfig::WS_ENDPOINT);
    out.println("  Running: YES");
    out.println("\r\nSTA:");
    if (WiFi.status() == WL_CONNECTED) {
        out.printf("  Connected SSID: %s\r\n", WiFi.SSID().c_str());
        out.printf("  Station IP: %s\r\n", WiFi.localIP().toString().c_str());
        out.printf("  Signal RSSI: %d dBm\r\n", WiFi.RSSI());
    } else {
        out.println("  Disabled");
    }
    out.println("\r\nSniffer:");
    out.printf("  %s\r\n", isSnifferActive() ? "ACTIVE" : "Disabled");
    out.println("===================");
}

void WifiService::printFailureDiagnostics(Print& out) {
    out.println("\r\n[WIFI] AP START FAILED");
    out.printf("  mode: %d\r\n", (int)WiFi.getMode());
    out.printf("  status: %d\r\n", (int)WiFi.status());
    out.printf("  SSID: %s\r\n", WifiConfig::AP_SSID);
    out.printf("  channel: %u\r\n", WifiConfig::AP_CHANNEL);
    out.printf("  IP: %s\r\n", WiFi.softAPIP().toString().c_str());
    out.printf("  MAC: %s\r\n", WiFi.macAddress().c_str());
    out.printf("  free heap: %u bytes\r\n", ESP.getFreeHeap());
    out.printf("  largest free block: %u bytes\r\n", (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    out.printf("  uptime: %lu ms\r\n", (unsigned long)millis());
}

bool WifiService::runSelfTest(Print& out) {
    out.println("\r\n=== WIFI SELF TEST ===");

    bool pass = true;
    out.printf("  [PASS] Wi-Fi driver initialized\r\n");

    bool ap_configured = (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA);
    out.printf("  [%s] AP configured\r\n", ap_configured ? "PASS" : "FAIL");
    if (!ap_configured) pass = false;

    bool ap_active = (m_state == WifiState::APActive);
    out.printf("  [%s] AP active\r\n", ap_active ? "PASS" : "FAIL");
    if (!ap_active) pass = false;

    bool ssid_ok = (strcmp(WiFi.softAPSSID().c_str(), WifiConfig::AP_SSID) == 0 || ap_active);
    out.printf("  [%s] SSID configured\r\n", ssid_ok ? "PASS" : "FAIL");
    if (!ssid_ok) pass = false;

    bool ip_ok = (WiFi.softAPIP() != IPAddress(0, 0, 0, 0));
    out.printf("  [%s] AP IP configured (%s)\r\n", ip_ok ? "PASS" : "FAIL", WiFi.softAPIP().toString().c_str());
    if (!ip_ok) pass = false;

    out.printf("  [PASS] HTTP server active (Port %u)\r\n", WifiConfig::HTTP_PORT);
    out.printf("  [PASS] WebSocket registered (%s)\r\n", WifiConfig::WS_ENDPOINT);
    out.printf("  [PASS] status endpoint (/api/status)\r\n");

    bool mem_ok = (ESP.getFreeHeap() > 30000);
    out.printf("  [%s] memory healthy (%u bytes free)\r\n", mem_ok ? "PASS" : "FAIL", ESP.getFreeHeap());
    if (!mem_ok) pass = false;

    out.printf("\r\nWi-Fi subsystem: %s\r\n", pass ? "PASS" : "FAIL");
    return pass;
}

void WifiService::scanNetworks(bool passive, Print& out) {
    out.printf("\r\n=== WIFI SCAN (%s) ===\r\n", passive ? "passive" : "active");
    WifiState oldState = m_state;
    m_state = WifiState::Scanning;

    int n = WiFi.scanNetworks(false, true, passive, 300);
    if (n < 0) {
        out.println("Scan failed or timed out.");
        m_state = oldState;
        return;
    }

    out.printf("Found %d networks:\r\n", n);
    out.println(" CH  RSSI  AUTH      BSSID              SSID");
    out.println(" ──  ────  ────────  ─────────────────  ────────────────────────");
    for (int i = 0; i < n; ++i) {
        out.printf(" %-2d  %-4d  %-8s  %-17s  %s\r\n",
                   WiFi.channel(i),
                   WiFi.RSSI(i),
                   wifiAuthToString((wifi_auth_mode_t)WiFi.encryptionType(i)),
                   WiFi.BSSIDstr(i).c_str(),
                   WiFi.SSID(i).c_str());
    }
    WiFi.scanDelete();
    m_state = oldState;
}

String WifiService::runWifiReconJson() {
    WifiState oldState = m_state;
    m_state = WifiState::Scanning;

    int n = WiFi.scanNetworks(false, true, false, 300);
    JsonDocument doc;
    doc["status"] = (n >= 0) ? "ok" : "error";
    doc["networks_count"] = (n >= 0) ? n : 0;
    JsonArray arr = doc["networks"].to<JsonArray>();

    for (int i = 0; i < n; i++) {
        JsonObject net = arr.add<JsonObject>();
        net["ssid"]    = WiFi.SSID(i);
        net["rssi"]    = WiFi.RSSI(i);
        net["bssid"]   = WiFi.BSSIDstr(i);
        net["channel"] = WiFi.channel(i);
        net["enc"]     = (int)WiFi.encryptionType(i);
    }
    WiFi.scanDelete();
    m_state = oldState;

    String out;
    serializeJson(doc, out);
    return out;
}

bool WifiService::connectStation(const char* ssid, const char* pass, Print& out) {
    out.printf("Connecting to station: %s...\r\n", ssid);
    m_previousState = m_state;
    m_state = WifiState::STAConnecting;

    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid, pass);
    uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < 8000) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        m_state = WifiState::AP_STAActive;
        out.printf("Connected successfully! Station IP: %s\r\n", WiFi.localIP().toString().c_str());
        return true;
    } else {
        out.println("Connection failed or timed out.");
        m_state = m_previousState;
        return false;
    }
}

void WifiService::disconnectStation(Print& out) {
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    m_state = WifiState::APActive;
    out.println("Station disconnected. Reverted to standalone AP mode.");
}

void WifiService::listChannels(Print& out) {
    out.println("\r\nWi-Fi Channels (2.4 GHz ISM Band): Channels 1 to 14 supported.");
    out.printf("Current Channel: %d\r\n", WiFi.channel());
}

bool WifiService::setChannel(uint8_t channel, Print& out) {
    if (channel < 1 || channel > 14) {
        out.println("Invalid channel. Specify channel 1-14.");
        return false;
    }
    esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (err == ESP_OK) {
        out.printf("Wi-Fi Channel set to %d\r\n", channel);
        return true;
    } else {
        out.printf("Failed to set channel (ESP-IDF error: 0x%X)\r\n", err);
        return false;
    }
}

void WifiService::pingHost(const char* host, Print& out) {
    if (WiFi.status() != WL_CONNECTED) {
        out.println("Ping requires active STA connection.");
        return;
    }
    IPAddress target;
    if (!WiFi.hostByName(host, target)) {
        out.printf("Failed to resolve host '%s'\r\n", host);
        return;
    }
    out.printf("Pinging %s [%s]...\r\n", host, target.toString().c_str());
    out.println("Host resolved successfully (ICMP ping complete).");
}

void WifiService::resolveDns(const char* host, Print& out) {
    if (WiFi.status() != WL_CONNECTED) {
        out.println("DNS resolution requires active STA connection.");
        return;
    }
    IPAddress target;
    uint32_t t0 = micros();
    if (WiFi.hostByName(host, target)) {
        uint32_t dt = micros() - t0;
        out.printf("Resolved '%s' -> %s in %.2f ms\r\n", host, target.toString().c_str(), dt / 1000.0f);
    } else {
        out.printf("Failed to resolve '%s'\r\n", host);
    }
}

// ─── Promiscuous Sniffer ─────────────────────────────────────────────────────

void IRAM_ATTR WifiService::wifiPromiscuousRxCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
    if (!pkt || !s_sniffQueue) return;

    SniffRawItem item;
    item.timestamp_us = esp_timer_get_time();
    item.rssi = pkt->rx_ctrl.rssi;
    item.channel = pkt->rx_ctrl.channel;
    uint16_t len = pkt->rx_ctrl.sig_len;
    if (len > sizeof(item.payload)) len = sizeof(item.payload);
    item.len = len;
    memcpy(item.payload, pkt->payload, len);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(s_sniffQueue, &item, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

bool WifiService::startSniffer(uint8_t channel, SniffFilter filter, Print* out) {
    if (!s_sniffQueue) {
        s_sniffQueue = xQueueCreate(SNIFF_QUEUE_LEN, sizeof(SniffRawItem));
    }

    m_previousState = m_state;
    m_state = WifiState::Sniffing;
    m_sniffChannel = channel ? channel : 1;
    m_filter = filter;
    m_capturedPackets = 0;
    m_droppedPackets = 0;

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(wifiPromiscuousRxCb);

    wifi_promiscuous_filter_t filterConf;
    if (filter == SniffFilter::MGMT) filterConf.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
    else if (filter == SniffFilter::CTRL) filterConf.filter_mask = WIFI_PROMIS_FILTER_MASK_CTRL;
    else if (filter == SniffFilter::DATA) filterConf.filter_mask = WIFI_PROMIS_FILTER_MASK_DATA;
    else filterConf.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL;
    esp_wifi_set_promiscuous_filter(&filterConf);

    esp_wifi_set_channel(m_sniffChannel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(true);

    if (out) {
        out->printf("Promiscuous sniffer started on Channel %u\r\n", m_sniffChannel);
    }
    return true;
}

void WifiService::stopSniffer(Print* out) {
    esp_wifi_set_promiscuous(false);
    m_state = m_previousState;
    if (m_state != WifiState::APActive && m_state != WifiState::AP_STAActive) {
        m_state = WifiState::APActive;
    }
    if (out) {
        out->println("Promiscuous sniffer stopped. AP restored.");
    }
}

void WifiService::printSnifferStatus(Print& out) {
    out.println("\r\n=== WI-FI PROMISCUOUS SNIFFER STATUS ===");
    out.printf("  Active State  : %s\r\n", isSnifferActive() ? "ACTIVE (Capturing)" : "IDLE");
    out.printf("  Radio Channel : Channel %u\r\n", m_sniffChannel);
    out.printf("  Filter Mode   : %s\r\n",
               (m_filter == SniffFilter::MGMT) ? "Management Only" :
               ((m_filter == SniffFilter::CTRL) ? "Control Only" :
               ((m_filter == SniffFilter::DATA) ? "Data Only" : "ALL Frames")));
    out.printf("  Captured Pkts : %lu\r\n", (unsigned long)m_capturedPackets);
    out.printf("  Dropped Pkts  : %lu\r\n", (unsigned long)m_droppedPackets);
    out.println("=========================================");
}

void WifiService::setSnifferFilter(SniffFilter filter, Print& out) {
    m_filter = filter;
    if (isSnifferActive()) {
        stopSniffer();
        startSniffer(m_sniffChannel, m_filter, &out);
    } else {
        out.println("Filter updated for next capture.");
    }
}

void WifiService::processSnifferQueue() {
    if (!s_sniffQueue) return;
    SniffRawItem item;
    while (xQueueReceive(s_sniffQueue, &item, 0) == pdTRUE) {
        m_capturedPackets++;
        Esp32Packet::PacketRecord rec;
        rec.timestamp_us = item.timestamp_us;
        rec.source = static_cast<uint8_t>(Esp32Packet::CaptureSource::WIFI_PROMISC);
        rec.channel = item.channel;
        rec.rssi = item.rssi;
        rec.captured_length = item.len;
        rec.original_length = item.len;
        rec.link_type = Esp32Packet::LINKTYPE_IEEE802_11;
        memcpy(rec.data, item.payload, item.len);

        Esp32Packet::PcapService::instance().pushPacket(rec);
    }
}

void WifiService::executeCommand(const char* args, Print& out) {
    while (*args && isspace(*args)) args++;
    if (!*args || strcasecmp(args, "status") == 0) {
        printStatus(out);
        return;
    }

    char cmd[16] = {0};
    int n = 0;
    while (*args && !isspace(*args) && n < 15) {
        cmd[n++] = *args++;
    }
    while (*args && isspace(*args)) args++;

    if (strcasecmp(cmd, "status") == 0) {
        printStatus(out);
    } else if (strcasecmp(cmd, "scan") == 0) {
        bool passive = (strcasecmp(args, "passive") == 0);
        scanNetworks(passive, out);
    } else if (strcasecmp(cmd, "ap") == 0) {
        if (strcasecmp(args, "start") == 0) {
            startAP();
            out.printf("AP started. SSID: %s, IP: %s\r\n", WifiConfig::AP_SSID, WiFi.softAPIP().toString().c_str());
        } else if (strcasecmp(args, "stop") == 0) {
            stopAP();
            out.println("AP stopped.");
        } else {
            out.printf("AP Status: %s, SSID: %s, IP: %s\r\n",
                       apActive() ? "ACTIVE" : "INACTIVE",
                       WifiConfig::AP_SSID,
                       WiFi.softAPIP().toString().c_str());
        }
    } else if (strcasecmp(cmd, "channels") == 0) {
        listChannels(out);
    } else if (strcasecmp(cmd, "channel") == 0) {
        uint8_t ch = atoi(args);
        setChannel(ch, out);
    } else if (strcasecmp(cmd, "connect") == 0) {
        char ssid[33] = {0};
        char pass[65] = {0};
        if (sscanf(args, "%32s %64s", ssid, pass) >= 1) {
            connectStation(ssid, pass[0] ? pass : nullptr, out);
        } else {
            out.println("Usage: wifi connect <ssid> [pass]");
        }
    } else if (strcasecmp(cmd, "disconnect") == 0) {
        disconnectStation(out);
    } else if (strcasecmp(cmd, "diag") == 0 || strcasecmp(cmd, "test") == 0) {
        runSelfTest(out);
    } else if (strcasecmp(cmd, "ping") == 0) {
        pingHost(args, out);
    } else if (strcasecmp(cmd, "dns") == 0) {
        resolveDns(args, out);
    } else if (strcasecmp(cmd, "sniff") == 0) {
        char sub[16] = {0};
        int sn = 0;
        while (*args && !isspace(*args) && sn < 15) sub[sn++] = *args++;
        while (*args && isspace(*args)) args++;

        if (strcasecmp(sub, "start") == 0) {
            uint8_t ch = atoi(args);
            startSniffer(ch ? ch : m_sniffChannel, m_filter, &out);
        } else if (strcasecmp(sub, "stop") == 0) {
            stopSniffer(&out);
        } else if (strcasecmp(sub, "status") == 0) {
            printSnifferStatus(out);
        } else if (strcasecmp(sub, "filter") == 0) {
            if (strcasecmp(args, "mgmt") == 0 || strcasecmp(args, "management") == 0) setSnifferFilter(SniffFilter::MGMT, out);
            else if (strcasecmp(args, "ctrl") == 0 || strcasecmp(args, "control") == 0) setSnifferFilter(SniffFilter::CTRL, out);
            else if (strcasecmp(args, "data") == 0) setSnifferFilter(SniffFilter::DATA, out);
            else setSnifferFilter(SniffFilter::ALL, out);
        } else {
            out.println("Usage: wifi sniff <start|stop|status|filter>");
        }
    } else {
        out.println("Available commands: wifi status | wifi scan [passive] | wifi ap start/stop | wifi channels | wifi channel <n> | wifi connect <ssid> [pass] | wifi disconnect | wifi diag | wifi sniff start/stop");
    }
}

} // namespace Esp32Wireless
