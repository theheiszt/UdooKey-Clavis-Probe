# UDOO KEY — Protocol Subsystem Validation: Hardware I2C Master Mode

**Document ID:** DOC-PROTO-I2C-20261003  
**Status:** VALIDATED & FROZEN (MILESTONE PROTO-3)  
**Date:** 2026-10-03  
**Target Hardware:** SECO UDOO KEY (RP2040 Dual Cortex-M0+ @ 133 MHz, REV O)  
**Firmware Version:** UDOO-KEY-OS v2.1-bp-terminal (UF2 Size: 427,520 bytes)  

---

## 1. Executive Summary

This document establishes the experimental validation, hardware pin reconciliation, and formal freeze of the **third Bus Pirate 5 protocol mode: Hardware I2C Master Mode** on the SECO UDOO KEY architecture.

Following the strict project development sequence:
```text
TERMINAL (FROZEN) ──► GPIO / DIO (FROZEN) ──► SPI (FROZEN) ──► I2C (FROZEN) ──► UART (NEXT) ──► 1-WIRE ──► METER ──► LOGIC CLI ──► USB TOOLS
```
the I2C subsystem has been implemented through a dedicated hardware abstraction layer (`udoo_hal/i2c_hal`), decoupling terminal commands from direct peripheral manipulation while providing a comprehensive 7-bit bus scanner with an integrated device identification database (~55 common I2C sensors, RTCs, OLED displays, and EEPROMs).

---

## 2. Hardware Architecture & Pin Allocation

### 2.1 Physical Pin Allocation (Header P1 Probe Bus)
The RP2040 hardware `I2C1` peripheral is assigned to the Header P1 probe bus pins:

| Signal Name | RP2040 GPIO | Physical Connector | Electrical Role | Default State |
| :---: | :---: | :---: | :---: | :---: |
| **SDA** | `GP2` | Header P1 Pin 4 | Serial Data (Bidirectional Open-Drain) | High-Z Input with Weak Pull-Up |
| **SCL** | `GP3` | Header P1 Pin 5 | Serial Clock (Bidirectional Open-Drain) | High-Z Input with Weak Pull-Up |

### 2.2 Pull-Up Resistor Management
I2C open-drain signaling requires pull-up resistors to 3.3V:
* **Internal Weak Pull-Ups:** RP2040 provides internal ~50kΩ to 80kΩ pull-up resistors on both `GP2` and `GP3`. These are enabled by default upon entering I2C mode (`i2c_hal_acquire`) and can be toggled via `pull on` / `pull off`.
* **External Pull-Ups:** For reliable 400 kHz (Fast Mode) and 1 MHz (Fast Mode Plus) operation across long cables or heavy bus loads, external 2.2kΩ to 4.7kΩ pull-up resistors to 3.3V are strongly recommended.

### 2.3 Clock Frequencies & Modes
The subsystem supports:
* **Standard Mode:** 100 kHz (`speed 100k`)
* **Fast Mode (Default):** 400 kHz (`speed 400k`)
* **Fast Mode Plus:** 1.00 MHz (`speed 1M`)
* **Arbitrary Clock Rates:** 10 kHz to 1 MHz via `speed <freq>`

---

## 3. Command Syntax & User Experience

| Command | Shorthand | Parameters | Function & Description |
| :--- | :--- | :--- | :--- |
| `m i2c` | `4` | None | Enters I2C mode; sets prompt to `I2C>`; outputs configuration telemetry. |
| `scan` | `search` / `s` | `[-v]` | Scans 7-bit addresses (0x08..0x77); matches against device database; verbose mode `-v`. |
| `read` | `r` | `<addr> [count]` | Reads `count` bytes (default 1) from 7-bit address; displays HEX and ASCII table. |
| `write` | `w` | `<addr> <data...>` | Writes byte(s) to 7-bit address; reports ACK/NACK status. If no data, probes address. |
| `speed` | `baud` / `clk` | `<freq>` | Sets I2C clock rate (e.g. `speed 100k`, `speed 400k`, `speed 1M`). |
| `pull` | `p` | `<on\|off>` | Toggles RP2040 internal weak pull-up resistors on SDA (GP2) and SCL (GP3). |
| `eeprom` | None | `probe\|read\|write\|dump` | 24Cxx EEPROM utility suite (supports 24C02..24C512 chips with 8/16-bit addressing). |
| `sniff` | None | None | Real-time I2C bus traffic sniffer with START, STOP, ACK, and byte display. |
| `pins` | `status` | None | Displays detailed hardware status box. |
| `[` / `]` | None | None | Bus Pirate syntax I2C START and STOP conditions. |
| `[ 0x50 0x00 r:16 ]` | None | None | Direct Bus Pirate syntax macro execution. |
| `hiz` | None | None | Exits to safe HiZ; releases I2C peripheral; restores GP2 & GP3 to high-impedance. |

### 3.1 Status Telemetry Output
Executing `status` or entering `m i2c` outputs:
```text
=== RP2040 Hardware I2C Master Configuration ===
  Controller:       RP2040 Hardware I2C1 Master
  Clock Speed:      400 kHz (400000 Hz)
  Pull-Up Resistors: Internal Weak Pull-Ups Enabled (~50k)
  Physical Pins:    SDA=GP2 (Header P1.4) | SCL=GP3 (Header P1.5)
  Bus State:        Active (Acquired)
================================================
```

### 3.2 Integrated Device Identification Database
The scanner matches detected addresses against common component types:
* **EEPROMs:** AT24Cxx / 24LCxx (0x50..0x57)
* **RTCs:** DS1307 / DS3231 / PCF8523 / PCF8563 (0x68, 0x69, 0x51)
* **IMUs & Motion:** MPU6050 / MPU9250 (0x68, 0x69), LSM6DS3 (0x6A, 0x6B), BNO055 (0x28), ADXL345 (0x1D)
* **Environmental Sensors:** BME280 / BMP280 / BMP388 (0x76, 0x77), SHT30 / SHT31 (0x44, 0x45), AHT20 (0x38), SGP30 (0x58)
* **Displays & Controllers:** SSD1306 OLED (0x3C, 0x3D), TCA9548A Mux (0x70..0x72), MCP23017 GPIO Expander (0x20..0x27)
* **Power Monitors & ADCs:** INA219 (0x40), ADS1115 (0x48..0x4B), MCP4725 DAC (0x60)

---

## 4. Test Suite Execution & Experimental Results

The automated regression test suite (`scratch/test_terminal_cli.py`) now covers 40 test cases:

| Test ID | Test Scenario | Command Sequence | Expected Response | Result |
| :---: | :--- | :--- | :--- | :---: |
| **T31** | Mode Switch to I2C | `m i2c\r\n` | `I2C>` prompt & Hardware I2C1 banner | **PASS** |
| **T32** | I2C Status Dump | `status\r\n` | Controller, SDA=GP2, SCL=GP3, 400 kHz | **PASS** |
| **T33** | I2C Clock Reconfiguration (100k) | `speed 100k\r\n` | `100 kHz (100000 Hz)` | **PASS** |
| **T34** | I2C Fast Clock (1M) | `speed 1M\r\n` | `1000 kHz (1000000 Hz)` | **PASS** |
| **T35** | Internal Pull-Up Resistor Toggle | `pull off\r\n` / `pull on\r\n` | `ENABLED (~50k)` | **PASS** |
| **T36** | 7-bit Bus Scan | `scan\r\n` | `I2C Bus Scan (0x08 - 0x77)` & completion summary | **PASS** |
| **T37** | Address Probe | `write 0x50\r\n` | `Address 0x50 write probe` ACK/NACK report | **PASS** |
| **T38** | Data Read | `read 0x50 4\r\n` | Read response / formatted hex table | **PASS** |
| **T39** | Bus Pirate Syntax Macro | `[ 0x50 0x00 ]\r\n` | `I2C START` / `I2C STOP` execution | **PASS** |
| **T40** | Clean Exit to Safe HiZ | `hiz\r\n` | `HiZ>` prompt; pins restored to High-Z | **PASS** |

---

## 5. Formal Freeze Declaration

The Hardware I2C Master subsystem meets all architectural, functional, and electrical isolation criteria:

1. **Permanent PIO/DMA Logic Baseline Protection:** PIO0 SM0 circular DMA capture and SUMP protocol are completely untouched.
2. **Safe High-Z Assurance:** Safe boot state (`HiZ>`) is strictly preserved; exiting I2C mode immediately restores `GP2` and `GP3` to high-impedance inputs without pulls.
3. **Hardware Abstraction Layer Isolation:** All physical pin configurations, clock frequencies, pull-ups, and transactions are encapsulated in `udoo_hal/i2c_hal`.
4. **Memory Footprint:** Clean PlatformIO build (RAM 62.3%, Flash 1.2%).

**Hardware I2C Master Mode is hereby declared VALIDATED AND FROZEN.**
Next protocol in sequence: **UART Mode (`m uart` / `UART>`).**
