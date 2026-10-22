# UDOO KEY Central Resource Map & Hardware Arbiter

**Document ID:** DOC-RESOURCE-MASTER-20261005  
**Status:** AUTHORITATIVE RESOURCE ALLOCATION MATRIX  
**Date:** 2026-10-05  
**Authority:** Master Specification Section 37 & Section 61  

---

## 1. Hardware Resource Allocation & Ownership Matrix

| Resource Identifier | Hardware Block | Dedicated Owner | Sharing Policy | Mutex / Conflict Protection |
| :--- | :--- | :--- | :--- | :--- |
| **RP2040 GP0 / GP1** | UART0 Peripheral | Inter-MCU Transport | **EXCLUSIVE** | Locked at boot. Barred from user probes. |
| **RP2040 GP2..GP9** | GPIO / PIO0 Pins | Multi-Mode Probe Bus | **MUTUALLY EXCLUSIVE** | Arbitrated by `ModeManager` (Logic, USB, SPI, I2C, UART, DIO). |
| **RP2040 GP10** | GPIO / SIO | External Trigger (EXT TRIG) | **EXCLUSIVE** | Dedicated hardware interrupt / acquisition qualifier. |
| **RP2040 GP11..GP13**| GPIO / SIO | ST7735 Display Control | **EXCLUSIVE** | Dedicated to Reset, DC, and CS. |
| **RP2040 GP14** | GPIO / SIO | 74HC74 Prescaler Select | **EXCLUSIVE** | Dedicated to input frequency divider ratio. |
| **RP2040 GP15..GP17**| GPIO / SIO | Navigation Buttons (U/D/L) | **EXCLUSIVE** | Debounced tactile input events. |
| **RP2040 GP18..GP20**| SPI0 Peripheral | ST7735 Display Bus | **EXCLUSIVE** | High-speed display refresh. |
| **RP2040 GP21, GP22**| GPIO / SIO | Navigation Buttons (R/SEL) | **EXCLUSIVE** | Debounced tactile input events. |
| **RP2040 GP23..GP24**| Internal Pins | SMPS / VBUS Power Sense | **BOARD INTERNAL** | Inaccessible on external headers. |
| **RP2040 GP25** | GPIO / SIO | Green User LED | **SHARED** | Heartbeat, acquisition arming, diagnostic blinks. |
| **RP2040 GP26..GP28**| ADC Peripheral | Precision Voltmeter / Sensors | **EXCLUSIVE** | Analog SAR converter (0–3.3V). |
| **RP2040 GP29** | ADC3 Channel | VSYS Power Supply Monitor | **BOARD INTERNAL** | 1/3 voltage divider measurement. |
| **RP2040 PIO0 SM0** | PIO0 Subsystem | Logic Analyzer Engine | **EXCLUSIVE** | Parallel 8-bit bus capture at up to 100 MSPS. |
| **RP2040 PIO0 SM1** | PIO0 Subsystem | USB Sniffer Bit Capture | **EXCLUSIVE** | NRZI differential symbol capture. |
| **RP2040 DMA CH0** | DMA Controller | Capture Primary Channel | **EXCLUSIVE** | Transfers PIO FIFO samples to 64 KiB SRAM circular buffer. |
| **RP2040 DMA CH1** | DMA Controller | Capture Ping-Pong Re-arm | **EXCLUSIVE** | Chains buffer wrapping for continuous acquisition. |
| **RP2040 PWM CH0** | PWM Subsystem | Hardware Frequency Counter | **SHARED** | Uses GP2 as clock counter input. |
| **ESP32 GPIO19 / 22**| UART1 Peripheral | Inter-MCU Transport | **EXCLUSIVE** | 1 Mbps link to RP2040 GP0/GP1. |
| **ESP32 GPIO16 / 17**| UART2 Peripheral | External Scope (RS-232) | **EXCLUSIVE** | Links through MAX3232 to HP 54520A. |
| **ESP32 UEXT SPI** | VSPI Peripheral | nRF24L01+ Transceiver | **EXCLUSIVE** | Governed by `ResourceManager` SPI mutex. |
| **ESP32 RMT TX/RX** | RMT Subsystem | Infrared (IR) Engine | **EXCLUSIVE** | High-precision pulse generation and demodulation. |
| **ESP32 Wi-Fi Radio**| 802.11 b/g/n PHY | Network Diagnostic Service | **EXCLUSIVE** | Station, AP scan, promiscuous frame ingestion. |
| **ESP32 BLE Radio** | Bluetooth Controller | BLE Scanner Service | **EXCLUSIVE** | Advertisement observation and RSSI tracking. |

---

## 2. Resource Conflict Arbitration Rules

1. **Probe Bus Conflict (GP2..GP9)**:
   - When the **Logic Analyzer** is armed, all protocol master modes (`I2C`, `SPI`, `UART`, `1-WIRE`, `DIO`, `USB`) are locked out.
   - When a protocol mode (e.g. `SPI`) is acquired, probe bus pins GP2..GP5 are reassigned to SCK, MOSI, MISO, and CS. Attempting to start logic acquisition returns `ERR_RESOURCE_BUSY`.
2. **Display vs Probe Bus Isolation**:
   - The ST7735 display is wired to dedicated RP2040 pins (GP11..GP13, GP18..GP20).
   - Display operations **never interfere with probe bus GP2..GP9**, ensuring 100% deterministic logic and protocol captures.
3. **ESP32 Radio & Bus Locks**:
   - The ESP32 `ResourceManager` enforces mutex protection before activating the nRF24 module, Wi-Fi promiscuous sniffer, or IR transmitter to prevent peripheral bus starvation.
