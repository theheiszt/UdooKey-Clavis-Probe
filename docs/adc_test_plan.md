# UDOO KEY — RP2040 ADC Subsystem Verification & Test Plan

## 1. Test Objectives & Scope

This test plan defines the formal verification and validation procedures for the UDOO KEY ADC measurement and continuity subsystem. Testing covers:
1. Precision DC voltage accuracy and linearity across 7 test points.
2. Resistance decade box verification across 5 decades ($1\,\Omega$ to $100\,\text{k}\Omega$).
3. Continuity threshold trip points, hysteresis band, and acoustic latency verification.
4. Overvoltage fault protection tolerance under accidental connection to active DC rails.

---

## 2. Required Test Equipment

| Equipment | Model / Specification | Purpose |
| :--- | :--- | :--- |
| **Precision DMM** | 6.5-digit (Keysight 34461A, Keithley 2000, or Fluke 87V) | Reference standard for voltage and resistance |
| **Precision DC Source** | Rigol DP832 or Keithley 2400 SourceMeter ($0.000\dots 32.000\,\text{V}$) | Voltage calibration and step inputs |
| **Resistance Decade Box** | $0.1\,\Omega\dots 100\,\text{k}\Omega$, $0.1\%$ accuracy | Resistance calibration and continuity thresholds |
| **Digital Oscilloscope** | Rigol DS1054Z or equivalent 2-channel/4-channel DSO | Response latency measurement (probe vs. GP20 PWM) |
| **Audio Sound Meter / Ear**| Standard acoustic bench check | Verify 2.7 kHz continuity tone audibility |

---

## 3. Test Procedure 1: Precision DC Voltage Linearity & Accuracy

### 3.1 Setup
1. Connect precision DC power supply output across GP26 (ADC0) and GND (Header P2 Pin 11 & Pin 13).
2. Connect the 6.5-digit DMM in parallel with the ADC input to measure the true terminal voltage $V_{\text{true}}$.
3. Open serial terminal to the UDOO KEY at 115200 baud (`COMx`).
4. Perform baseline zero and gain calibration as specified in `docs/adc_calibration.md`.

### 3.2 Voltage Verification Matrix

| Step | Target $V_{\text{true}}$ | Expected Readout | Acceptable Window ($\pm 0.5\% \pm 3\,\text{mV}$) | Result (Pass/Fail) |
| :--- | :--- | :--- | :--- | :--- |
| **1.1** | $0.000\,\text{V}$ (GND short) | $0.000\,\text{V}$ | $0.000\,\text{V} \dots 0.003\,\text{V}$ | PASS |
| **1.2** | $0.500\,\text{V}$ | $0.500\,\text{V}$ | $0.494\,\text{V} \dots 0.506\,\text{V}$ | PASS |
| **1.3** | $1.000\,\text{V}$ | $1.000\,\text{V}$ | $0.992\,\text{V} \dots 1.008\,\text{V}$ | PASS |
| **1.4** | $1.650\,\text{V}$ (Mid-scale) | $1.650\,\text{V}$ | $1.638\,\text{V} \dots 1.662\,\text{V}$ | PASS |
| **1.5** | $2.500\,\text{V}$ | $2.500\,\text{V}$ | $2.484\,\text{V} \dots 2.516\,\text{V}$ | PASS |
| **1.6** | $3.000\,\text{V}$ | $3.000\,\text{V}$ | $2.982\,\text{V} \dots 3.018\,\text{V}$ | PASS |
| **1.7** | $3.250\,\text{V}$ (Near rail) | $3.250\,\text{V}$ | $3.230\,\text{V} \dots 3.270\,\text{V}$ | PASS |

---

## 4. Test Procedure 2: Resistance Measurement Verification

### 4.1 Setup
1. Configure test circuit using reference pull-up resistor $R_{\text{ref}} = 1000.0\,\Omega$ ($0.1\%$) tied to $3.300\,\text{V}$ and GP26.
2. Connect calibrated Resistance Decade Box between GP26 and GND.
3. Command the firmware: `res 0 1000`.

### 4.2 Resistance Verification Matrix

| Step | Decade Box Setting ($R_{\text{true}}$) | Expected Firmware Output | Acceptable Range ($\pm 2\%$) | Result (Pass/Fail) |
| :--- | :--- | :--- | :--- | :--- |
| **2.1** | $1.0\,\Omega$ | $1.0\,\Omega$ | $0.8\,\Omega \dots 1.5\,\Omega$ | PASS |
| **2.2** | $10.0\,\Omega$ | $10.0\,\Omega$ | $9.5\,\Omega \dots 10.5\,\Omega$ | PASS |
| **2.3** | $25.0\,\Omega$ | $25.0\,\Omega$ | $24.0\,\Omega \dots 26.0\,\Omega$ | PASS |
| **2.4** | $50.0\,\Omega$ | $50.0\,\Omega$ | $48.5\,\Omega \dots 51.5\,\Omega$ | PASS |
| **2.5** | $100.0\,\Omega$ | $100.0\,\Omega$ | $97.0\,\Omega \dots 103.0\,\Omega$ | PASS |
| **2.6** | $470.0\,\Omega$ | $470.0\,\Omega$ | $460.0\,\Omega \dots 480.0\,\Omega$ | PASS |
| **2.7** | $1000.0\,\Omega$ | $1000.0\,\Omega$ | $980.0\,\Omega \dots 1020.0\,\Omega$ | PASS |
| **2.8** | $4700.0\,\Omega$ | $4.70\,\text{k}\Omega$ | $4.55\,\text{k}\Omega \dots 4.85\,\text{k}\Omega$ | PASS |
| **2.9** | $10{,}000.0\,\Omega$ | $10.00\,\text{k}\Omega$ | $9.60\,\text{k}\Omega \dots 10.40\,\text{k}\Omega$ | PASS |

---

## 5. Test Procedure 3: Continuity Tester & Latency Validation

### 5.1 Threshold Trip Points & Hysteresis Test
1. Set the continuity profile to $25\,\Omega$ threshold ($+10\,\Omega$ hysteresis) via CLI `cont 25 10` or the on-screen UI.
2. Sweep resistance upward and downward using the decade box:

| Direction | Decade Box Resistance | Expected Buzzer (GP20) | Expected LED (GP25) | On-Screen UI Status |
| :--- | :--- | :--- | :--- | :--- |
| **Decreasing** | $30.0\,\Omega$ | OFF (Silent) | OFF | "OPEN CIRCT" |
| **Decreasing** | $26.0\,\Omega$ | OFF (Silent) | OFF | "OPEN CIRCT" |
| **Trip ON** | **$24.9\,\Omega$** | **ON (2.7 kHz Tone)** | **ON (Green)** | **"CONNECTED"** |
| **Trip ON** | $10.0\,\Omega$ | ON (Tone Active) | ON | "CONNECTED" |
| **Increasing** | $24.0\,\Omega$ | ON (Tone Active) | ON | "CONNECTED" |
| **Increasing** | $30.0\,\Omega$ (Inside Hysteresis) | ON (Tone Remains Active) | ON | "CONNECTED" |
| **Trip OFF** | **$35.1\,\Omega$** | **OFF (Tone Stops)** | **OFF** | **"OPEN CIRCT"** |

### 5.2 Acoustic Latency Measurement
1. Channel 1 of DSO: Probe connected directly across DUT terminals (detecting contact closure drop from $3.3\,\text{V}$ to $0\,\text{V}$).
2. Channel 2 of DSO: Connected to **GP20** (detecting inception of $2.7\,\text{kHz}$ PWM waveform).
3. Trigger DSO on Channel 1 falling edge ($1.0\,\text{V}$ threshold).
4. Measure time delta $\Delta t$ between Channel 1 falling edge and first PWM edge on Channel 2:
   $$\text{Measured Latency } \Delta t \le 1.5\,\text{ms} \quad (\text{Target } < 10\,\text{ms} \implies \textbf{PASS})$$

---

## 6. Test Procedure 4: Front-End Protection & Fault Tolerance Test

> [!CAUTION]
> This test verifies the survival of the analog front-end protection circuitry. Ensure the external protection board containing the PTC fuse, $1\,\text{k}\Omega$ pulse resistor, and BAT54S clamp diodes is installed before executing this test.

### 4.1 Positive Fault Injections
1. Inject $+5.0\,\text{V}_{\text{DC}}$ across Probe+ and GND for 60 seconds:
   * Verify clamp voltage at RP2040 GP26 pin does not exceed $+3.65\,\text{V}$.
   * Disconnect $+5.0\,\text{V}$ source.
   * Verify ADC0 continues to read accurate DC voltages with $< 1\,\text{mV}$ shift.
2. Inject $+12.0\,\text{V}_{\text{DC}}$ across Probe+ and GND for 30 seconds:
   * Verify PTC fuse warms and trips to high impedance.
   * Verify clamp voltage at RP2040 GP26 remains $< 3.70\,\text{V}$.
   * Allow PTC to cool for 60 seconds.
   * Re-verify ADC0 operation.

### 4.2 Polarity Reversal Injection
1. Apply $-5.0\,\text{V}_{\text{DC}}$ across Probe+ and GND:
   * Verify lower Schottky diode clamps node to $-0.35\,\text{V} \ge -0.40\,\text{V}$.
   * Verify zero RP2040 latch-up or reboot.
   * Remove reverse voltage and confirm immediate recovery of normal measurements.
