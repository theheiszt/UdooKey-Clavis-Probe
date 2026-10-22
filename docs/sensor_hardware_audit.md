# UDOO KEY — Sensor Hardware & Measurement Interface Audit

**Document ID:** DOC-SENSOR-HW-20261003  
**Status:** VALIDATED AUDIT (Phase SENSOR-0)  
**Date:** 2026-10-03  
**Target Hardware:** SECO UDOO KEY (RP2040 + ESP32-WROVER-E)  
**Authority Reference:** SECO UDOO KEY User Manual Rev 1.6 & Schematic  

---

## 1. Executive Summary

This document establishes the authoritative hardware reality for extending the UDOO KEY into an autonomous **Sensor Testing and Identification Subsystem**.

It evaluates the analog front-end, digital bus interfaces, excitation circuits, protection constraints, power rail capacities, and pin ownership conflicts to guarantee that sensor testing functions cleanly coexist with the frozen logic analyzer, display, and inter-MCU transport systems.

### Key Architectural Findings
1. **Analog Measurement (ADC)**:
   - Three uncommitted 12-bit SAR ADC channels (`ADC0/GP26`, `ADC1/GP27`, `ADC2/GP28`) are physically exposed on Header P2.
   - Input voltage range is $0.0\,\text{V}$ to $3.3\,\text{V}$ ($V_{\text{REF}} = 3.300\,\text{V}$ nominal).
   - Capable of DC voltage, resistance, thermistor NTC, current shunt sensing, and DMA analog waveform captures up to 500 kSPS.
2. **Digital Sensor Interfaces (I²C / SPI / UART / 1-Wire / PWM)**:
   - Contiguous Probe Bus on Header P1 (`GP2..GP9`) natively supports all major serial protocols via RP2040 hardware peripherals and PIO state machines.
   - External Header P3 exposes secondary I2C, SPI, and UART lines directly from the ESP32 coprocessor.
3. **Onboard Sensors**:
   - The UDOO KEY (Full variant) includes an onboard 6-axis IMU and PDM microphone.
   - Header `P4` selects whether RP2040 (`P4` 1-2) or ESP32 (`P4` 2-3) connects to the onboard IMU via I2C.
4. **Excitation & Continuity Circuitry**:
   - Two-wire resistance measurement and fast bench continuity testing are implemented via a precision pull-up reference resistor ($R_{\text{ref}} = 1.000\,\text{k}\Omega$, $0.1\%$).
   - Dedicated acoustic feedback is driven via PWM on `GP20` (Header P1 Pin 15, resonant frequency 2.7 kHz) with optical feedback on green user LED `GP25`.
5. **Zero Conflict with Acquisition Baseline**:
   - Mode switching is strictly serialized through `InstrumentModeManager`.
   - When digital sensor modes (I2C/SPI/UART) are active, the logic analyzer is halted and pins are remapped.
   - When logic analyzer is running, sensor pins return to high-impedance PIO inputs.
   - Analog ADC channels (`GP26..GP28`) are electrically isolated from the digital probe bus (`GP2..GP9`) and can run concurrently without interference.

---

## 2. Sensor Physical Pinout & Authority Matrix

| Interface | Signal | RP2040 GPIO | Physical Connector | Pin Number | Hardware Function | Peripheral Options | Conflict Status |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Analog In 0** | **ADC0** | `GP26` | Header P2 | Pin 11 (Arduino 31) | Precision Voltmeter / Continuity Probe+ | 12-bit SAR ADC Ch0, PWM5_A | **NO CONFLICT** (Dedicated Analog) |
| **Analog In 1** | **ADC1** | `GP27` | Header P2 | Pin 12 (Arduino 32) | Precision Voltmeter / Current Shunt | 12-bit SAR ADC Ch1, PWM5_B | **NO CONFLICT** (Dedicated Analog) |
| **Analog In 2** | **ADC2** | `GP28` | Header P2 | Pin 14 (Arduino 34) | Auxiliary Sensor / NTC Thermistor | 12-bit SAR ADC Ch2, PWM6_A | **NO CONFLICT** (Dedicated Analog) |
| **Internal Supply**| **ADC3** | `GP29` | Internal | Internal | VSYS Monitor ($1/3 \times V_{\text{SYS}}$) | 12-bit SAR ADC Ch3 | **NO CONFLICT** (Internal) |
| **Core Temp** | **ADC4** | N/A | Internal | Internal | RP2040 Silicon Die Temp | Diode $V_{\text{BE}}$ Sensor | **NO CONFLICT** (Internal) |
| **Digital Probe 0**| **CH0** | `GP2` | Header P1 | Pin 4 | Digital Sensor / I2C SDA / SPI SCK | PIO0, I2C1 SDA, SPI0 SCK, PWM1_A | Managed via `InstrumentModeManager` |
| **Digital Probe 1**| **CH1** | `GP3` | Header P1 | Pin 5 | Digital Sensor / I2C SCL / SPI MOSI| PIO0, I2C1 SCL, SPI0 TX, PWM1_B | Managed via `InstrumentModeManager` |
| **Digital Probe 2**| **CH2** | `GP4` | Header P1 | Pin 6 | Digital Sensor / SPI MISO / UART TX | PIO0, SPI0 RX, UART1 TX, PWM2_A | Managed via `InstrumentModeManager` |
| **Digital Probe 3**| **CH3** | `GP5` | Header P1 | Pin 7 | Digital Sensor / SPI CS / UART RX | PIO0, SPI0 CSn, UART1 RX, PWM2_B| Managed via `InstrumentModeManager` |
| **Digital Probe 4**| **CH4** | `GP6` | Header P1 | Pin 9 | Digital Sensor / 1-Wire DQ | PIO0, PWM3_A, SIO | Managed via `InstrumentModeManager` |
| **Digital Probe 5**| **CH5** | `GP7` | Header P1 | Pin 10 | Digital Sensor / SWD SWCLK | PIO0, PWM3_B, SIO | Managed via `InstrumentModeManager` |
| **Digital Probe 6**| **CH6** | `GP8` | Header P1 | Pin 11 | Digital Sensor / SWD SWDIO | PIO0, PWM4_A, SIO | Managed via `InstrumentModeManager` |
| **Digital Probe 7**| **CH7** | `GP9` | Header P1 | Pin 12 | Digital Sensor / AUX / PWM In | PIO0, PWM4_B, SIO | Managed via `InstrumentModeManager` |
| **Buzzer Out** | **PWM** | `GP20` | Header P1 | Pin 15 | Acoustic Continuity Buzzer Drive | PWM Slice 2 Channel A | **NO CONFLICT** (TFT MISO unneeded) |
| **Activity LED** | **LED** | `GP25` | Onboard | Pin 30 (D12) | Optical Continuity / Trigger LED | SIO Output | **NO CONFLICT** (Dedicated LED) |

---

## 3. Power Rails, Voltage Domains & Current Limits

| Power Rail | Voltage Range | Source | Physical Location | Max Current Available | Sensor Usage |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **+5V VBUS** | $+4.75\,\text{V}$ to $+5.25\,\text{V}$ | USB Type-C `CN1` | Header P2 Pin 20 | **2.5 A** (Polyfuse limited) | Powering 5V sensors (relay, servo, optical) |
| **+5V VSYS** | $+4.5\,\text{V}$ to $+5.5\,\text{V}$ | Filtered USB / Ext | Header P2 Pin 19 | **500 mA** | Clean 5V sensor transducer rail |
| **+3.3V RP2040** | $+3.30\,\text{V} \pm 2\%$ | RT6150 Buck-Boost | Header P1 / P2 | **~500 mA** headroom | Primary logic & excitation reference rail |
| **+3.3V ESP32** | $+3.30\,\text{V} \pm 2\%$ | Dedicated LDO | Header P3 Pin 1 | **150 mA** Max | Low-power I2C/SPI sensor modules |
| **GND** | $0.0\,\text{V}$ Reference | Ground Plane | Header P1 (3, 8, 13, 18) | N/A | Low-impedance common return reference |

> [!WARNING]
> **Voltage Domain Rule**: RP2040 GPIOs and ADC inputs operate exclusively in the **$0.0\,\text{V}$ to $+3.3\,\text{V}$ domain**. Connecting a 5V sensor output directly to an RP2040 pin will cause latch-up and silicon failure. 5V sensors must use resistive dividers or level shifters.

---

## 4. Excitation & Measurement Circuit Architectures

### 4.1 Resistance & Continuity Testing Circuit (Option A: Resistor-Limited Bridge)

```text
3.3V Rail (RT6150)
     │
 ┌───┴────────────────────────┐
 │ R_ref = 1000 Ω (0.1% Metal)│
 └───┬────────────────────────┘
     │
     ├────────────────────────► PROBE+ ──► DUT (Trace / Sensor) ──► PROBE- ──► GND
     │
     ▼
RP2040 GP26 (ADC0)
(12-bit SAR ADC, 0-3.3V)
```

#### Engineering Characteristics:
- **Test Current**: $I_{\text{DUT}} = \frac{3.3\,\text{V}}{R_{\text{ref}} + R_{\text{DUT}}}$.
  - Under short circuit ($R_{\text{DUT}} = 0\,\Omega$): $I_{\text{max}} = \frac{3.3\,\text{V}}{1000\,\Omega} = 3.3\,\text{mA}$.
  - Perfectly safe for small sensitive electronics, semiconductor junctions, and traces.
- **DUT Voltage Drop**:
  - Across $10\,\Omega$ trace: $V_{\text{DUT}} = 3.3\,\text{V} \times \frac{10}{1010} \approx 32.7\,\text{mV}$ (won't turn on silicon diodes!).
  - Across $25\,\Omega$ continuity threshold: $V_{\text{DUT}} \approx 80.5\,\text{mV}$.
- **Resolution**:
  - $12$-bit ADC provides $805.66\,\mu\text{V}/\text{LSB}$.
  - Resistance sensitivity: $\Delta R \approx 0.25\,\Omega$ near zero ohms.
- **Continuity Thresholds**:
  - **Electrical Threshold**: Configurable $R_{\text{thresh}} = 10\,\Omega, 25\,\Omega, 50\,\Omega, 100\,\Omega$.
  - **Acoustic Hysteresis**: Release threshold set to $R_{\text{thresh}} + 10\,\Omega$ to eliminate erratic clicking on scratchy probes.

### 4.2 NTC Thermistor & Resistive Sensor Transducer
- Reuses the excitation bridge on `ADC0` or `ADC2`.
- Voltage output relates to resistance by:
  $$R_{\text{sensor}} = R_{\text{ref}} \times \frac{V_{\text{ADC}}}{3.3\,\text{V} - V_{\text{ADC}}}$$
- Temperature calculated via the Steinhart-Hart / $\beta$-parameter formula:
  $$\frac{1}{T} = \frac{1}{T_0} + \frac{1}{\beta} \ln\left(\frac{R_{\text{sensor}}}{R_0}\right)$$

### 4.3 Current Measurement Architecture (Shunt Resistor)
- High-side or low-side current shunt ($R_{\text{shunt}} = 0.1\,\Omega$ or $1.0\,\Omega$, $1\%$, $1\,\text{W}$).
- Voltage across shunt measured directly across `ADC1` (GP27) and `ADC0` (GP26).
- Current calculation: $I = \frac{V_{\text{ADC1}} - V_{\text{ADC0}}}{R_{\text{shunt}}}$.

---

## 5. Front-End Protection & Signal Conditioning

To make the UDOO KEY robust for general electronics bench testing, external inputs must be conditioned:

```text
External Probe ─── [ 100Ω PTC Resettable Fuse ] ──┬─── [ SP720 / Dual Diode Clamp to 3.3V & GND ] ──► MCU Pin
                                                  │
                                          [ 1MΩ Pull-Down ]
```

1. **PTC Resettable Fuse ($100\,\Omega$, $50\,\text{mA}$ trip)**: Limits fault current if connected to 12V or 24V.
2. **SP720 Diode Clamp Array**: Directs overvoltage transients to 3.3V rail and negative transients to GND.
3. **$1\,\text{M}\Omega$ Bias**: Prevents floating ADC readings when probes are open circuit.

---

## 6. Digital Sensor Discovery & Testing Capabilities

### 6.1 I²C Sensor Bus Architecture
- Pins: `GP2` (SDA) and `GP3` (SCL) on Header P1.
- Internal configurable pull-ups: RP2040 programmable $50\,\text{k}\Omega$ pull-ups enabled, with pads for external $2.2\,\text{k}\Omega$ to $4.7\,\text{k}\Omega$ pull-ups.
- Capabilities:
  - 7-bit bus scanner ($0x08$ through $0x77$) with ACK detection.
  - Standard mode ($100\,\text{kHz}$), Fast mode ($400\,\text{kHz}$), Fast mode Plus ($1\,\text{MHz}$).
  - Sub-register interrogation (`WHO_AM_I`, `DEVID`, Chip ID).
  - Transaction logger: START $\rightarrow$ ADDR+W $\rightarrow$ REG $\rightarrow$ REPEATED START $\rightarrow$ ADDR+R $\rightarrow$ DATA $\rightarrow$ STOP.

### 6.2 SPI Sensor Bus Architecture
- Pins: `GP2` (SCK), `GP3` (MOSI), `GP4` (MISO), `GP5` (CS) on Header P1.
- Programmable modes: Mode 0 (CPOL=0, CPHA=0), Mode 1 (0, 1), Mode 2 (1, 0), Mode 3 (1, 1).
- Clock rates: $100\,\text{kHz}$ up to $30\,\text{MHz}$.
- Automated JEDEC ID reader ($0x9F$ command) for flash/EEPROM sensors.

### 6.3 1-Wire Sensor Architecture
- Pin: `GP6` (Header P1 Pin 9) with $4.7\,\text{k}\Omega$ pull-up to 3.3V.
- Dallas 1-Wire Search ROM algorithm (`0xF0`) to identify 64-bit unique ROM codes.
- DS18B20 digital temperature sensor resolution ($9$ to $12$ bits).

### 6.4 PWM & Frequency Sensor Architecture
- Pin: `GP2`..`GP9` configurable.
- Uses RP2040 PIO Edge Analyzer + Hardware PWM counter.
- Measures: Frequency ($1\,\text{Hz}$ to $60\,\text{MHz}$), Period ($\pm 7.5\,\text{ns}$ resolution), Duty Cycle ($0.1\%$ to $99.9\%$), High/Low pulse widths.

---

## 7. Subsystem Isolation & Conflict Resolution

```text
┌────────────────────────────────────────────────────────┐
│               InstrumentModeManager                    │
│   Guarantees Mutual Exclusion on Probe Bus GP2..GP9    │
├──────────────────────────┬─────────────────────────────┤
│   LOGIC_ANALYZER Mode    │      SENSOR_TEST Mode       │
│                          │                             │
│ • PIO0 SM0 Active        │ • PIO0 Stopped              │
│ • DMA Channel 0 Active   │ • Hardware I2C1 / SPI0 /    │
│ • All 8 pins input only  │   UART1 or SIO enabled      │
│ • Frozen SUMP baseline   │ • Pull-ups active           │
└──────────────────────────┴─────────────────────────────┘
                           ▲
                           │ Independent Hardware
                           ▼
┌────────────────────────────────────────────────────────┐
│            RP2040 12-Bit SAR ADC Subsystem             │
│        (GP26 / ADC0, GP27 / ADC1, GP28 / ADC2)         │
│                                                        │
│ • Completely independent silicon peripheral            │
│ • Runs concurrently with ANY digital mode              │
│ • Zero impact on DMA Channel 0 or PIO0                 │
└────────────────────────────────────────────────────────┘
```

---

## 8. Audit Conclusion

1. **Analog Capabilities**: Fully verified on `GP26`, `GP27`, and `GP28` (Header P2). Supports voltage, continuity, resistance, and analog sensors.
2. **Digital Capabilities**: Fully verified on `GP2..GP9` (Header P1). Supports I2C, SPI, UART, 1-Wire, and PWM sensors.
3. **Coexistence**: Guaranteed by `InstrumentModeManager` serialization and hardware isolation between analog and digital pins.
4. **Acquisition Baseline**: Completely untouched and protected.
