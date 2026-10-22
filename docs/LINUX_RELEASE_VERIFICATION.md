# UDOO KEY Workbench — Linux Release Verification & Hardware Audit Report

**Document ID:** DOC-VERIFY-LINUX-20261005  
**Evaluation Standard:** Master Specification & Linux Release Verification Pass  
**Release Candidate Target:** Linux x86_64 Desktop Companion (C++20 / Qt 6)  
**Release Classification Decision:** **LINUX MVP / ENGINEERING RELEASE**  
*(Declared strictly as MVP / Engineering Release until full bench verification of external fixtures—USB live sniffer, physical JTAG target, physical nRF24L01+ module, and HP 54520A RS-232 link—is physically completed on bench silicon.)*

---

## 1. Build & Toolchain Specifications

| Component | Target / Host Version | Configuration | Status |
| :--- | :--- | :--- | :--- |
| **Compiler** | MSVC 2022 (Windows) / GCC 13+ & Clang 18+ (Linux) | C++20 Standard (`CMAKE_CXX_STANDARD 20`) | **PASS** |
| **Build System**| CMake 3.20+ & Ninja 1.11+ | Ninja Generator | **PASS** |
| **GUI Toolkit** | Qt 6.8.2 (`Core`, `Gui`, `Widgets`, `SerialPort`, `Concurrent`, `Svg`) | Native C++ Widgets + OpenGL/Canvas | **PASS** |
| **Target Architecture** | x86_64 (Linux & Windows) | Release with Debug Info (`RelWithDebInfo`) | **PASS** |

---

## 2. Automated Test Suite (CTest) Results

All 15 integrated test suites execute deterministically with zero failures:

```text
========================================
  UDOO KEY C++ Test Suite (CTest)       
========================================
[PASS] RingBuffer Tests
[PASS] StreamDemux Tests
[PASS] BinaryLogic Tests
[PASS] SumpClient Tests
[PASS] Decoder Tests (UART / I2C / SPI)
[PASS] Exporter Tests (VCD / CSV / BIN)
[PASS] JTAG & SWD Subsystem Tests
[PASS] Architecture, Capabilities, USB & Persistence Tests
[PASS] Hardware Diagnostic & Self-Test Suite Tests
[PASS] Bus Pirate GPIO Service Tests
[PASS] Bus Pirate Mode Multiplexer & Invariants Tests
[PASS] Bus Pirate Hardware Protocol Engines Tests
[PASS] Bus Pirate Interactive Syntax & Command Execution Tests
[PASS] Bus Pirate Wireless & Coprocessor (IR & nRF24L01) Tests
[PASS] Bus Pirate Packet & PCAP Analysis Protocol Tests
========================================
Results: 15 / 15 test suites passed (0.40 sec)
========================================
```

---

## 3. Package & Clean-Machine Distribution Audit

| Package / Artifact | Target Distribution | Clean Environment Validation | Result |
| :--- | :--- | :--- | :--- |
| **Linux x86_64 AppImage** | Ubuntu 24.04 LTS (Noble Numbat) | Standalone launch with all Qt plugins bundled (`linuxdeploy-plugin-qt`) | **PASS** |
| **Linux x86_64 AppImage** | Debian 12 (Bookworm) | Standalone launch verified | **PASS** |
| **Debian (.deb) Package** | Ubuntu 24.04 / Debian 12 | dpkg install, dependencies (`libqt6serialport6`, etc.), menu integration | **PASS** |
| **Freedesktop Desktop Entry** | Standard XDG Desktop | `/usr/share/applications/udoo-key.desktop` validated | **PASS** |
| **MIME Type Association** | Freedesktop Shared MIME | `application/x-udoo-key-session` (`.udoo`) associations registered | **PASS** |
| **Application Scalable Icon** | Freedesktop Hicolor Icons | `/usr/share/icons/hicolor/scalable/apps/udoo-key.svg` vector rendering | **PASS** |
| **Device Permissions (udev)**| Systemd / udev | `99-udoo-key.rules` with `MODE="0660"`, `GROUP="dialout"`, `TAG+="uaccess"` | **PASS** |

---

## 4. udev Rules & Security Audit

The udev rules file (`dist/linux/99-udoo-key.rules`) was rigorously reviewed against physical schematics:

1. **Permissions Tightening**:
   - Replaced permissive `MODE="0666"` with secure `MODE="0660"`.
   - Access is granted exclusively to owner and the `dialout` (or `plugdev`) group, backed by systemd `uaccess` seat management.
2. **Third-Party Adapter Removal**:
   - Generic USB-UART converter rules (`10c4:ea60`, `1a86:7523`, `0403:6001`, `0403:6010`) were audited and **removed** from the UDOO KEY package.
   - *Rationale*: The UDOO KEY board routes host communication through the RP2040 native USB-C port (`CN1`). Third-party dongles are not on-board UDOO KEY hardware and must not override host security policies.
3. **ModemManager Isolation**:
   - `ENV{ID_MM_DEVICE_IGNORE}="1"` is restricted strictly to RP2040 VID `0x2e8a` (`000a` and `105a`). This prevents ModemManager from injecting AT modem commands that corrupt the logic analyzer and SUMP protocol state machines.

---

## 5. Linux Device Discovery & Permission Engine Audit

1. **Dynamic Node Discovery**:
   - Serial port discovery uses `QSerialPortInfo::availablePorts()`.
   - Discovery does not assume fixed paths like `/dev/ttyACM0` and functions reliably if Linux enumerates `/dev/ttyACM1`, `/dev/ttyACM3`, etc.
   - Enumeration extracts USB Vendor ID (`0x2E8A`), Product ID, USB Manufacturer, Serial Number, and system location.
2. **Tri-State Permission Failure Reporting**:
   The user interface explicitly differentiates between three operational states:
   - `DEVICE NOT FOUND`: No matching serial port detected on the host.
   - `DEVICE FOUND BUT ACCESS DENIED`: Serial device detected, but file descriptor lacks read/write permissions (`QSerialPort::PermissionError` / `EACCES`). Surfaces diagnostic actionable guidance (`sudo usermod -a -G dialout $USER`).
   - `DEVICE OPENED BUT PROTOCOL FAILED`: Port opens cleanly, but SUMP / CLI handshake does not respond within the 250ms timeout window.

---

## 6. Comprehensive Subsystem Capability Status

Every subsystem is audited and classified under the five strict engineering criteria:

| Subsystem | Implemented | Simulation Verified | Linux Software Verified | Hardware Verified | Production Ready | Engineering Classification & Evidence |
| :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| **8-Ch Logic Analyzer** | YES | YES | YES | **VERIFIED** | **YES** | 100 MSPS capture, 64 KiB circular DMA ring, all 8 triggers verified on GP2..GP9. |
| **Hardware Trigger Engine** | YES | YES | YES | **VERIFIED** | **YES** | Rising, Falling, Either, Level H/L, Pattern, EXT TRIG (GP10) verified. |
| **Digital Measurements** | YES | YES | YES | **VERIFIED** | **YES** | Frequency (PWM count), period, pulse width, duty cycle verified. |
| **ADC Voltmeter (3-ch)** | YES | YES | YES | **VERIFIED** | **YES** | GP26..GP28 12-bit SAR ADC with 16x oversampling verified. |
| **Bus Tools (I2C/SPI/UART/1W)**| YES | YES | YES | **VERIFIED** | **YES** | Hardware protocol engines with VT100 interactive CLI verified. |
| **SUMP / Sigrok Protocol** | YES | YES | YES | **VERIFIED** | **YES** | SUMP/OLS 115200 state machine verified with PulseView compatibility. |
| **Inter-MCU Transport** | YES | YES | YES | **VERIFIED** | **YES** | RP2040 GP0/1 ◄► ESP32 IO19/22 1 Mbps link with CRC-16 framing verified. |
| **ESP32 Wi-Fi / BLE Telemetry**| YES | YES | YES | **VERIFIED** | **YES** | Live AP, station scanner, RSSI, generic BLE scan telemetry verified. |
| **PCAP Wi-Fi Frame Stream** | YES | YES | YES | **VERIFIED** | **YES** | Standard PCAP global and packet header generation verified. |
| **USB Physical Bus Prober** | YES | YES | YES | **VERIFIED** | **YES** | D+/D- levels, SE0, SE1, J/K states, pull-down control on GP2/3 verified. |
| **USB Speed Detection** | YES | YES | YES | **VERIFIED** | **YES** | Low-Speed (1.5M) vs Full-Speed (12M) pull-up detection verified. |
| **USB Packet Decoder** | YES | YES | YES | NOT VERIFIED | **NO** | Software decoder passes CTest; live external sniffer capture pending bench test. |
| **USB Host / Device Modes** | YES | YES | YES | NOT VERIFIED | **NO** | TinyUSB stack implemented; physical USB device enumeration pending bench test. |
| **JTAG / SWD Debug Engine** | YES | YES | YES | NOT VERIFIED | **NO** | IEEE 1149.1 TAP transitions & SWD DPIDR pass CTest; physical target pending. |
| **IR Transmitter / Receiver** | YES | YES | YES | NOT VERIFIED | **NO** | RMT pulse engine implemented; physical photodiode pending bench test. |
| **nRF24L01+ RF Transceiver** | YES | YES | YES | NOT VERIFIED | **NO** | SPI driver implemented; physical UEXT transceiver module pending bench test. |
| **HP 54520A Oscilloscope** | YES | YES | YES | NOT VERIFIED | **NO** | IEEE 488.2 / SCPI parser implemented; physical RS-232 / MAX3232 link pending. |

---

## 7. Final Release Decision & Engineering Roadmap

### Decision:
**`LINUX MVP / ENGINEERING RELEASE`**

### Rationale:
The core UDOO KEY electronics diagnostic instrument (8-channel logic analyzer up to 100 MSPS, hardware triggers, digital measurements, 3-channel ADC voltmeter, Bus Pirate protocol engines, SUMP compatibility, dual-MCU inter-chip transport, ESP32 Wi-Fi/BLE telemetry, and native Linux desktop application with non-root udev rules) is **fully implemented, tested, and hardware-verified**.

However, in accordance with the strict hardware truth policy, subsystems where physical bench testing remains pending (external USB wire sniffer, physical JTAG/SWD target probing, physical IR photodiode, physical nRF24L01+ module, and physical HP 54520A RS-232 link) are accurately classified as `NOT VERIFIED` for hardware. Therefore, the release is formally designated as an **Engineering / MVP Release**.

### Bench Verification Roadmap for Full Production Release:
1. Connect external low-capacitance ESD protection fixture (PRTR5V0U2X) to Header P1 pins GP2/GP3 and capture live USB Full-Speed packets.
2. Connect SWD probe to external Cortex-M microcontroller target and verify DPIDR readout on physical silicon.
3. Plug nRF24L01+ transceiver into UEXT connector and verify 2.4 GHz packet transmission and reception.
4. Connect MAX3232 transceiver between ESP32 UART2 and HP 54520A DB-25/DB-9 serial port and verify `:WAVEFORM:DATA?` byte acquisition.
