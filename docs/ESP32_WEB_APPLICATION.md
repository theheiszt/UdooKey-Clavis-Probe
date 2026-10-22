# UDOO KEY — ESP32 SoftAP Web Application Architecture

## 1. Architectural Overview

The UDOO KEY hosts a full-featured web-based diagnostic suite and logic analyzer directly from the onboard ESP32 coprocessor. The ESP32 serves as:

1. **Wi-Fi SoftAP Access Point**: Broadcasting SSID `UDOO-BIT-PIRATE-SCOPE` with WPA2-PSK on 2.4 GHz Channel 1 at static IP `192.168.4.1`.
2. **HTTP Web Server**: Serving the zero-dependency, high-performance HTML5/Canvas single-page application (SPA) stored in PROGMEM (`logic_scope_ui.h`).
3. **WebSocket Server (`/ws`)**: Bi-directional asynchronous control channel for low-latency command dispatch and high-speed binary capture streaming.
4. **Command/Stream Bridge**: Inter-MCU link translator bridging JSON browser messages and `InstrumentProtocol v1.0` binary packets over the 1,000,000 baud factory UART link (RP2040 GP0/GP1 $\leftrightarrow$ ESP32 IO22/IO19).

```text
                               UDOO KEY
┌────────────────────────────────────────────────────────────────────────┐
│                                                                        │
│                          ESP32 COPROCESSOR                             │
│                                                                        │
│   SoftAP (192.168.4.1:80)                                              │
│     │                                                                  │
│     ├── HTTP Server (AsyncWebServer Port 80)                           │
│     │     ├── GET /                     -> Logic Scope Web App         │
│     │     ├── REST API (/api/*)         -> Telemetry & Control         │
│     │     └── Captive Portal Detection  -> Redirect /generate_204      │
│     │                                                                  │
│     └── WebSocket Server (/ws)                                         │
│           ├── Client JSON Commands      -> Arm / Stop / Config         │
│           ├── Client Binary Frames      -> WebLogicCaptureHeader       │
│           └── Telemetry / CLI stream    -> Status & VT100 Terminal     │
│                                                                        │
│                         Inter-MCU Transport                            │
│                 (1,000,000 Baud UART / IO22 RX, IO19 TX)               │
│                                   │                                    │
│                                   ▼                                    │
│                       RP2040 REAL-TIME ENGINE                          │
│                                                                        │
│       GP2..GP9 = 8 Digital Probes (CH0..CH7)                           │
│       GP10     = External Trigger (EXT_TRIG)                           │
│       PIO0     = Circular DMA Logic Acquisition (Up to 100 MSPS)       │
│       DMA      = 64 KiB Ring Buffer in SRAM                            │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Operational Principle: Real Hardware Acquisition

The web application exposes **real hardware functionality exclusively**:
- High-speed acquisition is executed strictly by the RP2040 dual Cortex-M0+ cores using PIO0 and DMA.
- Captured digital samples originate directly from the physical probe pins (GP2 through GP9).
- The ESP32 reassembles incoming 512-byte streaming chunks (`PKT_TYPE_CAPTURE_DATA_CHUNK`) into its 64 KiB buffer and broadcasts them verbatim to connected web clients with the authoritative `WebLogicCaptureHeader`.
- Simulated waveforms, fake demo generators, or placeholder signals are strictly prohibited.

---

## 3. REST API Specification

All REST API endpoints are hosted on port 80:

| Endpoint | Method | Format | Description |
|---|---|---|---|
| `/` | `GET` | `text/html` | Logic Scope Single Page Application |
| `/api/status` | `GET` | `application/json` | System health, Wi-Fi mode, IP, client count, uptime, and free heap |
| `/api/capabilities` | `GET` | `application/json` | Hardware features (8 logic channels, 100 MSPS, ADC, PWM, SPI, I2C, UART) |
| `/api/logic/status` | `GET` | `application/json` | Acquisition state (`IDLE`, `ARMED`, `TRIGGERED`, `READY`), sample rate, total samples, trigger offset, RP2040 link state |
| `/api/logic/config` | `GET` | `application/json` | Current acquisition configuration (rate, sample depth, channels, trigger offset) |
| `/api/logic/arm` | `POST` | `application/json` | Arms the RP2040 acquisition engine |
| `/api/logic/single` | `POST` | `application/json` | Arms for a single capture run |
| `/api/logic/stop` | `POST` | `application/json` | Aborts ongoing acquisition |
| `/api/logic/capture` | `GET` | `application/octet-stream` or `json` | Downloads most recent capture buffer. Appending `?format=json` returns JSON |
| `/api/adc` | `GET` | `application/json` | Live analog voltages from RP2040 ADC0 (LDR), ADC1 (GP27), and ADC2 (GP28) |
| `/api/wifi` | `GET` | `application/json` | Wi-Fi SoftAP telemetry (SSID, channel, MAC address, connected stations) |
| `/api/system` | `GET` | `application/json` | ESP32 CPU frequency, flash size, free heap, and uptime |

### Captive Portal Detection
To streamline immediate connectivity on mobile phones and tablets (iOS, Android, Windows, macOS), the web server intercepts captive portal test requests:
- `/generate_204`
- `/hotspot-detect.html`
- `/canonical.html`
- `/connecttest.txt`
- `/ncsi.txt`

These requests are redirected directly to `http://192.168.4.1/`, immediately launching the Logic Scope user interface.

---

## 4. Memory Architecture & Buffer Management

The ESP32-D0WDQ6 coprocessor operates with internal SRAM (no external PSRAM). To preserve SRAM stability and prevent linker DRAM segment overflow:
- **Static DRAM BSS**: Reserved for core operating system tasks, AsyncTCP network buffers, and FreeRTOS control blocks.
- **Dynamic 64 KiB Capture Buffer**: Stored in heap memory (`malloc(MAX_SAMPLE_BUFFER_SIZE)` where `MAX_SAMPLE_BUFFER_SIZE = 65536`). This prevents `.dram0.bss` exhaustion while providing sufficient memory for the RP2040's full 64 KiB buffer.
- **Zero-Copy Flash Storage**: The entire HTML5/CSS/JavaScript single-page application is stored in SPI Flash PROGMEM (`logic_scope_ui.h`), consuming 0 bytes of RAM at rest.

---

## 5. Multi-Client WebSocket Distribution

When a new capture is completed:
1. RP2040 sends `PKT_TYPE_CAPTURE_INFO_RESP` containing capture metadata.
2. RP2040 streams 512-byte payload chunks via `PKT_TYPE_CAPTURE_DATA_CHUNK`.
3. ESP32 reassembles the chunks into `last_sample_buffer`.
4. Upon receiving the final chunk (`chunk_index + 1 == total_chunks`), the ESP32 prefixes the 32-byte `WebLogicCaptureHeader` and broadcasts the entire binary frame via `ws.binaryAll()`.
5. Simultaneously, a lightweight JSON notification (`{"type": "capture_ready", "capture_id": ...}`) is broadcast via `ws.textAll()`.
