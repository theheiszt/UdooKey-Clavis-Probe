# UDOO KEY — USB Hardware & Physical Layer Audit

**Document ID:** DOC-USB-HW-20261003  
**Status:** VALIDATED AUDIT (Phase USB-0)  
**Date:** 2026-10-03  
**Target Hardware:** SECO UDOO KEY (RP2040 + ESP32-WROVER-E)  
**Authority Reference:** SECO UDOO KEY User Manual Rev 1.6 & Schematic  

---

## 1. Executive Summary

This document establishes the authoritative hardware reality of the USB subsystem on the SECO UDOO KEY board. It assesses the feasibility, safety, constraints, and architecture for:
1. USB physical-layer bus probing (electrical monitoring)
2. USB protocol sniffing (Low-Speed 1.5 Mbps & Full-Speed 12 Mbps)
3. USB Host and Device capabilities
4. USB hardware protection and signal integrity constraints

### Non-Negotiable Findings
- **Physical Connector**: The UDOO KEY features **exactly one USB connector** (`CN1`, USB Type-C receptacle). There are no secondary USB ports, USB-A host receptacles, or micro-USB connectors on the board.
- **Controller Allocation**:
  - The **RP2040** contains an integrated USB 1.1 controller (Host/Device, 12 Mbps Full-Speed / 1.5 Mbps Low-Speed) with an on-die PHY on pins `USB_DP` and `USB_DM`.
  - The **ESP32-WROVER-E** (ESP32-D0WD-V3) has **NO native USB PHY or USB controller**. It relies on an external UART-to-USB bridge IC to communicate over USB.
- **Routing & Jumper JP1**: `CN1`'s physical D+ and D- lines are switched via jumper `JP1`:
  - `JP1` Placed (Closed): Connected directly to RP2040 `USB_DP` and `USB_DM`.
  - `JP1` Not Placed (Open): Connected to onboard UART-to-USB bridge driving ESP32 UART0.
- **Header Accessibility**: Neither `CN1` D+/D- nor RP2040 `USB_DP`/`USB_DM` are broken out to external headers P1, P2, P3, P4, or P5.
- **Sniffing Architecture**: Because `CN1` is mandatory for board power and host PC communication (USB CDC serial / PulseView / CLI), **passive USB sniffing of an external target device cannot use CN1**. External USB sniffing must use the instrument's external probe inputs (GP2..GP9) through a dedicated protection and conditioning probe interface.

---

## 2. USB Interface Authority Matrix

| Interface | Connector | USB Version | Controller / PHY | D+ Signal | D- Signal | Host / Device Capability | Physically Accessible on Headers? | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Primary USB** | **CN1** (Type-C) | USB 2.0 Physical (USB 1.1 Protocol) | **RP2040 Native USB PHY** (when `JP1` Closed) | RP2040 `USB_DP` (Pin 47) | RP2040 `USB_DM` (Pin 46) | Device (Active CDC) / Host Capable | **NO** (Not on P1/P2/P3 headers) | Board power in (+5V, 2.5A polyfuse). Used for CLI / SUMP. |
| **ESP32 Console** | **CN1** (Type-C) | USB 2.0 FS | **UART-to-USB Bridge IC** (when `JP1` Open) | Bridge D+ | Bridge D- | Device Only (Virtual COM) | **NO** (Connects internally to ESP32 IO1/IO3) | ESP32 flashing & monitoring. ESP32 has no USB PHY. |
| **External Sniffer Probe** | **Header P1** (Pins 4, 5, 14) | USB 1.1 (LS: 1.5M, FS: 12M) | **RP2040 PIO0 / SIO / ADC** | Target D+ via `GP2` (Probe CH0) | Target D- via `GP3` (Probe CH1) | **Passive Sniffer (Observer)** | **YES** (Header P1 Pins 4 & 5) | Requires external protection network before attaching to target. |

---

## 3. Physical USB Hardware & Schematic Details

### 3.1 Connector CN1 Electrical Pinout
- **VBUS (Pins A4, A9, B4, B9)**: Direct connection to +5V DC input rail via 2.5A 32VDC resettable polyfuse. Power supply for RP2040, ESP32, and peripherals.
- **GND (Pins A1, A12, B1, B12, G1, G2)**: System ground return.
- **CC1 / CC2 (Pins A5, B5)**: Terminated with $5.1\,\text{k}\Omega$ pull-down resistors to GND, establishing the UDOO KEY as a standard Upstream Facing Port (UFP / Device).
- **D+ / D- (Pins A6/B6, A7/B7)**: Routed through analog routing switch controlled by jumper `JP1`.

### 3.2 Jumper Selection Functionality
```text
           USB Type-C CN1 (D+ / D-)
                      │
              [ Jumper JP1 ]
              ┌───────┴───────┐
         JP1 CLOSED       JP1 OPEN
              │               │
              ▼               ▼
        RP2040 Native    UART-to-USB
        USB Controller   Bridge IC
        (USB_DP/DM)           │
                              ▼
                         ESP32 UART0
                         (IO1 / IO3)
```

### 3.3 Controller Ownership & Silicon Limitations
1. **RP2040**:
   - Fixed-frequency hardware: Operates at 12 Mbps (Full-Speed) and 1.5 Mbps (Low-Speed).
   - High-Speed (480 Mbps) is **physically impossible** on the native RP2040 USB controller due to lack of a 480 MHz UTMI/ULPI PHY.
   - Dual-core Cortex-M0+ can execute software/PIO decoding up to 12 Mbps Full-Speed.
2. **ESP32-WROVER-E**:
   - Does NOT feature USB hardware.
   - Cannot participate in physical USB transaction analysis or sampling.
   - Functions strictly as a high-level UI/WebSockets/Wi-Fi host transport.

---

## 4. Definition of USB Sniffing Levels & Feasibility

USB is a differential physical-layer protocol utilizing NRZI encoding, bit stuffing, SYNC preambles, and cyclic redundancy checks (CRC5/CRC16). The instrument must distinguish between four tiers of analysis:

```text
┌─────────────────────────────────────────────────────────────┐
│ Level 4: USB Device Analysis                                │
│ (Descriptors, Enumeration, Class: HID/CDC/Mass Storage)     │
├─────────────────────────────────────────────────────────────┤
│ Level 3: USB Transaction Analysis                           │
│ (SETUP, IN, OUT, ACK, NAK, STALL, DATA0/1, SOF, Endpoints)  │
├─────────────────────────────────────────────────────────────┤
│ Level 2: Packet & Bitstream Decoding                        │
│ (NRZI Unstuffing, Packet Framing, PID Validation, CRC Check)│
├─────────────────────────────────────────────────────────────┤
│ Level 1: Electrical & Bus State Probing                     │
│ (D+, D-, SE0, J-State, K-State, Bus Reset, Idle, Transitions)│
└─────────────────────────────────────────────────────────────┘
```

### 4.1 Level 1 — USB Electrical Probing
- **Definition**: Direct real-time measurement of physical lines:
  - $D+$ voltage and logic level
  - $D-$ voltage and logic level
  - **SE0 (Single-Ended Zero)**: $D+ < 0.8\,\text{V}$ and $D- < 0.8\,\text{V}$ (Used for EOP and Reset)
  - **J-State**: Normal idle state (FS: $D+ = \text{HIGH}, D- = \text{LOW}$; LS: $D+ = \text{LOW}, D- = \text{HIGH}$)
  - **K-State**: Inverted state (FS: $D+ = \text{LOW}, D- = \text{HIGH}$; LS: $D+ = \text{HIGH}, D- = \text{LOW}$)
  - **Bus Reset**: Continuous SE0 lasting $> 10\,\text{ms}$
- **Feasibility on UDOO KEY**: **100% FEASIBLE** on external probe pins GP2 and GP3.

### 4.2 Level 2 — Low-Speed (1.5 Mbps) & Full-Speed (12 Mbps) Packet Decoding
- **Low-Speed (1.5 Mbps)**:
  - Bit period: $T_{\text{bit}} \approx 666.7\,\text{ns}$.
  - RP2040 PIO state machine sampling at 12 MSPS provides $8\times$ oversampling per bit.
  - NRZI decoding, bit unstuffing, SYNC detection, and PID checking are completely feasible in real-time.
- **Full-Speed (12 Mbps)**:
  - Bit period: $T_{\text{bit}} \approx 83.33\,\text{ns}$.
  - Requires 48 MSPS ($4\times$) to 96 MSPS ($8\times$) sampling.
  - Our frozen PIO capture engine (up to 100 MSPS into 64 KiB SRAM) can capture raw Full-Speed bursts:
    - At 48 MSPS: 64 KiB buffer stores $1.36\,\text{ms}$ of continuous traffic.
    - At 96 MSPS: 64 KiB buffer stores $0.68\,\text{ms}$ of continuous traffic.
  - Software post-processing unrolls NRZI, removes stuffed bits, and parses packets without real-time CPU starvation.
- **High-Speed (480 Mbps)**:
  - Bit period: $2.08\,\text{ns}$.
  - **UNSUPPORTED**: RP2040 GPIOs and PIO cannot sample at the required GHz rates.

### 4.3 Level 3 & Level 4 — Transaction & Device Analysis
- Software state machine reconstructs token-data-handshake transactions (SETUP $\rightarrow$ DATA0 $\rightarrow$ ACK).
- Device descriptors (Device, Configuration, Interface, Endpoint) parsed from standard control transfers on Endpoint 0.

---

## 5. USB Probe Hardware Safety & Protection Circuitry

> [!CAUTION]
> **NEVER directly connect external USB D+ and D- to RP2040 GPIO pins!**  
> External target USB buses can exhibit transient spikes up to $+5.5\,\text{V}$, ground bounce, ESD discharges ($> 8\,\text{kV}$), and short circuits to +5V VBUS. RP2040 GPIOs have an absolute maximum rating of $+3.63\,\text{V}$. Direct exposure will permanently destroy the microcontroller.

### 5.1 Required Dedicated USB Probe Interface Circuit

To safely probe an external USB bus without disturbing signal integrity or damaging the RP2040, the following probe front-end must be used:

```text
TARGET USB BUS                                       UDOO KEY PROBE
                  ┌───────────────┐
Target D+ ──┬────┤ R_damp (47Ω)  ├─────┬───────────► Header P1 Pin 4 (GP2 / CH0)
            │     └───────────────┘     │
            │                           ▼
            │                     [ ESD Clamp ] (e.g. USBLC6-2SC6 / SP720)
            │                     [ Low-Cap   ] (Capacitance < 1.5 pF)
            │                           ▲
            │                           │
            │     ┌───────────────┐     │
Target D- ──┼────┤ R_damp (47Ω)  ├─────┴───────────► Header P1 Pin 5 (GP3 / CH1)
            │     └───────────────┘
            │
Target GND ─┴──────────────────────────────────────► Header P1 Pin 3 (GND)
```

### 5.2 Component Specifications:
1. **Damping Resistors ($R_{\text{damp}} = 47\,\Omega$, $1\%$ metal film)**:
   - Limits fault current into RP2040 ESD structures to $< 5\,\text{mA}$ if target line touches 5V VBUS.
   - Suppresses stub line ringing and transmission reflections.
2. **Low-Capacitance ESD Clamps (USBLC6-2SC6 / PRTR5V0U2X / SP720)**:
   - Ultra-low input capacitance ($C_{\text{in}} < 1.5\,\text{pF}$) ensures USB Full-Speed signal eye diagrams are not distorted.
   - Clamps positive transients to 3.3V rail and negative transients to GND.
3. **Common Ground Connection**:
   - The target system GND **MUST** be connected to UDOO KEY GND before connecting D+ or D-.

---

## 6. USB Sniffer Development Roadmap

```text
Phase USB-0: Hardware & Safety Audit (THIS DOCUMENT) ──► COMPLETED
      │
Phase USB-1: Electrical Monitor (D+, D-, SE0, J, K, Reset state machine)
      │
Phase USB-2: Speed Detection (Low-Speed vs Full-Speed pull-up detection)
      │
Phase USB-3: NRZI & Bit Unstuffing Decoder Core
      │
Phase USB-4: Packet Decoder (SYNC, PID, ADDR/ENDP, CRC5, CRC16)
      │
Phase USB-5: Transaction Analyzer (SETUP, IN, OUT, DATA0/1, Handshakes)
      │
Phase USB-6: Device Descriptor & Enumeration Parser
```

---

## 7. Audit Conclusion & Operational Boundaries

1. **CN1 USB Port**: Exclusively dedicated to board power and PC host communication (USB CDC serial / SUMP / Web UI).
2. **External USB Sniffing**: Supported up to Full-Speed (12 Mbps) through Header P1 probe inputs (GP2/GP3) using external protection network.
3. **High-Speed 480 Mbps**: Formally declared **UNSUPPORTED** due to fundamental RP2040 silicon limitations.
4. **Acquisition Baseline Preservation**: USB packet capture will reuse the frozen 64 KiB PIO DMA buffer with zero modifications to the logic analyzer infrastructure.
