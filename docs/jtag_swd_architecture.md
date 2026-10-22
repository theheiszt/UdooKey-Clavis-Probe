# UDOO KEY — JTAG / SWD Debug Subsystem Architecture

## 1. System Overview

The UDOO KEY JTAG/SWD Debug Subsystem provides professional hardware-level debugging, TAP state manipulation, chain enumeration, and ARM CoreSight access for external microcontroller/FPGA targets as well as onboard MCU self-testing.

```text
 ┌─────────────────────────────────────────────────────────────┐
 │               DESKTOP COMPANION APPLICATION                 │
 │                      (udoo-key-app)                         │
 │                                                             │
 │   ┌─────────────────┐ ┌─────────────────┐ ┌─────────────┐   │
 │   │   JtagClient    │ │    SwdClient    │ │  DebugView  │   │
 │   └────────┬────────┘ └────────┬────────┘ └──────┬──────┘   │
 │            │                   │                 │          │
 │            └─────────────┬─────┴─────────────────┘          │
 │                          ▼                                  │
 │                  [IDebugTransport]                          │
 │                    │           │                            │
 │         ┌──────────┴──┐     ┌──┴──────────────┐             │
 │         ▼             ▼     ▼                 ▼             │
 │   MockDebugTransport   SerialDebugTransport (USB CDC)       │
 └─────────┬──────────────────────┬────────────────────────────┘
           │                      │
       (Simulation)          (Binary Protocol Sync: 0x5B)
           │                      │
           ▼                      ▼
 ┌─────────────────────────────────────────────────────────────┐
 │                 UDOO KEY EMBEDDED HARDWARE                  │
 │                                                             │
 │  [RP2040 Probe Bus Engine]       [ESP32 Self-Test Probe]    │
 │   • Header P1 Pins 6..12          • Hardwired IO2/4/5/23    │
 │   • GP4: TDI    GP7: TMS/SWCLK    • Zero-wire RP2040 DPIDR  │
 │   • GP5: TDO    GP8: TRST/SWDIO     verification            │
 │   • GP6: TCK    GP9: SRST/nRESET  • Output Buffer OE Ctrl   │
 └─────────────────────────────────────────────────────────────┘
```

---

## 2. Desktop Subsystem (`udoo-key-app/src/debug`)

### 2.1 IEEE 1149.1 TAP Controller Model (`JtagTap`)
- **16 Standard States**: Implements the exact 16-state finite state machine specified by IEEE 1149.1:
  - `TestLogicReset`, `RunTestIdle`
  - `SelectDrScan`, `CaptureDr`, `ShiftDr`, `Exit1Dr`, `PauseDr`, `Exit2Dr`, `UpdateDr`
  - `SelectIrScan`, `CaptureIr`, `ShiftIr`, `Exit1Ir`, `PauseIr`, `Exit2Ir`, `UpdateIr`
- **Shortest-Path Navigation (`navigateTo`)**: Uses graph BFS to determine the exact minimal-length TMS clock sequence from any source state to any destination state.
- **Guaranteed Reset (`resetSequence`)**: 5 consecutive clocks with TMS=1 guarantees transition to `TestLogicReset` from ANY state.

### 2.2 Bit-Precise Scan Buffer (`JtagScanData`)
- Designed for non-byte-aligned transfers (e.g. 5-bit IR instructions, 37-bit boundary-scan vectors, 32-bit IDCODEs).
- LSB-first bit manipulation with direct conversion to/from `uint32_t`, `uint64_t`, and hexadecimal strings.

### 2.3 JEDEC JEP-106 Manufacturer Decoder (`JtagIdCode`)
- Decodes IEEE 1149.1 32-bit IDCODEs:
  - Bits [31:28]: Version
  - Bits [27:12]: Part Number
  - Bits [11:1]: Manufacturer code (JEP-106 continuation bank and identity code)
  - Bit [0]: Fixed '1'
- Built-in manufacturer table: ARM Ltd. (`0x23B`), STMicroelectronics (`0x020`), Texas Instruments (`0x017`), Microchip (`0x029`), NXP (`0x015`), Raspberry Pi (`0x4F5`), Espressif (`0x492`), Lattice (`0x06E`), Xilinx (`0x049`), Altera/Intel (`0x0C1`).

### 2.4 ARM CoreSight SWD Model (`SwdTypes`)
- **Header Generation (`SwdPacket::makeHeader`)**: Constructs standard 8-bit SWD request bytes with even parity (e.g. Read DPIDR = `0xA5`, Write ABORT = `0x81`).
- **DPIDR Decoder (`SwdDpidr`)**: Extracts architecture version, designer ID, part number, and MIN-DP flag (e.g. RP2040 Cortex-M0+ returns `0x0BC12477`).
- **Core DP / AP Register Addressing**:
  - DP: `IDCODE (0x00)`, `ABORT (0x00)`, `CTRL_STAT (0x04)`, `SELECT (0x08)`, `RDBUFF (0x0C)`.
  - AP: `CSW (0x00)`, `TAR (0x04)`, `DRW (0x0C)`, `IDR (0xFC)`.

---

## 3. Hardware Resource Allocation & Mutual Exclusion

### 3.1 RP2040 External Probe Bus (Header P1)
| Signal | GPIO | Header P1 Pin | Direction | Function |
| :--- | :--- | :--- | :--- | :--- |
| **TDI** | `GP4` | Pin 6 | Output | JTAG Test Data In |
| **TDO** | `GP5` | Pin 8 | Input (Pull-up) | JTAG Test Data Out |
| **TCK** | `GP6` | Pin 7 | Output | JTAG Test Clock |
| **TMS / SWCLK** | `GP7` | Pin 9 | Output | JTAG Mode Select / SWD Clock |
| **TRST / SWDIO**| `GP8` | Pin 10 | Bidirectional | JTAG Tap Reset / SWD Data I/O |
| **SRST / nRESET**| `GP9` | Pin 12 | Output (Open-Drain)| System Reset |

### 3.2 ESP32 Onboard RP2040 SWD Self-Test Link
| Signal | ESP32 GPIO | Connected RP2040 Signal | Function |
| :--- | :--- | :--- | :--- |
| **SWDIO** | `GPIO2` | RP2040 SWDIO (Pin 45) | Bidirectional debug data |
| **SWCLK** | `GPIO4` | RP2040 SWCLK (Pin 46) | Clock input to RP2040 |
| **SWD_OE**| `GPIO5` | External Level Buffer OE | Active HIGH driver enable |
| **/RPI_RESET** | `GPIO23` | RP2040 RUN (Pin 26) | Active LOW system reset |

### 3.3 Mutual Exclusion State Machine
When switching to JTAG or SWD debug modes:
1. Active Logic Analyzer sampling is stopped (`pio_sm_set_enabled(pio0, sm, false)`).
2. Shared pins GP4..GP9 are reconfigured from PIO inputs to SIO GPIO outputs/inputs.
3. Upon exiting debug mode, pins are returned to high-impedance safe state (`release_pins()`).
