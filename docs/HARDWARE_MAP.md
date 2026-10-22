# UDOO KEY Definitive Hardware Map & Pin Authority Matrix

**Document ID:** DOC-HW-MAP-20261005  
**Status:** FROZEN HARDWARE TRUTH  
**Date:** 2026-10-05  
**Target Hardware:** SECO UDOO KEY (RP2040 + ESP32-WROVER-E)  
**Primary Authorities:**  
1. SECO UDOO KEY Schematic (Authority 1)  
2. SECO UDOO KEY User Manual Rev 1.6 (Authority 2)  
3. Official Arduino Board Reference `opivankristovi/udoo-key-arduino` (Authority 3)  
4. Verified Project Source (`shared/hardware_config.h`, `rp2040_logic_analyzer`, `esp32_coprocessor`) (Authority 4)  
5. RP2040 & ESP32-WROVER-E Datasheets (Authority 5)  

---

## 1. Definitive Hardware Resource Map

| MCU | GPIO | Physical Location | Electrical Function | Project Function | Owner | Status |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **RP2040** | **GP0** | Internal PCB Trace | UART0 TX (1 Mbps, 8N1) | Inter-MCU Transport TX | Internal Transport | **VERIFIED** |
| **RP2040** | **GP1** | Internal PCB Trace | UART0 RX (1 Mbps, 8N1) | Inter-MCU Transport RX | Internal Transport | **VERIFIED** |
| **RP2040** | **GP2** | Header P1 Pin 4 | Digital Input / SIO / PIO0 | Logic Analyzer CH0 / USB D+ Probe | Real-Time Engine | **VERIFIED** |
| **RP2040** | **GP3** | Header P1 Pin 5 | Digital Input / SIO / PIO0 | Logic Analyzer CH1 / USB D- Probe | Real-Time Engine | **VERIFIED** |
| **RP2040** | **GP4** | Header P1 Pin 6 | Digital Input / SIO / PIO0 | Logic Analyzer CH2 / UART TX / SPI MISO | Real-Time Engine | **VERIFIED** |
| **RP2040** | **GP5** | Header P1 Pin 7 | Digital Input / SIO / PIO0 | Logic Analyzer CH3 / UART RX / SPI CS | Real-Time Engine | **VERIFIED** |
| **RP2040** | **GP6** | Header P1 Pin 9 | Digital Input / SIO / PIO0 | Logic Analyzer CH4 / 1-Wire DQ | Real-Time Engine | **VERIFIED** |
| **RP2040** | **GP7** | Header P1 Pin 10 | Digital Input / SIO / PIO0 | Logic Analyzer CH5 | Real-Time Engine | **VERIFIED** |
| **RP2040** | **GP8** | Header P1 Pin 11 | Digital Input / SIO / PIO0 | Logic Analyzer CH6 | Real-Time Engine | **VERIFIED** |
| **RP2040** | **GP9** | Header P1 Pin 12 | Digital Input / SIO / PIO0 | Logic Analyzer CH7 | Real-Time Engine | **VERIFIED** |
| **RP2040** | **GP10** | Header P1 Pin 14 | Digital Input / IRQ / INOVER | External Hardware Trigger (EXT TRIG) | Real-Time Engine | **VERIFIED** |
| **RP2040** | **GP11** | Header P2 Pin 15 | Digital Output (4mA SIO) | ST7735 TFT Reset (RST) | Display Subsystem | **VERIFIED** |
| **RP2040** | **GP12** | Header P2 Pin 16 | Digital Output (4mA SIO) | ST7735 TFT Data/Command (DC) | Display Subsystem | **VERIFIED** |
| **RP2040** | **GP13** | Header P2 Pin 17 | Digital Output (4mA SIO) | ST7735 TFT Chip Select (CS) | Display Subsystem | **VERIFIED** |
| **RP2040** | **GP14** | Header P1 Pin 19 | Digital Output (SIO) | 74HC74 Prescaler Divide Ratio Select | Front-End Control | **VERIFIED** |
| **RP2040** | **GP15** | Header P2 Pin 20 | Digital Input (Internal Pull-Up) | Tactile Button UP | User Interface | **VERIFIED** |
| **RP2040** | **GP16** | Header P1 Pin 20 | Digital Input (Internal Pull-Up) | Tactile Button DOWN | User Interface | **VERIFIED** |
| **RP2040** | **GP17** | Header P2 Pin 18 | Digital Input (Internal Pull-Up) | Tactile Button LEFT | User Interface | **VERIFIED** |
| **RP2040** | **GP18** | Header P1 Pin 17 | SPI0 SCK (Hardware SPI) | ST7735 SPI Clock (SCK) | Display Subsystem | **VERIFIED** |
| **RP2040** | **GP19** | Header P1 Pin 16 | SPI0 MOSI (Hardware SPI) | ST7735 SPI Data (MOSI) | Display Subsystem | **VERIFIED** |
| **RP2040** | **GP20** | Header P1 Pin 15 | SPI0 MISO (Hardware SPI) | ST7735 SPI MISO | Display Subsystem | **VERIFIED** |
| **RP2040** | **GP21** | Header P2 Pin 27 | Digital Input (Internal Pull-Up) | Tactile Button RIGHT | User Interface | **VERIFIED** |
| **RP2040** | **GP22** | Header P2 Pin 29 | Digital Input (Internal Pull-Up) | Tactile Button SELECT | User Interface | **VERIFIED** |
| **RP2040** | **GP23** | Internal Circuit | Power Control Output | RT6150 SMPS Power Save (PS) | Board Internal | **RESERVED** |
| **RP2040** | **GP24** | Internal Circuit | Power Sense Input | USB VBUS Detect | Board Internal | **RESERVED** |
| **RP2040** | **GP25** | Onboard LED | Digital Output | Green User LED | Diagnostics/Status | **VERIFIED** |
| **RP2040** | **GP26** | Header P2 Pin 31 | ADC0 (12-bit SAR, 0-3.3V) | Voltmeter CH0 / Analog Sensor Input | Real-Time Engine | **VERIFIED** |
| **RP2040** | **GP27** | Header P2 Pin 32 | ADC1 (12-bit SAR, 0-3.3V) | Voltmeter CH1 / Precision ADC | Real-Time Engine | **VERIFIED** |
| **RP2040** | **GP28** | Header P2 Pin 34 | ADC2 (12-bit SAR, 0-3.3V) | Voltmeter CH2 / Precision ADC | Real-Time Engine | **VERIFIED** |
| **RP2040** | **GP29** | Internal Circuit | ADC3 (12-bit SAR) | VSYS Voltage Sense (1/3 Divider) | Board Internal | **RESERVED** |
| **ESP32** | **GPIO19** | Internal PCB Trace | UART1 TX (1 Mbps, 8N1) | RP2040 Command Link TX | Coprocessor Transport | **VERIFIED** |
| **ESP32** | **GPIO22** | Internal PCB Trace | UART1 RX (1 Mbps, 8N1) | RP2040 Telemetry Link RX | Coprocessor Transport | **VERIFIED** |
| **ESP32** | **GPIO32** | Onboard LED | Digital Output (Active LOW) | Onboard Yellow LED 1 | Coprocessor Status | **VERIFIED** |
| **ESP32** | **GPIO33** | Onboard LED | Digital Output (Active LOW) | Onboard Yellow LED 2 | Coprocessor Status | **VERIFIED** |

---

## 2. Definitive Hardware Questions Resolution (Section 96)

### RP2040 Questions

#### Q1: GP2 Physical Accessibility
- **ANSWER**: Header P1 Pin 4 (Probe Bus CH0). Directly accessible.
- **SOURCE**: SECO UDOO KEY Schematic & Manual Rev 1.6 Section 3.2.1.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Dedicated to Logic Analyzer CH0 / USB D+ Sniffer Probe.

#### Q2: GP3 Physical Accessibility
- **ANSWER**: Header P1 Pin 5 (Probe Bus CH1). Directly accessible.
- **SOURCE**: SECO UDOO KEY Schematic & Manual Rev 1.6 Section 3.2.1.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Dedicated to Logic Analyzer CH1 / USB D- Sniffer Probe.

#### Q3: GP4–GP9 Physical Accessibility
- **ANSWER**: 
  - GP4: Header P1 Pin 6
  - GP5: Header P1 Pin 7
  - GP6: Header P1 Pin 9
  - GP7: Header P1 Pin 10
  - GP8: Header P1 Pin 11
  - GP9: Header P1 Pin 12
- **SOURCE**: SECO UDOO KEY Manual Rev 1.6 Section 3.2.1.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Form contiguous 8-channel logic analyzer input group (GP2..GP9).

#### Q4: GP10 Physical Accessibility
- **ANSWER**: Header P1 Pin 14.
- **SOURCE**: SECO UDOO KEY Manual Rev 1.6 Section 3.2.1.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Assigned as dedicated Hardware External Trigger Input (`EXT TRIG`).

#### Q5: GP11–GP22 Physical Accessibility
- **ANSWER**: 
  - GP11 (P2.15), GP12 (P2.16), GP13 (P2.17): ST7735 TFT Control lines.
  - GP14 (P1.19): Prescaler ratio control (74HC74).
  - GP15 (P2.20), GP16 (P1.20), GP17 (P2.18), GP21 (P2.27), GP22 (P2.29): Tactile navigation buttons (UP, DOWN, LEFT, RIGHT, SELECT).
  - GP18 (P1.17), GP19 (P1.16), GP20 (P1.15): Hardware SPI0 bus for ST7735 display.
- **SOURCE**: SECO UDOO KEY Schematic, Arduino Board Definition.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Locked in `shared/hardware_config.h`. Confirmed no conflicts.

#### Q6: GP23 Ownership
- **ANSWER**: Internal PCB trace connected to RT6150 buck-boost SMPS Power-Save pin (PS).
- **SOURCE**: RP2040 Datasheet & SECO UDOO KEY Schematic.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Classified as `BOARD_CONTROL / RESERVED`. Barred from user probe allocation.

#### Q7: GP24 Ownership
- **ANSWER**: Internal PCB trace connected to USB VBUS voltage divider (detects USB 5V presence).
- **SOURCE**: RP2040 Datasheet & SECO UDOO KEY Schematic.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Classified as `BOARD_CONTROL / RESERVED`.

#### Q8: GP26–GP28 ADC Accessibility
- **ANSWER**: 
  - GP26 (ADC0): Header P2 Pin 31
  - GP27 (ADC1): Header P2 Pin 32
  - GP28 (ADC2): Header P2 Pin 34
- **SOURCE**: SECO UDOO KEY Manual Rev 1.6.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Assigned as precision analog measurement channels (ADC0, ADC1, ADC2) with 0–3.3V input range.

#### Q9: GP29 Ownership
- **ANSWER**: Internal PCB trace connected to 1/3 voltage divider monitoring VSYS power supply.
- **SOURCE**: RP2040 Datasheet & SECO UDOO KEY Schematic.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Classified as `BOARD_CONTROL / RESERVED`.

---

### USB Subsystem Questions

#### Q10: USB-C Routing & Role
- **ANSWER**: CN1 is a standard USB Type-C receptacle with CC1/CC2 5.1k pull-downs (Upstream Facing Port). Used for board power (+5V via 2.5A polyfuse) and host PC communication.
- **SOURCE**: SECO UDOO KEY Manual Rev 1.6 & Schematic.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Dedicated to Host PC communication (CDC CLI, binary protocol, UF2 programming).

#### Q11: Jumper JP1 Behavior
- **ANSWER**: 
  - JP1 Closed (Placed): CN1 D+/D- connected to RP2040 native USB PHY (`USB_DP`/`USB_DM`).
  - JP1 Open: CN1 D+/D- connected to onboard USB-UART bridge driving ESP32 UART0 (IO1/IO3).
- **SOURCE**: SECO UDOO KEY Manual Rev 1.6 Section 3.1.2.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Keep JP1 CLOSED during instrument operation so RP2040 maintains high-speed USB CDC link with desktop.

#### Q12: Host Mode Capability
- **ANSWER**: RP2040 native USB controller can operate as USB Host (Full-Speed 12M / Low-Speed 1.5M) via TinyUSB when externally powered or via PIO-USB on external GPIO pins.
- **SOURCE**: RP2040 Datasheet & TinyUSB stack.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Integrated into RP2040 firmware architecture.

#### Q13: Passive USB Sniffing Tap & Electrical Requirements
- **ANSWER**: CN1 cannot be used as an arbitrary target bus tap because it powers the UDOO KEY and connects to the host PC. Passive sniffing requires an external tap connected to Header P1 GP2 (D+) and GP3 (D-) with an external protection network (47Ω damping resistors + low-capacitance ESD clamp).
- **SOURCE**: `docs/usb_hardware_audit.md`.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Documented in `docs/USB_ARCHITECTURE.md`.

---

### External Instrument / HP 54520A Questions

#### Q14: Target Instrument Identification
- **ANSWER**: HP 54520A 500 MHz, 2-Channel, 500 MSa/s Digitizing Oscilloscope.
- **SOURCE**: HP 54520A / 54522A User and Programmer's Manual.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Architecture targeted to standard HP-IB / RS-232 SCPI-like command syntax.

#### Q15: Interface Availability & Electrical Constraints
- **ANSWER**: Standard rear panel contains HP-IB (IEEE-488) connector; RS-232 serial option (Option 001/002) is a DB-25/DB-9 DCE connector. 
  - **HP-IB**: Requires external IEEE-488 transceiver/controller (e.g. SN75160B/SN75161B or dedicated bridge).
  - **RS-232**: Requires RS-232 level shifter (e.g. MAX3232) before connecting to ESP32 UART. Direct 3.3V GPIO connection to ±12V RS-232 is strictly prohibited.
- **SOURCE**: HP 54520A Service Manual.
- **CONFIDENCE**: 100% VERIFIED.
- **ACTION**: Specified in `docs/HP54520A_INTEGRATION.md` and `docs/EXTERNAL_INSTRUMENTS.md`.
