# UDOO KEY — Logic Scope User & Technical Guide

## 1. Overview

The **UDOO KEY Logic Scope** is an integrated 8-channel logic analyzer interface accessible directly via any web browser when connected to the instrument's Wi-Fi Access Point (`UDOO-BIT-PIRATE-SCOPE` at `http://192.168.4.1/`).

It combines real-time hardware acquisition executed by the Raspberry Pi RP2040 microcontroller with responsive visualization, timing cursors, automated signal measurements, and hardware protocol decoding executed in the browser client.

---

## 2. Hardware Channel Mapping

The Logic Scope monitors 8 digital probe channels and 1 dedicated external trigger channel:

| Channel | Physical RP2040 Pin | Trace Color | Hex Code | Default Role |
|---|---|---|---|---|
| **CH0** | `GP2` | Neon Green | `#10b981` | Digital Input / UART TX / SPI SCK / I2C SCL |
| **CH1** | `GP3` | Cyan | `#06b6d4` | Digital Input / UART RX / SPI MOSI / I2C SDA |
| **CH2** | `GP4` | Yellow | `#eab308` | Digital Input / SPI MISO |
| **CH3** | `GP5` | Orange | `#f97316` | Digital Input / SPI CS |
| **CH4** | `GP6` | Magenta | `#ec4899` | Digital Input |
| **CH5** | `GP7` | Purple | `#a855f7` | Digital Input |
| **CH6** | `GP8` | Sky Blue | `#38bdf8` | Digital Input |
| **CH7** | `GP9` | Silver/White | `#f1f5f9` | Digital Input |
| **EXT TRIG** | `GP10` | Red Accent | `#ef4444` | External Hardware Trigger Input |

---

## 3. Acquisition Control & Parameters

### Capture Controls
- **`[ARM / RUN]`**: Sets the configured parameters, arms the RP2040 PIO/DMA acquisition engine, and prepares to capture on the specified trigger condition. In *Continuous Run* mode, re-arms automatically upon buffer transfer.
- **`[SINGLE]`**: Arms the instrument for a single acquisition cycle and stops after one buffer is captured.
- **`[STOP]`**: Transmits an immediate abort signal to the RP2040, halting the PIO state machine.
- **`[CLEAR]`**: Resets the canvas waveform display and timing cursors.

### Configurable Parameters
1. **Sample Rate**:
   - `100 kHz` (10 $\mu$s per sample)
   - `500 kHz` (2 $\mu$s per sample)
   - `1 MHz` (1 $\mu$s per sample)
   - `2 MHz` (500 ns per sample)
   - `5 MHz` (200 ns per sample)
   - `10 MHz` (100 ns per sample — Default)
   - `25 MHz` (40 ns per sample)
   - `50 MHz` (20 ns per sample)
   - `100 MHz` (10 ns per sample — Max RP2040 PIO rate)
2. **Sample Depth**:
   - `1,024`, `2,048`, `4,096`, `8,192` (Default), `16,384`, `32,768`, `65,536` (Maximum 64 KiB RP2040 DMA circular buffer).
3. **Trigger Source**:
   - Channels 0 through 7 (`GP2`..`GP9`)
   - External Trigger (`GP10`)
4. **Trigger Mode**:
   - **Auto / Immediate**: Triggers instantly without waiting for a transition.
   - **Rising Edge ($\uparrow$)**: Triggers on 0 to 1 transition.
   - **Falling Edge ($\downarrow$)**: Triggers on 1 to 0 transition.
   - **Any Edge ($\updownarrow$)**: Triggers on either rising or falling edge.
   - **High Level (1)**: Triggers when channel is high.
   - **Low Level (0)**: Triggers when channel is low.
5. **Pre-Trigger Buffer Ratio**:
   - Configurable between `0%` (all post-trigger), `25%`, `50%`, `70%` (standard instrument default), and `90%` (pre-trigger examination).

---

## 4. Canvas Waveform Viewer & Navigation

The waveform viewer is rendered on an HTML5 `<canvas>` element with high-DPI retina display scaling (`window.devicePixelRatio`):
- **Horizontal Zoom**:
  - Mouse wheel zoom centered at pointer position.
  - Dedicated `[Zoom +]` and `[Zoom -]` buttons.
  - `[Fit All]` button to reset view to 100% of the buffer.
  - Pinch-to-zoom on touch screens.
- **Horizontal Pan**:
  - Click-and-drag horizontally on the canvas.
  - Touch-swipe to pan on mobile devices.
- **Time Ruler**:
  - Dynamic microsecond ($\mu$s), nanosecond (ns), or millisecond (ms) scale divisions.
- **Trigger Marker ($T_0$)**:
  - Red dashed vertical line positioned at the precise trigger sample index.

---

## 5. Dual Timing Cursors

- Click on the waveform canvas to position **Cursor A** (Amber vertical line).
- Click again to position **Cursor B** (Sky Blue vertical line).
- Click a third time to reset Cursor A to the new position.
- Real-time readouts displayed on the top toolbar:
  - **Cursor A**: Sample index and time offset ($t_A$) relative to trigger $T_0$.
  - **Cursor B**: Sample index and time offset ($t_B$) relative to trigger $T_0$.
  - **$\Delta t$**: Absolute elapsed time between cursors ($|t_B - t_A|$).
  - **$\Delta S$**: Sample distance ($|\text{Sample}_B - \text{Sample}_A|$).
  - **Frequency ($f$)**: Calculated as $1 / \Delta t$ (Hz, kHz, or MHz).

---

## 6. Real-Time Hardware Signal Measurements

Computed automatically across the captured sample array for any selected channel:
- **Frequency**: Detected fundamental frequency.
- **Period**: Full cycle duration in microseconds.
- **Duty Cycle**: Active HIGH percentage ($0.0\%$ to $100.0\%$).
- **Pulse Width High ($t_{high}$)**: Average duration of high pulses.
- **Pulse Width Low ($t_{low}$)**: Average duration of low pulses.
- **Total Transitions**: Complete count of rising and falling edges.

---

## 7. In-Browser Protocol Decoders

Operating directly on the raw binary `Uint8Array` in the browser client:

### UART / Serial Decoder
- Select channel (`CH0`..`CH7`), baud rate (9600 to 1,000,000), and parity mode (8N1, 8E1, 8O1).
- Detects falling start bit edge, samples at bit midpoints ($1.5 \times T_{bit}, 2.5 \times T_{bit}, \dots$).
- Decodes hex bytes and ASCII characters.
- Overlays decoded bubble markers directly on the waveform lane and populates the token table.

### SPI Decoder
- Select SCK, MOSI, MISO, and CS channels.
- Configurable Clock Polarity (CPOL) and Clock Phase (CPHA) for Modes 0, 1, 2, and 3.
- Decodes full-duplex MOSI and MISO transfers during active CS assertion.

### I²C Decoder
- Select SCL and SDA channels.
- Detects START conditions (SDA fall while SCL high) and STOP conditions (SDA rise while SCL high).
- Decodes 7-bit addresses, Read/Write direction bits, ACK/NACK status, and data payload bytes.

---

## 8. Export Capabilities

- **VCD (Value Change Dump)**: Standard IEEE 1364 format compatible with PulseView, Sigrok, and GTKWave.
- **CSV (Comma-Separated Values)**: `Sample,Time_us,CH0,CH1,CH2,CH3,CH4,CH5,CH6,CH7` format for Excel or MATLAB.
- **JSON**: Machine-readable metadata and base64-encoded binary sample array.
