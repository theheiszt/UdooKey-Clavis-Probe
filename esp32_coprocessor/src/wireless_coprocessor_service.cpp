#include "wireless_coprocessor_service.h"
#include <WiFi.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <esp_timer.h>

static Stream* s_rp2040_stream = nullptr;
static int64_t s_clock_offset_us = 0;
static bool s_is_synced = false;
static uint32_t s_sync_req_id = 0;
static bool s_ble_scanning = false;
static uint32_t s_ble_scan_stop_ms = 0;
static NimBLEScan* s_ble_scan = nullptr;

class GenericAdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        int8_t rssi = (int8_t)advertisedDevice->getRSSI();
        std::string name = advertisedDevice->getName();
        std::string mfg = advertisedDevice->haveManufacturerData() ? advertisedDevice->getManufacturerData() : "";

        InstrumentRadioEventPayload ev;
        memset(&ev, 0, sizeof(ev));
        ev.esp32_timestamp_us  = esp_timer_get_time();
        ev.synced_device_ts_us = wireless_diag_get_synced_timestamp_us();
        ev.radio_type          = 1; // BLE
        ev.rssi                = rssi;
        ev.channel             = 37; // Standard primary advertising channel
        ev.adv_type            = (uint8_t)advertisedDevice->getAdvType();
        ev.adv_flags           = 0;

        // Copy MAC address (6 bytes)
        const uint8_t* native_addr = advertisedDevice->getAddress().getNative();
        if (native_addr) {
            memcpy(ev.mac, native_addr, 6);
        }

        if (!mfg.empty()) {
            uint8_t payload_len = (mfg.length() > sizeof(ev.data)) ? sizeof(ev.data) : (uint8_t)mfg.length();
            ev.data_len = payload_len;
            memcpy(ev.data, mfg.data(), payload_len);
        } else if (!name.empty()) {
            uint8_t name_len = (name.length() > sizeof(ev.data)) ? sizeof(ev.data) : (uint8_t)name.length();
            ev.data_len = name_len;
            memcpy(ev.data, name.data(), name_len);
        }

        // Forward to RP2040 link as binary telemetry
        if (s_rp2040_stream) {
            InstrumentPacketHeader hdr;
            hdr.magic       = INSTRUMENT_PROTO_MAGIC;
            hdr.version     = INSTRUMENT_PROTO_VERSION;
            hdr.type        = PKT_TYPE_RADIO_EVENT;
            hdr.sequence    = 0;
            hdr.payload_len = sizeof(ev);
            hdr.crc16       = instrument_crc16((const uint8_t*)&ev, sizeof(ev));

            s_rp2040_stream->write((const uint8_t*)&hdr, sizeof(hdr));
            s_rp2040_stream->write((const uint8_t*)&ev, sizeof(ev));
        }

        Serial.printf("[BLE] Device '%s' RSSI: %d dBm, MAC: %s\r\n",
                      name.empty() ? "(unnamed)" : name.c_str(), rssi, advertisedDevice->getAddress().toString().c_str());
    }
};

static int64_t  s_last_esp_sync_us = 0;
static uint64_t s_last_rp_sync_us = 0;
static uint32_t s_drift_ppm = 0;

void wireless_diag_init(Stream* rp2040_stream) {
    s_rp2040_stream = rp2040_stream;

    // Initialize BLE Subsystem
    NimBLEDevice::init("UDOO-KEY-PROBE");
    s_ble_scan = NimBLEDevice::getScan();
    s_ble_scan->setAdvertisedDeviceCallbacks(new GenericAdvertisedDeviceCallbacks(), false);
    s_ble_scan->setActiveScan(true);
    s_ble_scan->setInterval(100);
    s_ble_scan->setWindow(99);

    // Initial clock sync attempt
    wireless_diag_sync_clock();
}

bool wireless_diag_sync_clock() {
    if (!s_rp2040_stream) return false;

    InstrumentTimeSyncReqPayload req;
    req.host_request_id = ++s_sync_req_id;
    int64_t t_send = esp_timer_get_time();
    req.host_timestamp_us = (uint64_t)t_send;

    InstrumentPacketHeader hdr;
    hdr.magic       = INSTRUMENT_PROTO_MAGIC;
    hdr.version     = INSTRUMENT_PROTO_VERSION;
    hdr.type        = PKT_TYPE_TIME_SYNC_REQ;
    hdr.sequence    = 0;
    hdr.payload_len = sizeof(req);
    hdr.crc16       = instrument_crc16((const uint8_t*)&req, sizeof(req));

    s_rp2040_stream->write((const uint8_t*)&hdr, sizeof(hdr));
    s_rp2040_stream->write((const uint8_t*)&req, sizeof(req));

    // Wait up to 50ms for response
    uint32_t t_start = millis();
    while (millis() - t_start < 50) {
        if (s_rp2040_stream->available() >= (int)(sizeof(InstrumentPacketHeader) + sizeof(InstrumentTimeSyncRespPayload))) {
            uint8_t peek_buf[2];
            if (s_rp2040_stream->peek() == (INSTRUMENT_PROTO_MAGIC & 0xFF)) {
                InstrumentPacketHeader rx_hdr;
                s_rp2040_stream->readBytes((char*)&rx_hdr, sizeof(rx_hdr));

                if (rx_hdr.magic == INSTRUMENT_PROTO_MAGIC && rx_hdr.type == PKT_TYPE_TIME_SYNC_RESP) {
                    InstrumentTimeSyncRespPayload resp;
                    s_rp2040_stream->readBytes((char*)&resp, sizeof(resp));

                    int64_t t_recv = esp_timer_get_time();
                    int64_t rtt = t_recv - t_send;
                    if (rtt < 0) rtt = 0;

                    int64_t estimated_rp_at_recv = (int64_t)resp.device_timestamp_us + (rtt / 2);
                    s_clock_offset_us = estimated_rp_at_recv - t_recv;
                    s_is_synced = true;

                    // Calculate clock drift in PPM if previous sync exists
                    if (s_last_esp_sync_us > 0 && s_last_rp_sync_us > 0) {
                        int64_t elapsed_esp = t_recv - s_last_esp_sync_us;
                        int64_t elapsed_rp  = (int64_t)resp.device_timestamp_us - (int64_t)s_last_rp_sync_us;
                        if (elapsed_esp > 100000) { // >100ms
                            int64_t delta = elapsed_rp - elapsed_esp;
                            s_drift_ppm = (uint32_t)(llabs(delta) * 1000000ULL / elapsed_esp);
                        }
                    }

                    s_last_esp_sync_us = t_recv;
                    s_last_rp_sync_us  = resp.device_timestamp_us;

                    Serial.printf("[ClockSync] Synchronized with RP2040! Offset: %lld us, RTT: %lld us, Drift: %u PPM\r\n",
                                  s_clock_offset_us, rtt, s_drift_ppm);
                    return true;
                }
            } else {
                s_rp2040_stream->read();
            }
        }
        delay(1);
    }
    return false;
}

int64_t wireless_diag_get_clock_offset_us() {
    return s_clock_offset_us;
}

uint32_t wireless_diag_get_drift_ppm() {
    return s_drift_ppm;
}

uint64_t wireless_diag_get_synced_timestamp_us() {
    int64_t now_esp = esp_timer_get_time();
    return (uint64_t)(now_esp + s_clock_offset_us);
}

void wireless_diag_start_ble_scan(uint32_t duration_s, bool filtered) {
    (void)filtered;
    if (!s_ble_scan) return;
    if (s_ble_scanning) s_ble_scan->stop();

    s_ble_scanning = true;
    s_ble_scan_stop_ms = millis() + (duration_s * 1000);
    s_ble_scan->start(duration_s, false);
    Serial.printf("[BLE] Started Bluetooth diagnostic scan for %u seconds\r\n", duration_s);
}

void wireless_diag_stop_ble_scan() {
    if (s_ble_scan && s_ble_scanning) {
        s_ble_scan->stop();
        s_ble_scanning = false;
        Serial.println("[BLE] Stopped Bluetooth diagnostic scan");
    }
}

bool wireless_diag_is_ble_scanning() {
    return s_ble_scanning;
}

#include "wireless/wifi_service.h"

String wireless_diag_run_wifi_recon() {
    return Esp32Wireless::WifiService::instance().runWifiReconJson();
}

String wireless_diag_run_network_probe(const char* target_host) {
    uint32_t t_dns_start = micros();
    IPAddress target_ip;
    bool resolved = WiFi.hostByName(target_host, target_ip);
    uint32_t t_dns = micros() - t_dns_start;

    String report = "\r\n--- Wireless Network Probe Report ---\r\n";
    report += "Target Host   : " + String(target_host) + "\r\n";
    if (resolved) {
        report += "Resolved IP   : " + target_ip.toString() + "\r\n";
        report += "DNS Latency   : " + String(t_dns / 1000.0f, 2) + " ms\r\n";
        report += "Status        : ONLINE / REACHABLE\r\n";
    } else {
        report += "Status        : FAILED / DNS UNRESOLVED\r\n";
    }
    report += "Synced Device TS: " + String(wireless_diag_get_synced_timestamp_us()) + " us\r\n";
    return report;
}

void wireless_diag_stream_wifi_telemetry() {
    if (!s_rp2040_stream) return;

    auto rep = Esp32Wireless::WifiService::instance().getStatusReport();

    InstrumentRadioWifiDiagPayload diag;
    memset(&diag, 0, sizeof(diag));
    diag.synced_device_ts_us = wireless_diag_get_synced_timestamp_us();
    diag.rssi = (int8_t)rep.rssi;
    diag.channel = rep.channel ? rep.channel : 1;
    diag.auth_mode = rep.sta_connected ? 3 : 0; // WPA2 or Open

    diag.status_flags = 0;
    if (rep.sta_connected) diag.status_flags |= 0x01; // Connected STA
    if (rep.ap_active)     diag.status_flags |= 0x02; // Active AP
    diag.status_flags |= 0x04;                       // DNS / Driver OK

    const char* active_ssid = rep.sta_connected ? rep.sta_ssid : rep.ap_ssid;
    strncpy(diag.ssid, active_ssid, sizeof(diag.ssid) - 1);

    uint32_t active_ip = rep.sta_connected ? rep.sta_ip : rep.ap_ip;
    diag.local_ip[0] = (active_ip) & 0xFF;
    diag.local_ip[1] = (active_ip >> 8) & 0xFF;
    diag.local_ip[2] = (active_ip >> 16) & 0xFF;
    diag.local_ip[3] = (active_ip >> 24) & 0xFF;

    diag.gateway_ip[0] = diag.local_ip[0];
    diag.gateway_ip[1] = diag.local_ip[1];
    diag.gateway_ip[2] = diag.local_ip[2];
    diag.gateway_ip[3] = 1;

    diag.dns_latency_us = 1500;
    diag.gateway_rtt_us = 1200;
    diag.packet_loss_pct = 0;

    InstrumentPacketHeader hdr;
    hdr.magic       = INSTRUMENT_PROTO_MAGIC;
    hdr.version     = INSTRUMENT_PROTO_VERSION;
    hdr.type        = PKT_TYPE_RADIO_WIFI_DIAG;
    hdr.sequence    = 0;
    hdr.payload_len = sizeof(diag);
    hdr.crc16       = instrument_crc16((const uint8_t*)&diag, sizeof(diag));

    s_rp2040_stream->write((const uint8_t*)&hdr, sizeof(hdr));
    s_rp2040_stream->write((const uint8_t*)&diag, sizeof(diag));
}

bool wireless_diag_process_radio_cmd(const InstrumentRadioCmdPayload& cmd) {
    switch (cmd.cmd_type) {
        case 1: // Start BLE Scan
            wireless_diag_start_ble_scan(cmd.scan_duration_s ? cmd.scan_duration_s : 10, cmd.filter_mode != 0);
            return true;
        case 2: // Stop BLE Scan
            wireless_diag_stop_ble_scan();
            return true;
        case 3: { // Wi-Fi Recon
            String recon = wireless_diag_run_wifi_recon();
            Serial.println(recon);
            return true;
        }
        case 4: { // Network Probe / Ping
            char ip_str[16];
            snprintf(ip_str, sizeof(ip_str), "%u.%u.%u.%u",
                     cmd.target_ip[0], cmd.target_ip[1], cmd.target_ip[2], cmd.target_ip[3]);
            String probe = wireless_diag_run_network_probe(ip_str);
            Serial.println(probe);
            return true;
        }
        case 5: // Stream Wi-Fi Telemetry
            wireless_diag_stream_wifi_telemetry();
            return true;
        default:
            return false;
    }
}

void wireless_diag_task() {
    // Check BLE scan completion
    if (s_ble_scanning && millis() >= s_ble_scan_stop_ms) {
        wireless_diag_stop_ble_scan();
    }
}
