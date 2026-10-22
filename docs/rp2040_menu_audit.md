# UDOO KEY RP2040 Menu & ST7735 Front Panel Audit

**Document ID:** DOC-RP2040-MENU-AUDIT-20261006  
**Authority:** DOC-STATUS-MASTER-20261005-REV2 (`docs/IMPLEMENTATION_STATUS.md`)  
**Scope:** RP2040 Local UI, ST7735 Color TFT Display, 5-Way Button Navigation Matrix, CLI Simulation Bridge  

---

## 1. Executive Summary

This engineering audit examines the RP2040 on-device user interface, the ST7735 SPI0 color TFT display driver, the 5-way tactile button navigation engine, and their integration with the underlying firmware subsystems.

The local display and tactile button matrix serve as the authoritative standalone front panel of the UDOO KEY diagnostic instrument. All UI rendering is executed on the RP2040 while maintaining complete decoupling from the timing-critical PIO0/DMA circular capture engine.

---

## 2. Firmware Architecture & File Locations

| Component | Source File | Responsibilities |
| :--- | :--- | :--- |
| **UI State Machine & Renderers** | `rp2040_logic_analyzer/src/display_ui.cpp`<br>`rp2040_logic_analyzer/src/display_ui.h` | Screen state machine (`UIScreen`), rasterization routines, waveform display, color palettes, toast notifications, refresh timing. |
| **Button Navigation & Debounce** | `rp2040_logic_analyzer/src/button_nav.cpp`<br>`rp2040_logic_analyzer/src/button_nav.h` | Active-LOW 5-way button debouncing (20 ms), hold-to-repeat (500 ms delay, 150 ms repeat), circular event queue, CLI injection. |
| **CLI Display & Button Bridge** | `rp2040_logic_analyzer/src/bus_pirate_core.cpp`<br>`rp2040_logic_analyzer/src/terminal/terminal.cpp` | `disp` command suite (`disp test`, `disp tab`, `disp speed`, `disp inv`, `disp vcom`, `disp rot`), `btn` injection commands (`btn up/down/left/right/select`). |
| **Display SPI Bus Arbiter** | `rp2040_logic_analyzer/src/display_ui.cpp`<br>`rp2040_logic_analyzer/src/udoo_hal/spi_hal.cpp` | Dynamic hardware `SPI0` peripheral arbiter between onboard display (`GP18..GP20`) and external probe bus (`GP2..GP5`). |
| **Core Main Dispatcher** | `rp2040_logic_analyzer/src/main.cpp` | Non-blocking execution of `ui_task(start_req, stop_req)`, trigger handoff, and capture completion notification (`ui_notify_capture_complete()`). |

---

## 3. Physical Hardware & Pin Assignments

### 3.1 ST7735 Color TFT Display (SPI0)
The display is wired to dedicated RP2040 pins that never conflict with the 8-channel logic probe bus (`GP2..GP9`) or inter-MCU UART (`GP0/GP1`).

| Signal | RP2040 Pin | Function | UDOO KEY Header | Electrical Notes |
| :--- | :---: | :--- | :--- | :--- |
| **TFT_SCK** | **GP18** | SPI0 Clock | Header P1 Pin 17 | 4mA drive strength, default 8 MHz (configurable 500 kHz to 32 MHz) |
| **TFT_MOSI** | **GP19** | SPI0 MOSI (SDA) | Header P1 Pin 16 | 4mA drive strength |
| **TFT_MISO** | **GP20** | SPI0 MISO | Header P1 Pin 15 | Optional; shared with continuity buzzer during non-SPI operations |
| **TFT_CS** | **GP13** | SIO Digital Out | Header P2 Pin 17 | Active-LOW chip select; auto-deselected when probe SPI active |
| **TFT_DC** | **GP12** | SIO Digital Out | Header P2 Pin 16 | Data (HIGH) / Command (LOW) selector |
| **TFT_RST** | **GP11** | SIO Digital Out | Header P2 Pin 15 | Active-LOW hardware reset pulse (25 ms low, 120 ms recovery) |
| **VCC** | — | Power | Header P2 Pin 16 / 20 | 3.3V (or 5V if module has onboard 662K 3.3V LDO regulator) |
| **GND** | — | Ground | Header P1 Pin 18 / P2 Pin 3 | System ground plane |

### 3.2 Five-Way Tactile Button Matrix
Configured with internal pull-ups (`INPUT_PULLUP`), active LOW:

| Button | RP2040 Pin | Raspberry Pi Pico Pin | UDOO KEY Header | Function |
| :--- | :---: | :---: | :--- | :--- |
| **BTN_UP** | **GP15** | Pin 20 | Header P1 Pin 20 | Previous menu item / Increase parameter / Zoom in |
| **BTN_DOWN** | **GP16** | Pin 21 | Header P2 Pin 1 | Next menu item / Decrease parameter / Zoom out |
| **BTN_LEFT** | **GP17** | Pin 22 | Header P2 Pin 2 | Back / Exit submenu / Abort / Return to Main Menu |
| **BTN_RIGHT** | **GP21** | Pin 27 | Header P2 Pin 7 | Enter submenu / Advance / Cycle secondary option |
| **BTN_SELECT**| **GP22** | Pin 29 | Header P2 Pin 9 | Execute / Arm Capture / Toggle / Confirm |

---

## 4. Existing Menu Hierarchy & Screen Transitions

```text
MAIN MENU (UIScreen::MAIN_MENU)
 ├── 1. Logic Analyzer (UIScreen::LOGIC_ANALYZER)
 │    ├── Live 8-channel waveform rendering (CH0..CH7)
 │    ├── Status banner (READY / RUNNING / SIGROK)
 │    ├── Sample rate selection (100k, 1M, 5M, 10M, 25M, 50M, 100M)
 │    ├── Trigger position line ('T' marker, 0%..90%)
 │    └── Controls: SEL (Run/Stop), U/D (Rate), R (Trig Pos), L (Back)
 │
 ├── 2. Continuity Test (UIScreen::CONTINUITY)
 │    ├── Fast ADC0 (GP26) resistance measurement
 │    ├── Acoustic beeper driver on GP20
 │    ├── Connected / Open circuit banner
 │    └── Controls: U/D (Adjust Threshold 10/25/50/100Ω), SEL (Mute Buzzer), L (Back)
 │
 ├── 3. Measurements (UIScreen::MEASUREMENTS)
 │    ├── 3-Channel Calibrated ADC Voltmeter (ADC0/GP26, ADC1/GP27, ADC2/GP28)
 │    ├── VSYS voltage & RP2040 internal temperature
 │    ├── Resistance bridge readout (CH0)
 │    ├── Digital Frequency Measurement (PWM / Edge Counter)
 │    ├── Digital Period, Pulse Width, & Duty Cycle
 │    └── Controls: SEL (Refresh / Cycle Channel), L (Back)
 │
 ├── 4. Analog Scope (UIScreen::ANALOG_SCOPE)
 │    ├── Real-time DMA waveform rasterizer on GP26 (50 kSPS, 128 samples)
 │    ├── Graticule grid (28x16 px) & trace rendering
 │    ├── Waveform statistics: Vpp, Vavg, estimated frequency, trigger lock
 │    └── Controls: Live auto-refresh, SEL (Hold/Refresh), L (Back)
 │
 ├── 5. Protocol Tools (UIScreen::PROTOCOL_TOOLS)
 │    ├── I2C Bus Scan (7-bit address discovery 0x08..0x77)
 │    ├── SPI Flash Info (JEDEC ID 0x9F & SFDP metadata)
 │    ├── UART Bridge & Terminal
 │    ├── 1-Wire Search (Dallas 64-bit ROM enumeration)
 │    ├── Digital I/O (DIO) State Monitor
 │    ├── USB Physical Bus Prober (D+/D- state, speed detect)
 │    ├── ARM SWD Probe [HW TEST PENDING]
 │    ├── Safe Hi-Z Mode (all probe pins tri-stated)
 │    └── Controls: U/D (Navigate), SEL/R (Execute Tool), L (Back)
 │
 ├── 6. Wireless & ESP32 (UIScreen::WIRELESS_ESP32)
 │    ├── Dual-MCU UART link health (GP0/GP1 @ 1 Mbps, TX/RX counters)
 │    ├── SoftAP Status (SSID: UDOO-BIT-PIRATE-SCOPE, IP: 192.168.4.1)
 │    ├── BLE Scanner Status & Ingestion Count
 │    ├── PCAP / Wi-Fi Promiscuous Sniffer Telemetry
 │    └── Controls: SEL (Ping ESP32), U/D (Toggle BLE Scan), L (Back)
 │
 ├── 7. Generator (UIScreen::GENERATOR)
 │    ├── PWM Output: 1 kHz (50% duty on GP2)
 │    ├── PWM Output: 10 kHz (50% duty on GP2)
 │    ├── PWM Output: 1 MHz (50% duty on GP2)
 │    ├── PWM Output: 10 MHz (50% duty on GP2)
 │    ├── Hardware Glitch Injector
 │    ├── Stop Generator (Safe Hi-Z)
 │    └── Controls: U/D (Navigate), SEL/R (Apply Signal), L (Back)
 │
 └── 8. Settings & Diagnostics (UIScreen::SETTINGS)
      ├── Trigger Mode Selector (Auto, Edge Rise, Edge Fall, Level Qual, Pattern)
      ├── Analog Prescaler (GP14: x1, x2, x4 division)
      ├── Front-End Overvoltage Protection (Toggle Relay/FET)
      ├── Display Orientation (0=Port, 1=Land, 2=Inv Port, 3=Inv Land)
      ├── Hardware Diagnostics (UIScreen::DIAGNOSTICS)
      ├── Display Test Pattern (Red, Green, Blue, White bar pattern)
      ├── Hardware Self-Test Suite (17 verified hardware checks)
      ├── System Information (UIScreen::SYS_INFO)
      └── Controls: U/D (Navigate), SEL/R (Change / Enter), L (Back)
```

---

## 5. Comprehensive Screen Audit & Subsystem Mapping

| Screen / View | Existing in Code | Fully Functional | Backend API / Subsystem | Verification Status (`DOC-STATUS-MASTER-20261005-REV2`) | Operational Notes |
| :--- | :---: | :---: | :--- | :---: | :--- |
| **MAIN_MENU** | YES | YES | Internal state machine (`UIScreen`) | PRODUCTION READY | Windowed 5-row scrolling with up/down arrows. |
| **LOGIC_ANALYZER** | YES | YES | `acquisition_api.h`<br>`pio_capture.h`<br>`trigger_engine.h` | **HARDWARE VERIFIED** | 8 channels (GP2..GP9), 64 KiB circular DMA, deterministic trigger position cursor. |
| **CONTINUITY** | YES | YES | `adc_subsystem.h`<br>`hardware_config.h` | **HARDWARE VERIFIED** | ADC0 voltage divider, GP20 buzzer driver with configurable threshold (10..100Ω) & hysteresis. |
| **MEASUREMENTS** | YES | YES | `adc_subsystem.h`<br>`freq_meter.h`<br>`pio_edge_analyzer.h` | **HARDWARE VERIFIED** | 3-ch calibrated ADC (GP26..GP28), hardware frequency meter, period, pulse width, and duty cycle. |
| **ANALOG_SCOPE** | YES | YES | `adc_subsystem.h` (DMA ADC mode) | **HARDWARE VERIFIED** | 50 kSPS 128-sample real-time trace, Vpp, Vavg, trigger lock indicator. |
| **PROTOCOL: I2C Scan** | YES | YES | `bus_pirate_core.h`<br>`udoo_hal/i2c_hal.h` | **HARDWARE VERIFIED** | Probes 0x08..0x77 on GP4(SDA)/GP5(SCL); results printed to stream & toast. |
| **PROTOCOL: SPI Flash** | YES | YES | `bus_pirate_core.h`<br>`udoo_hal/spi_hal.h` | **HARDWARE VERIFIED** | Reads JEDEC ID (0x9F); dynamic SPI0 bus switching between display and probe bus. |
| **PROTOCOL: UART Bridge** | YES | YES | `bus_pirate_core.h`<br>`udoo_hal/uart_hal.h` | **HARDWARE VERIFIED** | Transparent hardware bridge on GP4(TX)/GP5(RX). |
| **PROTOCOL: 1-Wire** | YES | YES | `bus_pirate_core.h`<br>`udoo_hal/onewire_hal.h` | **HARDWARE VERIFIED** | DS18B20 / Dallas ROM search on GP6. |
| **PROTOCOL: Digital I/O** | YES | YES | `bus_pirate_core.h`<br>`terminal/terminal.cpp` | **HARDWARE VERIFIED** | 8-bit probe bus direction/level manipulation. |
| **PROTOCOL: USB Bus Prober**| YES | YES | `bus_pirate_core.h`<br>`terminal/terminal.cpp` | **HARDWARE VERIFIED** | Physical line states (D+=GP2, D-=GP3, SE0/SE1, J/K, LS/FS speed detection). |
| **PROTOCOL: ARM SWD** | YES | PARTIAL | `bus_pirate_core.h`<br>`swd_host.cpp` | **HW TEST PENDING** | Marked `[UNVERIF]` on screen. SWD protocol implemented but physical target unverified. |
| **PROTOCOL: Safe Hi-Z** | YES | YES | `bus_pirate_core.h` | **HARDWARE VERIFIED** | Tri-states all probe pins; releases shared buses. |
| **WIRELESS & ESP32** | YES | YES | `interchip_link.h`<br>`instrument_protocol.h` | **HARDWARE VERIFIED** | 1 Mbps inter-MCU UART link health, SoftAP SSID/IP, BLE scanner count, PCAP sniffer status. |
| **GENERATOR** | YES | YES | `bus_pirate_core.h`<br>`hardware/pwm.h` | **HARDWARE VERIFIED** | Hardware PWM on GP2 (1 kHz to 10 MHz), hardware glitch injector pulse. |
| **SETTINGS** | YES | YES | `trigger_engine.h`<br>`frontend_control.h` | **HARDWARE VERIFIED** | Cycles trigger engine modes, GP14 prescaler (x1/x2/x4), front-end protection FET. |
| **DIAGNOSTICS** | YES | YES | `hardware_config.h`<br>`interchip_link.h` | **HARDWARE VERIFIED** | Live 8-ch probe bit states, button states, ADC rails, clock, DMA health, inter-MCU ping. |
| **DISPLAY TEST** | YES | YES | `display_ui.cpp` | **HARDWARE VERIFIED** | Direct color bar draw (Red, Green, Blue, White), tab profiles, SPI clock diagnostic. |
| **SYS_INFO** | YES | YES | `hardware_config.h` | **HARDWARE VERIFIED** | Displays RP2040 clocks (133 MHz PLL), 264 KB SRAM, 16 MB QSPI Flash, build date. |

---

## 6. Real-Time Safety & Timing Decoupling

A critical architectural requirement is that **UI updates must never interrupt, slow down, or corrupt ongoing logic captures**:

1. **PIO0 & DMA Independence**:
   - High-speed digital capture runs entirely in hardware via PIO0 state machines and DMA channels.
   - Circular buffer capture operates autonomously without CPU intervention.
2. **Dynamic UI Refresh Throttling**:
   - When `acquisition_is_running()` is true, heavy full-frame display redraws are suppressed.
   - The UI displays an animated/static `RUNNING` status banner without re-rendering the full 8-channel waveform area, preventing SPI bus bus-contention.
3. **Capture Complete Callback**:
   - Once DMA transfer finishes and `acquisition_is_ready()` is asserted, `ui_notify_capture_complete()` triggers an immediate single-pass buffer refresh.
4. **Shared SPI0 Bus Arbiter**:
   - The ST7735 display is wired to `GP18..GP20` on `SPI0`. When the user enters Bus Pirate SPI Probe mode (`GP2..GP5`), `ModeMgr.isProbeSpiActive()` pauses the `ui_task()`, deselects the display (`TFT_CS = 1`), and cleanly hands off the hardware SPI0 peripheral to the external probe bus. Upon exiting back to Hi-Z, `SPI0` is immediately restored to the display pins.

---

## 7. Display Driver Diagnostics & Hardware Variations

### 7.1 Production Hardware Target (1.44" 128x128)
- **1.44-inch ST7735S (Square)**: 128x128 resolution, uses `INITR_144GREENTAB`. Has a 2-pixel column/row offset in hardware.
- The UI geometry and framebuffer routines are strictly locked to 128x128 pixels (`DISPLAY_WIDTH = 128`, `DISPLAY_HEIGHT = 128`).
- Diagnostic switching is accessible via CLI if testing alternate glass profiles:
  ```text
  disp tab <144green|black|red>
  ```
  Switching tab profiles re-executes `tft_main.initR()` with the proper hardware offsets.

### 7.2 Washed-Out Contrast / Low Brightness Root Causes & Fixes
1. **Module 3.3V LDO Voltage Drop**:
   - Many hobbyist ST7735 breakout boards contain an onboard 3.3V low-dropout regulator (e.g. 662K).
   - If connected to the RP2040's 3.3V rail (`Header P2 Pin 16`), the regulator drops the display VDD down to ~2.8V, starving the internal ST7735 charge pump and washing out colors.
   - **Remedy**: Connect display `VCC` to the 5V rail (`VBUS`, Header P2 Pin 20), allowing the onboard LDO to output a steady 3.3V.
2. **Color Inversion (`INVON` vs `INVOFF`)**:
   - Some ST7735 panels require display inversion active, while others do not.
   - The firmware provides the `disp inv` command and `ui_toggle_invert()` to instantly flip between normal and inverted modes.
3. **VCOM Contrast Voltage**:
   - The ST7735 VCOM voltage register (`ST7735_VMCTR1`) directly controls liquid crystal contrast.
   - Default is `0x0E`. Can be adjusted via CLI: `disp vcom 0x2C` (values range from `0x0E` to `0x3F`).
