# UDOO KEY External Instrument Control Architecture

**Document ID:** DOC-EXT-INST-20261005  
**Status:** SPECIFICATION & INTERFACE ARCHITECTURE  
**Date:** 2026-10-05  
**Authority:** Master Specification Section 39–50  

---

## 1. System Vision & Purpose

The UDOO KEY functions not only as a standalone digital diagnostic tool, but as an **instrumentation coordinator and bridge** for professional bench equipment.

Instead of replacing large bench instruments like digitizing oscilloscopes, spectrum analyzers, and programmable power supplies, the UDOO KEY interfaces with them to provide:
1. **Automated Remote Control**: SCPI / IEEE-488 command automation.
2. **High-Speed Waveform Retrieval**: Ingesting analog waveforms for digital post-processing and archival.
3. **Cross-Instrument Event Correlation**: Merging high-speed digital protocol traces (e.g. USB, SPI, I2C captured by RP2040) with high-bandwidth analog oscilloscope waveforms (captured by the bench instrument) using synchronized timestamps.

---

## 2. Supported Physical Transports

| Transport | Physical Layer | Electrical Requirements | Transceiver IC | Target Instruments |
| :--- | :--- | :--- | :--- | :--- |
| **RS-232** | Asynchronous Serial (±12V bipolar) | **MANDATORY**: Level translation from 3.3V logic to RS-232 bipolar levels. Direct GPIO connection strictly prohibited. | MAX3232 / SP3232 | HP 54520A (Serial Option), multimeters, bench supplies. |
| **HP-IB / GPIB** | IEEE-488 Parallel Bus (8 data lines, 8 handshake/management lines) | Dedicated IEEE-488 bus drivers with open-collector / tristate drivers capable of 48mA sink. | SN75160B + SN75161B / Dedicated GPIB controller | HP 54520A, HP 34401A, Tektronix TDS series. |
| **USB-TMC** | USB Test & Measurement Class | USB Host controller with TMC bulk endpoints. | Native USB Host / TinyUSB | Rigol, Siglent, modern Keysight scopes. |
| **SCPI over LAN** | TCP/IP Socket (Port 5025 / VXI-11) | Standard 10/100 Ethernet or Wi-Fi. | ESP32 Wi-Fi / LwIP stack | Modern LXI / LAN-enabled instruments. |

---

## 3. Generic Command Architecture

To ensure adaptability, the instrument control pipeline operates in two modes:

### 3.1 Raw Pass-Through Command Mode (`scope raw <command>`)
- Sends unmodified command strings directly to the instrument over the active link.
- Ingests and prints raw ASCII responses verbatim.
- Serves as the primary debugging tool and allows full access to vendor-specific command sets.

### 3.2 Normalized Instrument Abstraction Layer
Provides a uniform C++ API across differing instrument models:
```cpp
class IExternalInstrument {
public:
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual std::string identify() = 0;
    virtual bool reset() = 0;
    virtual bool setTimebase(double scale_sec_div) = 0;
    virtual bool setVerticalScale(int channel, double volts_div) = 0;
    virtual bool acquireWaveform(int channel, WaveformData& out_wave) = 0;
};
```

---

## 4. Waveform Data Model

Retrieved analog waveforms are normalized into a unified memory structure:

```text
WaveformRecord
 ├── source_instrument: "HP 54520A"
 ├── channel_index:     1
 ├── timestamp_us:      12483921
 ├── sample_count:      500
 ├── x_increment:       2.0e-9      (2 ns per sample -> 500 MSa/s)
 ├── x_origin:          -500.0e-9   (trigger point reference)
 ├── y_increment:       0.03906     (volts per ADC count)
 ├── y_origin:          0.000       (ground reference offset)
 └── raw_samples:       [std::vector<uint8_t>]
```

This normalized representation allows the C++ Desktop Companion (`udoo-key-app`) to plot analog waveforms directly on the same time axis as digital logic analyzer channels.
