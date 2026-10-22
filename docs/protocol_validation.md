# UDOO KEY — Protocol Subsystem Validation: GPIO / DIO Mode

**Document ID:** DOC-PROTO-DIO-20261003  
**Status:** VALIDATED & FROZEN (MILESTONE PROTO-1)  
**Date:** 2026-10-03  
**Target Hardware:** SECO UDOO KEY (RP2040 Dual Cortex-M0+ @ 133 MHz, REV O)  
**Firmware Version:** UDOO-KEY-OS v2.1-bp-terminal (UF2 Size: 395,264 bytes)  

---

## 1. Executive Summary

This document certifies the experimental validation, hardware safety reconciliation, and formal freeze of the **first Bus Pirate 5 protocol mode: Direct Digital I/O (GPIO / DIO)** adapted to the SECO UDOO KEY architecture.

Following the strict project sequence:
```text
TERMINAL (FROZEN) ──► GPIO / DIO (FROZEN) ──► SPI ──► I2C ──► UART ──► 1-WIRE ──► METER ──► LOGIC CLI ──► USB TOOLS
```
the GPIO/DIO subsystem has been built using a clean hardware abstraction layer (`udoo_hal/gpio_hal`), separating Bus Pirate 5 interactive terminal conventions from RP2040 silicon.

---

## 2. Hardware Architecture & Safety Invariants

### 2.1 Physical Pin Allocation (Header P1 Probe Bus)
The GPIO/DIO subsystem controls exclusively the 8 probe bus lines available on Header P1:

| Probe Index | RP2040 GPIO | Physical Connector | Signal Role / Alternate Functions | Safe Boot State |
| :---: | :---: | :---: | :---: | :---: |
| **CH0** | `GP2` | Header P1 Pin 4 | DIO / LA Ch0 / SPI SCK / I2C SDA | High-Z Input (Pulls Off) |
| **CH1** | `GP3` | Header P1 Pin 5 | DIO / LA Ch1 / SPI MOSI / I2C SCL | High-Z Input (Pulls Off) |
| **CH2** | `GP4` | Header P1 Pin 6 | DIO / LA Ch2 / SPI MISO / UART TX | High-Z Input (Pulls Off) |
| **CH3** | `GP5` | Header P1 Pin 7 | DIO / LA Ch3 / SPI CS / UART RX | High-Z Input (Pulls Off) |
| **CH4** | `GP6` | Header P1 Pin 9 | DIO / LA Ch4 / 1-Wire DQ | High-Z Input (Pulls Off) |
| **CH5** | `GP7` | Header P1 Pin 10 | DIO / LA Ch5 / SWD SWCLK | High-Z Input (Pulls Off) |
| **CH6** | `GP8` | Header P1 Pin 11 | DIO / LA Ch6 / SWD SWDIO | High-Z Input (Pulls Off) |
| **CH7** | `GP9` | Header P1 Pin 12 | DIO / LA Ch7 / AUX DIO | High-Z Input (Pulls Off) |

### 2.2 Protected System Pin Invariant
To prevent hardware damage and maintain instrument integrity, the `udoo_hal/gpio_hal` layer enforces a strict boundary rejection mechanism. Any command targeting pins outside `GP2..GP9` (e.g., `GP0`, `GP1`, `GP10..GP29`) is rejected with an explicit error:
* **GP0, GP1:** ESP32 UART0 hardware link (Hardwired PCB trace) — **PROTECTED**
* **GP10:** Dedicated EXT TRIG input — **PROTECTED**
* **GP11..GP13, GP15..GP17:** ST7735 TFT Control lines (CS, DC, RST) — **PROTECTED**
* **GP14:** 74HC74 hardware prescaler ratio control — **PROTECTED**
* **GP18..GP20:** Display SPI0 bus (SCK, MOSI, MISO) — **PROTECTED**
* **GP21, GP22:** Touch controller / Navigation buttons — **PROTECTED**
* **GP23..GP25, GP29:** Power rail controls, onboard LED, VSYS divider — **PROTECTED**
* **GP26..GP28:** Analog inputs (ADC0, ADC1, ADC2) — **PROTECTED**

### 2.3 Glitch-Free Direction Sequencing
When transitioning a pin from input to output, the output value is latched into the RP2040 SIO output register *prior* to enabling the output driver in the direction register:
```c
gpio_put(gpio, initial_level ? 1 : 0); // Latch value first
gpio_set_dir(gpio, GPIO_OUT);          // Enable driver glitch-free
```
This guarantees that devices under test (DUT) never observe transient glitches during direction changes.

### 2.4 Mode Isolation Lifecycle
Every mode transition follows the strict state machine:
```text
ANY MODE ──► leaveMode() ──► Safe High-Z ──► enterMode(DIO) ──► DIO>
```
When leaving DIO mode (or executing `hiz`), all 8 probe pins immediately return to SIO inputs with internal pull-up and pull-down resistors disabled.

---

## 3. Bus Pirate 5 Adapted Command Syntax

| Command | Shorthand | Parameters | Function & Description |
| :--- | :--- | :--- | :--- |
| `m dio` | `8` | None | Enters DIO mode; sets prompt to `DIO>`; dumps current bus table. |
| `read` | `r` | `[pin]` | If pin omitted: reads all 8 probe pins simultaneously. If specified: reads individual pin level. |
| `write` | `w` | `<pin> <0\|1>` | Configures pin as output (glitch-free) and drives logic 0 (LOW) or 1 (HIGH). |
| `dir` | `d` | `<pin> <in\|out>` | Sets pin direction to INPUT or OUTPUT. |
| `toggle` | `t` | `<pin>` | Inverts logic state of output pin. |
| `pull` | `p` | `<pin> <none\|up\|down>` | Configures internal pull resistor (UP = pull-up, DOWN = pull-down, NONE = float). |
| `mask` | None | `<dir_mask> <val_mask>` | Sets directions and levels across all 8 pins (e.g. `mask 0xFF 0xAA`). |
| `pins` | `dump` | None | Displays full VT100 formatted pin status table. |
| `0b...` / `0x...` | None | `<number>` | Direct 8-bit bus write (BP5 style numeric syntax). |
| `hiz` | None | None | Immediately returns to safe high-impedance mode (`HiZ>`). |

### 3.1 Interactive Bus Status Table Output
Executing `pins` or entering `m dio` presents an instantaneous status table:
```text
+-------+------+-------------+-----------+-------+----------+
| Probe | GPIO | Connector   | Direction | Level | Pull     |
+-------+------+-------------+-----------+-------+----------+
| CH0   | GP2  | Header P1.4 | INPUT     | LOW   | NONE     |
| CH1   | GP3  | Header P1.5 | INPUT     | HIGH  | PULL-UP  |
| CH2   | GP4  | Header P1.6 | OUTPUT    | HIGH  | NONE     |
| CH3   | GP5  | Header P1.7 | OUTPUT    | LOW   | NONE     |
| CH4   | GP6  | Header P1.9 | INPUT     | LOW   | NONE     |
| CH5   | GP7  | Header P1.10| INPUT     | LOW   | NONE     |
| CH6   | GP8  | Header P1.11| INPUT     | LOW   | NONE     |
| CH7   | GP9  | Header P1.12| INPUT     | LOW   | NONE     |
+-------+------+-------------+-----------+-------+----------+
Probe Bus State: 0x06 | Binary: 0b00000110 (CH7..CH0)
```

---

## 4. Test Suite Execution & Experimental Results

The automated regression test suite (`scratch/test_terminal_cli.py`) covers 20 test cases spanning terminal mechanics and DIO operations:

| Test ID | Test Scenario | Command Sequence | Expected Response | Result |
| :---: | :--- | :--- | :--- | :---: |
| **T01** | Prompt Detection | `\r\n` | `HiZ> ` prompt | **PASS** |
| **T02** | Help Menu | `?\r\n` | Global command listing | **PASS** |
| **T03** | System Telemetry | `info\r\n` | Clocks, flash, VSYS voltage | **PASS** |
| **T04** | Build Metadata | `version\r\n` | Framework and build date | **PASS** |
| **T05** | Mode Menu | `m\r\n` | 8 available operating modes | **PASS** |
| **T06** | Logic Mode Transition | `m logic\r\n` | `LOGIC>` prompt | **PASS** |
| **T07** | Safe Return to HiZ | `hiz\r\n` | `HiZ>` prompt | **PASS** |
| **T08** | Line Ending: LF | `info\n` | Clean command execution | **PASS** |
| **T09** | Line Ending: CR | `version\r` | Clean command execution | **PASS** |
| **T10** | VT100 History Recall | `\x1b[A\r\n` | Last command recalled | **PASS** |
| **T11** | Mode Switch to DIO | `m dio\r\n` | `DIO>` prompt & pin table | **PASS** |
| **T12** | Probe Bus Table | `pins\r\n` | Table header & 8 probe channels | **PASS** |
| **T13** | Direction Setting | `dir 0 out\r\n` | `CH0 (GP2) direction set to OUTPUT` | **PASS** |
| **T14** | Logic High Output | `write 0 1\r\n` | `CH0 (GP2) driven HIGH (1)` | **PASS** |
| **T15** | Single Pin Read | `read 0\r\n` | `Level is HIGH (1)` | **PASS** |
| **T16** | Output Toggle | `toggle 0\r\n` | `toggled to LOW (0)` | **PASS** |
| **T17** | Pull-Up Enable | `pull 1 up\r\n` | `CH1 (GP3) pull resistor set to UP` | **PASS** |
| **T18** | Multi-Pin Mask Write | `mask 0x0F 0x05\r\n` | `DirMask=0x0F, ValMask=0x05` applied | **PASS** |
| **T19** | System Pin Protection | `write 10 1\r\n` | `Invalid probe pin` error | **PASS** |
| **T20** | Safe Fallback to HiZ | `hiz\r\n` | `HiZ>` prompt; all pins High-Z | **PASS** |

---

## 5. Firmware Resource Utilization

Compiled using Earle Philhower Arduino-Pico core v6.1.1 on GCC 12.3 (PlatformIO release build):
* **RAM Consumption:** 163,256 bytes / 262,144 bytes (**62.3%**)
* **Flash Consumption:** 185,448 bytes / 16,248,832 bytes (**1.1%**)
* **Binary UF2 Image:** 395,264 bytes (`firmware.uf2`)
* **Logic Analyzer PIO/DMA Engine:** 100MSPS PIO0 SM0 + DMA circular ring (64 KiB) completely preserved without memory displacement.

---

## 6. Formal Protocol Baseline Freeze Declaration

The GPIO / DIO subsystem components:
1. `src/udoo_hal/gpio_hal.h`
2. `src/udoo_hal/gpio_hal.cpp`
3. `src/instrument_mode_manager.h` / `.cpp` (`InstrumentMode::DIO`)
4. `src/bus_pirate_core.h` / `.cpp` (`BP_MODE_DIO`)
5. `src/terminal/terminal.cpp` (DIO command lexer and table formatter)

are certified as **VALIDATED & FROZEN**.

In compliance with the project development protocol, SPI mode adaptation is now unlocked as the next development milestone.
