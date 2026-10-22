# UDOO KEY — Continuity Subsystem Engineering Architecture & Trade Study

## 1. Executive Summary & Design Mission

A bench-grade continuity tester must be fast, reliable, safe for delicate components, and acoustically responsive. The UDOO KEY continuity subsystem combines:
* A low-voltage, resistor-limited test bridge.
* Hardware-driven clamp and fuse protection.
* Non-blocking ADC sampling and a state machine with hysteresis.
* Acoustic buzzer feedback on **GP20 (Header P1, Pin 15, PWM Slice 2A)**.
* Synchronized optical indicator feedback on **GP25 (Onboard Green LED D12)**.

The continuity engine delivers an end-to-end response time of **$< 10\,\text{ms}$**, completely avoiding human-perceivable lag and ensuring zero blocking delays in the main execution thread.

---

## 2. Architectural Comparison: Option A vs. Option B

To select the optimal continuity hardware architecture, an engineering evaluation was performed comparing a resistor-limited voltage bridge (**Option A**) against an active constant-current source (**Option B**).

### 2.1 Option A: Resistor-Limited Continuity Bridge
```text
      +3.3V
        │
        [ R_ref = 1000 Ω ]
        │
        ├──────────────────────► Sense Node (ADC0 / GP26 via Protection)
        │
     [PROBE+]
        │
      [DUT] (Device Under Test)
        │
     [PROBE-]
        │
       GND
```
In this circuit:
$$V_{\text{DUT}} = 3.300\,\text{V} \times \left(\frac{R_{\text{DUT}}}{R_{\text{ref}} + R_{\text{DUT}}}\right)$$
$$R_{\text{DUT}} = R_{\text{ref}} \times \left(\frac{V_{\text{DUT}}}{3.300\,\text{V} - V_{\text{DUT}}}\right)$$

### 2.2 Option B: Active Constant-Current Source
```text
      +3.3V / +5V
        │
  [ Current Source ] (Op-Amp + BJT / Current Mirror, e.g. 1.0 mA)
        │
        ├──────────────────────► Sense Node (ADC0 / GP26 via Protection)
        │
     [PROBE+]
        │
      [DUT]
        │
     [PROBE-]
        │
       GND
```
In this circuit:
$$V_{\text{DUT}} = I_{\text{test}} \times R_{\text{DUT}}$$
$$R_{\text{DUT}} = \frac{V_{\text{DUT}}}{I_{\text{test}}}$$

---

### 2.3 Detailed Comparative Trade Matrix

| Criterion | Option A: Resistor-Limited Bridge ($R_{\text{ref}} = 1\,\text{k}\Omega$) | Option B: Constant-Current Source ($I = 1\,\text{mA}$) | Selection Advantage |
| :--- | :--- | :--- | :--- |
| **1. Component Count** | **1 Resistor** ($1\,\text{k}\Omega, 0.1\%$) + protection network. | **5 to 8 Components** (Op-amp IC, BJT/FET, reference diode, bias resistors, decoupling caps). | **Option A** (Substantially simpler & smaller PCB area). |
| **2. Low-Ohms Accuracy** | Excellent near $0\dots 100\,\Omega$: $\Delta V \approx 3.2\,\text{mV}/\Omega$. | Excellent: $\Delta V = 1.0\,\text{mV}/\Omega$ linear. | **Option A** provides higher $\Delta V$ sensitivity at $<100\,\Omega$. |
| **3. Test Current** | Bounded: $0\,\text{mA}$ (open) to $3.3\,\text{mA}$ (short). | Fixed: $1.0\,\text{mA}$ constant. | **Tie** (Both $< 5\,\text{mA}$, fully safe for delicate traces). |
| **4. DUT Voltage at Continuity** | **$32.7\,\text{mV}$ at $10\,\Omega$; $157\,\text{mV}$ at $50\,\Omega$**. Well below silicon diode forward drop ($600\,\text{mV}$). | **$10\,\text{mV}$ at $10\,\Omega$; $50\,\text{mV}$ at $50\,\Omega$**. | **Tie** (Both strictly prevent turning on silicon PN junctions). |
| **5. Open-Circuit Voltage** | Clamped at $3.3\,\text{V}$ (safe logic rail). | Saturates to upper op-amp supply rail ($3.3\,\text{V}$ or $5.0\,\text{V}$). | **Option A** (Guaranteed within RP2040 absolute maximum ratings). |
| **6. Short-Circuit Behavior** | Completely benign: $I_{\text{sc}} = 3.3\,\text{V} / 1\,\text{k}\Omega = 3.3\,\text{mA}$; power dissipation $P = 10.9\,\text{mW}$. | Safe: $I_{\text{sc}} = 1.0\,\text{mA}$. | **Tie** (Both are completely indestructible under dead short). |
| **7. Max Measurable Resistance** | Useful up to $\sim 500\,\text{k}\Omega$ (continuous transfer curve). | Clamps/saturates at $R_{\text{max}} = V_{\text{rail}} / I_{\text{test}} \approx 3.3\,\text{k}\Omega$. | **Option A** (Enables wider resistance meter capability). |
| **8. Response Time** | $< 10\,\mu\text{s}$ electrical rise time (limited only by RC filter). | $50\dots 200\,\mu\text{s}$ (op-amp slew rate and settling from saturation). | **Option A** (Significantly faster probe step response). |
| **9. Fault & Overvoltage Protection** | Rugged: Resistor naturally limits fault currents under external AC/DC contacts. | Vulnerable: Active op-amp / BJT output stage easily damaged by external voltage injection. | **Option A** (Much higher fault resilience). |
| **10. Firmware Complexity** | Simple single-point algebraic transformation: $R = R_{\text{ref}} \frac{V}{V_{\text{dd}} - V}$. | Linear multiplication: $R = V / I$. | **Tie** (Both take $< 1\,\mu\text{s}$ on RP2040 Cortex-M0+ FPU). |

### 2.4 Engineering Decision
**Option A (Resistor-Limited Bridge with $R_{\text{ref}} = 1000\,\Omega$) is selected.**
It provides superior voltage sensitivity in the critical $0\dots 50\,\Omega$ range ($3.2\,\text{mV}/\Omega$ vs. $1.0\,\text{mV}/\Omega$), features an inherently bounded open-circuit voltage ($3.3\,\text{V}$), exhibits instantaneous step response with no amplifier saturation delay, and offers dramatically superior ruggedness against accidental probe faults.

---

## 3. Threshold, Hysteresis, and Debounce Design

Continuity must never be an arbitrary raw ADC number; it must correspond to an engineering resistance threshold.

### 3.1 Threshold Presets
The firmware provides four industry-standard threshold profiles selectable on the fly:
* **$< 10\,\Omega$ (Precision / Solder Joint)**: Ideal for detecting solder bridges, low-resistance PCB traces, and relay contacts.
* **$< 25\,\Omega$ (Standard Bench)**: Default general-purpose setting for switches, wiring harnesses, and connectors.
* **$< 50\,\Omega$ (Cable / Wire Harness)**: Suitable for long cable runs, automotive wiring, and transformer windings.
* **$< 100\,\Omega$ (General Component)**: High-tolerance setting for fuses, filament lamps, and inductive coils.

### 3.2 Hysteresis Design
Without hysteresis, dirty probes or borderline resistance contacts cause rapid acoustic "chatter" or buzzing oscillation. The firmware implements an asymmetric hysteresis window:

$$\text{Connect Condition (Beep ON)}: R_{\text{measured}} \le R_{\text{threshold}}$$
$$\text{Release Condition (Beep OFF)}: R_{\text{measured}} \ge R_{\text{threshold}} + R_{\text{hysteresis}}$$

| Threshold ($R_{\text{thresh}}$) | Hysteresis ($R_{\text{hyst}}$) | Beep Turn-ON Point | Beep Turn-OFF Point |
| :--- | :--- | :--- | :--- |
| **$10\,\Omega$** | $+5\,\Omega$ | $\le 10\,\Omega$ | $\ge 15\,\Omega$ |
| **$25\,\Omega$** | $+10\,\Omega$ | $\le 25\,\Omega$ | $\ge 35\,\Omega$ |
| **$50\,\Omega$** | $+15\,\Omega$ | $\le 50\,\Omega$ | $\ge 65\,\Omega$ |
| **$100\,\Omega$** | $+25\,\Omega$ | $\le 100\,\Omega$ | $\ge 125\,\Omega$ |

---

## 4. Continuity State Machine Architecture

The continuity engine is implemented as a non-blocking finite state machine (FSM) executed in `adc_continuity_update()`:

```text
                  ┌────────────────────────┐
                  │    ADC Conversion      │
                  │  (Hardware Oversampled) │
                  └───────────┬────────────┘
                              │
                              ▼
                  ┌────────────────────────┐
                  │ Calculate Resistance   │
                  │  Rx = Rref * V/(3.3-V) │
                  └───────────┬────────────┘
                              │
                              ▼
                  ┌────────────────────────┐
                  │  Rolling Median/Filter │
                  │     (Reject Glitches)  │
                  └───────────┬────────────┘
                              │
                ┌─────────────┴─────────────┐
                │                           │
  [Rx <= Threshold]           [Rx >= Threshold + Hysteresis]
                │                           │
                ▼                           ▼
        ┌───────────────┐           ┌───────────────┐
        │  STATE_CLOSED │           │   STATE_OPEN  │
        │  Buzzer: ON   │           │  Buzzer: OFF  │
        │  LED: ON      │           │  LED: OFF     │
        └───────────────┘           └───────────────┘
```

### 4.1 Timing & Latency Breakdown
* **ADC Conversion Time**: $2.0\,\mu\text{s}$ per sample $\times 16$ oversamples $= 32\,\mu\text{s}$.
* **Resistance Calculation**: $\approx 1.2\,\mu\text{s}$ (32-bit floating point).
* **FSM Evaluation**: $\approx 0.5\,\mu\text{s}$.
* **Hardware PWM Update**: Immediate register write to `pwm_set_chan_level()`.
* **Total Acoustic Latency**: **$< 1.0\,\text{ms}$**, far faster than the human acoustic perception threshold ($\approx 10\dots 20\,\text{ms}$).

---

## 5. Acoustic & Visual Annunciation Hardware

### 5.1 Dedicated Piezo Buzzer Driver (GP20)
* **Pin**: `GP20` (UDOO KEY Pin 26, Header P1 Pin 15).
* **Hardware Peripheral**: RP2040 PWM Slice 2, Channel A.
* **Frequency**: $2700\,\text{Hz}$ (the optimal resonant frequency for miniature electromagnetic/piezo transducers, delivering maximum acoustic sound pressure level per milliwatt).
* **Volume / Duty Cycle**: Configurable $0\dots 100\%$ ($50\%$ nominal).

### 5.2 Optical Indicator (GP25 / D12)
* Synchronized with the buzzer state. Provides instant visual feedback on the bench even in high-noise laboratory environments or when the buzzer is muted via the UI.
