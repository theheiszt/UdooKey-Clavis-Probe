# UDOO KEY — JTAG / SWD Testing & Verification Report

## 1. Test Summary

The JTAG/SWD Debug Subsystem has been verified across all architectural layers:
- **Desktop Companion C++ Automated Tests (CTest)**: 7 / 7 test suites passing (100% green).
- **RP2040 Firmware (`rp2040_logic_analyzer`)**: Cleanly compiled to `.pio/build/rp2040_logic_analyzer/firmware.uf2`.
- **ESP32 Firmware (`esp32_coprocessor`)**: Cleanly compiled to `.pio/build/esp32_coprocessor/firmware.bin`.
- **Desktop Application (`udoo-key-app`)**: Cleanly compiled and linked with native `DebugView` workbench tab.

---

## 2. Test Execution Results (CTest)

```text
========================================
  UDOO KEY C++ Test Suite (CTest)       
========================================
[PASS] RingBuffer Tests
[PASS] StreamDemux Tests
[PASS] BinaryLogic Tests
[PASS] SumpClient Tests
[PASS] Decoder Tests (UART / I2C / SPI)
[PASS] Exporter Tests (VCD / CSV / BIN)
[TEST] JtagTap Transitions & Navigation...
  -> PASSED
[TEST] JtagScanData Bit-Precise Manipulations...
  -> PASSED
[TEST] JtagIdCode & JEP106 Manufacturer Decoding...
  -> PASSED
[TEST] ARM CoreSight SWD Protocol & DPIDR...
  -> PASSED
[TEST] DebugProtocol Binary Serialization & CRC16...
  -> PASSED
[TEST] MockDebugTransport, JtagClient & SwdClient End-to-End...
  -> PASSED
[PASS] JTAG & SWD Subsystem Tests
========================================
Results: 7 / 7 test suites passed.
========================================
```

---

## 3. Coverage Breakdown

| Test Area | Target Validated | Verification Details | Result |
| :--- | :--- | :--- | :--- |
| **TAP Transitions** | IEEE 1149.1 State Machine | Tested all 16 states for TMS=0/1. Verified `resetSequence()` recovers `TestLogicReset` from every state. | **PASS** |
| **Shortest Path** | Graph BFS Navigation | Minimal TMS sequences verified for transitions between all states. | **PASS** |
| **Bit-Precise Scan** | Non-byte-aligned data | Verified 5-bit IR (BYPASS 0x1F), 37-bit boundary-scan vectors, 32-bit IDCODEs, and hex string conversions. | **PASS** |
| **JEP-106 Lookup** | Manufacturer ID Decoder | Verified ARM (`0x23B`), ST (`0x020`), TI (`0x017`), Lattice (`0x06E`), Raspberry Pi (`0x4F5`), and invalid IDCODE rejection. | **PASS** |
| **SWD Protocol** | ARM CoreSight Framing | Verified request header construction (`Read DPIDR = 0xA5`, `Write ABORT = 0x81`), even parity, and DPIDR decoding (`0x0BC12477`). | **PASS** |
| **DebugProtocol Wire** | Sync `0x5B` Framing & CRC-16 | Verified serialize, deserialize, sequence counter, payload length, and bit corruption CRC failure detection. | **PASS** |
| **Mock Simulation** | End-to-End Client Execution | Verified TAP reset, clock configuration, multi-device chain enumeration, Shift-IR/DR, SWD line reset, DP read/write, MEM-AP memory write. | **PASS** |
| **Firmware Build** | RP2040 UF2 & ESP32 Bin | Successfully compiled with Earle Philhower RP2040 core and Espressif ESP32 framework. | **PASS** |
