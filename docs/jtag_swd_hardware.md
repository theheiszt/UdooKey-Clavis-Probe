# UDOO KEY — JTAG / SWD Hardware Connection & Electrical Guide

## 1. Physical Pin Maps

### 1.1 External JTAG / SWD Probe Bus (RP2040 Header P1)
The primary interface for external targets is mapped to **Header P1** (Pins 6 to 12):

```text
               HEADER P1 PINOUT
            ┌──────────────────┐
     3.3V   │  1            2  │  GND
     GP0    │  3            4  │  GP1
     GP2    │  5            6  │  GP4  (JTAG TDI)
     GP6    │  7            8  │  GP5  (JTAG TDO)
     GP7    │  9           10  │  GP8  (JTAG TRST / SWDIO)
     GP10   │ 11           12  │  GP9  (JTAG SRST / SWD nRESET)
     GP14   │ 13           14  │  GP11
     GP20   │ 15           16  │  GP12
     GP15   │ 17           18  │  GP13
     GP16   │ 19           20  │  GP17
            └──────────────────┘
```

#### Detailed Pin Function Table
| Header P1 Pin | GPIO | JTAG Mode | SWD Mode | Logic Analyzer | Default Safe State |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Pin 6** | `GP4` | **TDI** (Output) | GPIO | Ch2 | Input with pull-up |
| **Pin 7** | `GP6` | **TCK** (Output) | GPIO | Ch4 | Input with pull-up |
| **Pin 8** | `GP5` | **TDO** (Input) | GPIO | Ch3 | Input with pull-up |
| **Pin 9** | `GP7` | **TMS** (Output) | **SWCLK** (Output) | Ch5 | Input with pull-up |
| **Pin 10** | `GP8` | **TRST** (Output)| **SWDIO** (Bidir) | Ch6 | Input with pull-up |
| **Pin 12** | `GP9` | **SRST** (Open-Drain)| **nRESET** (Open-Drain)| Ch7 | Input with pull-up |

---

### 1.2 Onboard RP2040 Self-Test Link (ESP32 Side)
The UDOO KEY contains hardwired debug links directly from the ESP32 to the RP2040:

```text
 ┌──────────────┐                       ┌──────────────┐
 │    ESP32     │                       │    RP2040    │
 │              │      Buffer (OE)      │              │
 │  GPIO5 (OE)  ├──────────►[OE]        │              │
 │  GPIO4 (CLK) ├──────────►[A -> B]───►│ SWCLK (P46)  │
 │  GPIO2 (DIO) ├──────────◄►[A <-> B]─►│ SWDIO (P45)  │
 │              │                       │              │
 │  GPIO23 (RST)├──────────────────────►│ RUN (Pin 26) │
 └──────────────┘                       └──────────────┘
```

- **Zero-Wire Self-Test**: Probes the onboard RP2040 ARM Cortex-M0+ DPIDR (`0x0BC12477`) with zero external jumpers.
- **Safety Isolation**: `GPIO5` (`SWD_OE`) is kept `LOW` (disabled) during normal operation to prevent any interference with RP2040 execution.

---

## 2. Electrical Specifications & Safety Rules

1. **Operating Voltage (3.3V LVCMOS)**:
   - All UDOO KEY GPIO lines are 3.3V LVCMOS.
   - **Do NOT connect directly to 5.0V systems** without level shifters.
2. **TDO Pin Protection**:
   - `GP5` (TDO) is strictly configured as `INPUT` with an internal weak pull-up (50 kOhm). It is never driven by the instrument.
3. **Open-Drain Reset Signals**:
   - `GP9` (SRST / nRESET) is driven actively LOW during reset and released to high-impedance with pull-up during normal operation to prevent bus fighting with external target reset circuits.
4. **SWDIO Turnaround**:
   - During SWD reads, an explicit turnaround clock cycle (`Trn`) is executed to ensure the master releases the bus before the target asserts ACK.
