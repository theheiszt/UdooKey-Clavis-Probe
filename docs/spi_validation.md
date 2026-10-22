# UDOO KEY — Protocol Subsystem Validation: Hardware SPI Master Mode

**Document ID:** DOC-PROTO-SPI-20261003  
**Status:** VALIDATED & FROZEN (MILESTONE PROTO-2)  
**Date:** 2026-10-03  
**Target Hardware:** SECO UDOO KEY (RP2040 Dual Cortex-M0+ @ 133 MHz, REV O)  
**Firmware Version:** UDOO-KEY-OS v2.1-bp-terminal (UF2 Size: 407,552 bytes)  

---

## 1. Executive Summary

This document establishes the experimental validation, hardware arbiter reconciliation, and formal freeze of the **second Bus Pirate 5 protocol mode: Hardware SPI Master Mode** on the SECO UDOO KEY architecture.

Following the strict project development sequence:
```text
TERMINAL (FROZEN) ──► GPIO / DIO (FROZEN) ──► SPI (FROZEN) ──► I2C (NEXT) ──► UART ──► 1-WIRE ──► METER ──► LOGIC CLI ──► USB TOOLS
```
the SPI subsystem has been implemented through a dedicated hardware abstraction layer (`udoo_hal/spi_hal`), decoupling Bus Pirate 5 terminal interactions from the RP2040 hardware peripherals while resolving the physical `SPI0` peripheral sharing between the onboard ST7735 display and the external probe bus.

---

## 2. Hardware Architecture & Display Arbiter

### 2.1 Physical Pin Allocation (Header P1 Probe Bus)
The RP2040 hardware `SPI0` peripheral is routed to the Header P1 probe bus pins:

| Signal Name | RP2040 GPIO | Physical Connector | Electrical Role | Idle State |
| :---: | :---: | :---: | :---: | :---: |
| **SCK** | `GP2` | Header P1 Pin 4 | SPI Serial Clock | Configurable (Low in Mode 0/1, High in Mode 2/3) |
| **MOSI / TX** | `GP3` | Header P1 Pin 5 | Master Out Slave In | Driven Low |
| **MISO / RX** | `GP4` | Header P1 Pin 6 | Master In Slave Out | High-Z Input |
| **CS# (Default)** | `GP5` | Header P1 Pin 7 | Chip Select (Active LOW) | Idle HIGH (3.3V) |

### 2.2 Dynamic SPI0 Bus Arbiter (Display vs. Probe Bus)
On the SECO UDOO KEY, the primary ST7735 color TFT display shares the RP2040 `SPI0` controller using `GP18` (SCK), `GP19` (MOSI), and `GP20` (MISO).

To prevent hardware bus conflicts:
1. **Mode Entry (`m spi`):**
   - Calls `spi_hal_acquire()`, which invokes `ModeMgr.acquireProbeSpi()`.
   - Releases the ST7735 display bus, safely reallocating `SPI0` to probe pins `GP2..GP5`.
   - The UI rendering task (`ui_task()`) checks `ModeMgr.isProbeSpiActive()` and pauses display SPI traffic.
2. **Mode Exit (`hiz` / mode switch):**
   - Deasserts CS (`GP5` HIGH).
   - Calls `spi_hal_release()`, which invokes `ModeMgr.releaseProbeSpi()`.
   - Restores probe pins `GP2..GP5` to Safe High-Z inputs with pulls disabled.
   - Restores `SPI0` back to the display pins (`GP18..GP20`), automatically resuming live TFT refreshes.

### 2.3 Clock Frequencies & SPI Modes
The subsystem supports:
* **Clock Speeds:** 10 kHz to 30 MHz (default: 10.00 MHz).
* **Modes:** Mode 0 (`CPOL=0, CPHA=0`), Mode 1 (`CPOL=0, CPHA=1`), Mode 2 (`CPOL=1, CPHA=0`), Mode 3 (`CPOL=1, CPHA=1`).
* **Bit Ordering:** MSB First (Standard) or LSB First.
* **Chip Select Flexibility:** Any probe pin (`GP2..GP9`) can be assigned as active CS via `cs <pin>`.

---

## 3. Command Syntax & User Experience

| Command | Shorthand | Parameters | Function & Description |
| :--- | :--- | :--- | :--- |
| `m spi` | `5` | None | Enters SPI mode; sets prompt to `SPI>`; outputs configuration telemetry. |
| `[` | `cs low` | None | Asserts Chip Select (drives active CS pin LOW). |
| `]` | `cs high` | None | Deasserts Chip Select (drives active CS pin HIGH). |
| `cs` | None | `<pin>` | Reconfigures active CS pin to alternative probe pin (e.g. `cs GP6`). |
| `read` | `r` | `[count]` | Clocks out `count` bytes (dummy 0xFF); displays HEX and ASCII. |
| `write` | `w` | `<data...>` | Sends one or more bytes; reports TX data. |
| `transfer` | `x` / `xfer` | `<data...>` | Full-duplex transfer; reports TX bytes and simultaneous RX bytes. |
| `speed` | `baud` | `<freq>` | Sets SPI baud rate (e.g., `speed 10M`, `speed 400k`, `speed 2500000`). |
| `mode` | `config` | `<0..3> [msb\|lsb]` | Sets SPI Mode 0..3 and bit ordering. |
| `probe` | `flash probe` | None | Reads JEDEC ID (0x9F) and identifies SPI flash manufacturer and capacity. |
| `flash` | None | `<subcommand>` | Flash commands: `status`, `sfdp`, `read <addr> <len>`, `dump <pages>`. |
| `pins` | `status` | None | Displays detailed hardware status box. |
| `[ 0x9F r:3 ]`| None | None | Direct Bus Pirate macro syntax (asserts CS, writes 0x9F, reads 3 bytes, deasserts CS). |
| `hiz` | None | None | Exits to safe HiZ; releases probe SPI; resumes display updates. |

### 3.1 Status Telemetry Output
Executing `status` or entering `m spi` outputs:
```text
=== RP2040 Hardware SPI Master Configuration ===
  Controller:       RP2040 Hardware SPI0 Master
  Clock Speed:      10.00 MHz (10000000 Hz)
  SPI Mode:         Mode 0 (CPOL=0, CPHA=0)
  Bit Order:        MSB First (Standard)
  Chip Select:      Probe GP5 (Header P1.7) | State: IDLE (HIGH)
  Physical Pins:    SCK=GP2 (P1.4) | MOSI=GP3 (P1.5) | MISO=GP4 (P1.6)
  Bus Arbiter:      Probe Bus Active (TFT Paused)
================================================
```

---

## 4. Test Suite Execution & Experimental Results

The automated regression test suite (`scratch/test_terminal_cli.py`) now covers 30 test cases:

| Test ID | Test Scenario | Command Sequence | Expected Response | Result |
| :---: | :--- | :--- | :--- | :---: |
| **T21** | Mode Switch to SPI | `m spi\r\n` | `SPI>` prompt & SPI0 Master banner | **PASS** |
| **T22** | SPI Status Dump | `status\r\n` | Controller, GP2..GP5 pin mapping | **PASS** |
| **T23** | Speed Configuration | `speed 5M\r\n` | `5.00 MHz (5000000 Hz)` | **PASS** |
| **T24** | Mode Configuration | `mode 3\r\n` | `Mode 3 (CPOL=1, CPHA=1)` | **PASS** |
| **T25** | CS Assertion | `[\r\n` | `/CS Asserted (LOW on GP5)` | **PASS** |
| **T26** | SPI Write | `write 0xAA 0x55\r\n` | `TX: 0xAA 0x55` | **PASS** |
| **T27** | SPI Read | `read 3\r\n` | `SPI READ (3 bytes)` with HEX & ASCII | **PASS** |
| **T28** | CS Deassertion | `]\r\n` | `/CS Deasserted (HIGH on GP5)` | **PASS** |
| **T29** | BP Macro Execution | `[ 0x9F r:3 ]\r\n` | CS toggle, write, and 3-byte read | **PASS** |
| **T30** | Safe Return to HiZ | `hiz\r\n` | `HiZ>` prompt; arbiter released | **PASS** |

---

## 5. Firmware Resource Utilization

Compiled with GCC 12.3 under PlatformIO release build:
* **RAM Consumption:** 163,272 bytes / 262,144 bytes (**62.3%**)
* **Flash Consumption:** 191,680 bytes / 16,248,832 bytes (**1.2%**)
* **Binary UF2 Image:** 407,552 bytes (`firmware.uf2`)
* **Logic Analyzer PIO/DMA Engine:** PIO0 SM0 + DMA circular ring (64 KiB buffer) fully intact and operational.

---

## 6. Formal Protocol Baseline Freeze Declaration

The Hardware SPI Master subsystem components:
1. `src/udoo_hal/spi_hal.h`
2. `src/udoo_hal/spi_hal.cpp`
3. `src/instrument_mode_manager.h` / `.cpp` (`acquireProbeSpi` / `releaseProbeSpi` / `isProbeSpiActive`)
4. `src/display_ui.cpp` (Display SPI Arbiter Guard)
5. `src/terminal/terminal.cpp` (SPI command dispatcher and telemetry formatter)

are certified as **VALIDATED & FROZEN**.

In compliance with the project development sequence, **I2C mode adaptation** is now unlocked as the next development milestone.
