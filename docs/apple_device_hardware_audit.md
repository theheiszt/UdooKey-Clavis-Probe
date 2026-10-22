# UDOO KEY — Apple Diagnostics Hardware & Peripheral Audit

**Document Version:** 1.0.0  
**Date:** 2026-10-03  
**Status:** Canonical Hardware Audit & Architectural Reference  
**Target Hardware:** SECO UDOO KEY (BD REV O / CSD81REVB)  

---

## 1. Executive Summary & Objective

The objective of this engineering audit is to evaluate and define the exact hardware and firmware configuration of the **UDOO KEY dual-MCU architecture** to serve as a high-precision **physical-layer companion instrument** for Apple device diagnostics, USB testing, Bluetooth/Wi-Fi telemetry, and JTAG/SWD embedded debugging.

### Architectural Principle
> **The UDOO KEY firmware is not redesigned around proprietary Apple protocols.** Instead, the UDOO KEY acts as a deterministic, synchronized **Diagnostic Instrument Platform** (`PROFILE_APPLE_DIAGNOSTICS`). The RP2040 serves as the real-time acquisition and timing authority (PIO logic analyzer, hardware triggers, ADC, nanosecond timestamps, JTAG/SWD). The ESP32 serves as the asynchronous communications and wireless telemetry engine (BLE advertisement scanning, Wi-Fi network analysis, web UI). The desktop application (`udoo-key-app`) performs device discovery, protocol decoding, session management, and timeline correlation.

---

## 2. Dual-MCU Processing Units & Memory Budgets

### 2.1 Real-Time Engine: Raspberry Pi RP2040
* **Processor:** Dual ARM Cortex-M0+ @ 133.0 MHz (SysTick / 64-bit hardware timer resolution: 1.0 µs native, 7.52 ns cycle resolution).
* **Total SRAM:** 264 KiB multi-banked striping SRAM.
* **Flash:** 2 MB Quad-SPI Flash (XIP cache enabled).
* **SRAM Memory Budget:**
  | Subsystem | Allocation | Location / Purpose |
  | :--- | :--- | :--- |
  | **Logic Capture Buffer** | **64 KiB (65,536 B)** | Dedicated circular DMA ring buffer for 8-channel PIO acquisition |
  | **ADC Waveform Buffer** | **4 KiB (2,048 samples)** | Dedicated 12-bit analog sample buffer |
  | **Instrument Event Queue**| **4 KiB (128 events)** | High-precision `InstrumentEvent` FIFO queue |
  | **Display Framebuffer** | **0 KiB (Line-streamed)** | Direct streaming to ST7735 SPI TFT (zero RAM overhead) |
  | **Terminal & VT100** | **8 KiB** | Command editor buffer and scroll history |
  | **DMA & USB Stack** | **16 KiB** | USB CDC serial endpoints and DMA control descriptors |
  | **Stack & Heap Headroom**| **~168 KiB** | Available headroom for real-time task execution |

### 2.2 Telemetry & Wireless Engine: Espressif ESP32-WROVER-E
* **Processor:** Dual-Core Xtensa 32-bit LX6 @ 240.0 MHz.
* **Internal SRAM:** 520 KiB SRAM.
* **External PSRAM:** 4 MB / 8 MB QSPI PSRAM (memory mapped).
* **Flash:** 4 MB / 16 MB SPI Flash.
* **Wireless Subsystems:**
  * Wi-Fi 802.11 b/g/n (2.4 GHz, 150 Mbps PHY rate).
  * Bluetooth v4.2 BR/EDR and BLE specification.
* **PSRAM Memory Budget:**
  | Subsystem | Allocation | Location / Purpose |
  | :--- | :--- | :--- |
  | **BLE Diagnostic Cache** | **512 KiB** | Apple Continuity / iBeacon / GATT advertisement frame log |
  | **Wi-Fi Packet Capture** | **1,024 KiB** | 802.11 probe request / beacon monitor ring buffer |
  | **Ingested Logic Mirror**| **64 KiB** | Mirrored logic capture buffer for Web UI scope rendering |
  | **Async Web Server & WS**| **256 KiB** | HTML5 scope client & JSON-RPC WebSocket sessions |
  | **Free PSRAM Headroom** | **> 2,000 KiB** | Dynamic allocations for diagnostic trace sessions |

---

## 3. Comprehensive Pin & Peripheral Allocation Table

The table below documents every physical pin on the UDOO KEY, its hardwired connection, its primary instrument function, and its role during Apple diagnostic sessions.

| Pin | MCU Pin | PCB Connection | Default Instrument Mode | Apple Diagnostics Role | Conflict & Multiplexing Constraints |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **GP0** | RP2040 GP0 | Copper Trace | Inter-MCU UART0 TX | Sync & Telemetry TX (RP2040 ➔ ESP32) | **Dedicated hardwired trace.** Never reassign. |
| **GP1** | RP2040 GP1 | Copper Trace | Inter-MCU UART0 RX | Sync & Telemetry RX (ESP32 ➔ RP2040) | **Dedicated hardwired trace.** Never reassign. |
| **GP2** | RP2040 GP2 | Header P1 Pin 4 | Logic Analyzer CH0 (Bit 0) | Logic CH0 / USB CC1 / Lightning ID0 | **Contiguous PIO bus bit 0.** Never break contiguous mapping. |
| **GP3** | RP2040 GP3 | Header P1 Pin 5 | Logic Analyzer CH1 (Bit 1) | Logic CH1 / USB CC2 / Lightning ID1 | **Contiguous PIO bus bit 1.** |
| **GP4** | RP2040 GP4 | Header P1 Pin 6 | Logic Analyzer CH2 (Bit 2) | Logic CH2 / VBUS Enable / ACC_PWR | **Contiguous PIO bus bit 2.** |
| **GP5** | RP2040 GP5 | Header P1 Pin 7 | Logic Analyzer CH3 (Bit 3) | Logic CH3 / I2C SDA (Accessory bus) | **Contiguous PIO bus bit 3.** |
| **GP6** | RP2040 GP6 | Header P1 Pin 9 | Logic Analyzer CH4 (Bit 4) | Logic CH4 / I2C SCL (Accessory bus) | **Contiguous PIO bus bit 4.** |
| **GP7** | RP2040 GP7 | Header P1 Pin 10 | Logic Analyzer CH5 (Bit 5) | Logic CH5 / UART RX (Accessory log) | **Contiguous PIO bus bit 5.** |
| **GP8** | RP2040 GP8 | Header P1 Pin 11 | Logic Analyzer CH6 (Bit 6) | Logic CH6 / UART TX (Accessory stimulus) | **Contiguous PIO bus bit 6.** |
| **GP9** | RP2040 GP9 | Header P1 Pin 12 | Logic Analyzer CH7 (Bit 7) | Logic CH7 / SWD_SWCLK / `SYNC_OUT` | **Contiguous PIO bus bit 7.** Can emit sync pulse when not in LA mode. |
| **GP10** | RP2040 GP10 | Header P1 Pin 14 | External Trigger (`PIN_EXT_TRIG`) | `EXT_TRIG` in (VBUS threshold / Ext Analyzer)| Dedicated hardware trigger comparator input. |
| **GP11** | RP2040 GP11 | PCB Trace | ST7735 TFT Reset (Active LOW) | Display UI Reset | **Dedicated display pin.** |
| **GP12** | RP2040 GP12 | PCB Trace | ST7735 TFT D/C (Data/Cmd) | Display UI Data/Cmd | **Dedicated display pin.** |
| **GP13** | RP2040 GP13 | PCB Trace | ST7735 TFT CS | Display UI Chip Select | **Dedicated display pin.** |
| **GP14** | RP2040 GP14 | Internal / Hdr | Front-End Prescaler Select | Prescaler control (1:1, 1:2, 1:4) | Selects 74HC74 clock divider. |
| **GP15** | RP2040 GP15 | Pushbutton | Button UP | UI Navigation UP | Pulled up internally, active low to GND. |
| **GP16** | RP2040 GP16 | Pushbutton | Button DOWN | UI Navigation DOWN | Pulled up internally, active low to GND. |
| **GP17** | RP2040 GP17 | Pushbutton | Button LEFT | UI Navigation LEFT | Pulled up internally, active low to GND. |
| **GP18** | RP2040 GP18 | PCB Trace | ST7735 SPI0 SCK | Display UI Clock | **Dedicated hardware SPI0.** |
| **GP19** | RP2040 GP19 | PCB Trace | ST7735 SPI0 TX / MOSI | Display UI Data MOSI | **Dedicated hardware SPI0.** |
| **GP20** | RP2040 GP20 | Header P1 Pin 15 | Buzzer / Continuity Tone | Audio Continuity & Trigger Alert | Hardware PWM Slice 2A. |
| **GP21** | RP2040 GP21 | Pushbutton | Button RIGHT | UI Navigation RIGHT | Pulled up internally, active low to GND. |
| **GP22** | RP2040 GP22 | Pushbutton | Button SELECT | UI Select / Run / Stop | Pulled up internally, active low to GND. |
| **GP23** | RP2040 GP23 | Internal | SMPS Power-Save (DO NOT REPURPOSE)| SMPS Low Noise Mode | **Internal power rail control.** |
| **GP24** | RP2040 GP24 | Internal | USB VBUS Detect | Host USB Connection Sense | **Internal USB hardware detect.** |
| **GP25** | RP2040 GP25 | LED | Onboard Green LED | Activity & Trigger Fired Strobe | Active HIGH. |
| **GP26** | RP2040 GP26 | Header P2 Pin 11 | ADC0 (0..3.3V) | VBUS Voltage Sense (5V/9V/15V/20V via divider)| RP2040 SAR ADC channel 0. |
| **GP27** | RP2040 GP27 | Header P2 Pin 12 | ADC1 (0..3.3V) | Current Shunt Sense (Charge Current) | RP2040 SAR ADC channel 1. |
| **GP28** | RP2040 GP28 | Header P2 Pin 14 | ADC2 (0..3.3V) | Accessory Supply / Battery Monitor | RP2040 SAR ADC channel 2. |
| **GP29** | RP2040 GP29 | Internal | VSYS Voltage Monitor (ADC3) | Instrument Internal Power Monitor | Scaled 1/3 VSYS sense. |
| **IO22** | ESP32 GPIO22 | Copper Trace | ESP32 UART RX (◄ RP2040 GP0) | Inter-MCU Transport Link RX | **Dedicated hardwired trace.** |
| **IO19** | ESP32 GPIO19 | Copper Trace | ESP32 UART TX (➔ RP2040 GP1) | Inter-MCU Transport Link TX | **Dedicated hardwired trace.** |
| **IO23** | ESP32 GPIO23 | Copper Trace | RP2040 Hardware Reset (Active LOW)| RP2040 Watchdog & Target Reset | Driven HIGH on boot; low asserts reset. |
| **IO2**  | ESP32 GPIO2  | PCB Trace | RP2040 SWDIO | Target Debug SWDIO | High-Z during normal operation. |
| **IO4**  | ESP32 GPIO4  | PCB Trace | RP2040 SWCLK | Target Debug SWCLK | High-Z during normal operation. |
| **IO5**  | ESP32 GPIO5  | PCB Trace | External SWD Output Enable | Target Debug Buffer Control | Active LOW enable. |

---

## 4. USB Capability Reality Check & Architecture Constraints

### 4.1 USB Hardware Realities
1. **RP2040 USB Controller:** The RP2040 contains a single USB 1.1 Full-Speed (12 Mbps) / Low-Speed (1.5 Mbps) device/host controller. It connects to the micro-USB/Type-C port on the board.
2. **Physical Inabilities:**
   * RP2040 **cannot** sniff USB 2.0 High-Speed (480 Mbps) differential signals (D+/D-) directly.
   * RP2040 **cannot** sniff USB 3.0 / 3.1 / 3.2 SuperSpeed (5 Gbps+) differential pairs.
   * RP2040 **cannot** perform PHY-level hardware packet decoding of high-speed USB traffic.
3. **Mandatory Capability Declaration:**
   The instrument firmware and desktop capability matrix report:
   ```c
   capabilities.usb_physical_analyzer = CAPABILITY_NOT_SUPPORTED;
   capabilities.external_analyzer_sync = CAPABILITY_SUPPORTED;
   ```

### 4.2 Realistic Apple USB Diagnostics & Physical-Layer Role
While PHY packet sniffing is not possible directly on RP2040 pins, the UDOO KEY provides essential physical-layer instrumentation that Apple software diagnostics cannot provide:
1. **Power Delivery & VBUS Transition Timing:**
   * Monitor VBUS rise times and step transitions (5V ➔ 9V ➔ 15V ➔ 20V) on ADC0/ADC1.
   * Measure inrush current and capacitive transient spikes during Apple accessory attachment.
2. **Sideband & Accessory Handshake Capture:**
   * Monitor Apple Lightning ID0/ID1 lines, Type-C CC line state transitions (pull-up/pull-down negotiations) at up to 100 MSPS on logic channels CH0..CH3.
   * Decode bidirectional Single-Wire UART / accessory identification buses.
3. **Hardware Synchronization (`SYNC_OUT` & `EXT_TRIG`):**
   * Output a nanosecond-precise trigger strobe pulse on `SYNC_OUT` (GP9 or EXT_TRIG configured as output) whenever a trigger condition occurs, synchronizing external high-speed analyzers (e.g. Total Phase Beagle USB, Teledyne LeCroy, Ellisys).
   * Receive trigger signals on `EXT_TRIG` (GP10) from an external USB test fixture or host test fixture.
4. **Desktop Software Marker Correlation:**
   * Host application injects `MARK_EVENT` timestamps when host-side Apple events occur (e.g. `ioregistry` device insertion, `log stream` error, XCTest assertion failure), aligning host software events with microsecond-level hardware logic and power traces.

---

## 5. ESP32 Wireless Diagnostic Architecture

The ESP32-WROVER-E coprocessor operates as a dual-role wireless probe:

### 5.1 BLE Diagnostics Engine (Apple Continuity & Proximity)
* Operates BLE passive/active background scan using `NimBLE-Arduino`.
* Extracts Manufacturer Specific Data (Company ID `0x004C` — Apple Inc.):
  * **0x05:** Apple AirDrop
  * **0x07:** AirPods / Beats Proximity Pairing
  * **0x09:** AirPlay Destination
  * **0x10:** Nearby Action / Continuity Camera / Universal Control
  * **0x12:** Find My Offline Finding Beacon
* Formats events as `RadioDiagnosticEvent` tagged with local timestamp and delta against RP2040 timebase.

### 5.2 Wi-Fi Diagnostics Engine
* Scans 802.11 b/g/n channels (1–14).
* Measures RSSI, noise floor, beacon intervals, channel utilization.
* Performs automated ICMP ping probes to local gateway or Apple captive portal endpoints (`captive.apple.com`).
* Measures DNS resolution latency for Apple infrastructure domains.

---

## 6. JTAG / SWD Subsystem Integration

* **RP2040 as SWD/JTAG Master:** Uses bit-banged or PIO-driven state machine on selectable GPIOs (e.g. CH4..CH7) to debug target MCUs in Apple accessories (cables, power bricks, audio dongles).
* **ESP32 as RP2040 Debugger:** ESP32 is hardwired to RP2040 SWD lines (IO2, IO4, IO5) allowing full recovery, firmware reflashing, or hardware breakpoints on the RP2040 core without an external debugger.
* **Non-Interference:** When JTAG/SWD is active, the Pin-Mux Manager enforces mutual exclusion against the Logic Analyzer to prevent signal contention.

---

## 7. Conclusions & Implementation Directives

1. **Protocol Layer:** Enhance `shared/instrument_protocol.h` to include rich device capabilities, 64-bit monotonic timestamps, structured `InstrumentEvent` streaming, and radio diagnostic telemetry packets.
2. **RP2040 Core:** Implement 64-bit hardware timer timestamping (`device_timestamp_us()`), `SYNC_OUT` strobe, configurable pre/post trigger ratios (0–100%), and safe pin multiplexing.
3. **ESP32 Core:** Implement BLE advertisement scanning for Apple manufacturer IDs, Wi-Fi network diagnostics, and inter-MCU timestamp synchronization.
4. **Desktop App:** Build `AppleHardwareBridge` in `udoo-key-app` to seamlessly fuse physical GPIO/ADC events with Apple host diagnostics.
