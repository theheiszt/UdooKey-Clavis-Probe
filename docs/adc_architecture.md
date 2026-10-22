# UDOO KEY — RP2040 ADC Measurement Subsystem Architecture

## 1. Executive Summary & System Mission

The UDOO KEY measurement instrument firmware integrates a high-performance, modular Analog-to-Digital Converter (ADC) subsystem hosted on the Raspberry Pi RP2040 dual-core microcontroller. While the onboard ESP32 handles higher-level transport and cloud/host communication, the RP2040 operates as the real-time acquisition engine.

The ADC subsystem converts the three external analog-capable pins on the UDOO KEY header—**GP26 (ADC0)**, **GP27 (ADC1)**, and **GP28 (ADC2)**—into precision instrumentation channels capable of:
1. High-accuracy DC voltage measurement with software calibration.
2. Raw diagnostic ADC readout (codes, input voltages, scaled engineering units).
3. DMA-driven analog waveform capture up to 500 kSPS with analog triggering.
4. Automatic waveform parameter extraction ($V_{\text{min}}, V_{\text{max}}, V_{\text{pp}}, V_{\text{avg}}, V_{\text{rms}}$, and zero-crossing frequency estimation).
5. High-speed, non-blocking continuity testing with acoustic buzzer (GP20) and optical LED (GP25) feedback.
6. Bench resistance, current shunt, and linear/NTC sensor measurement.
7. Internal monitoring of `VSYS` supply voltage (GP29 / ADC3) and on-die silicon temperature (ADC4).

---

## 2. Hardware Pinout & Signal Mapping

The UDOO KEY routes the RP2040 ADC lines to Header P2 and internal peripherals as defined below:

| ADC Channel | RP2040 GPIO | UDOO KEY Pin | Header / Location | Physical Circuit Connection | Primary Function |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **ADC0** | `GP26` | Pin 31 | Header P2, Pin 11 | Direct uncommitted analog input | Ch 0 / Continuity Probe+ / Voltage In |
| **ADC1** | `GP27` | Pin 32 | Header P2, Pin 12 | Direct uncommitted analog input | Ch 1 / Voltage Sense / Current Shunt |
| **ADC2** | `GP28` | Pin 34 | Header P2, Pin 14 | Direct uncommitted analog input | Ch 2 / Sensor Input / Auxiliary |
| **ADC3** | `GP29` | Internal | Onboard resistor divider | $1/3 \times V_{\text{SYS}}$ voltage divider | Power Supply / Battery Monitor |
| **ADC4** | N/A | Internal | Internal die sensor | Forward-biased diode $V_{\text{BE}}$ junction | RP2040 Core Temperature |
| **BUZZER** | `GP20` | Pin 26 | Header P1, Pin 15 | PWM Slice 2, Channel A output | Dedicated Continuity Acoustic Buzzer |
| **LED** | `GP25` | Pin 30 | Onboard D12 | Green LED cathode drive | Visual Continuity / Trigger Indicator |

> **Hardware Pin Verification**: Auditing of the official UDOO KEY schematic and hardware manual confirms that GPIO 26, 27, and 28 connect directly from the RP2040 package to external header P2. Claims in legacy example code suggesting GP26 is hardwired to an onboard photoresistor (LDR) are incorrect; GP26 is fully uncommitted and available for external instrumentation.

---

## 3. RP2040 ADC Hardware Core & Clocking

### 3.1 ADC Architecture
The RP2040 features an internal 12-bit Successive Approximation Register (SAR) analog-to-digital converter:
* **Resolution**: 12 bits ($2^{12} = 4096$ quantization levels, $0\dots 4095$).
* **Nominal Reference Voltage ($V_{\text{REF}}$)**: $3.300\,\text{V}$ (tied to the $3.3\,\text{V}$ regulated analog rail).
* **Quantization Step ($V_{\text{LSB}}$)**:
  $$V_{\text{LSB}} = \frac{V_{\text{REF}}}{4096} = \frac{3.300\,\text{V}}{4096} \approx 805.66\,\mu\text{V}$$
* **Effective Number of Bits (ENOB)**: Nominally $8.7\,\text{ENOB}$ due to internal thermal noise, package coupling, and power rail variations. With 16-sample to 64-sample oversampling in firmware, effective resolution approaches 10.5–11.0 ENOB for DC measurements.
* **Internal Input Impedance**: Approximately $100\,\text{k}\Omega$ to $1\,\text{M}\Omega$ dynamic sampling capacitance ($C_{\text{SAMPLE}} \approx 1\,\text{pF}$). High-impedance source signals require external buffering or low-pass filtering.

### 3.2 Clock Generation & Pacing
* The ADC block is clocked by the $48\,\text{MHz}$ system ADC PLL (`clk_adc`).
* Each conversion cycle requires **96 clock cycles**:
  $$t_{\text{conv}} = \frac{96}{48\,\text{MHz}} = 2.0\,\mu\text{s} \implies f_{\text{max}} = 500\,\text{kSPS}$$
* Slower sampling rates are established by the hardware fractional clock divider register `DIV` (`adc_set_clkdiv(div)`):
  $$\text{div} = \frac{48\,\text{MHz}}{f_{\text{target}}} - 1$$
  For example, for $f_{\text{sample}} = 50\,\text{kSPS}$:
  $$\text{div} = \frac{48{,}000{,}000}{50{,}000} - 1 = 960 - 1 = 959.0$$

---

## 4. DMA Waveform Engine & Memory Architecture

High-speed analog signal capture uses the RP2040 hardware ADC FIFO and a dedicated Direct Memory Access (DMA) channel, completely bypassing CPU polling during high-speed bursts.

```
       ┌───────────┐      ┌───────────┐      ┌───────────┐      ┌───────────────┐
AIN ──►│ RP2040    │─────►│ 4-Deep    │─────►│ Dedicated │─────►│ SRAM DMA      │
       │ SAR ADC   │      │ ADC FIFO  │ DREQ │ DMA Ch    │      │ Buffer        │
       └─────┬─────┘      └───────────┘      └───────────┘      │ (4096 Bytes / │
             ▲                                                  │  2048 Samples)│
             │ Pacing Clock (48 MHz / Div)                      └───────┬───────┘
       ┌─────┴─────┐                                                    │
       │ Clk Div   │                                                    ▼
       └───────────┘                                           Automated Analysis
                                                               (Min/Max/RMS/Freq)
```

### 4.1 DMA Configuration
* **Data Request (DREQ)**: `DREQ_ADC` paces each 16-bit DMA transfer directly from the ADC FIFO data register `&adc_hw->fifo`.
* **Transfer Width**: 16-bit (`DMA_SIZE_16`). Only the lower 12 bits contain ADC data; upper bits are zero.
* **FIFO Threshold**: The FIFO generates a DMA request whenever it contains at least 1 valid sample.
* **Buffer Size**: Dedicated static RAM array of $2{,}048$ samples ($4{,}096$ bytes).

### 4.2 SRAM Budget & Coexistence with Digital Logic Analyzer
The RP2040 contains $264\,\text{KiB}$ of SRAM split across four striped banks (SRAM0–SRAM3) and two $4\,\text{KiB}$ scratchpads. Memory allocations are strictly partitioned to ensure zero memory exhaustion:

| Memory Consumer | Allocated Size | Percentage of Total SRAM (264 KiB) | Function |
| :--- | :--- | :--- | :--- |
| **Logic Analyzer Capture** | $65{,}536\,\text{B}$ ($64\,\text{KiB}$) | $24.2\%$ | PIO DMA digital waveform buffer (8 channels) |
| **ADC Waveform Buffer** | $4{,}096\,\text{B}$ ($4\,\text{KiB}$) | $1.5\%$ | 2048 samples $\times$ 16-bit analog DMA buffer |
| **Display & UI State** | $3{,}584\,\text{B}$ (~$3.5\,\text{KiB}$) | $1.3\%$ | ST7735 graphics state, menu trees, toast |
| **Inter-Chip UART / CLI** | $2{,}048\,\text{B}$ ($2.0\,\text{KiB}$) | $0.8\%$ | Ring buffers for ESP32/Host communication |
| **Stack, Heaps, BSS** | $86{,}736\,\text{B}$ (~$85\,\text{KiB}$) | $32.0\%$ | RTOS/core execution context |
| **Free SRAM Headroom** | **$102{,}144\,\text{B}$ (~$100\,\text{KiB}$)**| **$40.2\%$** | **Available uncommitted memory space** |

Total RAM utilized across the entire instrument firmware is **$162{,}000\,\text{bytes}$ ($61.8\%$)**, guaranteeing absolute stability with over $100\,\text{KiB}$ of dynamic headroom.

---

## 5. Subsystem Functional Modules

### 5.1 DC Voltage Measurement & Oversampling
DC voltages are measured through a configurable pipeline:
1. Select ADC channel via `adc_select_input(channel)`.
2. Perform $N$-sample hardware oversampling ($N = 16$ or $64$).
3. Convert raw 12-bit code to normalized voltage:
   $$V_{\text{raw}} = \frac{\text{raw\_code}}{4095.0} \times V_{\text{source}}$$
4. Apply channel-specific calibration coefficients:
   $$V_{\text{cal}} = (V_{\text{raw}} - \text{offset}) \times \text{gain}$$
5. Apply external voltage divider scaling:
   $$V_{\text{in}} = V_{\text{cal}} \times \text{divider\_ratio}$$

### 5.2 Waveform Capture & Parameter Extraction
When `adc_waveform_start()` is called:
1. The ADC and DMA are armed for $N$ samples (up to 2048).
2. The ADC free-running mode is enabled at the target rate (1 kSPS to 500 kSPS).
3. Once DMA completes, `adc_waveform_compute_stats()` extracts:
   * **Minimum Voltage ($V_{\text{min}}$)**: Lowest sample voltage in buffer.
   * **Maximum Voltage ($V_{\text{max}}$)**: Highest sample voltage in buffer.
   * **Peak-to-Peak ($V_{\text{pp}}$)**: $V_{\text{max}} - V_{\text{min}}$.
   * **DC Average ($V_{\text{avg}}$)**: Arithmetic mean over all samples.
   * **True RMS ($V_{\text{rms}}$)**: Square root of mean of squared samples:
     $$V_{\text{rms}} = \sqrt{\frac{1}{N}\sum_{i=0}^{N-1} V[i]^2}$$
   * **Frequency Estimation**: Zero-crossing analysis around $V_{\text{avg}}$ with hysteresis band rejection.

### 5.3 Analog Trigger Engine
The firmware implements an analog threshold trigger:
* **Trigger Slope**: Rising Edge, Falling Edge, or Auto/Free-Run.
* **Trigger Threshold**: User-defined voltage level (e.g. $1.65\,\text{V}$).
* **Trigger Hysteresis**: Rejection band (e.g. $\pm 50\,\text{mV}$) to prevent trigger chatter on noisy signals.
* **Pre-Trigger Buffer**: Configurable window (up to $50\%$) providing visibility prior to trigger inception.

### 5.4 High-Speed Continuity Engine
* Operates on a non-blocking state machine updating every cycle with $<10\,\mu\text{s}$ conversion time.
* Evaluates measured loop resistance against configurable thresholds ($10\,\Omega, 25\,\Omega, 50\,\Omega, 100\,\Omega$).
* Controls GP20 hardware PWM tone generator ($2.7\,\text{kHz}$) and onboard GP25 green LED simultaneously.

---

## 6. Serial Console CLI Command Interface

The ADC subsystem is fully controllable via the interactive USB serial CLI:

| Command | Arguments | Description | Example |
| :--- | :--- | :--- | :--- |
| `adc` | `[0\|1\|2]` | Read calibrated DC voltage on specified channel (default 0). | `adc 0` |
| `volt` | `[0\|1\|2] [ch2...]` | Display tabular multi-channel voltage readings. | `volt` |
| `raw` | `[0\|1\|2]` | Display diagnostic raw 12-bit ADC code and raw voltage. | `raw 0` |
| `res` | `[ch] [r_ref]` | Measure external resistance using reference resistor. | `res 0 1000` |
| `cont` | `[thresh] [hyst]` | Configure or query continuity testing threshold and hysteresis. | `cont 25 10` |
| `wave` | `[rate_hz] [samples]` | Capture DMA waveform and report comprehensive signal stats. | `wave 50000 512` |
| `cal` | `[ch] [off] [gain]` | Inspect or update two-point calibration parameters. | `cal 0 0.005 1.002` |
| `curr` | `[ch] [shunt_ohms]` | Measure current in Amperes across an external shunt resistor. | `curr 1 0.1` |
| `buzzer`| `[freq] [ms]` | Acoustic buzzer diagnostic tone generator (GP20). | `buzzer 2700 200` |
| `temp` | *(none)* | Read RP2040 internal core temperature and VSYS rail. | `temp` |
