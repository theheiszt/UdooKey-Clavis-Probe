# HP 54520A Digitizing Oscilloscope Integration Specification

**Document ID:** DOC-HP-54520A-20261005  
**Status:** AUTHORITATIVE INTEGRATION SPECIFICATION  
**Date:** 2026-10-05  
**Target Instrument:** Hewlett-Packard HP 54520A 500 MHz, 2-Channel, 500 MSa/s Digitizing Oscilloscope  
**Authority:** Master Specification Section 40–50 & Section 89  

---

## 1. Instrument Specifications & Overview

The HP 54520A is a classic high-performance bench digitizing oscilloscope:
- **Bandwidth**: 500 MHz repetitive, real-time single-shot up to 500 MSa/s.
- **Channels**: 2 analog input channels (1 MΩ // 7 pF or 50 Ω).
- **Vertical Resolution**: 8-bit flash A/D converters.
- **Memory Depth**: 32,768 points per channel (500 to 8,000 points in standard waveform query).
- **Command Syntax**: HP-IB / SCPI-compatible syntax conforming to IEEE 488.2.

---

## 2. Physical Interfaces & Hardware Wiring

### 2.1 Interface Selection
1. **HP-IB (IEEE-488)**: Standard 24-pin ribbon connector on rear panel. Requires an external GPIB-to-UART or GPIB-to-USB bridge controller.
2. **RS-232 Serial Port (Option 001 / Rear Panel)**: Standard DB-25 or DB-9 DCE connector.
   - Baud rates: 300, 1200, 2400, 4800, 9600, 19200 baud.
   - Protocol: 8 data bits, no parity, 1 stop bit, hardware flow control (RTS/CTS or XON/XOFF).

### 2.2 RS-232 Level Translation Circuit
> [!CAUTION]
> The HP 54520A RS-232 lines swing between $-12\,\text{V}$ and $+12\,\text{V}$. Connecting these directly to ESP32 or RP2040 GPIOs will cause catastrophic gate oxide breakdown and destroy the MCU.

An external **MAX3232** transceiver must be placed between the ESP32 UART and the HP 54520A:
```text
ESP32 (3.3V Logic)                             MAX3232 Transceiver                             HP 54520A (RS-232 DB-9)
───────────────────                             ───────────────────                             ───────────────────────
GPIO17 (UART2 TX)  ────────────► Pin 11 (T1IN)  Pin 14 (T1OUT) ────────────► Pin 2 (RXD)
GPIO16 (UART2 RX)  ◄──────────── Pin 12 (R1OUT) Pin 13 (R1IN)  ◄──────────── Pin 3 (TXD)
GND                ───────────────────────────── GND           ───────────── Pin 5 (GND)
+3.3V Rail         ───────────────────────────── VCC (with 0.1uF charge pump caps)
```

---

## 3. Command Protocol & Identification Sequence

### 3.1 Device Identification (`*IDN?`)
- **Query**: `*IDN?\n`
- **Expected Response**: `HEWLETT-PACKARD,54520A,0,XX.XX` (where XX.XX is the installed ROM firmware revision).

### 3.2 Basic Scope Configuration Commands
| Command String | Description |
| :--- | :--- |
| `*RST` | Reset instrument to factory default state. |
| `*CLS` | Clear status registers and error queues. |
| `:AUTOSCALE` | Perform automated autoscaling on active signals. |
| `:VIEW CHANNEL1` | Enable Channel 1 display. |
| `:BLANK CHANNEL2` | Disable Channel 2 display. |
| `:TIMEBASE:RANGE 1.0E-6` | Set timebase full-span to $1\,\mu\text{s}$ ($100\,\text{ns}/\text{div}$). |
| `:CHANNEL1:RANGE 8.0` | Set Channel 1 full-span to $8.0\,\text{V}$ ($1.0\,\text{V}/\text{div}$). |
| `:TRIGGER:MODE EDGE` | Set trigger mode to Edge. |
| `:TRIGGER:SOURCE CHANNEL1` | Set trigger source to Channel 1. |
| `:DIGITIZE CHANNEL1` | Acquire a complete, frozen single-shot buffer into memory. |

---

## 4. Waveform Acquisition & Data Format Pipeline

### 4.1 Waveform Query Sequence
1. Select source: `:WAVEFORM:SOURCE CHANNEL1\n`
2. Select data format: `:WAVEFORM:FORMAT BYTE\n` (8-bit binary unsigned bytes) or `:WAVEFORM:FORMAT ASCII\n`.
3. Query preamble metadata: `:WAVEFORM:PREAMBLE?\n`
   - Response contains 10 comma-separated parameters:
     1. `FORMAT`: 0 (BYTE), 1 (WORD), 2 (ASCII)
     2. `TYPE`: 1 (Normal), 2 (Peak Detect), 3 (Average)
     3. `POINTS`: Number of data points (e.g. 500)
     4. `COUNT`: Number of averages
     5. `XINCREMENT`: Time between samples (e.g. `2.0E-9` s)
     6. `XORIGIN`: Time of first sample relative to trigger
     7. `XREFERENCE`: Sample index of trigger reference
     8. `YINCREMENT`: Volts per bit (e.g. `0.03125`)
     9. `YORIGIN`: Voltage at bottom of screen
     10. `YREFERENCE`: Center reference count (typically 128)
4. Query waveform bytes: `:WAVEFORM:DATA?\n`
   - The instrument responds with an IEEE 488.2 definite-length block header: `#800000500<500 binary bytes>`.

### 4.2 Engineering Unit Conversion Formula
For each received raw byte $D_i \in [0, 255]$:
$$\text{Voltage}_i = (D_i - \text{YREFERENCE}) \times \text{YINCREMENT} + \text{YORIGIN}$$
$$\text{Time}_i = (i - \text{XREFERENCE}) \times \text{XINCREMENT} + \text{XORIGIN}$$

---

## 5. Cross-Instrument Event & Timestamp Correlation

### 5.1 Software-Assisted Correlation Workflow
```text
      Target System Under Test
         │                │
         │ (Digital Bus)  │ (Analog Line)
         ▼                ▼
   UDOO KEY RP2040    HP 54520A Scope
   Logic Analyzer     Analog Front-End
         │                │
    Packet Decoded   Trigger / Freeze
   (e.g. USB SETUP)       │
         │           Read Waveform
         │                │
         └───────┬────────┘
                 ▼
      C++ Desktop Companion
   (Correlated Multi-Domain View)
```

1. **Digital Capture**: RP2040 detects protocol event (e.g. USB `SETUP` packet or SPI transaction) with microsecond timestamp $T_{\text{dig}}$.
2. **Analog Waveform Acquisition**: HP 54520A captures analog electrical signal with trigger timestamp $T_{\text{ana}}$.
3. **Correlation Engine**: The desktop companion aligns the analog waveform trace directly beneath the logic analyzer channels, allowing visual inspection of signal rise times, ringing, and runt pulses corresponding to decoded protocol packets.

---

## 6. Interactive Terminal Commands (`SCOPE>`)

When operating the CLI in `SCOPE` mode:
- `scope id` $\rightarrow$ Query instrument identification.
- `scope run` / `scope stop` / `scope single` $\rightarrow$ Control acquisition state.
- `scope autoset` $\rightarrow$ Autoscale channels.
- `scope ch1 [volts_div]` $\rightarrow$ Configure Channel 1 scale.
- `scope tb [time_div]` $\rightarrow$ Configure timebase scale.
- `scope wave [ch]` $\rightarrow$ Download and dump waveform points.
- `scope raw <SCPI>` $\rightarrow$ Send raw SCPI command directly to the oscilloscope.
