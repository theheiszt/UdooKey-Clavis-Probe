# UDOO KEY — ESP32 Wireless Telemetry & Radio Diagnostics

**Document Version:** 1.0.0  
**Date:** 2026-10-03  
**Status:** Canonical Wireless Subsystem Documentation  
**Coprocessor:** Espressif ESP32-WROVER-E (Dual-Core Xtensa LX6 @ 240 MHz)  

---

## 1. Subsystem Architecture

The ESP32-WROVER-E on the UDOO KEY operates as a dual-role communications engine and 2.4 GHz RF diagnostic probe. It offloads all networking, BLE scanning, and HTML5 Web UI serving from the real-time RP2040 core, streaming structured telemetry over the 1,000,000 baud inter-MCU UART link.

```
┌────────────────────────────────────────────────────────┐
│             ESP32-WROVER-E (240 MHz, 4MB PSRAM)        │
│                                                        │
│  ┌────────────────────────┐  ┌───────────────────────┐ │
│  │   BLE Apple Scanner    │  │  802.11 Wi-Fi Engine  │ │
│  │  (NimBLE 1.4.1 Stack)  │  │  (Recon & Latency)    │ │
│  └───────────┬────────────┘  └───────────┬───────────┘ │
│              │                           │             │
│              ▼                           ▼             │
│    Apple Frame Extractor        Network Health Prober  │
│   (AirDrop, AirPods, FindMy)    (DNS, HTTP Handshake)  │
│              │                           │             │
│              └─────────────┬─────────────┘             │
│                            │                           │
│                            ▼                           │
│            Ping-Pong Microsecond Synchronizer          │
│            (Maps ESP32 ts ➔ RP2040 Hardware Clock)     │
│                            │                           │
└────────────────────────────┼───────────────────────────┘
                             │ PKT_TYPE_RADIO_EVENT (0x2A)
                             ▼ (1 Mbps Factory UART)
                     Raspberry Pi RP2040
```

---

## 2. Apple Bluetooth Low Energy (BLE) Frame Extraction

The ESP32 passive/active scanner continuously filters for packets advertising **Apple Inc. Company ID `0x004C`** in the Manufacturer Specific Data field (AD Type `0xFF`).

### 2.1 Apple Protocol Types
The third byte of the manufacturer data payload identifies the Apple Continuity subsystem:

| Type Byte | Apple Subsystem | Diagnostic Value & Telemetry Extracted |
| :--- | :--- | :--- |
| **0x05** | **AirDrop** | Active AirDrop availability, session hash, zero-conf presence. |
| **0x07** | **AirPods / Proximity Pairing** | Charging case open/lid status, battery levels (left/right/case), model ID. |
| **0x09** | **AirPlay Destination** | AirPlay receiver advertisements, screen mirroring target discovery. |
| **0x10** | **Nearby Action / Continuity** | Continuity Camera, Universal Control, Handoff activity, Wi-Fi password sharing. |
| **0x12** | **Find My Offline Beacon** | Apple Find My / AirTag encrypted public keys, status byte, battery level. |

### 2.2 Telemetry Packet Formatting (`PKT_TYPE_RADIO_EVENT`)
Whenever an Apple BLE packet is detected, the ESP32 constructs an `InstrumentRadioEventPayload`:
* `esp32_timestamp_us`: Local microsecond timestamp from `esp_timer_get_time()`.
* `synced_device_ts_us`: Normalized microsecond timestamp aligned with the RP2040 hardware timer.
* `radio_type`: `1` (BLE).
* `rssi`: Signal strength in dBm (-100 to 0 dBm).
* `channel`: Primary advertising channel (37, 38, or 39).
* `apple_type`: Subsystem code (0x05, 0x07, 0x10, 0x12).
* `mac[6]`: Bluetooth Device Address.
* `data[32]`: Raw manufacturer payload bytes.

---

## 3. Wi-Fi Reconnaissance & Network Probes

### 3.1 802.11 Channel Reconnaissance (`apple wifi`)
* Scans channels 1 through 14 across the 2.4 GHz band.
* Records SSID, BSSID, RSSI, channel width, and encryption type (WPA2/WPA3).
* Formats results as JSON and streams them to the PC companion app and HTML5 web console.

### 3.2 Automated Network Probes (`apple probe [host]`)
* **DNS Resolution Latency:** Measures time to resolve target domains (default `captive.apple.com`).
* **Reachability Verification:** Confirms whether the Apple device has a valid gateway and active Internet connectivity.

---

## 4. Interactive CLI Commands

When connected to the ESP32 USB serial console (COM6 @ 115200 baud) or the WebSocket web terminal:

| Command | Action |
| :--- | :--- |
| `apple` | Displays Apple Diagnostics status, BLE state, and clock offset. |
| `apple ble start` | Starts a 15-second BLE scan for Apple Continuity frames. |
| `apple ble stop` | Aborts active BLE scan. |
| `apple sync` | Triggers immediate ping-pong clock synchronization with RP2040. |
| `apple wifi` | Performs full 2.4 GHz Wi-Fi channel scan and returns JSON report. |
| `apple probe [host]` | Performs DNS latency and reachability test to specified host. |
