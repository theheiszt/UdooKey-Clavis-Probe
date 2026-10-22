# UDOO KEY Comprehensive Test Plan & Verification Matrix

**Document ID:** DOC-TEST-PLAN-20261005  
**Status:** AUTHORITATIVE TEST PLAN  
**Date:** 2026-10-05  
**Authority:** Master Specification Section 63 & Section 85  

---

## 1. Test Classification Hierarchy

Every test across the UDOO KEY project must be classified into one of the following distinct categories:

| Classification | Definition | Environment | Example |
| :--- | :--- | :--- | :--- |
| **UNIT** | Self-contained software logic test with zero external hardware dependencies. | Host PC (CTest) | USB CRC5/16 calculation, PID decode, SCPI parser. |
| **SIMULATION** | Emulated peripheral or loopback mock validating protocol framing and timing. | Host PC / Mock Transport | `MockUsbHost`, `MockTransport`, synthetic packet streams. |
| **INTEGRATION** | End-to-end communication between multiple software modules or processes. | Host PC / Host-to-Device | Desktop JSON-RPC to firmware command dispatcher. |
| **FLASHED** | Firmware compiled, linked, and flashed into target microcontroller ROM. | RP2040 / ESP32 | Bootloader verification, FreeRTOS task startup. |
| **HARDWARE** | Real physical electrical signals measured or generated on target hardware pins. | UDOO KEY PCB + Probes | Real square wave on GP2, live I2C sensor scan, Wi-Fi AP scan. |
| **EXTERNAL-INSTRUMENT** | Physical interaction with external bench equipment. | UDOO KEY + External Unit | Live SCPI query and waveform capture from HP 54520A. |

---

## 2. Benchmark Subsystem Test Procedures (Section 85)

### 2.1 Logic Analyzer (8 Channels, GP2..GP9)
- **Classification**: HARDWARE
- **Signal Source**: Known 1 kHz to 10 MHz square wave from external signal generator (or RP2040 PWM test pin).
- **Procedure**:
  1. Attach CH0 (GP2) through CH7 (GP9) to target bus or square-wave generator.
  2. In CLI: `m logic`, `rate 10000000`, `depth 65536`, `trig ch0 rise`, `run`.
  3. Verify DMA acquisition completes, buffer freezes, and waveform matches expected period.

### 2.2 Frequency & Digital Measurements
- **Classification**: HARDWARE
- **Signal Source**: Known frequency standard (e.g. 10.000 kHz, 1.000 MHz).
- **Procedure**:
  1. Connect input to GP2 (or GP10 external trigger).
  2. In CLI: `measure freq`.
  3. Verify measured frequency is within ±0.05% of reference standard.
  4. Test prescaler control (`prescaler 1`, `prescaler 2`, `prescaler 4` on GP14) and verify frequency scaling.

### 2.3 Hardware Trigger Engine
- **Classification**: HARDWARE
- **Trigger Modes Tested**:
  - Rising Edge / Falling Edge on GP2..GP9.
  - Level High / Level Low.
  - 8-bit Pattern with Mask (e.g., `pattern 1010xxxx mask 11110000`).
  - External Hardware Trigger on GP10 (`EXT TRIG`).
- **Pass Criteria**: DMA acquisition stops deterministically upon trigger condition, retaining exact 70% pre-trigger and 30% post-trigger samples.

### 2.4 Precision Analog Voltmeter (ADC0..ADC2, GP26..GP28)
- **Classification**: HARDWARE
- **Signal Source**: Calibrated precision DC voltage source (0.500V, 1.650V, 3.300V).
- **Procedure**:
  1. Apply voltage to Header P2 Pin 31 (ADC0), Pin 32 (ADC1), Pin 34 (ADC2).
  2. In CLI: `v` or `adc`.
  3. Verify reading equals applied voltage within ±10 mV.

### 2.5 USB Physical Probing & Protocol Sniffing
- **Classification**: HARDWARE
- **Target Bus**: External USB Full-Speed device (12 Mbps) and Low-Speed device (1.5 Mbps).
- **Procedure**:
  1. Connect Target GND to P1.03 (GND).
  2. Connect Target D+ to P1.04 (GP2) through 47Ω damping resistor.
  3. Connect Target D- to P1.05 (GP3) through 47Ω damping resistor.
  4. In CLI: `m usb`, `probe`.
  5. Verify Full-Speed device reports `D+=1, D-=0 -> J-State [Full-Speed (12 Mbps)]`.
  6. Verify Low-Speed device reports `D+=0, D-=1 -> J-State [Low-Speed (1.5 Mbps)]`.
  7. Run packet sniffer: `sniff 16 3000`. Verify tokens, data payloads, and CRCs decode cleanly.

### 2.6 ESP32 Wireless & RF (Wi-Fi, BLE, IR, nRF24)
- **Classification**: HARDWARE
- **Procedures**:
  - **Wi-Fi**: Run `wifi scan` in CLI. Confirm detection of local SSIDs, RSSI values, and channels.
  - **BLE**: Run `ble scan 10` in CLI. Confirm detection of advertising BLE beacons and UUIDs.
  - **IR**: Connect photodiode to IR RX pin. Press button on standard TV remote. Confirm decoded protocol.
  - **nRF24**: Connect nRF24L01 module to UEXT. Execute carrier scan across 2.4 GHz channels.

### 2.7 External Oscilloscope Integration (HP 54520A)
- **Classification**: EXTERNAL-INSTRUMENT
- **Transport**: RS-232 via MAX3232 transceiver or HP-IB via IEEE-488 controller.
- **Procedure**:
  1. In CLI / Desktop App: `scope identify`. Confirm response `HEWLETT-PACKARD,54520A,...`.
  2. Execute `scope waveform 1`.
  3. Verify binary/ASCII waveform payload decodes into sample counts, voltage offsets, and timebase units.
  4. Display correlated waveform alongside digital logic analyzer capture.
