# UDOO KEY — Universal Terminal Command Reference

**Document ID:** DOC-TERM-CMD-20261003  
**Status:** IMPLEMENTED (Milestone 1)  
**Date:** 2026-10-03  
**Target Hardware:** SECO UDOO KEY (RP2040)  

---

## 1. Top-Level Command Set (Milestone 1 Core)

The terminal parser accepts case-insensitive commands, tolerates arbitrary whitespace, and supports standard shorthand abbreviations.

| Command | Aliases | Description | Example Usage |
| :--- | :--- | :--- | :--- |
| **`?`** | `help`, `h` | Display global command reference and help menu | `?` |
| **`m`** | `mode` | Change active instrument mode (interactive or direct) | `m logic`, `m hiz` |
| **`info`** | `i` | Print system identity, clocks, memory, and hardware status | `info` |
| **`version`**| `v` | Print firmware version, Git commit, and build timestamp | `version` |
| **`config`** | `c` | Display or modify terminal settings (VT100 on/off, color) | `config` |
| **`cls`** | `clear` | Clear terminal screen and home cursor | `cls` |
| **`hiz`** | *(none)* | Immediately force all probe pins to safe High-Z state | `hiz` |
| **`reset`** | `reboot` | Perform software reset of the RP2040 microcontroller | `reset` |
| **`selftest`**| `test` | Run automated hardware self-test diagnostic suite | `selftest` |

---

## 2. Command Syntax & Argument Parsing

### Number Formats
The lexer accepts three standard numerical representations:
- **Hexadecimal**: Prefixed with `0x` or `0X` (e.g. `0x1A`, `0xFF`)
- **Decimal**: Plain digits (e.g. `1000000`, `25`)
- **Binary**: Prefixed with `0b` or `0B` (e.g. `0b10100101`)

### Mode Switching (`m`)
Typing `m` without arguments opens an interactive mode selection menu:
```text
HiZ> m

Available Modes:
 1. HiZ (Safe High-Impedance State)
 2. LOGIC (8-Channel 100MSPS Logic Analyzer)
 3. METER (Multifunction ADC & Continuity)
 4. I2C (I2C Master & Protocol Tools)
 5. SPI (SPI Master & Flash Interrogator)
 6. UART (Serial Terminal & Monitor)
 7. 1WIRE (Dallas 1-Wire Search & Read)
 8. USB (USB Electrical & Bus Tools)

Mode [1-8] >
```

Typing `m <mode_name>` directly switches mode:
```text
HiZ> m logic
Switched to LOGIC ANALYZER mode.
LOGIC>
```

---

## 3. Help System (`?` / `help`)

Executing `?` displays an organized help menu:

```text
==================================================
 UDOO KEY Universal Electronics Instrument (v2.1)
==================================================
General Commands:
  ? / help      Show this command help menu
  m [mode]      Select operating mode (HiZ, logic, meter, etc.)
  hiz           Immediately disable all outputs and enter safe HiZ
  info          Display system clocks, SRAM, flash, and voltages
  version       Display firmware version and build date
  cls / clear   Clear terminal screen
  config        Configure terminal features (VT100, color)
  selftest      Execute diagnostic self-test suite
  reset         Reboot RP2040 instrument

Active Mode Commands:
  (Available commands depend on the active mode prompt)
==================================================
```

---

## 4. System Information (`info`)

Outputs live system telemetry:
- MCU: Raspberry Pi RP2040 (Dual-Core ARM Cortex-M0+ @ 133 MHz)
- Coprocessor: Espressif ESP32-WROVER-E (1 Mbps UART Link)
- Internal Rails: VSYS voltage (measured via ADC3), Core temperature (ADC4)
- Memory: SRAM utilization, Flash size
- Terminal: VT100 mode state, columns, history depth
