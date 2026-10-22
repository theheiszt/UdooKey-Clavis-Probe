#ifndef WIRELESS_COPROCESSOR_SERVICE_H
#define WIRELESS_COPROCESSOR_SERVICE_H

#include <Arduino.h>
#include "instrument_protocol.h"

// Initialize Wireless & Time Sync service on ESP32
void wireless_diag_init(Stream* rp2040_stream);

// Performs microsecond time synchronization ping-pong exchange with RP2040
bool wireless_diag_sync_clock();

// Returns calculated clock offset relative to RP2040 hardware timer in microseconds
int64_t wireless_diag_get_clock_offset_us();

// Returns current ESP32 time mapped into RP2040 microsecond timebase
uint64_t wireless_diag_get_synced_timestamp_us();

// Starts background BLE scanner for generic BLE advertisements
void wireless_diag_start_ble_scan(uint32_t duration_s = 10, bool filtered = false);

// Stops active BLE scanning
void wireless_diag_stop_ble_scan();

// Checks if BLE scanner is active
bool wireless_diag_is_ble_scanning();

// Performs 802.11 Wi-Fi recon and populates JSON report
String wireless_diag_run_wifi_recon();

// Performs DNS / network reachability probe
String wireless_diag_run_network_probe(const char* target_host = "one.one.one.one");

// Streams structured Wi-Fi diagnostic telemetry (SSID, RSSI, channel, DNS latency, RTT)
void wireless_diag_stream_wifi_telemetry();

// Processes incoming radio command packet (scan, probe, wifi diag)
bool wireless_diag_process_radio_cmd(const InstrumentRadioCmdPayload& cmd);

// Returns measured clock drift in PPM between ESP32 and RP2040
uint32_t wireless_diag_get_drift_ppm();

// Background task called from main loop
void wireless_diag_task();

#endif // WIRELESS_COPROCESSOR_SERVICE_H
