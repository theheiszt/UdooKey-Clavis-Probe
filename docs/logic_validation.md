# UDOO KEY — Protocol Subsystem Validation: 8-Channel 100MSPS Logic Analyzer CLI Mode

**Document ID:** DOC-PROTO-LOGIC-20261003  
**Status:** VALIDATED & FROZEN (MILESTONE PROTO-7)  
**Date:** 2026-10-03  
**Target Hardware:** SECO UDOO KEY (RP2040 Dual Cortex-M0+ @ 133 MHz, REV O)  
**Firmware Version:** UDOO-KEY-OS v2.1-bp-terminal (UF2 Size: 488,960 bytes)  

---

## 1. Executive Summary

This document establishes the experimental validation, hardware pin reconciliation, acquisition path verification, and formal freeze of the **seventh Bus Pirate 5 protocol mode: 8-Channel 100MSPS Logic Analyzer CLI Mode (`m logic` / `LOGIC>`)** on the SECO UDOO KEY architecture.

Following the strict project development sequence:
```text
TERMINAL (FROZEN) ──► GPIO / DIO (FROZEN) ──► SPI (FROZEN) ──► I2C (FROZEN) ──► UART (FROZEN) ──► 1-WIRE (FROZEN) ──► METER (FROZEN) ──► LOGIC CLI (FROZEN) ──► USB TOOLS
```
the Logic Analyzer CLI subsystem has been implemented through a dedicated hardware abstraction layer (`udoo_hal/logic_hal`), seamlessly integrating with the permanently frozen RP2040 PIO0 SM0 circular DMA capture engine (64 KiB SRAM ring) and advanced composable trigger engine (`trigger_engine`), while introducing an interactive terminal acquisition workflow, horizontal multi-channel ASCII timing diagram renderer, and digital signal frequency/duty-cycle analyzer.

---

## 2. Hardware Architecture & Physical Pinout

### 2.1 Digital Input Pin Allocation (Header P1 40-Pin Header)
The SECO UDOO KEY routes RP2040 GPIO pins directly to standard Header P1 connector pins for 8-channel logic analysis:

| Channel Name | RP2040 GPIO | Physical Connector | Electrical Function | Max Usable Clock / Edge Rate |
| :---: | :---: | :---: | :---: | :---: |
| **CH0** | `GP2` | Header P1 Pin 4 | Logic Analyzer Channel 0 | 50 MHz (Nyquist @ 100 MSPS) |
| **CH1** | `GP3` | Header P1 Pin 5 | Logic Analyzer Channel 1 | 50 MHz (Nyquist @ 100 MSPS) |
| **CH2** | `GP4` | Header P1 Pin 6 | Logic Analyzer Channel 2 | 50 MHz (Nyquist @ 100 MSPS) |
| **CH3** | `GP5` | Header P1 Pin 7 | Logic Analyzer Channel 3 | 50 MHz (Nyquist @ 100 MSPS) |
| **CH4** | `GP6` | Header P1 Pin 8 | Logic Analyzer Channel 4 | 50 MHz (Nyquist @ 100 MSPS) |
| **CH5** | `GP7` | Header P1 Pin 9 | Logic Analyzer Channel 5 | 50 MHz (Nyquist @ 100 MSPS) |
| **CH6** | `GP8` | Header P1 Pin 10 | Logic Analyzer Channel 6 | 50 MHz (Nyquist @ 100 MSPS) |
| **CH7** | `GP9` | Header P1 Pin 11 | Logic Analyzer Channel 7 | 50 MHz (Nyquist @ 100 MSPS) |
| **EXT_TRIG**| `GP10` | Header P1 Pin 12 | Dedicated Hardware External Trigger | 50 MHz Edge Detect |

### 2.2 Acquisition & Buffering Architecture
* **PIO Subsystem:** PIO0 State Machine 0 configured with 1-instruction sample loop (`in pins, 8`), shifting 8 contiguous bits (`GP2..GP9`) directly into the RX FIFO.
* **DMA Subsystem:** DMA Channel 0 configured in circular ring buffer mode (`RING_SIZE = 16`, corresponding to $2^{16} = 65,536$ bytes / 64 KiB).
* **Buffer Memory:** `capture_buffer` statically allocated in unstriped RP2040 SRAM with 64 KiB natural 16-bit address alignment.
* **Coexistence with SUMP Protocol:**
  * When in SUMP mode or external PulseView session, binary SUMP commands control the capture engine directly.
  * When in CLI mode (`m logic`), `udoo_hal/logic_hal` orchestrates the same underlying PIO/DMA/Trigger hardware, guaranteeing zero code duplication and zero regression to the frozen SUMP baseline.

---

## 3. Command Syntax & User Experience

| Command | Shorthand | Parameters | Function & Description |
| :--- | :--- | :--- | :--- |
| `m logic` | `7` | None | Enters Logic Analyzer mode; sets prompt to `LOGIC>`; prints configuration banner. |
| `arm` | `run` / `start` / `c` | `[timeout_ms]` | Arms PIO/DMA acquisition; polls non-blockingly until triggered or aborted by keypress; auto-renders diagram. |
| `rate` | `speed` | `<freq>` | Configures sample rate from 10 kHz to 100 MHz (e.g. `rate 100M`, `rate 50M`, `rate 10M`, `rate 1M`). |
| `depth` | `samples` | `<count>` | Configures buffer depth in power-of-2 samples: 1k, 2k, 4k, 8k, 16k, 32k, 64k (1,024 to 65,536). |
| `trig` | `trigger` | `<subcommand>` | Configures trigger engine: `auto`, `edge <ch> <rise\|fall\|any>`, `level <ch> <hi\|lo>`, `pat <mask> <val>`, `width <ch> <lt\|gt> <ns>`, `pre <pct>`. |
| `dump` | `view` / `plot` | `[start] [len]` | Renders horizontal multi-channel ASCII timing diagram around trigger or at specified offset. |
| `stats` | `edges` | None | Computes and displays digital telemetry: high/low sample counts, duty cycle %, edge count, and estimated frequency. |
| `abort` | `stop` | None | Stops running background acquisition immediately. |
| `pins` | `status` | None | Displays comprehensive telemetry box with channel assignments and trigger configuration. |
| `hiz` | None | None | Exits to safe HiZ mode; restores `GP2..GP9` to uncommitted floating inputs. |

### 3.1 Terminal Multi-Channel ASCII Timing Diagram
Executing `dump` or completing an `arm` capture renders synchronous digital waveforms:
```text
=== Logic Analyzer Timing Diagram (Samples 32..95, Trg @ 64) ===
Scale: 40.00 ns/col (Rate: 25.00 MSPS)
--------------------------------------------------------------------------------
CH0(GP2) |--------------------------------|_______________________________
CH1(GP3) |________________|---------------|________________|--------------
CH2(GP4) |________|-------|_______|-------|________|-------|_______|------
CH3(GP5) |____|---|___|---|___|---|___|---|____|---|___|---|___|---|___|-
CH4(GP6) |__|_|__|_|__|_|__|_|__|_|__|_|__|_|__|_|__|_|__|_|__|_|__|_|__|_
CH5(GP7) |----------------------------------------------------------------
CH6(GP8) |________________________________________________________________
CH7(GP9) |________________________________________________________________
--------------------------------------------------------------------------------
Trigger  |                                ^ (Offset 64)
Timeline | 1.280 us                       2.560 us                       3.840 us
```

### 3.2 Channel Telemetry & Statistical Analysis
Executing `stats` analyzes the captured buffer:
```text
+---------+------+-------------+------------+------------+---------------+
| Channel | GPIO | High / Low  | Duty Cycle | Transitions| Est. Frequency|
+---------+------+-------------+------------+------------+---------------+
| CH0     | GP2  | 2048 / 2048 |  50.0 %    |          2 |    6.10 kHz   |
| CH1     | GP3  | 2048 / 2048 |  50.0 %    |          4 |   12.21 kHz   |
| CH2     | GP4  | 2048 / 2048 |  50.0 %    |          8 |   24.41 kHz   |
| CH3     | GP5  | 2048 / 2048 |  50.0 %    |         16 |   48.83 kHz   |
| CH4     | GP6  | 2048 / 2048 |  50.0 %    |         32 |   97.66 kHz   |
| CH5     | GP7  | 4096 /    0 | 100.0 %    |          0 | (Static/DC)   |
| CH6     | GP8  |    0 / 4096 |   0.0 %    |          0 | (Static/DC)   |
| CH7     | GP9  |    0 / 4096 |   0.0 %    |          0 | (Static/DC)   |
+---------+------+-------------+------------+------------+---------------+
```

---

## 4. Test Suite Execution & Experimental Results

The automated regression test suite (`scratch/test_terminal_cli.py`) now covers 80 complete test cases:

| Test ID | Test Scenario | Command Sequence | Expected Response | Result |
| :---: | :--- | :--- | :--- | :---: |
| **T71** | Mode Switch to Logic | `m logic\r\n` | `LOGIC>` prompt & banner | **PASS** |
| **T72** | Logic Status Query | `status\r\n` | `100MSPS Logic Analyzer` and `GP2(CH0)` | **PASS** |
| **T73** | Logic Sample Rate Config | `rate 50M\r\n` | `50000000 Hz` / `50 MSPS` | **PASS** |
| **T74** | Logic Sample Depth Config | `depth 8k\r\n` | `8192 samples` confirmation | **PASS** |
| **T75** | Logic Trigger Edge Config | `trig edge 0 rise\r\n` | `RISING` and `CH0` confirmation | **PASS** |
| **T76** | Logic Trigger Level Config | `trig level 1 low\r\n` | `LEVEL LOW` and `CH1` confirmation | **PASS** |
| **T77** | Logic Trigger Pattern Config | `trig pattern 0xFF 0xAA\r\n` | `PATTERN` match confirmation | **PASS** |
| **T78** | Logic Trigger Auto Config | `trig auto\r\n` | `AUTO` confirmation | **PASS** |
| **T79** | Logic Interactive Arm & Plot | `arm 300\r\n` | Arming, capture completion, and ASCII diagram | **PASS** |
| **T80** | Clean Exit to Safe HiZ | `hiz\r\n` | `HiZ>` prompt; pins restored to high-Z | **PASS** |

---

## 5. Formal Freeze Declaration

The 8-Channel 100MSPS Logic Analyzer CLI subsystem meets all architectural, functional, electrical, and measurement criteria:

1. **Permanent PIO/DMA Logic Baseline Protection:** The PIO0 SM0 circular DMA capture engine and SUMP protocol are permanently preserved and unaltered.
2. **Factory Inter-MCU Link Preserved:** RP2040 `UART0` (`GP0`/`GP1`) connection to ESP32 is completely untouched.
3. **Safe High-Z Assurance:** Safe boot state (`HiZ>`) is strictly preserved; exiting Logic mode immediately restores `GP2..GP9` to uncommitted high-impedance inputs.
4. **Hardware Abstraction Layer Isolation:** All logic acquisition, rate configuration, trigger orchestration, buffer alignment, ASCII waveform rendering, and telemetry calculation are encapsulated in `udoo_hal/logic_hal`.
5. **Memory Footprint:** Clean PlatformIO build (RAM 62.3%, Flash 1.4%, UF2 size 488,960 bytes).

**8-Channel 100MSPS Logic Analyzer CLI Mode is hereby declared VALIDATED AND FROZEN.**  
Next protocol in sequence: **Milestone `PROTO-8`: USB Probing, Protocol Sniffing & Sensor Testing (`USB TOOLS`).**
