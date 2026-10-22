# UDOO KEY — JTAG & SWD Debug Subsystem Specification

## 1. Executive Summary & Architecture

The UDOO KEY JTAG/SWD Debug Subsystem transforms the UDOO KEY dual-core platform into a professional-grade hardware debugger, boundary-scan analyzer, and ARM CoreSight probe. The system integrates tightly with the desktop companion application (`udoo-key-app`) while maintaining strict physical and electrical safety boundaries with onboard and external targets.

```text
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │                       DESKTOP APPLICATION (udoo-key-app)                    │
 │                                                                             │
 │    ┌────────────────────────┐  ┌────────────────────────┐  ┌─────────────┐  │
 │    │       JtagClient       │  │       SwdClient        │  │  DebugView  │  │
 │    │ (TAP FSM, Chain Enum)  │  │ (CoreSight, DPIDR, AP) │  │ (Qt GUI UI) │  │
 │    └───────────┬────────────┘  └───────────┬────────────┘  └──────┬──────┘  │
 │                │                           │                      │         │
 │                └─────────────┬─────────────┴──────────────────────┘         │
 │                              ▼                                              │
 │                      [IDebugTransport]                                      │
 │                       │             │                                       │
 │           ┌───────────┴──┐       ┌──┴────────────────┐                      │
 │           ▼              ▼       ▼                   ▼                      │
 │     MockDebugTransport        SerialDebugTransport (USB CDC)                │
 └───────────┬──────────────────────────────┬──────────────────────────────────┘
             │                              │
         (Test Sim)                   (Framed Sync: 0x5B)
             │                              │
             ▼                              ▼
 ┌─────────────────────────────────────────────────────────────────────────────┐
 │                          UDOO KEY HARDWARE PLATFORM                         │
 │                                                                             │
 │   [RP2040 Master Engine]                     [ESP32 Diagnostics Link]       │
 │   • Header P1 Pins 6..12                     • Direct Onboard SWD Probe     │
 │   • GP4: TDI      GP7: TMS / SWCLK           • IO2: SWDIO    IO5: SWD_OE    │
 │   • GP5: TDO      GP8: TRST / SWDIO          • IO4: SWCLK    IO23: nRESET   │
 │   • GP6: TCK      GP9: SRST / nRESET         • Zero-Wire Self-Test Target   │
 │   • 1 µs SYNC_OUT Hardware Marker            • Apple BLE/Wi-Fi Coexistence  │
 └─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. ARM CoreSight Serial Wire Debug (SWD) Subsystem

### 2.1 Physical Layer & Signaling
Serial Wire Debug (SWD) operates over a two-wire bidirectional physical interface:
- **SWCLK**: Master-driven clock signal (up to 10 MHz).
- **SWDIO**: Bidirectional data line with tri-state turnaround phases.

#### SWD Line Reset Sequence
To guarantee synchronization from an indeterminate target state:
1. **Reset Pattern**: Drive `SWDIO` HIGH for at least 50 consecutive clock cycles.
2. **Idle Cycle**: Drive `SWDIO` LOW for at least 2 clock cycles.

#### JTAG-to-SWD Switching
Targets supporting combined JTAG/SWD interfaces require a defined 16-bit switching sequence:
1. Issue SWD Line Reset (50+ clocks with `SWDIO` = 1).
2. Transmit 16-bit JTAG-to-SWD sequence `0xE79E` (LSB first: `0111 1001 1110 0111b`).
3. Issue SWD Line Reset (50+ clocks with `SWDIO` = 1).
4. Transmit at least 4 idle cycles (`SWDIO` = 0).

```text
SWCLK  ──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌── ... ──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──┐ ┌──
         └─┘  └─┘  └─┘  └─┘  └─┘         └─┘  └─┘  └─┘  └─┘  └─┘  └─┘
SWDIO  ────────────────────────── ... ─────────\________________________
       ◄──────── 50+ Clocks HIGH ───────► ◄── Trn ──► ◄── 4+ Idle ──►
```

### 2.2 Packet Protocol & Framing
Every SWD operation begins with an 8-bit packet request from the host:

| Bit | Field | Description |
| :--- | :--- | :--- |
| **0** | `START` | Must be `1`. |
| **1** | `APnDP` | `0` = Debug Port (DP) access, `1` = Access Port (AP) access. |
| **2** | `RnW` | `0` = Write transfer, `1` = Read transfer. |
| **3:4** | `A[2:3]` | 2-bit register address offset (`0x00`, `0x04`, `0x08`, `0x0C`). |
| **5** | `PARITY` | Single even parity bit over bits [1:4]. |
| **6** | `STOP` | Must be `0`. |
| **7** | `PARK` | Must be `1` (line driven high). |

#### Target Acknowledge (ACK)
Following the host request, an explicit **Turnaround (`Trn`)** period occurs where the host releases the line. The target then transmits a 3-bit ACK response:
- `001b` (1): **ACK_OK** — Operation accepted, proceeding to data phase.
- `010b` (2): **ACK_WAIT** — Target busy; transfer must be retried.
- `100b` (4): **ACK_FAULT** — Protocol or access error; check sticky flags in `CTRL/STAT`.

#### Data Phase & Turnaround
- **Read Operation**: Target drives 32-bit data word (LSB first) followed by 1 even parity bit, followed by 1 `Trn` cycle where the target floats and host reclaims control.
- **Write Operation**: Host executes 1 `Trn` cycle, drives 32-bit data word (LSB first) followed by 1 even parity bit.

### 2.3 Core DP & AP Register Map

#### Debug Port (DP) Registers
| Address | Access | Register | Purpose |
| :--- | :--- | :--- | :--- |
| `0x00` | Read | `DPIDR` | Identification Code Register |
| `0x00` | Write | `ABORT` | Clear sticky error flags and abort transactions |
| `0x04` | R/W | `CTRL/STAT`| Control and Status (power-up domains, debug clock) |
| `0x08` | Write | `SELECT` | AP Bank and Register Selector |
| `0x0C` | Read | `RDBUFF` | Read Buffer (returns previous read result) |

#### DPIDR Decoding
The 32-bit `DPIDR` field breakdown:
```text
 31       28 27                  17 16 12 11        1 0
┌───────────┬──────────────────────┬─────┬───────────┬───┐
│ REVISION  │     PART NUMBER      │MINDP│ DESIGNER  │ 1 │
│  (4-bit)  │       (11-bit)       │(1b) │  (11-bit) │   │
└───────────┴──────────────────────┴─────┴───────────┴───┘
```
- **RP2040 Expected Value**: `0x0BC12477`
  - Revision: `0x0`
  - Part Number: `0xBC1` (ARM Cortex-M0+ SW-DP)
  - MINDP: `0` (Full DP architecture)
  - Designer: `0x23B` (ARM Ltd., JEP-106 continuation `0x4`, identity `0x3B`)
  - Bit 0: Fixed `1`

---

## 3. IEEE 1149.1 JTAG Subsystem

### 3.1 TAP Finite State Machine
The IEEE 1149.1 Test Access Port (TAP) is controlled by `TCK` and `TMS`. The 16-state finite state machine consists of two symmetric branches (DR and IR):

```text
                           ┌──────────────────┐
                 ┌────────►│ Test-Logic-Reset │◄────────┐
                 │         └────────┬─────────┘         │
                 │ 1              0 │                   │ 1
                 │         ┌────────▼─────────┐         │
                 │    ┌───►│  Run-Test/Idle   │         │
                 │    │  0 └────────┬─────────┘         │
                 │    └─────────────┘ 1                 │
                 │         ┌──────────────────┐         │
                 │         │  Select-DR-Scan  ├─────────┤
                 │         └────────┬─────────┘ 1       │
                 │                0 │                   │
                 │         ┌────────▼─────────┐         │
                 │         │    Capture-DR    │         │
                 │         └────────┬─────────┘         │
                 │                0 │         1 ┌───┐   │
                 │         ┌────────▼─────────┐ │   │   │
                 │    ┌───►│     Shift-DR     │ │   │   │
                 │    │  0 └────────┬─────────┘ │   │   │
                 │    └─────────────┘ 1         │   │   │
                 │         ┌──────────────────┐ │   │   │
                 │         │     Exit1-DR     │◄┘   │   │
                 │         └────────┬─────────┘     │   │
                 │                0 │         1     │   │
                 │         ┌────────▼─────────┐     │   │
                 │    ┌───►│     Pause-DR     │     │   │
                 │    │  0 └────────┬─────────┘     │   │
                 │    └─────────────┘ 1             │   │
                 │         ┌──────────────────┐     │   │
                 │         │     Exit2-DR     │     │   │
                 │         └────────┬─────────┘     │   │
                 │                0 │         1     │   │
                 │         ┌────────▼─────────┐     │   │
                 │         │    Update-DR     │─────┴───┘
                 │         └──────────────────┘
                 │ (Symmetric branch for Select-IR-Scan)
```

### 3.2 State Machine Navigation Engine (`JtagTap`)
- **Breadth-First Search (BFS)**: `JtagTap::navigateTo(targetState)` computes the mathematically minimal TMS bitstream between any two states.
- **Deterministic Reset**: Five clock cycles with `TMS = 1` guarantees arrival at `Test-Logic-Reset` regardless of starting state:
  $$\text{Seq} = [1, 1, 1, 1, 1] \implies \text{State} \equiv \text{Test-Logic-Reset}$$

### 3.3 IEEE 1149.1 IDCODE Decoding (`JtagIdCode`)
32-bit IDCODE structure according to JEDEC Standard 106:
```text
 31       28 27                           12 11        1 0
┌───────────┬───────────────────────────────┬───────────┬───┐
│  VERSION  │          PART NUMBER          │  MANUF_ID │ 1 │
│  (4-bit)  │           (16-bit)            │  (11-bit) │   │
└───────────┴───────────────────────────────┴───────────┴───┘
```

#### Known Hardware IDCODE Signatures
| Device / Family | IDCODE (Hex) | Manufacturer | Part Number |
| :--- | :--- | :--- | :--- |
| **RP2040 Core 0/1** | `0x0BC12477` | ARM Ltd. (`0x23B`) | Cortex-M0+ (`0xBC1`) |
| **STM32F401/411** | `0x16423041` | STMicro (`0x020`) | STM32 Medium-Density |
| **ESP32-WROVER (Tensilica)**| `0x1000563D` | Tensilica/Espressif | Xtensa LX6 Core |
| **Xilinx Artix-7 XC7A35T** | `0x0362D093` | Xilinx (`0x049`) | XC7A35T FPGA |
| **Lattice iCE40UP5K** | `0x01215043` | Lattice (`0x06E`) | UltraPlus 5K |

---

## 4. Hardware Pin Mapping & Physical Interconnects

### 4.1 Header P1 External Debug Probe Bus (RP2040)
External target debug probes connect directly to **Header P1** (Pins 6–12):

```text
              HEADER P1 EXTERNAL PINOUT
            ┌───────────────────────────┐
     3.3V   │  1                     2  │  GND
     GP0    │  3                     4  │  GP1
     GP2    │  5                     6  │  GP4  (JTAG TDI)
     GP6    │  7 (JTAG TCK)          8  │  GP5  (JTAG TDO)
     GP7    │  9 (JTAG TMS / SWCLK) 10  │  GP8  (JTAG TRST / SWDIO)
     GP10   │ 11                    12  │  GP9  (JTAG SRST / SWD nRESET)
     GP14   │ 13                    14  │  GP11
     GP20   │ 15                    16  │  GP12
     GP15   │ 17                    18  │  GP13
     GP16   │ 19                    20  │  GP17
            └───────────────────────────┘
```

#### Signal Specifications
| Header Pin | RP2040 GPIO | Function | Default State | Electrical Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Pin 6** | `GP4` | **TDI** | Safe Input | High impedance; internal 50 kΩ pull-up |
| **Pin 7** | `GP6` | **TCK** | Safe Input | High impedance; driven push-pull during active session |
| **Pin 8** | `GP5` | **TDO** | Safe Input | Always Input; weak pull-up; captures on TCK falling edge |
| **Pin 9** | `GP7` | **TMS / SWCLK** | Safe Input | Push-pull driver for JTAG TMS or SWD clock |
| **Pin 10** | `GP8` | **TRST / SWDIO** | Safe Input | Push-pull (JTAG TRST) or dynamic bidirectional (SWDIO) |
| **Pin 12** | `GP9` | **SRST / nRESET**| Safe Input | Open-drain; pulled low for reset assertion; floats high |

### 4.2 Onboard Hardwired SWD Self-Test Link (ESP32 ↔ RP2040)
The UDOO KEY features a dedicated onboard link allowing the ESP32 coprocessor to debug, reset, and inspect the RP2040 with zero external jumper wires:

```text
 ┌───────────────────────────┐                       ┌───────────────────────────┐
 │           ESP32           │                       │          RP2040           │
 │        COPROCESSOR        │    74LVC Buffer       │      ACQUISITION CORE     │
 │                           │    ┌───────────┐      │                           │
 │  GPIO5 (SWD_OE)  ─────────┼───►│ OE        │      │                           │
 │  GPIO4 (SWCLK)   ─────────┼───►│ A ───► B  ├──────┼──► SWCLK (Pin 46)         │
 │  GPIO2 (SWDIO)   ◄────────┼───►│ A ◄─► B   ├──────┼──► SWDIO (Pin 45)         │
 │                           │    └───────────┘      │                           │
 │  GPIO23 (nRESET) ─────────┼───────────────────────┼──► RUN / RESET (Pin 26)   │
 └───────────────────────────┘                       └───────────────────────────┘
```

#### Operating Protocol for Onboard Self-Test
1. **Safety Gate**: `GPIO5` (`SWD_OE`) is held `LOW` at all times during normal instrument operation. This disconnects the ESP32 drivers and leaves RP2040 SWD lines floating/unaffected.
2. **Self-Test Activation**:
   - Drive `GPIO5` HIGH to enable buffer gates.
   - Configure `GPIO4` as output (clock) and `GPIO2` as open-drain/bidirectional (data).
   - Issue SWD line reset and read DPIDR at `0x00`.
   - Verify received DPIDR matches `0x0BC12477`.
   - Release `GPIO5` to LOW immediately upon test completion.

---

## 5. Binary Transport Protocol (`0x5B` Debug Protocol)

All JTAG and SWD transactions between host and device use a framed binary packet protocol with sync byte `0x5B`:

### 5.1 Frame Structure
```text
┌───────────┬───────────┬───────────┬─────────────┬───────────┐
│ Sync Byte │ Packet ID │ Length L  │   Payload   │ CRC16-CCITT
│   (0x5B)  │  (1 Byte) │ (2 Bytes) │  (L Bytes)  │  (2 Bytes)│
└───────────┴───────────┴───────────┴─────────────┴───────────┘
```

### 5.2 Command Set
| Packet ID | Command Name | Description |
| :--- | :--- | :--- |
| `0x01` | `DEBUG_CMD_ENTER` | Enter debug mode; configure pinmux and release logic analyzer |
| `0x02` | `DEBUG_CMD_EXIT` | Exit debug mode; return all pins to safe high-impedance state |
| `0x10` | `JTAG_CMD_RESET_TAP`| Execute 5+ clocks with TMS=1 into Run-Test/Idle |
| `0x11` | `JTAG_CMD_NAVIGATE` | Navigate TAP to requested target state |
| `0x12` | `JTAG_CMD_SCAN_IR` | Scan N bits into Instruction Register (capture TDO) |
| `0x13` | `JTAG_CMD_SCAN_DR` | Scan N bits into Data Register (capture TDO) |
| `0x14` | `JTAG_CMD_READ_ID` | Autonomous TAP reset + 32-bit DR shift to retrieve IDCODE |
| `0x20` | `SWD_CMD_LINE_RESET`| Execute 50+ clocks with SWDIO=1 followed by 4 idle clocks |
| `0x21` | `SWD_CMD_READ_DP` | Read Debug Port register (address 0x00 = DPIDR) |
| `0x22` | `SWD_CMD_WRITE_DP`| Write Debug Port register |
| `0x23` | `SWD_CMD_READ_AP` | Read Access Port register |
| `0x24` | `SWD_CMD_WRITE_AP`| Write Access Port register |
| `0x30` | `DEBUG_CMD_ASSERT_RST`| Drive SRST / nRESET LOW (active reset) |
| `0x31` | `DEBUG_CMD_DEASSERT_RST`| Release SRST / nRESET to high-Z (target running) |

---

## 6. Apple Diagnostics & System Synchronization

When paired with Apple diagnostic workflows (e.g. DFU recovery, accessory validation, USB-C Power Delivery state transitions), hardware debug operations can trigger or synchronize external analyzers:

1. **Hardware Marker Pulse (`PIN_SYNC_OUT`)**:
   - On assertion of JTAG/SWD reset or trigger conditions, the RP2040 pulses `PIN_SYNC_OUT` (GP9 or GP10) with a 1.0 µs strobe.
   - External logic analyzers or oscilloscopes capture this strobe, aligning physical trace captures with PC-side Apple diagnostic event logs.
2. **Nanosecond Time Correlation**:
   - Event markers carry 64-bit microsecond timestamps referenced to the RP2040 hardware timer (`time_us_64()`).
   - The desktop application correlates these hardware timestamps with host USB enumeration and BLE diagnostic events.
