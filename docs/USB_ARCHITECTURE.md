# UDOO KEY USB Subsystem Architecture & Implementation Specification

**Document ID:** DOC-USB-ARCH-MASTER-20261005  
**Status:** AUTHORITATIVE USB ARCHITECTURE  
**Date:** 2026-10-05  
**Authority:** Master Specification Section 19–30 & Section 88  

---

## 1. System Topology & Hardware Boundaries

The USB subsystem implements a multi-tier USB analysis and development architecture based on the physical reality of the UDOO KEY:

```text
┌────────────────────────────────────────────────────────────────────────┐
│ LEVEL 4: USB Device Analysis (Enumeration, Descriptors, Classes)       │
├────────────────────────────────────────────────────────────────────────┤
│ LEVEL 3: USB Transaction Engine (SETUP, IN, OUT, Handshakes, Transfers) │
├────────────────────────────────────────────────────────────────────────┤
│ LEVEL 2: Packet & Bitstream Engine (NRZI, Bit-Unstuffing, CRC5, CRC16) │
├────────────────────────────────────────────────────────────────────────┤
│ LEVEL 1: Physical Bus Probing (D+, D-, SE0, J/K States, Speed Detect)  │
└────────────────────────────────────────────────────────────────────────┘
```

### Connector CN1 vs External Sniffer Tap
1. **Onboard USB-C (CN1)**:
   - Dedicated exclusively to board power (+5V input) and PC host communication (USB CDC serial console, SUMP/Sigrok, UF2 firmware flashing).
   - Routed to RP2040 native PHY (`USB_DP`/`USB_DM`) when Jumper JP1 is CLOSED.
   - **Cannot be used as a passive sniffer tap** because it is an active host-facing link.
2. **External Passive Sniffer Tap**:
   - Uses Header P1 Pin 4 (GP2 / CH0) as Target D+.
   - Uses Header P1 Pin 5 (GP3 / CH1) as Target D-.
   - Uses Header P1 Pin 3 as common system Ground reference.
   - Requires external front-end protection network.

---

## 2. Electrical Front-End & Input Protection

> [!CAUTION]
> Direct connection of external USB lines to RP2040 GPIOs is prohibited. External USB buses carry up to 5.5V transients, ESD spikes, and potential VBUS shorts that exceed the RP2040 maximum rating of 3.63V.

### Required Front-End Protection Schematic
```text
TARGET USB BUS                                       UDOO KEY PROBE
                  ┌───────────────┐
Target D+ ──┬────┤ R_damp (47Ω)  ├─────┬───────────► Header P1 Pin 4 (GP2 / CH0)
            │     └───────────────┘     │
            │                           ▼
            │                     [ ESD Clamp ] (e.g. USBLC6-2SC6 / PRTR5V0U2X)
            │                     [ Low-Cap   ] (Capacitance < 1.5 pF)
            │                           ▲
            │                           │
            │     ┌───────────────┐     │
Target D- ──┼────┤ R_damp (47Ω)  ├─────┴───────────► Header P1 Pin 5 (GP3 / CH1)
            │     └───────────────┘
            │
Target GND ─┴──────────────────────────────────────► Header P1 Pin 3 (GND)
```

- **Damping Resistors ($47\,\Omega$, 1% metal film)**: Damp reflections and limit clamp current to $<5\,\text{mA}$.
- **Low-Capacitance Clamps ($<1.5\,\text{pF}$)**: Prevent high-frequency edge degradation on Full-Speed (12 Mbps) eye patterns.
- **Common Ground Reference**: Mandatory connection prior to attaching signal lines.

---

## 3. Physical Probing & Electrical State Machine (Level 1)

The line state is determined by continuous evaluation of $D+$ and $D-$:

| D+ Level | D- Level | Decoded Bus State | Electrical Meaning |
| :---: | :---: | :--- | :--- |
| **0** | **0** | **SE0** | Single-Ended Zero: USB Reset ($>10\,\text{ms}$), End-of-Packet (EOP), or Detach. |
| **1** | **0** | **J-State (FS) / K-State (LS)** | Full-Speed Idle Mark / Low-Speed Resume. |
| **0** | **1** | **J-State (LS) / K-State (FS)** | Low-Speed Idle Mark / Full-Speed Resume. |
| **1** | **1** | **SE1 (FAULT)** | Illegal State / Bus Contention. (Also occurs if probe leads are floating CMOS inputs). |

### Speed Detection Rules
- **Full-Speed (12 Mbps)**: Identified by upstream $1.5\,\text{k}\Omega$ pull-up resistor on $D+$. Line levels in idle: $D+ = 1, D- = 0$.
- **Low-Speed (1.5 Mbps)**: Identified by upstream $1.5\,\text{k}\Omega$ pull-up resistor on $D-$. Line levels in idle: $D+ = 0, D- = 1$.
- **Disconnected**: Both lines pulled to $0\,\text{V}$ by host $15\,\text{k}\Omega$ pull-down resistors ($D+ = 0, D- = 0$).

---

## 4. Packet & Protocol Decoding Pipeline (Level 2 & 3)

### NRZI & Bit Unstuffing
USB transmits data using Non-Return-to-Zero Inverted (NRZI) encoding with bit stuffing:
- A transition represents a logic `0`.
- No transition represents a logic `1`.
- After six consecutive `1`s, a `0` bit is stuffed into the stream to guarantee clock transitions. The decoder unstuffs this bit automatically.

### CRC Verification
- **CRC-5**: Polynomial $G(X) = X^5 + X^2 + 1$ ($0\text{x}05$), seed $0\text{x}1F$, verified over 11-bit token packets (7-bit Address + 4-bit Endpoint).
- **CRC-16**: Polynomial $G(X) = X^{16} + X^{15} + X^2 + 1$ ($0\text{x}8005$ / reflected $0\text{xA001}$), seed $0\text{xFFFF}$, inverted result, verified over all DATA0/DATA1/DATA2/MDATA payloads.

### Standard Setup Control Request Decoding
Every 8-byte SETUP transfer is structurally broken down:
```text
Offset 0: bmRequestType (Direction: Host-to-Device / Device-to-Host, Type: Standard/Class/Vendor, Recipient: Device/Interface/Endpoint)
Offset 1: bRequest (GET_STATUS, CLEAR_FEATURE, SET_FEATURE, SET_ADDRESS, GET_DESCRIPTOR, SET_DESCRIPTOR, GET_CONFIGURATION, SET_CONFIGURATION)
Offset 2..3: wValue (Descriptor Type & Index, Feature Selector)
Offset 4..5: wIndex (Zero, Interface Number, or Endpoint Number)
Offset 6..7: wLength (Number of bytes to transfer in Data phase)
```

---

## 5. Host & Device Capabilities

1. **USB Host Mode**:
   - Supported at Full-Speed (12 Mbps) and Low-Speed (1.5 Mbps).
   - Driven by TinyUSB Host stack (`CFG_TUSB_MCU=OPT_MCU_RP2040`).
   - Retrieves and parses Device, Configuration, Interface, Endpoint, and String descriptors.
2. **USB Device Mode**:
   - Supports emulating standard test endpoints (CDC Serial, HID Keyboard/Mouse, Vendor Diagnostic loopback).
3. **High-Speed (480 Mbps) Limitation**:
   - **Formally Declared Unavailable**: The RP2040 silicon lacks a 480 MHz UTMI+ PHY. Direct GPIO sampling at 480 Mbps is physically impossible. Capturing High-Speed traffic requires an external dedicated USB PHY or FPGA acquisition front-end.

---

## 6. Hardware Verification Procedure

1. Connect P1.03 (GND) to target USB GND.
2. Connect P1.04 (GP2) through 47Ω to target D+.
3. Connect P1.05 (GP3) through 47Ω to target D-.
4. In terminal:
   - `m usb` $\rightarrow$ Enter USB mode.
   - `probe` $\rightarrow$ Read instantaneous D+/D- levels and bus state.
   - `speed` $\rightarrow$ Confirm target speed (Full-Speed or Low-Speed).
   - `mon 5000` $\rightarrow$ Run 5-second physical transition monitor.
   - `sniff 16 3000` $\rightarrow$ Capture live packet bursts and verify CRC/PID decoding.
