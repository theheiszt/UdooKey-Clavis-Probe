# UDOO KEY — Bus Pirate 5 Mode & Hardware Mapping Matrix

**Document ID:** DOC-BP5-MAP-20261003  
**Status:** VALIDATED ARCHITECTURE  
**Date:** 2026-10-03  
**Target Hardware:** SECO UDOO KEY (RP2040)  

---

## 1. Concept-to-Hardware Mapping Matrix

This matrix establishes how each Bus Pirate 5 software concept is realized on the physical silicon and PCB traces of the SECO UDOO KEY:

| BP5 Concept | BP5 Hardware Architecture | UDOO KEY Hardware Realization | RP2040 Hardware Resource | Physical Connector | Electrical Verification Status |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **HiZ** | High-Z relay / buffer state | Software tri-state on GP2..GP9 | SIO input, pull-ups disabled | Header P1 Pins 4..12 | **VERIFIED** (Default boot state) |
| **LOGIC** | FALA / PIO capture | 8-Channel 100MSPS PIO+DMA | PIO0 SM0 + DMA Ch0 (64 KiB buffer) | Header P1 Pins 4..12 | **FROZEN BASELINE** |
| **METER** | Onboard 12-bit ADC | 3-Channel 12-bit SAR ADC | ADC0 (`GP26`), ADC1 (`GP27`), ADC2 (`GP28`) | Header P2 Pins 11, 12, 14 | **VERIFIED** |
| **CONTINUITY** | Buzzer tone on low-R | Resistor bridge + PWM tone | ADC0 (`GP26`) + PWM Slice 2A (`GP20`) | Header P2.11 + Header P1.15 | **VERIFIED** |
| **I2C** | Hardware I2C with pullups | RP2040 Hardware `I2C1` | GP2 (SDA) + GP3 (SCL) | Header P1 Pins 4 & 5 | **VERIFIED** |
| **SPI** | Hardware SPI master | RP2040 Hardware `SPI0` | GP2 (SCK), GP3 (MOSI), GP4 (MISO), GP5 (CS)| Header P1 Pins 4, 5, 6, 7 | **VERIFIED** |
| **UART** | Hardware UART bridge | RP2040 Hardware `UART1` | GP4 (TX) + GP5 (RX) | Header P1 Pins 6 & 7 | **VERIFIED** |
| **1-WIRE** | Hardware 1-Wire open-drain | RP2040 PIO / SIO with pullup | GP6 (DQ) | Header P1 Pin 9 | **VERIFIED** |
| **SWD / JTAG**| Bit-banged / PIO SWD | RP2040 SIO / PIO SWD probe | GP7 (SWCLK) + GP8 (SWDIO) | Header P1 Pins 10 & 11 | **VERIFIED** |
| **PWM / FREQ** | PWM generator / counter | PIO Edge Timer + PWM counter | GP2..GP9 (Configurable) | Header P1 Pins 4..12 | **VERIFIED** |
| **USB TOOLS** | N/A (BP5 has no sniffer) | Passive Probe Sniffer (LS/FS) | GP2 (D+) + GP3 (D-) with protection network| Header P1 Pins 4 & 5 | **AUDITED** (External probe tap) |
| **TERMINAL** | RP2040 USB CDC ACM | RP2040 Native USB CDC ACM | RP2040 `USB_DP` / `USB_DM` via `JP1` | Type-C Connector `CN1` | **VERIFIED** |
| **DISPLAYS** | SPI LCD on BP5 | ST7735 1.44" 128x128 SPI TFT | RP2040 SPI0 (`GP18/19/20`) + `GP11..13` | Header P1 / P2 | **VERIFIED** |

---

## 2. Hardware Enclosure & Pinout Comparison

| Function Pin | Bus Pirate 5 Standard Header | UDOO KEY Probe Header P1 | Wire Harness Color Convention |
| :--- | :--- | :--- | :--- |
| **Vout / Vreg** | Pin 1 (VOUT) | Header P2 Pin 19 / 20 (+5V / +3.3V) | Red |
| **Probe 0** | Pin 2 (IO0) | Header P1 Pin 4 (`GP2`) | Brown |
| **Probe 1** | Pin 3 (IO1) | Header P1 Pin 5 (`GP3`) | Red |
| **Probe 2** | Pin 4 (IO2) | Header P1 Pin 6 (`GP4`) | Orange |
| **Probe 3** | Pin 5 (IO3) | Header P1 Pin 7 (`GP5`) | Yellow |
| **Probe 4** | Pin 6 (IO4) | Header P1 Pin 9 (`GP6`) | Green |
| **Probe 5** | Pin 7 (IO5) | Header P1 Pin 10 (`GP7`) | Blue |
| **Probe 6** | Pin 8 (IO6) | Header P1 Pin 11 (`GP8`) | Violet |
| **Probe 7** | Pin 9 (IO7) | Header P1 Pin 12 (`GP9`) | Grey |
| **Ground** | Pin 10 (GND) | Header P1 Pin 3, 8, 13 (GND) | Black |

---

## 3. Mode Isolation & Safety Invariant

Every mode transition follows the strict state invariant:
```text
CURRENT MODE ──► leaveMode() ──► Safe High-Z ──► enterMode() ──► NEW MODE
```
1. Active interrupts, DMA channels, and PIO state machines associated with the previous mode are stopped.
2. The probe pins (`GP2..GP9`) are driven to inputs without pull-ups.
3. The new mode configures exclusively its designated pins.
4. If a command fails or aborts, the system falls back to `HiZ`.
