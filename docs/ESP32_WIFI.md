# UDOO KEY — ESP32 Wi-Fi Subsystem Reference Manual

## 1. Architectural Overview

The UDOO KEY integrates an **ESP32-D0WD-V3 / D0WDQ6** dual-core Xtensa LX6 coprocessor alongside a **Raspberry Pi RP2040** microcontroller on a unified PCB. In this architecture:
- The **RP2040** is the primary real-time deterministic sampling engine (Logic Analyzer, PIO, DMA, pulse capture).
- The **ESP32** is the wireless connectivity and auxiliary communication coprocessor (Wi-Fi 802.11 b/g/n, BLE 4.2/5.0, IR transceiver, 2.4 GHz nRF24L01+, PCAP promiscuous sniffer, Web dashboard, and HTTP/WebSocket server).

Inter-MCU communication is established via the dedicated factory hardware UART:
- **RP2040 GP0 (TX)** $\rightarrow$ **ESP32 IO22 (RX)**
- **RP2040 GP1 (RX)** $\leftarrow$ **ESP32 IO19 (TX)**
- Baud rate: **1,000,000 baud** (8N1).

```
                      +---------------------------------------+
                      |           UDOO KEY PCB                |
                      |                                       |
                      |   +-------------------------------+   |
                      |   |      RP2040 MCU Engine        |   |
                      |   |   (PIO Logic Analyzer, DMA)   |   |
                      |   +---------------+---------------+   |
                      |                   | GP0 (TX) / GP1 (RX)
                      |                   | 1,000,000 Baud
                      |                   v IO22 (RX) / IO19 (TX)
                      |   +-------------------------------+   |
                      |   |    ESP32-D0WD-V3 Coprocessor  |   |
                      |   |  (No PSRAM, 4MB SPI Flash)    |   |
                      |   +---------------+---------------+   |
                      +-------------------|-------------------+
                                          |
                      +-------------------+-------------------+
                      |       Esp32Wireless::WifiService      |
                      |       (Single Authoritative Owner)    |
                      +---+---------------+---------------+---+
                          |               |               |
              +-----------+--+     +------+-------+     +-+-------------+
              | 802.11 b/g/n |     | Async Web    |     | Promiscuous   |
              | SoftAP Mode  |     | Server / WS  |     | Sniffer Queue |
              | (Channel 1)  |     | (Port 80)    |     | (FreeRTOS)    |
              +--------------+     +--------------+     +---------------+
```

### Single Ownership Model
To eliminate race conditions and corrupted RF state transitions, **`Esp32Wireless::WifiService` is the sole authoritative owner of the ESP32 Wi-Fi hardware**. No other task, ISR, or module may invoke `WiFi.mode()`, `WiFi.begin()`, `WiFi.softAP()`, or `esp_wifi_*` directly. All network state changes are governed by the `WifiService` state machine.

---

## 2. Standalone Access Point (AP) Configuration

The default operating mode for the instrument is a standalone Wi-Fi Access Point, enabling headless diagnostics from any laptop, mobile device, or the desktop companion application.

| Parameter | Authoritative Value | Source Definition |
|---|---|---|
| **SSID** | `UDOO-BIT-PIRATE-SCOPE` | `WifiConfig::AP_SSID` |
| **Password** | `udoopass123` (WPA2-PSK) | `WifiConfig::AP_PASSWORD` |
| **IPv4 Address** | `192.168.4.1` | `WifiConfig::AP_IP` |
| **Subnet Mask** | `255.255.255.0` | `WifiConfig::AP_NETMASK` |
| **Gateway** | `192.168.4.1` | `WifiConfig::AP_GW` |
| **802.11 Channel** | `1` (2.412 GHz) | `WifiConfig::AP_CHANNEL` |
| **Hidden SSID** | `false` (Broadcast) | `WifiConfig::AP_HIDDEN` |
| **Max Stations** | `4` clients | `WifiConfig::AP_MAX_CLIENTS` |
| **Power Save** | `WIFI_PS_MIN_MODEM` | Required by ESP-IDF for Wi-Fi / Bluetooth coexistence |
| **TX Power** | `17 dBm` (WIFI_POWER_17dBm) | Clamped to prevent USB power brownout reset |

### Deterministic AP Initialization Sequence
```cpp
// 1. Acquire resource lock from central manager
auto& rm = Esp32Core::ResourceManager::instance();
rm.acquire(Esp32Core::Resource::WIFI_RADIO, Esp32Core::Owner::WIFI_SERVICE);

// 2. Disable NVS flash persistence to avoid flash wear and stale BSSID cache
WiFi.persistent(false);

// 3. Reset internal radio state machine
WiFi.mode(WIFI_MODE_NULL);
delay(50);

// 4. Set explicit AP mode (NOT AP+STA during initial boot)
m_state = WifiState::APStarting;
WiFi.mode(WIFI_AP);
delay(50);

// 5. Configure static IP address lease block
IPAddress ap_ip(192, 168, 4, 1);
IPAddress ap_gw(192, 168, 4, 1);
IPAddress ap_mask(255, 255, 255, 0);
WiFi.softAPConfig(ap_ip, ap_gw, ap_mask);

// 6. Launch SoftAP beaconing
bool apOk = WiFi.softAP("UDOO-BIT-PIRATE-SCOPE", "udoopass123", 1, false, 4);

// 7. Limit Wi-Fi TX power to 17 dBm to eliminate USB brownout spikes
WiFi.setTxPower(WIFI_POWER_17dBm);

// 8. Enable Wi-Fi modem sleep (mandatory in ESP-IDF when Bluetooth is compiled in)
esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

// 9. Verify live hardware state
wifi_mode_t curMode = WIFI_MODE_NULL;
esp_wifi_get_mode(&curMode);
if (apOk && (curMode == WIFI_MODE_AP || curMode == WIFI_MODE_APSTA) && WiFi.softAPIP() != IPAddress(0, 0, 0, 0)) {
    m_state = WifiState::APActive;
} else {
    m_state = WifiState::APError;
}
```

---

## 3. Station (STA) Mode & Coexistence

While Standalone AP is the default boot mode, Station mode can be dynamically activated via CLI or API to connect to an existing lab WLAN:
- When connecting to an external AP while keeping the local AP active, `WifiService` transitions to `WIFI_AP_STA`.
- **Channel Coexistence Rule**: The ESP32 contains a single physical 2.4 GHz transceiver. In `WIFI_AP_STA` mode, the SoftAP automatically switches its operating channel to match the channel of the external AP connected in STA mode.

---

## 4. Wi-Fi Finite State Machine

`WifiService` transitions strictly between defined states:

```
  +--------------+
  |   Disabled   |
  +-------+------+
          | initialize()
          v
  +--------------+
  | Initializing |
  +-------+------+
          |
          v
  +--------------+          softAP() fails
  |  APStarting  |----------------------------------> +---------+
  +-------+------+                                    | APError |
          | softAP() OK                               +----+----+
          v                                                |
  +--------------+   healthCheck() watchdog fails          |
  |   APActive   |<----------------------------------------+ (Recovery)
  +---+------+---+
      |      |
      |      +------------+ connectStation()
      |                   v
      |            +--------------+
      |            | STAConnecting|
      |            +------+-------+
      |                   | WL_CONNECTED
      |                   v
      |            +--------------+
      |            | AP_STAActive |
      |            +--------------+
      |
      +------------+ startSniffer()
      |            v
      |      +------------+
      |      |  Sniffing  |
      |      +------------+
      |
      +------------+ scanNetworks()
                   v
             +------------+
             |  Scanning  |
             +------------+
```

---

## 5. Serial CLI Command Reference

All Wi-Fi commands are accessible via the USB Serial terminal (115200 baud):

| Command | Arguments | Description | Example |
|---|---|---|---|
| `wifi status` | None | Displays current state, mode, IP, SSID, channel, stations, and uptime | `wifi status` |
| `wifi ap start` | None | Manually restarts the standalone SoftAP | `wifi ap start` |
| `wifi ap stop` | None | Disables the standalone SoftAP | `wifi ap stop` |
| `wifi scan` | `[passive]` | Performs active or passive 2.4 GHz channel survey | `wifi scan` |
| `wifi channels` | None | Lists available 2.4 GHz channels and current operating channel | `wifi channels` |
| `wifi channel <n>` | `<1-13>` | Changes operating radio channel | `wifi channel 6` |
| `wifi connect` | `<ssid> [pass]` | Connects to external WLAN in AP+STA coexistence mode | `wifi connect LabNet secret123` |
| `wifi disconnect` | None | Disconnects STA mode and returns to standalone AP | `wifi disconnect` |
| `wifi sniff start` | `[ch]` | Starts promiscuous 802.11 frame capture on channel | `wifi sniff start 1` |
| `wifi sniff stop` | None | Stops promiscuous capture and restores AP | `wifi sniff stop` |
| `wifi sniff filter` | `<all\|mgmt\|ctrl\|data>` | Filters promiscuous capture frame types | `wifi sniff filter mgmt` |
| `wifi sniff status` | None | Reports captured and dropped packet statistics | `wifi sniff status` |
| `wifi diag` | None | Executes comprehensive hardware self-test suite | `wifi diag` |
| `wifi ping <host>` | `<ip or domain>` | ICMP echo probe to remote IP | `wifi ping 1.1.1.1` |
| `wifi dns <host>` | `<hostname>` | Resolves DNS query to IPv4 | `wifi dns pool.ntp.org` |

---

## 6. HTTP REST API Reference

The ESP32 runs an asynchronous HTTP server on port 80.

### 6.1 `GET /`
- **Description**: Returns the responsive HTML5/JavaScript dashboard for instrument control and real-time telemetry.
- **Content-Type**: `text/html`

### 6.2 `GET /api/status`
- **Description**: Compact instrument system status probe. Used by companion applications for automatic discovery.
- **Response Format (`application/json`)**:
```json
{
  "device": "UDOO KEY",
  "subsystem": "ESP32",
  "status": "OK",
  "wifi": {
    "mode": "AP",
    "state": "AP_ACTIVE",
    "ssid": "UDOO-BIT-PIRATE-SCOPE",
    "ip": "192.168.4.1",
    "channel": 1,
    "clients": 0
  },
  "uptime_ms": 14250,
  "free_heap": 218940
}
```

### 6.3 `GET /api/wifi`
- **Description**: Detailed Wi-Fi network and RF configuration report.
- **Response Format (`application/json`)**:
```json
{
  "mode": "AP",
  "state": "AP_ACTIVE",
  "ssid": "UDOO-BIT-PIRATE-SCOPE",
  "ip": "192.168.4.1",
  "channel": 1,
  "mac": "24:6F:28:B2:1A:80",
  "clients": 1,
  "max_clients": 4
}
```

### 6.4 `GET /api/system`
- **Description**: Hardware silicon details, CPU frequency, and memory usage.
- **Response Format (`application/json`)**:
```json
{
  "device": "UDOO KEY BASIC",
  "chip": "ESP32-D0WDQ6",
  "cpu_freq_mhz": 240,
  "free_heap": 218450,
  "flash_size_bytes": 4194304,
  "uptime_ms": 14250
}
```

### 6.5 `GET /api/capabilities`
- **Description**: Peripheral feature matrix exposed by the firmware.
- **Response Format (`application/json`)**:
```json
{
  "wifi": true,
  "ble": true,
  "ir": true,
  "nrf24": true,
  "logic_scope": true,
  "bit_pirate": true,
  "swd": true,
  "uart": true,
  "spi": true,
  "i2c": true
}
```

---

## 7. WebSocket API Reference

The WebSocket server listens on path `/ws`. Clients exchange bi-directional JSON messages.

### Incoming Command Frame
```json
{
  "cmd": "wifi",
  "action": "status"
}
```
Available actions: `"status"`, `"scan"`, `"recon"`, `"diag"`.

### Terminal Command Frame
```json
{
  "cmd": "term",
  "text": "wifi status\r\n"
}
```

### Outgoing Event Frame
```json
{
  "event": "wifi_status",
  "state": "AP_ACTIVE",
  "ssid": "UDOO-BIT-PIRATE-SCOPE",
  "ip": "192.168.4.1",
  "channel": 1,
  "clients": 1
}
```

---

## 8. RP2040 UART Telemetry Transport

The ESP32 periodically synchronizes its network status with the RP2040 over the factory 1,000,000 baud UART link:
- Framed with prefix `[WIFI-TELEMETRY] `
- Encodes JSON payload:
```json
{
  "state": "AP_ACTIVE",
  "ip": "192.168.4.1",
  "ssid": "UDOO-BIT-PIRATE-SCOPE",
  "channel": 1,
  "clients": 0
}
```
The RP2040 forwards this telemetry over USB CDC to the desktop application, enabling the desktop companion to immediately identify when the ESP32 Wi-Fi AP is operational without requiring a prior network connection.

---

## 9. Promiscuous Sniffer Subsystem

The promiscuous sniffer operates under `Esp32Wireless::WifiService` and streams 802.11 packets into the PCAP subsystem:
1. `startSniffer(channel, filter)` puts the radio into promiscuous reception mode via `esp_wifi_set_promiscuous(true)`.
2. Packet header and payload up to 256 bytes are pushed into a high-speed FreeRTOS queue (`s_sniffQueue`) from the RX callback.
3. In `loop()`, `processSnifferQueue()` drains the items, encapsulates them in `Esp32Packet::PacketRecord` (Link type `LINKTYPE_IEEE802_11`), and routes them to `PcapService` for Wireshark streaming or ring-buffer logging.
4. Calling `stopSniffer()` restores standard AP operation and re-enables beacon frames.

---

## 10. Troubleshooting & Diagnostic Tree

```
                      +-----------------------------+
                      | SSID not visible on client  |
                      +--------------+--------------+
                                     |
                                     v
                   Check USB Serial at 115200 baud
                                     |
               +---------------------+---------------------+
               |                                           |
         Bootloop detected                          Boot banner OK
    (rst:0x3 / PSRAM probe abort)            ([WIFI] AP start ... OK)
               |                                           |
               v                                           v
    Ensure BOARD_HAS_PSRAM is NOT         Verify client 2.4 GHz capability
    defined; Board is UDOO KEY BASIC      (ESP32 does NOT support 5 GHz).
    (ESP32-D0WD-V3, 4MB Flash).           Check Channel 1 interference.
```

---

## 11. Physical Hardware Acceptance Test Procedure

1. **Power Supply**: Connect UDOO KEY to host via USB-C. Ensure stable 5V rail ($\ge 500\text{ mA}$).
2. **Serial Verification**:
   - Open serial terminal to ESP32 port (`COM6` / `/dev/ttyUSB0`) at 115200 baud.
   - Press Reset button (SW3). Confirm output:
     ```text
     [BOOT] UART transport ........ OK
     [BOOT] Resource manager ...... OK
     [WIFI] Initializing
     [WIFI] Mode .................. AP
     [WIFI] SSID .................. UDOO-BIT-PIRATE-SCOPE
     [WIFI] AP IP ................. 192.168.4.1
     [WIFI] AP start .............. OK
     [WIFI] HTTP server ........... OK
     [WIFI] WebSocket ............. OK
     ```
3. **RF Beacon Scan**:
   - On a host PC, tablet, or smartphone, open Wi-Fi settings.
   - Scan for SSID: `UDOO-BIT-PIRATE-SCOPE`.
   - Connect using password: `udoopass123`.
   - Verify DHCP assignment: Client assigned `192.168.4.x` (e.g., `192.168.4.2`).
4. **HTTP Dashboard & REST Validation**:
   - In browser, navigate to `http://192.168.4.1/`. Confirm web UI renders.
   - Request `http://192.168.4.1/api/status`. Confirm JSON status response with `"status":"OK"`.
5. **Desktop C++ Integration**:
   - Launch `udoo-key-app.exe`.
   - Observe automatic connection status indicating ESP32 reachable at `192.168.4.1`.

---

## 12. Known Limitations

1. **2.4 GHz Only**: The ESP32 hardware supports only IEEE 802.11 b/g/n (2.4 GHz). It does not transmit on 5 GHz bands.
2. **Channel Coupling in AP+STA**: Because the ESP32 has a single physical radio, connecting to an external router on Channel 6 will force the local SoftAP to shift from Channel 1 to Channel 6.
3. **Promiscuous Sniffer Beacon Pause**: When raw promiscuous capture is enabled, standard SoftAP beacon transmissions are suspended to maximize capture throughput.

---

## 13. PlatformIO Toolchain & Build Specification

- **Platform**: `espressif32 @ 6.9.0`
- **Framework**: `arduino`
- **Target Board**: `esp32dev` (Generic ESP32)
- **Silicon Target**: `ESP32-D0WD-V3 / D0WDQ6` (Dual core, 240 MHz, 520 KB SRAM, **NO PSRAM**)
- **Flash Size**: 4MB SPI Flash
- **Partition Scheme**: `huge_app.csv` (3MB App, 1MB SPIFFS/NVS)
- **Upload Speed**: `460800`

---

## 14. Security Considerations

> [!WARNING]
> The default SSID `UDOO-BIT-PIRATE-SCOPE` and passphrase `udoopass123` are intended solely for local lab instrumentation and prototyping. For production or sensitive industrial bench testing, change the credentials in `esp32_coprocessor/src/wireless/wifi_config.h` before flashing.
