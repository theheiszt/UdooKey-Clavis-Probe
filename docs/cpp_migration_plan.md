# UDOO KEY — Native C++ Desktop Application Migration Plan

**Document ID:** DOC-CPP-MIGRATION-20261003  
**Status:** SPECIFICATION & ROADMAP (PHASE 1)  
**Date:** 2026-10-03  
**Target Project:** `udoo-key-app`  
**Target Platforms:** Windows 10/11 & Linux (x86_64 / arm64)  
**Framework:** Qt 6 (Widgets, Core, Gui, SerialPort) + CMake  

---

## 1. Executive Summary

This document establishes the step-by-step engineering roadmap to transition from the prototype Python companion application (`python_companion_app/`) to the production-grade, native cross-platform C++ desktop application (`udoo-key-app/`).

The plan enforces strict separation between:
* **Headless Software Development** (implementable and verifiable via `MockTransport` without physical hardware).
* **Hardware-in-the-Loop Validation** (verifying against physical UDOO KEY hardware, USB CDC enumeration, and PIO/DMA acquisition).

---

## 2. Phased Development Roadmap

```text
PHASE 1: AUDIT & PROTOCOL SPECIFICATION ──► COMPLETED
   │
PHASE 2: NATIVE C++ SKELETON & CMAKE BUILD
   │     • Root CMakeLists.txt, CMakePresets.json (MSVC + GCC)
   │     • MainWindow shell, theme manager, settings, structured logging
   │
PHASE 3: TRANSPORT & DEVICE DISCOVERY
   │     • QSerialPort auto-detection (VID/PID 0x2E8A)
   │     • Headless MockTransport simulation engine
   │     • Handshake state machine & capability negotiation
   │
PHASE 4: LOGIC ACQUISITION & SUMP CLIENT
   │     • 0x5A binary protocol client + standard SUMP client
   │     • Non-blocking capture streaming & 64 KiB buffer assembly
   │
PHASE 5: HIGH-PERFORMANCE WAVEFORM ENGINE
   │     • Multi-resolution Level-of-Detail (LOD) min/max decimation
   │     • Locked 60 FPS viewport painting, zoom, pan
   │     • Draggable measurement cursors (A & B) with magnetic edge snapping
   │
PHASE 6: INTERACTIVE BUS TOOLS
   │     • SPI Transaction Builder with Hex/ASCII payload editors
   │     • Interactive 7-bit I2C Scanner Grid with device auto-identification
   │     • Target UART Terminal & Monitor with line logging
   │     • Direct DIO probe bus mask writer
   │
PHASE 7: PRECISION METER & ANALOG SCOPE
   │     • Calibrated multi-channel Voltmeter dashboard (ADC0..ADC2, VSYS, Core Temp)
   │     • Live resistance & acoustic continuity beeper interface
   │     • ADC DMA mini-scope graph
   │
PHASE 8: PROTOCOL DECODERS & TRANSACTION TABLE
   │     • Background UART (8N1/configurable framing) decoder
   │     • I2C transaction decoder (START/STOP/ADDR/ACK/DATA)
   │     • SPI full-duplex MOSI/MISO decoder
   │     • Virtualized QAbstractTableModel transaction table
   │
PHASE 9: DATA EXPORT & PROJECT MANAGEMENT
   │     • Asynchronous IEEE 1364 Value Change Dump (.vcd) writer
   │     • High-throughput tabular CSV export
   │     • Complete capture session project save/load (.json)
   │
PHASE 10: RELEASE PACKAGING & DISTRIBUTION
         • Windows standalone installer / portable zip
         • Linux AppImage / tar.gz binary bundle
```

---

## 3. Hardware vs. Headless Simulation Test Matrix

To guarantee that C++ application development can proceed rapidly and continuously without stalling on physical hardware availability, all subsystems are categorized by their simulation independence:

| Subsystem / Feature | Headless Simulation (MockTransport / Unit Tests) | Hardware-in-the-Loop (Physical UDOO KEY) | Verification Criteria |
| :--- | :---: | :---: | :--- |
| **CMake Build System** | **100% Headless** | N/A | Compiles cleanly with MSVC on Windows and GCC/Clang on Linux with zero warnings. |
| **Application UI Shell** | **100% Headless** | N/A | Main window, dockable panels, status bar, and Dark/Light themes render properly. |
| **Serial Port Enumeration** | Mock Port List | **Physical Hardware** | Correctly identifies UDOO KEY RP2040 VID `0x2E8A` / PID `0x000A` on COMx and `/dev/ttyACM*`. |
| **Connection Handshake** | Synthetic Responses | **Physical Hardware** | Reliably exchanges `CMD_GET_INFO` and parses firmware version string. |
| **Binary Protocol Parsing** | **100% Headless** | **Physical Hardware** | Deframer correctly parses headers, extracts payloads, and recovers from dropped bytes. |
| **SUMP Client Interop** | Synthetic SUMP Stream | **Physical Hardware** | Correctly issues `0x00`, `0x02`, `0x04`, `0x80`, `0x81` and receives 64 KiB buffer. |
| **LOD Waveform Decimator** | **100% Headless** | N/A | Correctly condenses 65,536 samples into screen pixels with min/max fidelity; zero lost edges. |
| **Waveform Canvas Render** | **100% Headless** | N/A | Sustains 60 FPS while panning and zooming across 64 KiB buffer. |
| **Timing Cursors HUD** | **100% Headless** | N/A | $\Delta t$ and $1/\Delta t$ calculations verified against synthetic clock signals (1 kHz, 1 MHz, 10 MHz). |
| **Magnetic Edge Snapping** | **100% Headless** | N/A | Cursor snaps within $\pm 5\,\text{px}$ to precise sample transition index. |
| **UART Protocol Decoder** | **100% Headless** | Synthetic / Real Waveform | Correctly extracts ASCII strings and hex bytes across standard baud rates (9600 to 2 Mbps). |
| **I2C Protocol Decoder** | **100% Headless** | Synthetic / Real Waveform | Correctly tags START, 7-bit ADDR, READ/WRITE flag, ACK/NACK, DATA, and STOP conditions. |
| **SPI Protocol Decoder** | **100% Headless** | Synthetic / Real Waveform | Correctly extracts MOSI/MISO full-duplex transfers across Modes 0, 1, 2, and 3. |
| **VCD File Exporter** | **100% Headless** | N/A | Exported `.vcd` file parses cleanly in GTKWave / Sigrok with exact timescale matching. |
| **CSV File Exporter** | **100% Headless** | N/A | Exported `.csv` file contains exact sample row counts and matching column headers. |
| **Interactive I2C Grid** | Mock Bus Table | **Physical Hardware** | Real I2C scan interrogates attached sensors and updates GUI grid. |
| **Precision Voltmeter** | Mock ADC Values | **Physical Hardware** | Accurately renders calibrated ADC voltages ($0.000\,\text{V}$ to $3.300\,\text{V}$) and VSYS rail. |
| **Continuity Beeper** | Mock Resistance | **Physical Hardware** | Triggers visual alert and acoustic tone when probe resistance $< 25\,\Omega$. |
| **USB Physical Probing** | Mock USB States | **Physical Hardware** | Correctly identifies Full-Speed vs Low-Speed pull-up state and SE0/SE1 conditions. |

---

## 4. Phase-by-Phase Deliverables & Verification Gates

### Phase 2: Native C++ Skeleton & CMake Build System
* **Deliverables**:
  - `CMakeLists.txt` configured with C++17, Qt 6.5+, compiler warning flags (`/W4` on MSVC, `-Wall -Wextra -pedantic` on GCC/Clang).
  - `CMakePresets.json` with presets: `windows-msvc-debug`, `windows-msvc-release`, `linux-gcc-debug`, `linux-gcc-release`.
  - `src/main.cpp` and `src/app/Application.cpp` initializing Qt application.
  - `src/ui/MainWindow.cpp` displaying modern dark-themed window with central splitter and dock widgets.
  - `src/app/SettingsManager.cpp` persisting window geometry and preferences.
* **Verification Gate**:
  - Compiles and runs cleanly on Windows and Linux, displaying the main application shell with zero errors.

### Phase 3: Device Discovery & Transport Layer
* **Deliverables**:
  - `src/transport/ITransport.h` interface.
  - `src/transport/SerialTransport.cpp` wrapping `QSerialPort` in a dedicated worker thread.
  - `src/transport/MockTransport.cpp` generating synthetic signals (1 MHz square wave, I2C traffic).
  - `src/core/DeviceManager.cpp` enumerating serial ports and filtering by VID `0x2E8A`.
* **Verification Gate**:
  - Automated test `test_mock_device` passes 100% without hardware.
  - Real connection test detects UDOO KEY and displays port name in connection toolbar.

### Phase 4: Logic Acquisition & SUMP Client
* **Deliverables**:
  - `src/protocol/BinaryLogicClient.cpp` implementing the `0x5A` frame parser and streaming handler.
  - `src/protocol/SumpClient.cpp` implementing standard SUMP commands and metadata negotiation.
  - `src/logic_analyzer/CaptureBuffer.cpp` holding the 64 KiB sample ring.
* **Verification Gate**:
  - Unit test verifies zero-copy buffer reception and checksum verification.
  - Hardware test downloads 65,536 samples from UDOO KEY in $< 20\,\text{ms}$.

### Phase 5: High-Performance Waveform Engine
* **Deliverables**:
  - `src/logic_analyzer/LodDecimator.cpp` computing pixel-column min/max summary bins.
  - `src/ui/waveform/WaveformWidget.cpp` drawing 8 channels with custom `QPainter` double-buffering.
  - `src/ui/waveform/TimingCursorBar.cpp` managing interactive Cursors A and B.
* **Verification Gate**:
  - Viewport maintains locked 60 FPS during continuous mouse wheel zooming and horizontal drag panning across 64 KiB buffer.
  - Magnetic snapping lands on exact edge transition indices.

### Phase 6: Protocol Decoders & Transaction Table
* **Deliverables**:
  - `src/decoders/UartDecoder.cpp`, `I2cDecoder.cpp`, `SpiDecoder.cpp`.
  - `src/decoders/DecoderModel.cpp` implementing virtualized `QAbstractTableModel`.
  - `src/ui/panels/DecoderPanel.cpp` hosting the decoded transaction table.
* **Verification Gate**:
  - Decoders run entirely in worker thread without freezing waveform interaction.
  - Decodes matching test captures with 100% parity against Python reference results.

### Phase 7: Interactive Bus Tools & Meter Panel
* **Deliverables**:
  - `src/ui/panels/I2cPanel.cpp`: Clickable 7-bit address matrix and transaction log.
  - `src/ui/panels/SpiPanel.cpp`: SPI transaction builder with hex/ASCII input and transfer button.
  - `src/ui/panels/MeterPanel.cpp`: Multichannel voltmeter gauges and continuity indicator.
  - `src/ui/panels/TerminalPanel.cpp`: Interactive Bus Pirate VT100 terminal console.
* **Verification Gate**:
  - All instrument views communicate through the core device layer without direct serial manipulation.

### Phase 8: Data Export & Session Management
* **Deliverables**:
  - `src/export/VcdExporter.cpp` writing IEEE 1364 Value Change Dump files asynchronously.
  - `src/export/CsvExporter.cpp` writing tabular CSV data.
  - `src/export/SessionExporter.cpp` saving complete workspace configurations and captures to `.json`.
* **Verification Gate**:
  - Exported VCD opens cleanly in PulseView / GTKWave with matching signal traces and timing.

### Phase 9: Release Packaging
* **Deliverables**:
  - CMake install targets and CPack configuration.
  - Standalone Windows bundle (executable + Qt DLLs + runtime).
  - Standalone Linux AppImage bundle.
* **Verification Gate**:
  - Clean deployment tested on fresh Windows and Linux virtual machines without development tools or Python installed.

---

## 5. Architectural Quality Guarantees

1. **Zero Python Runtime Dependency**: The resulting application contains zero Python code, zero embedded interpreters, and zero external script invocations.
2. **Backward Compatibility**: Fully compatible with the frozen UDOO KEY RP2040 firmware (PIO0 SM0 circular DMA buffer, SUMP engine, and universal terminal).
3. **Professional Responsiveness**: Long operations run in background workers; GUI thread remains responsive with zero frame drops.
