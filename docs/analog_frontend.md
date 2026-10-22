# UDOO KEY — Analog Front-End & Probe Protection Specification

## 1. Safety Boundary & Operating Mandate

> [!WARNING]
> **MANDATORY SAFETY BOUNDARY: DE-ENERGIZED CIRCUITS ONLY**
> Continuity testing and resistance measurements are engineered strictly for **de-energized (unpowered) circuits**. 
> Connecting continuity probes to energized circuits—especially AC mains ($110\,\text{V}_{\text{AC}} / 230\,\text{V}_{\text{AC}}$)—presents severe risks of electrical shock, fire, and catastrophic instrument destruction. Always discharge capacitors and verify circuits are fully powered off before connecting continuity probes.

The analog front-end protection circuit detailed herein protects the RP2040 against typical bench-level user errors, such as:
* Accidental contact with active DC logic rails ($+5\,\text{V}, +12\,\text{V}, +24\,\text{V}$).
* Moderate negative DC voltages (down to $-15\,\text{V}$).
* Human Body Model (HBM) electrostatic discharge (ESD up to $\pm 15\,\text{kV}$).
* Transient inductive spikes from relay coils or motors.

---

## 2. Multi-Stage Analog Front-End Schematic

Directly connecting an external test probe to an unprotected RP2040 GPIO pin violates maximum electrical ratings ($V_{\text{MAX}} = 3.63\,\text{V}, I_{\text{PIN\_MAX}} = 50\,\text{mA}$). The recommended bench front-end integrates a three-stage protection topology:

```text
PROBE (+)
   │
   ├──[ F1: PTC Fuse (50mA / 60V) ]───┐
   │                                  │
[TVS: PESD5V0]                        │
   │                               [ R_lim: 1 kΩ, 0.5W Metal Film ]
  GND                                 │
                                      ├───► Node A (Clamped)
                                      │
                                    ┌─┴─┐
                                    │ ▲ │ D1: BAT54S (Cathode to +3.3V)
                                    │ ─ │
                                    └─┬─┘
                                      ├─────────► +3.3V Analog Rail
                                    ┌─┴─┐
                                    │ ▲ │ D2: BAT54S (Anode to GND)
                                    │ ─ │
                                    └─┬─┘
                                      │
                                     GND
                                      │
                                      ├───[ R_filt: 1 kΩ ]───┬───► To RP2040 ADC
                                      │                      │     (GP26 / GP27 / GP28)
                                     Node A                  │
                                                           [ C_filt: 10 nF ]
                                                             │
                                                            GND
```

---

## 3. Component Functionality & Stress Analysis

### 3.1 Stage 1: Primary ESD Suppressor (TVS Diode)
* **Component**: `PESD5V0L1BA` or `USBLC6-2SC6` bidirectional Transient Voltage Suppressor.
* **Function**: Clamps high-speed ESD transients (up to $\pm 15\,\text{kV}$ air discharge, $\pm 8\,\text{kV}$ contact per IEC 61000-4-2) before high-energy pulses penetrate the instrument enclosure.
* **Capacitance**: Low capacitance ($< 3.5\,\text{pF}$) ensures zero signal degradation across high-speed capture.

### 3.2 Stage 2: Current Limiting & Resettable Thermal Fuse
* **PTC Resettable Fuse (`F1`)**: PolySwitch / PPTC rated for $50\,\text{mA}$ hold current, $100\,\text{mA}$ trip current, and $60\,\text{V}_{\text{DC}}$ maximum hold voltage (e.g. Bourns MF-MSMF005).
* **Current-Limiting Resistor (`R_lim`)**: $1.0\,\text{k}\Omega$, $0.5\,\text{W}$ pulse-withstanding metal film resistor.
* **Fault Behavior under $+24\,\text{V}_{\text{DC}}$ Connection**:
  1. If an accidental $+24\,\text{V}$ rail touches Probe+:
  2. Clamp diode $D_1$ conducts to $+3.3\,\text{V}$.
  3. Voltage drop across $R_{\text{lim}}$ is:
     $$V_{\text{drop}} = 24\,\text{V} - (3.3\,\text{V} + 0.35\,\text{V}) = 20.35\,\text{V}$$
  4. Current through $R_{\text{lim}}$ is:
     $$I_{\text{fault}} = \frac{20.35\,\text{V}}{1.0\,\text{k}\Omega} \approx 20.4\,\text{mA}$$
  5. Power dissipated in $R_{\text{lim}}$ is:
     $$P = I^2 R = (0.0204)^2 \times 1000 \approx 0.416\,\text{W} \le 0.5\,\text{W}$$
  6. The PTC fuse warms up and transitions to high-impedance state ($> 100\,\text{k}\Omega$), dropping the fault current to $< 1\,\text{mA}$ indefinitely without damaging any component.

### 3.3 Stage 3: Low-Leakage Schottky Clamping (`BAT54S`)
* **Component**: `BAT54S` dual series Schottky barrier diode.
* **Forward Drop ($V_f$)**: Typ. $350\,\text{mV}$ at $20\,\text{mA}$.
* **Clamping Action**:
  * **Positive Overvoltage**: Clamps Node A to $V_{\text{CC}} + V_f = 3.30\,\text{V} + 0.35\,\text{V} = 3.65\,\text{V}$, strictly respecting the RP2040 maximum pin voltage rating of $3.63\,\text{V}\dots 3.80\,\text{V}$.
  * **Negative Overvoltage**: Clamps Node A to $\text{GND} - V_f = -0.35\,\text{V}$, preventing the internal substrate parasitic PNP transistors of the RP2040 from latching up.

### 3.4 Stage 4: Anti-Aliasing & Low-Pass Filter (`R_filt` + `C_filt`)
* **Filter Configuration**: Symmetrical RC low-pass filter ($R = 1\,\text{k}\Omega, C = 10\,\text{nF}$).
* **Cutoff Frequency ($f_c$)**:
  $$f_c = \frac{1}{2 \pi R_{\text{filt}} C_{\text{filt}}} = \frac{1}{2 \pi \times 1000 \times 10 \times 10^{-9}} \approx 15.915\,\text{kHz}$$
* **System Advantages**:
  * **Noise Rejection**: Attenuates RF pickup, broadcast interference, and switch-mode power supply ripple ($> 100\,\text{kHz}$) prior to ADC conversion.
  * **Continuity Response**: Step response rise time ($10\%\dots 90\%$) is:
    $$t_{\text{rise}} = 2.2 \times R \times C = 2.2 \times 1000 \times 10^{-8} = 22\,\mu\text{s}$$
    A $22\,\mu\text{s}$ rise time is instantaneous on human bench timescales while providing complete RF rejection.

---

## 4. Input Protection Electrical Ratings Matrix

| Parameter | Absolute Rating (Safe Continuous) | Rating without Protection Circuit | Failure Mechanism if Violated |
| :--- | :--- | :--- | :--- |
| **Max Continuous Positive DC** | $+30.0\,\text{V}_{\text{DC}}$ | $+3.63\,\text{V}_{\text{DC}}$ | Gate oxide breakdown on RP2040 GPIO input FETs |
| **Max Continuous Negative DC** | $-15.0\,\text{V}_{\text{DC}}$ | $-0.30\,\text{V}_{\text{DC}}$ | Substrate diode forward bias, latch-up, thermal runaway |
| **Max Peak ESD (IEC 61000-4-2)**| $\pm 15\,\text{kV}$ Air / $\pm 8\,\text{kV}$ Contact | $\pm 2\,\text{kV}$ HBM | Immediate silicon puncturing |
| **Continuous Fault Current** | $< 25\,\text{mA}$ (PTC limits to $<1\text{mA}$) | Unbounded | Internal bonding wire and metallization burnout |
| **AC Mains Voltage** | **STRICTLY PROHIBITED (0 V)** | **STRICTLY PROHIBITED (0 V)** | Arc flash, component vaporization, fire, fatal electric shock |
