# UDOO KEY — Bus Pirate Functionality Matrix & Capability Mapping

**Document ID:** DOC-BP-MATRIX-20261004  
**Date:** 2026-10-04  
**Classification:** Architectural Integration Matrix  
**Reference Codebases:**
1. `BusPirate5-firmware-main` (Official Bus Pirate 5 firmware for RP2040, C / Pico SDK)
2. `ESP32-Bit-Pirate-pioarduino` (ESP32 Bit Pirate implementation, C++ / Arduino / PlatformIO)
3. `BP rp2040 src` / `BP esp32 s3 src` (Extracted mode algorithms and dispatchers)

---

## 1. Classification Categories & Criteria

Every Bus Pirate capability is evaluated against the physical silicon, board traces, and firmware architecture of the **SECO UDOO KEY (RP2040 + ESP32)** and assigned one of the following decisions:

* **`KEEP EXISTING`**: The UDOO KEY project already contains a superior, specialized, or frozen implementation that must NOT be replaced (e.g. 100 MSPS PIO+DMA logic analyzer vs BP5 8-bit FALA; hardware-timed circular DMA).
* **`ADD`**: The capability is valuable and currently absent or unexposed; it will be introduced natively into `InstrumentProtocol` v1.0 and both MCU/desktop layers.
* **`ADAPT`**: The reference algorithm or syntax is valuable but must be adapted to UDOO KEY pin definitions, dual-MCU architecture, or transport framing.
* **`REPLACE IMPLEMENTATION ONLY`**: An internal subroutine is updated using proven reference logic without altering the public API or protocol.
* **`DEFER`**: Useful secondary feature deferred to subsequent project iterations (e.g. complex standalone macro script engine).
* **`UNSUPPORTED HARDWARE`**: Features strictly reliant on specialized Bus Pirate 5 physical circuitry not present on the SECO UDOO KEY board (e.g. programmable 1.2V–5.0V VREG DAC, pull-up switchable voltage rails).

---

## 2. Bus Pirate 5 & ESP32 Bit Pirate Functionality Matrix

| Bus Pirate Feature | BP5 Reference (`BP5`) | ESP32 Bit Pirate (`ESP32-BP`) | UDOO KEY Realization | Target MCU | Decision | Technical Rationale & Hardware Mapping |
| :--- | :--- | :--- | :--- | :---: | :---: | :--- |
| **Logic Analyzer (Acquisition)** | `src/binmode/fala.c` (Linear buffer) | Software polling | 8-Ch 100 MSPS PIO0 SM0 + DMA circular ring (64 KiB) | **RP2040** | **KEEP EXISTING** | UDOO KEY hardware-wrapped DMA circular buffer with 70/30 pre-trigger is far superior to BP5 linear capture. Must remain frozen. |
| **Trigger Engine** | Software edge check | N/A | Hardware PIO + SIO trigger (Edge, Level, Pattern, Ext Trig GP10) | **RP2040** | **KEEP EXISTING** | Deterministic sub-microsecond hardware trigger engine already exceeds reference software detection. |
| **Generic GPIO Control (DIO)** | `src/mode/dio.c` | `src/Services/PinService.cpp` | SIO GP2..GP9 pin control, directions, pulls, reads, writes, masks | **RP2040** | **ADD** | Low-risk, high-utility feature. Directly leverages `gpio_hal.cpp` and exposes via new protocol opcodes (`0x60..0x65`). |
| **SUMP / PulseView Protocol** | `src/binmode/sump.c` | N/A | Native CDC ACM SUMP protocol state machine | **RP2040** | **KEEP EXISTING** | Validated and fully compatible with Sigrok / PulseView. |
| **Voltmeter (ADC)** | `src/commands/adc.c` | `src/Controllers/AdcController.cpp` | 3-Ch 12-bit SAR ADC (GP26..28) with 16x oversampling & calibration | **RP2040** | **KEEP EXISTING** | Existing implementation is calibrated and filtered for UDOO KEY analog inputs. |
| **Programmable Power Supply (VREG)** | `src/pirate/hwvreg.c` (1.2V–5V DAC/op-amp) | N/A | Fixed +3.3V and +5V rails from Headers P1/P2 | N/A | **UNSUPPORTED HARDWARE** | UDOO KEY has fixed buck-boost SMPS (RT6150) outputs; lacks programmable voltage DAC / regulator hardware. |
| **Switchable Pull-Up Resistors** | `src/pirate/pullup.c` (VPU select mux) | Internal pulls | Internal 50k–80k pull-up/down resistors on RP2040 pins | **RP2040** | **ADAPT** | BP5 uses external switchable resistor arrays to arbitrary VPU; UDOO KEY must utilize RP2040 internal SIO pull-ups/pull-downs. |
| **I2C Bus Engine (Master/Scan)** | `src/mode/i2c.c` | `src/Shells/I2cShell.cpp` | Hardware I2C1 (`GP2` SDA, `GP3` SCL), 7-bit / 10-bit address scan | **RP2040** | **ADD** | Add framed binary protocol packets (`0x6C..0x6F`) and connect to Desktop `I2cScannerWidget`. |
| **SPI Bus Engine & Flash SFDP** | `src/mode/spi.c`, `src/commands/flash.c` | `src/Shells/SpiShell.cpp` | Hardware SPI0 probe multiplexer (`GP2..5`) + JEDEC SFDP parser | **RP2040** | **ADD** | Expose SPI master transactions and Flash ID/SFDP query via protocol packets (`0x70..0x73`) to Desktop `SpiFlashWidget`. |
| **1-Wire Bus Engine & Search** | `src/mode/1wire.c` | N/A | RP2040 SIO/PIO on `GP6` (DQ) with internal pull-up | **RP2040** | **ADD** | Expose 1-Wire Search ROM algorithm via protocol packets (`0x74..0x77`) to discover DS18B20 and iButtons. |
| **UART Bridge / Tool** | `src/mode/hwuart.c` | `src/Shells/UartShell.cpp` | Hardware UART1 on `GP4` (TX) / `GP5` (RX), 300 to 2,000,000 baud | **RP2040** | **ADD** | Expose secondary UART bridge via protocol packets (`0x68..0x6B`) to Desktop `UartBridgeWidget`. |
| **JTAG & SWD Interrogator** | `src/commands/swd.c`, `src/mode/jtag.c` | N/A | Native C++20 `JtagChain`, `JtagClient`, `SwdClient` + RP2040 debug engine | **RP2040 & Desktop** | **KEEP EXISTING** | Recently completed IEEE 1149.1 chain scanner and DPIDR probe is fully featured and test-verified. |
| **Frequency Counter & Edge Timer** | `src/platform/freq.c` | `src/Services/` | Dual PIO state machines (`pio_edge_timer.pio`) + PWM counter | **RP2040** | **KEEP EXISTING** | Custom sub-nanosecond edge timer already outperforms BP5 standard timer. |
| **Hardware Prescaler Control** | N/A | N/A | 74HC74 hardware divider on `GP14` | **RP2040** | **KEEP EXISTING** | Unique to UDOO KEY front-end hardware. |
| **Interactive Terminal Syntax Parser** | `src/pirate.c` (`[ 0x50 r:4 ]`) | `src/Shells/TerminalShell.cpp` | Bus Pirate command syntax interpreter in CLI & Desktop Terminal | **RP2040 & Desktop** | **ADAPT** | Adapt the standardized square-bracket bus transaction syntax (`[ 0x90 0x00 0x00 0x00 r:4 ]`) to execute on UDOO KEY buses. |
| **WebSockets & HTML5 Scope UI** | N/A | `webui/index.h`, `webui/scripts.h`, `src/Servers/WebSocketServer.cpp` | ESP32 AsyncWebServer streaming waveforms to web browser/tablet | **ESP32** | **ADAPT** | Adapt Bit Pirate WebSockets streaming to receive capture buffers from RP2040 via `InstrumentProtocol` v1.0. |
| **Local Graphical UI & Display** | `src/displays.c` (SPI LCD) | `src/Views/` | ST7735 1.44" 128x128 SPI TFT on SPI0 (`GP18..20`) + 5 buttons | **RP2040** | **KEEP EXISTING** | Native display and button UI is optimized for the local screen. |
| **Infrared Transmitter & Receiver**| `src/mode/ir.c` | `src/Services/InfraredService.cpp` | ESP32 RMT peripheral on `IO25` (TX) / `IO26` (RX) | **ESP32** | **ADAPT** | Integrates into protocol range `0x30..0x3A` via ESP32 coprocessor. |
| **nRF24L01 2.4 GHz Module** | N/A | `src/Services/Rf24Service.cpp` | ESP32 UEXT connector P3 SPI (`IO12/14/35/15`) | **ESP32** | **ADAPT** | Integrates into protocol range `0x40..0x50` via ESP32 coprocessor. |
| **Self-Test Diagnostic Suite** | `src/commands/selftest.c` | `src/Controllers/TestController.cpp` | Dual-MCU hardware loopback and pin continuity validation | **RP2040 & ESP32** | **KEEP EXISTING** | Existing `PKT_TYPE_SELF_TEST_REQ` (`0x20`) handles comprehensive hardware self-tests. |
| **Scripting Macro Engine** | `src/pirate/macro.c` | N/A | Extended macro playback | **Desktop App** | **DEFER** | Best implemented on host desktop application rather than consuming precious microcontroller SRAM. |

---

## 3. Detailed Hardware Variance: Bus Pirate 5 vs. SECO UDOO KEY

Understanding the physical discrepancies between the Bus Pirate 5 PCB and the SECO UDOO KEY PCB prevents impossible hardware requests:

```text
Bus Pirate 5 Hardware                  SECO UDOO KEY Hardware
─────────────────────                  ──────────────────────
PIC32 / RP2040 single MCU              RP2040 (Primary) + ESP32-WROVER-E (Secondary)
Dedicated 10-pin Shrouded IO           Dual Headers P1 (20-pin) & P2 (34-pin) + P3 (UEXT)
Programmable VREG (1.2V - 5.0V DAC)    Fixed +3.3V and +5.0V Power Rails only
Analog Muxed Pull-up Voltage Rail      RP2040 Internal SIO Pull-ups / Pull-downs only
Current Sense ADC on VREG              3 External ADC Channels (GP26, GP27, GP28) + VSYS Sense
Single SPI LCD (ST7789 320x240)        ST7735 128x128 SPI TFT on RP2040 SPI0
No Wireless / No Coprocessor           ESP32 with Wi-Fi 802.11 b/g/n & Bluetooth 4.2 / BLE
```

### Critical Takeaway for Integration:
* **Power & Pull-up Commands:** Commands such as `W` (enable power supply) or `p` (enable pull-ups to VPU) in Bus Pirate syntax must map gracefully to UDOO KEY capabilities:
  * Power supply enable reports fixed +3.3V / +5V rails.
  * Pull-up commands configure RP2040 internal pull-up resistors (`gpio_pull_up()`) rather than attempting to switch external voltage reference multiplexers.
