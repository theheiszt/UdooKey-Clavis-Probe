# UDOO KEY — RP2040 ADC Calibration & Precision Guide

## 1. RP2040 ADC Hardware Characteristics & Silicon Reality

The RP2040 integrates a 12-bit Successive Approximation Register (SAR) ADC. While theoretical 12-bit resolution provides 4096 discrete quantization steps ($805.66\,\mu\text{V}$ per LSB at $3.3\,\text{V}$), real-world analog performance on microcontrollers is constrained by silicon characteristics, power rail noise, and reference voltage variations.

### 1.1 Resolution vs. Accuracy
It is critical to distinguish between **resolution** and **accuracy**:
* **Resolution**: The smallest incremental step the ADC can detect. For the RP2040 at $V_{\text{REF}} = 3.300\,\text{V}$, 12-bit resolution equals:
  $$\text{Resolution} = \frac{3.300\,\text{V}}{4096} = 805.66\,\mu\text{V} \approx 0.806\,\text{mV}$$
* **Accuracy**: How close the measured value is to the absolute, true physical voltage. Without calibration, the uncompensated RP2040 ADC exhibits up to $\pm 30\,\text{mV}$ to $\pm 50\,\text{mV}$ of absolute error due to:
  1. Internal reference voltage tolerance of the $3.3\,\text{V}$ regulator ($\pm 2\%$ to $\pm 3\%$).
  2. Input buffer offset voltage ($\pm 5\,\text{mV}$ typical).
  3. Integral Non-Linearity (INL) and Differential Non-Linearity (DNL).

### 1.2 Silicon Errata E9: ADC Differential Non-Linearity (DNL)
As documented in the official Raspberry Pi RP2040 Silicon Errata (RP2040-E9):
* The RP2040 SAR ADC capacitor DAC has unequal capacitive weightings, causing significant DNL spikes at binary boundaries:
  $$\text{Codes: } 512, 1536, 2560, \text{ and } 3584$$
* At these transitions, the differential non-linearity can reach $-1.0\,\text{LSB}$ to $+2.0\,\text{LSB}$, meaning some codes can be skipped or appear slightly wider than nominal.
* **Firmware Mitigation**:
  * **Oversampling**: The firmware applies 16-sample to 64-sample hardware/software oversampling for DC measurements, effectively dithering the ADC transfer curve and smoothing out localized DNL spikes.
  * **Two-point Gain & Offset Correction**: Compensates for systemic gain slope error and ground offset.

---

## 2. Two-Point Calibration Mathematics

The ADC subsystem models the relationship between raw ADC voltage $V_{\text{raw}}$ and calibrated actual voltage $V_{\text{cal}}$ using a linear transfer function:

$$V_{\text{cal}} = (V_{\text{raw}} - V_{\text{offset}}) \times K_{\text{gain}}$$

Where:
* $V_{\text{raw}}$ is the raw voltage calculated directly from the ADC code:
  $$V_{\text{raw}} = \frac{\text{code}_{\text{raw}}}{4095.0} \times 3.300\,\text{V}$$
* $V_{\text{offset}}$ is the zero-voltage ground offset error (expressed in Volts).
* $K_{\text{gain}}$ is the dimensionless slope correction factor (nominally $1.0000$).

### 2.1 Derivation of Calibration Coefficients
Given two high-precision calibration standards:
1. Low reference point: Known voltage $V_{\text{std\_low}}$ (typically $0.000\,\text{V}$ or $0.100\,\text{V}$), producing measured raw voltage $V_{\text{meas\_low}}$.
2. High reference point: Known voltage $V_{\text{std\_high}}$ (typically $2.500\,\text{V}$ or $3.000\,\text{V}$), producing measured raw voltage $V_{\text{meas\_high}}$.

The gain correction coefficient $K_{\text{gain}}$ is:
$$K_{\text{gain}} = \frac{V_{\text{std\_high}} - V_{\text{std\_low}}}{V_{\text{meas\_high}} - V_{\text{meas\_low}}}$$

The voltage offset $V_{\text{offset}}$ is:
$$V_{\text{offset}} = V_{\text{meas\_low}} - \left(\frac{V_{\text{std\_low}}}{K_{\text{gain}}}\right)$$

If $V_{\text{std\_low}} = 0.000\,\text{V}$ (probes shorted to AGND):
$$V_{\text{offset}} = V_{\text{meas\_low}}$$
$$K_{\text{gain}} = \frac{V_{\text{std\_high}}}{V_{\text{meas\_high}} - V_{\text{offset}}}$$

---

## 3. Step-by-Step Two-Point Calibration Procedure

To perform calibration of an ADC channel on the UDOO KEY:

### Step 1 — Zero Offset Calibration ($0.000\,\text{V}$)
1. Connect the target ADC channel (e.g., GP26 / ADC0) directly to system Analog Ground (GND).
2. Issue the raw readout command via the USB serial console:
   ```text
   raw 0
   ```
3. Record the reported raw voltage $V_{\text{meas\_low}}$ (e.g., `0.012 V`). This is your $V_{\text{offset}}$.

### Step 2 — High Reference Calibration ($2.500\,\text{V}$ or $3.000\,\text{V}$)
1. Connect a calibrated, precision reference voltage standard (e.g., a $2.500\,\text{V}$ ADR03 or LM4040 reference, or a 6.5-digit bench DMM monitored rail) to the target ADC input.
2. Issue the raw readout command:
   ```text
   raw 0
   ```
3. Record the reported raw voltage $V_{\text{meas\_high}}$ (e.g., `2.482 V`).

### Step 3 — Compute Correction Coefficients
Calculate $K_{\text{gain}}$:
$$K_{\text{gain}} = \frac{2.500\,\text{V}}{2.482\,\text{V} - 0.012\,\text{V}} = \frac{2.500}{2.470} = 1.01215$$

### Step 4 — Store Calibration in Firmware
Apply the new calibration factors via the CLI:
```text
cal 0 0.012 1.01215
```
Verify the calibrated readout:
```text
adc 0
```
The channel will now report $2.500\,\text{V} \pm 1\,\text{mV}$ accurately.

---

## 4. Calibration Data Structure & Non-Volatile Storage

The calibration configuration is structured in firmware as follows:

```cpp
struct AdcChannelConfig {
    uint8_t  gpio;             // Physical RP2040 GPIO (26, 27, 28, 29)
    float    divider_ratio;    // External divider multiplier (1.0 for direct probe)
    float    offset_v;         // Zero-point offset in Volts
    float    gain;             // Gain correction multiplier
    uint16_t averaging_samples;// Oversampling count (16, 32, 64)
};
```

### 4.1 EEPROM / Emulated Flash Layout
The RP2040 Arduino-Pico core emulates EEPROM in the upper flash sector (`0x10FFF000`). The calibration storage block uses the following layout:

| Byte Offset | Field | Type | Description |
| :--- | :--- | :--- | :--- |
| `0x00 - 0x03` | `MAGIC_HEADER` | `uint32_t` | Magic identifier `0x41444343` ("ADCC") |
| `0x04 - 0x05` | `VERSION` | `uint16_t` | Structure schema version (`0x0001`) |
| `0x06 - 0x07` | `RESERVED` | `uint16_t` | Reserved alignment padding |
| `0x08 - 0x1B` | `CH0_CONFIG` | 20 Bytes | ADC0 config (GPIO, divider, offset, gain, avg) |
| `0x1C - 0x2F` | `CH1_CONFIG` | 20 Bytes | ADC1 config (GPIO, divider, offset, gain, avg) |
| `0x30 - 0x43` | `CH2_CONFIG` | 20 Bytes | ADC2 config (GPIO, divider, offset, gain, avg) |
| `0x44 - 0x47` | `CRC32` | `uint32_t` | Checksum over bytes `0x00` through `0x43` |

On boot, `adc_subsystem_init()` checks for the magic header and valid CRC. If valid, calibrated coefficients are restored; otherwise, safe factory defaults ($\text{offset} = 0.000\,\text{V}$, $\text{gain} = 1.000$) are loaded.

---

## 5. External Voltage Dividers & High-Voltage Scaling

Direct input to the RP2040 ADC must never exceed $3.3\,\text{V}$. For higher voltage ranges ($\pm 15\,\text{V}$, $0\dots 30\,\text{V}$), an external resistive divider is required.

```
                  R1
VIN ────────────/\/\/\──────────┬──────────► V_ADC (to GP26/27/28)
                                │
                                \
                                /  R2
                                \
                                │
                                ▼
                               GND
```

### 5.1 Divider Ratio Formula
The total attenuation factor $\alpha$ is:
$$\alpha = \frac{V_{\text{IN}}}{V_{\text{ADC}}} = \frac{R_1 + R_2}{R_2} = 1 + \frac{R_1}{R_2}$$

### 5.2 Recommended Divider Resistor Values
To minimize loading on the measured circuit while keeping the source impedance within the RP2040 ADC sampling window:

| Target Max Input | R1 Value (0.1% Metal Film) | R2 Value (0.1% Metal Film) | Ratio ($\alpha$) | Max ADC Voltage |
| :--- | :--- | :--- | :--- | :--- |
| **Direct (3.3V)** | Direct Wire ($0\,\Omega$) | Unpopulated | $1.000\times$ | $3.300\,\text{V}$ |
| **0 to 10 V** | $20.0\,\text{k}\Omega$ | $10.0\,\text{k}\Omega$ | $3.000\times$ | $3.333\,\text{V}$ |
| **0 to 30 V** | $90.0\,\text{k}\Omega$ | $10.0\,\text{k}\Omega$ | $10.000\times$ | $3.000\,\text{V}$ |

The calculated divider ratio is entered into firmware using:
$$V_{\text{actual}} = V_{\text{ADC\_calibrated}} \times \alpha$$
