# UDOO KEY — JTAG / SWD Subsystem Audit & Technical Specification

## 1. Existing System Architecture

The **UDOO KEY** instrument is an integrated dual-MCU diagnostic workbench comprising:
1. **Raspberry Pi RP2040** (Dual ARM Cortex-M0+ @ 133 MHz, 264 KiB SRAM, 8 MB QSPI Flash):
   - **Primary Real-Time Engine**: Owns the 8-channel 100 MSPS logic analyzer (PIO0 SM0 circular DMA into 64 KiB SRAM), trigger engine, analog voltmeter/continuity (ADC0..ADC2), 1.44" ST7735 TFT display (SPI0), navigation keypad, and native USB 1.1 Full-Speed CDC console.
2. **Espressif ESP32-WROVER-E** (Dual Xtensa LX6 @ 240 MHz, 16 MB Flash, 8 MB PSRAM, Wi-Fi 802.11b/g/n, BLE 4.2):
   - **Coprocessor & Wireless Communications Engine**: Owns the HTTP/WebSocket wireless scope server, mobile HTML5 waveform streamer, Bluetooth, and auxiliary I2C/PWM peripheral tools.
3. **Host Desktop Application (`udoo-key-app`)**:
   - Modern native C++20 desktop application built on Qt 6.8.2 and CMake.
   - Modular layered architecture: `ITransport` abstraction (`SerialTransport`, `MockTransport`), `StreamDemux` (0x5A binary, SUMP, ASCII terminal), `BinaryLogicClient`, `SumpClient`, `TerminalClient`, multi-resolution `LodDecimator`, multi-protocol decoders (UART, I2C, SPI), and high-contrast dark UI views.

---

## 2. Existing Inter-MCU Communication

The two microcontrollers are interconnected on the PCB via four dedicated electrical paths:
1. **Native Factory UART Link (Copper PCB Traces)**:
   - **RP2040 UART0**: GP0 (TX), GP1 (RX)
   - **ESP32 UART1**: GPIO19 (TX), GPIO22 (RX)
   - Operating speed: **1,000,000 baud (1 Mbps, 8N1)**.
   - Carries bidirectional framed packets (`InstrumentProtocol` with `0x5A` / `0xAA` framing, CRC-16, and sequence numbers) as well as ASCII terminal command forwarding.
2. **Hardware Reset Control**:
   - **ESP32 GPIO23** is hardwired directly to the RP2040 `/RPI_RESET` (RUN) pin (active LOW). The ESP32 can reboot or hold the RP2040 in reset.
3. **Hardwired SWD Programming/Debug Link**:
   - **ESP32 GPIO2**: Hardwired to RP2040 `SWDIO`.
   - **ESP32 GPIO4**: Hardwired to RP2040 `SWCLK`.
   - **ESP32 GPIO5**: Hardwired to the external SWD buffer output enable (`SWD_OE`).
4. **Optional High-Speed Bus (UEXT / Jumpered)**:
   - SPI interface up to 10 MHz on UEXT pins with a dedicated `DATA_READY` interrupt line.

---

## 3. Physical GPIO Ownership & Pin Allocations

### 3.1 RP2040 Complete Pin Map
| Pin | Physical Location | Electrical Function | Instrument Ownership | Can Use for JTAG/SWD? |
| :--- | :--- | :--- | :--- | :--- |
| **GP0** | PCB Trace | UART0 TX | Inter-MCU Link | **NO** (Reserved) |
| **GP1** | PCB Trace | UART0 RX | Inter-MCU Link | **NO** (Reserved) |
| **GP2** | Header P1 Pin 4 | PIO0 / Digital In | Probe Bus CH0 (LA) | **YES** (Shared Probe Bus) |
| **GP3** | Header P1 Pin 5 | PIO0 / Digital In | Probe Bus CH1 (LA) | **YES** (Shared Probe Bus) |
| **GP4** | Header P1 Pin 6 | PIO0 / SIO | Probe Bus CH2 (LA) | **YES: JTAG TDI** |
| **GP5** | Header P1 Pin 7 | PIO0 / SIO | Probe Bus CH3 (LA) | **YES: JTAG TDO** |
| **GP6** | Header P1 Pin 9 | PIO0 / SIO | Probe Bus CH4 (LA) | **YES: JTAG TCK** |
| **GP7** | Header P1 Pin 10 | PIO0 / SIO | Probe Bus CH5 (LA) | **YES: JTAG TMS / SWCLK** |
| **GP8** | Header P1 Pin 11 | PIO0 / SIO | Probe Bus CH6 (LA) | **YES: SWDIO / JTAG TRST** |
| **GP9** | Header P1 Pin 12 | PIO0 / SIO | Probe Bus CH7 (LA) | **YES: JTAG SRST / nRESET** |
| **GP10** | Header P1 Pin 14 | Digital In / IRQ | Ext Hardware Trigger | **NO** (Preserve trigger) |
| **GP11** | Header P2 Pin 15 | Digital Out | ST7735 TFT Reset | **NO** (Display) |
| **GP12** | Header P2 Pin 16 | Digital Out | ST7735 TFT DC | **NO** (Display) |
| **GP13** | Header P2 Pin 17 | Digital Out | ST7735 TFT CS | **NO** (Display) |
| **GP14** | Header P1 Pin 19 | Digital Out | 74HC74 Prescaler | **NO** (Front-End) |
| **GP15** | Header P2 Pin 20 | Digital In (Pullup) | Button UP | **NO** (User UI) |
| **GP16** | Header P1 Pin 20 | Digital In (Pullup) | Button DOWN | **NO** (User UI) |
| **GP17** | Header P1 Pin 19 / P2.18 | Digital In (Pullup) | Button LEFT | **NO** (User UI) |
| **GP18** | Header P1 Pin 17 | SPI0 SCK | ST7735 SPI Clock | **NO** (Display) |
| **GP19** | Header P1 Pin 16 | SPI0 MOSI | ST7735 SPI MOSI | **NO** (Display) |
| **GP20** | Header P1 Pin 15 | SPI0 MISO / PWM | ST7735 MISO / Buzzer | **NO** (Display/Audio) |
| **GP21** | Header P2 Pin 27 | Digital In (Pullup) | Button RIGHT | **NO** (User UI) |
| **GP22** | Header P2 Pin 29 | Digital In (Pullup) | Button SELECT | **NO** (User UI) |
| **GP23** | Internal | Power Control | RT6150 SMPS PS | **NO** (Internal Power) |
| **GP24** | Internal | Power Sense | USB VBUS Detect | **NO** (Internal Power) |
| **GP25** | Onboard LED | Digital Out | Green User LED | **NO** (Status Indicator) |
| **GP26** | Header P2 Pin 31 | ADC0 (12-bit SAR) | DMM Voltmeter CH0 | **NO** (Analog) |
| **GP27** | Header P2 Pin 32 | ADC1 (12-bit SAR) | DMM Voltmeter CH1 | **NO** (Analog) |
| **GP28** | Header P2 Pin 34 | ADC2 (12-bit SAR) | DMM Voltmeter CH2 | **NO** (Analog) |
| **GP29** | Internal | ADC3 (12-bit SAR) | VSYS Monitor | **NO** (Internal Power) |

### 3.2 ESP32 Complete Pin Map
| Pin | Physical Location | Electrical Function | Instrument Ownership | Can Use for JTAG/SWD? |
| :--- | :--- | :--- | :--- | :--- |
| **GPIO0** | Pushbutton SW3 | Boot / User Button | Bootstrapping | **NO** |
| **GPIO2** | PCB Trace | Internal SWD | **RP2040 SWDIO Line** | **YES: Onboard SWD SWDIO** |
| **GPIO4** | PCB Trace | Internal SWD | **RP2040 SWCLK Line** | **YES: Onboard SWD SWCLK** |
| **GPIO5** | PCB Trace | Output Enable | **RP2040 SWD_OE Buffer** | **YES: Buffer Enable** |
| **GPIO12** | Header P3 Pin 8 | SPI MOSI | UEXT Expansion | **YES: External JTAG TDI** |
| **GPIO13** | Header P3 Pin 3 | UART2 TX | UEXT Expansion | **YES: External JTAG TRST** |
| **GPIO14** | Header P3 Pin 9 | SPI SCK | UEXT Expansion | **YES: External JTAG TCK** |
| **GPIO15** | Header P3 Pin 10 | SPI CS# | UEXT Expansion | **YES: External JTAG TMS** |
| **GPIO16** | Header P3 Pin 4 | UART2 RX | UEXT Expansion | Available |
| **GPIO17** | Header P3 Pin 3 | UART2 TX (Alt) | UEXT Expansion | Available |
| **GPIO18** | Header P3 Pin 6 | I2C SDA | UEXT Expansion | Available |
| **GPIO19** | PCB Trace | UART1 TX (1 Mbps) | Inter-MCU Link TX | **NO** (Reserved) |
| **GPIO21** | Header P3 Pin 5 | I2C SCL | UEXT Expansion | Available |
| **GPIO22** | PCB Trace | UART1 RX (1 Mbps) | Inter-MCU Link RX | **NO** (Reserved) |
| **GPIO23** | PCB Trace | Output (Active LOW)| **RP2040 Hardware Reset**| **YES: Target Reset** |
| **GPIO25** | External / Test | PWM / DAC | Auxiliary Tool | Available |
| **GPIO26** | Header P3 Pin 4 | ADC / UART | Auxiliary Tool | Available |
| **GPIO32** | Onboard LED | Digital Out | Yellow LED 1 | Status |
| **GPIO33** | Onboard LED | Digital Out | Yellow LED 2 | Status |
| **GPIO35** | Header P3 Pin 7 | Input Only | UEXT SPI MISO | **YES: External JTAG TDO** |

---

## 4. Confirmed JTAG-Capable Pins

### Primary External Target Interface (RP2040 Probe Bus — Header P1)
When operating in **JTAG Debug Mode**, the RP2040 Probe Bus (Header P1) is dynamically reconfigured from high-impedance logic inputs into a high-speed JTAG master:
- **TCK** (Test Clock): **RP2040 GP6** (Header P1 Pin 9) — Output.
- **TMS** (Test Mode Select): **RP2040 GP7** (Header P1 Pin 10) — Output.
- **TDI** (Test Data In to target): **RP2040 GP4** (Header P1 Pin 6) — Output.
- **TDO** (Test Data Out from target): **RP2040 GP5** (Header P1 Pin 7) — Input (with configurable pull-up).
- **TRST** (Optional Test Reset): **RP2040 GP8** (Header P1 Pin 11) — Output (Active LOW).
- **SRST / nRESET** (Optional System Reset): **RP2040 GP9** (Header P1 Pin 12) — Output (Open-Drain / Active LOW).

### Secondary External Target Interface (ESP32 UEXT — Header P3)
- **TCK**: ESP32 GPIO14 (Header P3 Pin 9)
- **TMS**: ESP32 GPIO15 (Header P3 Pin 10)
- **TDI**: ESP32 GPIO12 (Header P3 Pin 8)
- **TDO**: ESP32 GPIO35 (Header P3 Pin 7, dedicated Input-Only pin — electrical match for TDO)
- **TRST**: ESP32 GPIO13 (Header P3 Pin 3)

---

## 5. Confirmed SWD Pins

### External Target SWD (RP2040 Probe Bus — Header P1)
- **SWCLK**: **RP2040 GP7** (Header P1 Pin 10) — Output.
- **SWDIO**: **RP2040 GP8** (Header P1 Pin 11) — Bidirectional with turnaround control.
- **nRESET**: **RP2040 GP9** (Header P1 Pin 12) — Active LOW system reset output.

### Onboard RP2040 Self-Test Target (ESP32 Onboard Hardware Link)
The UDOO KEY PCB features a hardwired link between the ESP32 and the RP2040's native ARM CoreSight debug port:
- **SWCLK**: **ESP32 GPIO4**
- **SWDIO**: **ESP32 GPIO2**
- **SWD_OE**: **ESP32 GPIO5** (enables the level buffer linking ESP32 to RP2040)
- **nRESET**: **ESP32 GPIO23** (controls the RP2040 RUN line)
This allows the ESP32 to query the RP2040 CoreSight DP IDCODE (`0x0BC12477`, ARM Cortex-M0+ r0p1) and inspect debug registers without any external wiring.

---

## 6. Pin Conflict Analysis & Safety Isolation

1. **Probe Bus Multiplexing (GP2..GP9)**:
   - In Logic Analyzer mode, GP2..GP9 are pure digital inputs sampled continuously by PIO0 SM0.
   - In JTAG/SWD mode, GP4..GP9 are driven as debug master signals.
   - **Mitigation**: An explicit, mutual-exclusion hardware mode switch (`instrument_mode_manager`) guarantees that when switching to JTAG/SWD, the PIO0 acquisition engine is paused, and pins are configured with safe transitions (HiZ first, then driven). Upon leaving debug mode or on disconnection, pins immediately return to high-impedance (HiZ) inputs.
2. **ESP32 Boot Strapping (GPIO2)**:
   - ESP32 GPIO2 is a strapping pin that must be LOW during ESP32 power-on.
   - On the UDOO KEY PCB, the hardware buffer `SWD_OE` (controlled by GPIO5) isolates GPIO2 from the RP2040 until explicitly enabled in firmware.
3. **Internal Pins**:
   - GP23 (SMPS PS), GP24 (VBUS sense), and GP29 (VSYS) are permanently locked against any user or probe allocation.

---

## 7. Transport Limitations & Batch Transaction Requirement

1. **Host-to-Device Latency**:
   - Standard USB CDC transport experiences a round-trip latency of approximately 1 millisecond per packet.
   - If the host PC attempted to bit-bang JTAG pins over individual serial commands (e.g. PC sends `TCK=0`, `TCK=1`, `TMS=1`), a single 32-bit IDCODE scan would take over 100 milliseconds, and scanning a chain or shifting an instruction would take seconds.
2. **Deterministic Batching Mandate**:
   - The desktop application must **never** bit-bang JTAG/SWD over raw serial byte commands.
   - The desktop app constructs a **`JtagTransaction`** (or `SwdTransaction`) consisting of discrete high-level operations: `ResetTap`, `ClockTms`, `ShiftIR`, `ShiftDR`, `RunIdle`.
   - The embedded debug engine executes the entire transaction sequence in local hardware memory with deterministic microsecond/sub-microsecond clocking, records TDO bits, and returns a single structured binary response.

---

## 8. Recommended Physical Routing & Architecture

```text
┌─────────────────────────────────────────────────────────────┐
│               DESKTOP APPLICATION (udoo-key-app)            │
│                                                             │
│   [DebugView] ──────► [DebugSession]                        │
│                             │                               │
│              ┌──────────────┴──────────────┐                │
│              ▼                             ▼                │
│        [JtagClient]                   [SwdClient]           │
│              │                             │                │
│              └──────────────┬──────────────┘                │
│                             ▼                               │
│                   [DebugTransport]                          │
│                             │                               │
│                             ▼                               │
│                   [ITransport (Serial)]                     │
└─────────────────────────────┼───────────────────────────────┘
                              │ USB CDC (921,600 / 2,000,000 baud)
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                 UDOO KEY EMBEDDED ENGINE                    │
│                                                             │
│                     [DebugProtocol]                         │
│                             │                               │
│              ┌──────────────┴──────────────┐                │
│              ▼                             ▼                │
│        [JtagEngine]                   [SwdEngine]           │
│        (IEEE 1149.1)                  (ARM SW-DP)           │
│              │                             │                │
│              ▼                             ▼                │
│        [Probe Bus GP4..9]            [Probe Bus GP7..8]     │
│        (Header P1 Pins 6..12)        (External Target)      │
│                                            │                │
│                                      [ESP32 IO2/4/5/23]     │
│                                      (RP2040 Onboard SWD)   │
└─────────────────────────────────────────────────────────────┘
```

---

## 9. Engineering Risks & Mitigations

| Risk | Severity | Cause | Engineering Mitigation |
| :--- | :--- | :--- | :--- |
| **Logic Analyzer Collisions** | High | JTAG driving pins while PIO0 LA is sampling | Mutual exclusion mode state machine (`INSTRUMENT_MODE_DEBUG`). LA acquisition is halted and isolated before debug pins are configured. |
| **Voltage Mismatch** | High | External target operates at 5.0V or 1.8V while UDOO KEY is 3.3V LVCMOS | Display prominent target voltage caution in UI. Measure target voltage via ADC if connected to VTREF. Never drive target before user confirmation. |
| **Bus Contention on TDO** | Medium | Driving target TDO pin as output from instrument | TDO is strictly configured as `INPUT` with internal weak pull-up. |
| **SWDIO Turnaround Timing** | High | Miscalculating turnaround clock cycle in SWD | Implement explicit SWD turnaround clock cycle (`Trn`) switching pin direction between output and input pull-up before sampling ACK and read data. |
| **False-Positive 0x5A Framing** | Medium | Binary debug packets colliding with terminal ASCII | Use established `StreamDemux` framing with distinct debug opcodes, length validation, and CRC-16. |

---

## 10. Comprehensive Implementation Plan

### Stage 1: Core JTAG Types, TAP State Machine & SWD Models (C++)
- Implement IEEE 1149.1 16-state TAP state machine (`JtagTapState`, `JtagTap`).
- Deterministic transition table mapping for `TMS=0` and `TMS=1`.
- Navigation method: `gotoState(targetState)` generating exact minimal TMS sequences.
- Bit-precise scan buffers (`JtagScanData`) supporting non-byte-aligned scans (e.g. 5-bit IR, 37-bit DR).
- JEP106 manufacturer ID decoding table (ARM, ST, TI, Microchip, NXP, Espressif, Raspberry Pi, etc.).
- JTAG IDCODE and chain enumeration data structures (`JtagIdCode`, `JtagDevice`).
- SWD Core: packet structures, DPIDR decoder, ACK states (`OK`, `WAIT`, `FAULT`).

### Stage 2: Binary Debug Protocol Specification
- Header format: `[SYNC: 0x5B] [CMD] [SEQ] [LEN_MSB] [LEN_LSB] [PAYLOAD...] [CRC16]`.
- Command set:
  * `DEBUG_GET_STATUS` (0x01)
  * `JTAG_RESET` (0x10)
  * `JTAG_SET_CLOCK` (0x11)
  * `JTAG_TMS_SEQ` (0x12)
  * `JTAG_SHIFT_IR` (0x13)
  * `JTAG_SHIFT_DR` (0x14)
  * `JTAG_TRANSACTION` (0x15)
  * `JTAG_SCAN_CHAIN` (0x16)
  * `SWD_RESET` (0x20)
  * `SWD_CONNECT` (0x21)
  * `SWD_SET_CLOCK` (0x22)
  * `SWD_READ_DP` (0x23)
  * `SWD_WRITE_DP` (0x24)
  * `SWD_READ_AP` (0x25)
  * `SWD_WRITE_AP` (0x26)

### Stage 3: Mock Debug Transport & Automated CTest Suite
- Create `MockDebugTransport` simulating:
  * 16-state TAP transitions and reset verification.
  * Multi-device JTAG chains (e.g. Device 0: STM32F407, Device 1: FPGA).
  * 5-bit IR and non-byte-aligned DR shifts.
  * SWD line reset, JTAG-to-SWD switching, and ARM Cortex-M0+ DPIDR (`0x0BC12477`).
- Integrate into `udoo_key_tests` and verify 100% test pass under CTest.

### Stage 4: Desktop Application Client & UI Implementation
- Implement `JtagClient` and `SwdClient` over `DebugTransport`.
- Implement `DebugSession` managing connection lifecycle, clock, and target telemetry.
- Implement `DebugView` UI in `udoo-key-app`:
  * JTAG Panel: TAP state indicator, clock selector (100 kHz .. 10 MHz), [Reset TAP], [Scan Chain], interactive chain tree view, IR/DR manual shift boxes, raw transaction log.
  * SWD Panel: Target connect, DP IDCODE read, core information badge, DP/AP register inspector.
- Add "🛠 Debug (JTAG/SWD)" tab to `MainWindow`.

### Stage 5: Embedded Debug Engine in Firmware
- Embed `JtagEngine` and `SwdEngine` in firmware.
- Support batch transaction execution in hardware.
- Support onboard RP2040 SWD self-interrogation from ESP32.
- Build and verify firmware targets.
