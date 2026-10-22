# Bus Pirate Reference Architecture & UDOO KEY Implementation Mapping

This document provides the authoritative engineering cross-reference between the reference codebases (`BusPirate5-firmware-main`, `BP rp2040 src`, `ESP32-Bit-Pirate-pioarduino`) and the **UDOO KEY Dual-MCU Diagnostic Instrument**.

---

## 1. Reference Codebase Inventory

| Codebase | Location | Architecture / Build | Relevant Modules | Role in Project |
| :--- | :--- | :--- | :--- | :--- |
| **Bus Pirate 5 (Official)** | `BusPirate5-firmware-main/` | RP2040 (C / Pico SDK / CMake) | `src/mode/`, `src/commands/`, `src/binmode/`, `src/pirate/` | **Primary algorithmic and command syntax reference** for RP2040 instrument engine. |
| **BP RP2040 Extracted** | `BP rp2040 src/` | RP2040 (C / Pico SDK) | `modes.c`, `commands.c`, `system_monitor.c`, `wavegen.pio` | Compact reference extract for protocol modes & monitor routines. |
| **ESP32 Bit Pirate** | `ESP32-Bit-Pirate-pioarduino/` | ESP32 (C++ / PlatformIO / Arduino) | `src/Servers/`, `src/Shells/`, `src/Views/`, `webui/` | **Primary architectural reference for ESP32 UI**, WebSockets, WebUI terminal, and AsyncWebServer. |
| **BP ESP32-S3 Port** | `BP esp32 s3 src/` | ESP32-S3 (C++ / Arduino) | `src/Controllers/`, `src/Dispatchers/`, `src/Models/` | Architectural reference for FreeRTOS command dispatching on ESP32. |

---

## 2. Comprehensive Feature Cross-Reference Matrix

| Feature | Bus Pirate 5 Reference (`BP5`) | ESP32 Bit Pirate Reference (`ESP32-BP`) | UDOO KEY Target Implementation | MCU Owner | Action | Engineering Rationale |
| :--- | :--- | :--- | :--- | :---: | :---: | :--- |
| **8-Channel Digital Capture** | `src/binmode/fala.c` (Follow-Along Logic Analyzer) | `src/Analyzers/` (Software polling) | 8-bit PIO0 State Machine 0 continuous circular DMA stream into 64 KiB SRAM | **RP2040** | **Rewrite (Native)** | RP2040 PIO+DMA provides deterministic 100 MSPS capture with zero CPU load. Software polling on ESP32 is jitter-prone. |
| **Circular Buffer & Pre-Trigger** | `src/binmode/fala.c` (Linear buffer) | N/A | 64 KiB hardware-wrapped circular DMA ring buffer with configurable 70% pre / 30% post ratio | **RP2040** | **Rewrite (Native)** | RP2040 DMA ring wrapping (`channel_config_set_ring`) executes continuous cyclic writes in hardware. |
| **Hardware Trigger Engine** | `src/pirate/` (Software edge detection) | N/A | Multi-mode trigger: Edge (R/F/Any), Level (H/L), Pattern (Mask+Val), Glitch/Runt, Ext Trig (GP10) | **RP2040** | **Adapt & Enhance** | Adapts BP5 trigger concepts into real-time circular SRAM stream evaluation with sub-microsecond latency. |
| **Sub-ns Frequency & Timing** | `src/platform/freq.c` (PWM counter / timer) | `src/Services/` | `pio_edge_analyzer` (dual PIO state machines for sub-nanosecond high/low timing) + PWM counter | **RP2040** | **Adapt & Enhance** | Combines BP5 frequency counter with custom RP2040 PIO edge timer for period, duty cycle, and jitter measurements. |
| **Analog Voltmeter (ADC)** | `src/commands/adc.c` & `system_monitor.c` (12-bit SAR ADC) | `src/Controllers/AdcController.cpp` | 3-channel 12-bit SAR ADC on GP26 (`ADC0`), GP27 (`ADC1`), GP28 (`ADC2`) with oversampling | **RP2040** | **Adapt** | Adapts BP5 multi-sample averaging and calibration algorithms to RP2040 analog inputs. |
| **External Prescaler Control** | N/A (BP5 uses fixed divider) | N/A | Hardware GPIO14 digital prescaler select (1:1, 1:2, 1:4 division) with automatic measurement scaling | **RP2040** | **Native** | Unique to UDOO KEY front-end hardware design. |
| **Interactive Terminal Shell** | `src/pirate.c`, `src/commands.c` (Rich ANSI terminal parser) | `src/Shells/TerminalShell.cpp` | Dual-mode CLI: USB CDC (`Serial`) and Inter-MCU UART link (`Serial1` / `Serial2`) | **RP2040 & ESP32** | **Adapt** | Reuses BP5 command syntax (`m`, `v`, `f`, `g`, `p`, `?`, `scan`, `flash probe`, `sump`) for familiarity. |
| **Sigrok / PulseView SUMP** | `src/binmode/sump.c` (Standard SUMP / OLS 115200 protocol) | N/A | Full SUMP protocol state machine over USB CDC with auto-handshake detection | **RP2040** | **Adapt** | Directly adapted from BP5 SUMP engine to allow direct connection to Sigrok / PulseView. |
| **Local Graphical UI** | `src/displays.c` (SPI LCD) | `src/Views/` | Single 1.44" ST7735 SPI TFT (128x128) on SPI0 (`GP18/19/20`) + 5 tactile navigation keys | **RP2040** | **Adapt** | Custom 128x128 waveform visualizer and menu hierarchy with direct tactile switch handling. |
| **Wireless Web UI & Scope** | N/A | `webui/index.h`, `webui/scripts.h`, `src/Servers/WebSocketServer.cpp` | HTML5/Canvas live waveform renderer, mobile touch UI, and WebSockets binary streaming | **ESP32** | **Reuse & Adapt** | Directly leverages ESP32 Bit Pirate WebSockets architecture to stream captures to browser/tablets. |
| **Inter-MCU Transport** | N/A (BP5 is single MCU) | N/A | Framed binary protocol over Native Factory UART (`GP0/1` <-> `IO22/19`) at 1,000,000 baud | **RP2040 & ESP32** | **Native** | Custom transport-neutral protocol (`instrument_protocol.h`) with CRC-16 and sequence numbering. |
| **I2C Bus Protocol Sniffer** | `src/mode/i2c.c` | `src/Shells/I2cShell.cpp` | I2C master & address scanner with 7-bit ACK detection | **RP2040** | **Adapt** | Adapts BP5 I2C state machine for hardware I2C peripheral. |
| **SPI Protocol Tool & Flash** | `src/mode/spi.c`, `src/commands/flash.c` | `src/Shells/SpiShell.cpp` | SPI master tool + JEDEC SFDP flash interrogator | **RP2040** | **Adapt** | Reuses BP5 SPI flash identification and read/dump routines. |
| **1-Wire ROM Search** | `src/mode/1wire.c` | N/A | Dallas 1-Wire Search ROM algorithm (DS18B20 discovery) | **RP2040** | **Adapt** | Adapts BP5 binary tree search ROM algorithm. |
| **ARM SWD / JTAG Interrogator**| `src/commands/swd.c` | N/A | Bit-banged / PIO SWD DP-IDCODE reader | **RP2040** | **Adapt** | Adapts BP5 Cortex-M debug port identification. |
| **Self-Test & Diagnostic Mode**| `src/commands/selftest.c` | `src/Controllers/TestController.cpp` | Dedicated diagnostic test suite exercising GPIO, UART, SPI, ADC, buttons, display, PIO, DMA | **RP2040 & ESP32** | **Native** | Essential instrument validation mode per Section 27 of project specification. |

---

## 3. Algorithmic Code Extraction Guidelines

1. **When Reusing BP5 Algorithms**:
   * Keep mathematical and protocol logic intact (e.g. CRC-16 calculations, JEDEC SFDP parsing, 1-Wire search algorithms).
   * Replace all hardware-specific register manipulations (`hw_write_masked`, `spi_init`) with UDOO KEY `hardware_config.h` definitions and Pico SDK / Arduino-Pico abstractions.
2. **When Reusing ESP32 Bit Pirate Code**:
   * Reuse the WebSockets packet framing and HTML5 canvas waveform drawing algorithms.
   * Strip out any attempt to poll RP2040 acquisition pins directly from the ESP32.
3. **Preserving Determinism**:
   * High-speed sampling, circular DMA, triggering, and frequency measurement remain strictly on the RP2040.
