# UDOO KEY RP2040 Front Panel Menu Capability Matrix

**Document ID:** DOC-RP2040-MENU-CAPABILITY-MATRIX-20261006-REV2  
**Authority:** DOC-STATUS-MASTER-20261006-REV3 (`docs/IMPLEMENTATION_STATUS.md`)  
**Standard:** Single Source of Truth for Subsystem Hardware Verification & Menu Integration  

---

## 1. Classification Methodology (Strict 6-Level Hierarchy)

1. **Level 0 — NOT IMPLEMENTED**: No usable implementation exists.
2. **Level 1 — IMPLEMENTED**: Code exists and compiles.
3. **Level 2 — SOFTWARE TESTED**: Passed software unit tests, parser checks, or internal register assertions.
4. **Level 3 — ELECTRICAL TESTED**: Interface tested with electrical instruments (DMM, oscilloscope, calibrated source).
5. **Level 4 — HARDWARE VERIFIED**: Full physical subsystem demonstrated on actual UDOO KEY hardware without blocking anomalies.
6. **Level 5 — PRODUCTION READY**: Verified on hardware, documented limits, conflicts resolved, regression tested.

---

## 2. Complete Menu Subsystem Mapping Matrix

| Menu View / Item | Authoritative Subsystem Name | Verification Level | Production Ready? | Backend API / Function Call | Screen Display Representation | Hardware Evidence & Notes |
| :--- | :--- | :---: | :---: | :--- | :--- | :--- |
| **RP2040 Boot & Core** | RP2040 Boot & SIO | **LEVEL 5 (PROD READY)** | **YES** | SIO / Pico SDK boot | Diagnostic screen / CDC output | Boots cleanly @ 133 MHz; USB CDC active; terminal interactive. |
| **ST7735 Display** | ST7735 Color Display | **LEVEL 5 (PROD READY)** | **YES** | `Adafruit_ST7735`<br>`ui_run_display_test()` | 128x128 1.44" Color bars / Menus | **HARDWARE VERIFIED:** User bench verified with INITR_144GREENTAB. High contrast, sharp text, viewing angle wash-out resolved. |
| **Tactile Buttons** | Tactile Buttons (5-way) | **LEVEL 5 (PROD READY)** | **YES** | `button_nav_poll()`<br>`ui_get_button()` | Live `[U] [D] [L] [R] [SEL]` in Diag | **HARDWARE VERIFIED:** User bench verified (GP15 UP, GP16 DOWN, GP17 LEFT, GP21 RIGHT, GP22 SELECT). |
| **Logic Analyzer** | 8-Channel Digital Capture | **LEVEL 2 (SW TESTED)** | **NO** | `acquisition_arm()`<br>`pio_capture_get_aligned_buffer()` | Waveform traces CH0..CH7, Rate, Pos | 64 KiB circular DMA ring verified in RAM. External probe validation pending. |
| **Trigger Engine** | Trigger Engine | **LEVEL 2 (SW TESTED)** | **NO** | `trigger_engine_get_config()`<br>`trigger_engine_configure()` | Trigger line 'T' marker, Trig mode | Software trigger & INOVER internal injection verified. External GP10 wire trigger pending. |
| **Frequency Meter** | Digital Measurements | **LEVEL 2 (SW TESTED)** | **NO** | `freq_measure_pin(pin)`<br>`bp_measure_frequency(pin)` | `FREQ: 10.000 MHz` | Software PWM count verified internally. Calibrated frequency generator pending. |
| **Period Meter** | Digital Measurements | **LEVEL 2 (SW TESTED)** | **NO** | `freq_measure_pin(pin).period_s`<br>`pio_edge_measure()` | `PER : 100.0 ns` | Reciprocal calculation from frequency counter. Calibrated timing pending. |
| **Pulse Width Meter** | Digital Measurements | **LEVEL 2 (SW TESTED)** | **NO** | `pio_edge_analyzer` (`avg_high_ns`) | `PW  : 50.0 ns` | PIO interval timer in software. Physical pulse measurement pending. |
| **Duty Cycle Meter** | Digital Measurements | **LEVEL 2 (SW TESTED)** | **NO** | `freq_measure_pin(pin).duty_percent` | `DUTY: 50.0 %` | Microsecond accumulation in software. Calibrated PWM source pending. |
| **ADC Voltmeter** | ADC Voltmeter (3-ch) | **LEVEL 2 (SW TESTED)** | **NO** | `adc_read_calibrated_voltage(ch)` | `ADC0: 3.298 V`<br>`ADC1: 1.650 V`<br>`ADC2: 0.000 V` | 12-bit SAR ADC with 16x oversampling. Precision reference voltages (0V, 1V, 2V, 3V) pending. |
| **Continuity Tester** | ADC Voltmeter + Buzzer | **LEVEL 2 (SW TESTED)** | **NO** | `adc_continuity_update(ch)`<br>`buzzer_tone(freq)` | Large banner `CONNECTED` / `OPEN CIRCT` | Threshold logic verified in software. Probe lead tare & buzzer calibration pending. |
| **Analog Scope** | ADC DMA Waveform Engine | **LEVEL 2 (SW TESTED)** | **NO** | `adc_waveform_start()`<br>`adc_waveform_compute_stats()` | 128-sample live trace, Vpp, Vavg | 50 kSPS ADC ring buffer in RAM. Calibrated external waveform source pending. |
| **Hardware Prescaler**| Prescaler Control (GP14) | **LEVEL 2 (SW TESTED)** | **NO** | `frontend_get_prescaler()`<br>`frontend_set_prescaler(div)` | `PRE: x1 / x2 / x4` | GP14 output pin toggles in software. Physical 74HC74 external division measurement pending. |
| **Front-End Protect** | Front-End Protection Relay | **LEVEL 0 (NOT IMPLEMENTED)** | **NO** | `frontend_get_protection()` | `PROT: ON / OFF` | **MOCK ONLY:** `PIN_FRONTEND_PROT_EN = -1`. No physical GPIO or FET connected. |
| **I2C Bus Scan** | Bus Pirate Modes (I2C) | **LEVEL 2 (SW TESTED)** | **NO** | `bp_execute_command("scan")`<br>`i2c_hal_scan()` | Toast: `I2C Bus Scan: Scanning...` | I2C master logic passes unit tests. Physical I2C target slave pending. |
| **SPI Flash Info** | Bus Pirate Modes (SPI) | **LEVEL 2 (SW TESTED)** | **NO** | `bp_execute_command("flash")`<br>`spi_hal_read_jedec()` | Toast: `SPI Flash: Reading JEDEC` | SPI master logic passes unit tests. Physical SPI flash chip on probe pins pending. |
| **UART Bridge** | Bus Pirate Modes (UART) | **LEVEL 2 (SW TESTED)** | **NO** | `bp_execute_command("mode uart")` | Toast: `UART Mode Active` | Serial2 driver passes unit tests. Physical loopback / target terminal pending. |
| **1-Wire Search** | Bus Pirate Modes (1-Wire) | **LEVEL 2 (SW TESTED)** | **NO** | `bp_execute_command("1wire")`<br>`onewire_search()` | Toast: `1-Wire Searching...` | Timing pulses pass unit tests. Physical DS18B20 slave discovery pending. |
| **Digital I/O (DIO)** | Bus Pirate Modes (DIO) | **LEVEL 2 (SW TESTED)** | **NO** | `bp_execute_command("mode dio")` | `DIO: Live Pin Matrix` | SIO pin direction logic tested in software. Calibrated external load pending. |
| **USB Bus Prober** | USB Physical Bus Probing | **LEVEL 2 (SW TESTED)** | **NO** | `cmd_usb_probe()`<br>`usb_hal_get_state()` | `USB: D+ 3.3V D- 0.0V (FS J)` | Line state reading tested. Calibrated pull-up / pull-down target bus pending. |
| **Wireless & ESP32** | ESP32 Wi-Fi Telemetry | **LEVEL 5 (PROD READY)** | **YES** | `interchip_get_tx_count()`<br>`interchip_get_rx_count()` | `LINK: 1M OK`<br>`AP: UDOO-BIT-PIRATE-SCOPE` | **HARDWARE VERIFIED:** User bench verified. SoftAP connects stably; Web App active on 192.168.4.1. |
| **BLE Scanner Telemetry** | ESP32 BLE Scanner | **LEVEL 2 (SW TESTED)** | **NO** | `InstrumentRadioEventPayload` | `BLE: SCANNING (X devs)` | Telemetry framing passes unit tests. Physical BLE beacon reception pending. |
| **PCAP Telemetry** | PCAP / Wi-Fi Sniffer | **LEVEL 2 (SW TESTED)** | **NO** | `InstrumentWifiSniffStatusPayload` | `PCAP: CH1 ACTIVE` | Framing passes unit tests. Physical 802.11 sniffer reception pending. |
| **PWM Generator** | Signal Generator | **LEVEL 2 (SW TESTED)** | **NO** | `bp_execute_command("mode pwm <freq>")` | Toast: `PWM Output: X kHz` | PWM slice 1A configured in software. Oscilloscope verification pending. |
| **Glitch Injector** | Glitch Engine | **LEVEL 2 (SW TESTED)** | **NO** | `bp_execute_command("mode glitch")` | Toast: `Glitcher: Pulse Injected` | Pulse generation configured in software. Oscilloscope pulse width measurement pending. |
| **Self-Test Suite** | Self-Test Diagnostic | **LEVEL 2 (SW TESTED)** | **NO** | `acquisition_run_self_test(Serial)` | Toast: `Self Test: 17/17 PASS` | **AUDITED:** Internal registers & mocks. Does not prove external electrical health. |
| **SUMP / PulseView**| SUMP Protocol Engine | **LEVEL 2 (SW TESTED)** | **NO** | `sump_is_active()` | Header shows `SIGROK` | SumpClient unit test passes. End-to-end USB capture in PulseView pending. |
| **ARM SWD Probe** | JTAG / SWD Subsystem | **LEVEL 2 (SW TESTED)** | **NO** | `bp_execute_command("swd")` | **`ARM SWD [UNVERIF]`** | SWD bit-banging passes CTest. Physical Target DPIDR probing pending. |
| **USB Packet Decoder**| USB Packet & Transaction Decoder | **LEVEL 2 (SW TESTED)** | **NO** | `usb_decoder.cpp` | Not in local menu | Decoder passes CTest. Physical packet fixture on Header P1 pending. |
| **USB Host Mode** | USB Host Mode (TinyUSB) | **LEVEL 2 (SW TESTED)** | **NO** | `MockUsbHost.cpp` | Omitted from local front panel | Physical target host untested. |
| **USB Device Mode** | USB Device Mode | **LEVEL 2 (SW TESTED)** | **NO** | `device_mode.cpp` | Omitted from local front panel | Live host enumeration untested. |
| **USB Full Sniffer**| USB Full Sniffer / Analyzer | **LEVEL 1 (IMPLEMENTED)** | **NO** | `usb_sniffer.cpp` | Omitted from local front panel | External sniffer requires physical clamp fixture on Header P1. |
| **IR Transceiver** | IR Transmitter / Receiver | **LEVEL 2 (SW TESTED)** | **NO** | `ir_engine.cpp` | Omitted from local front panel | RMT pulses pass CTest. Physical IR diode pending. |
| **nRF24L01+ RF** | nRF24L01+ Transceiver | **LEVEL 2 (SW TESTED)** | **NO** | `nrf24_service.cpp` | Omitted from local front panel | Driver passes CTest. Physical UEXT transceiver module pending. |
| **HP 54520A Scope** | HP 54520A Integration | **LEVEL 2 (SW TESTED)** | **NO** | `hp54520a.cpp` | Omitted from local front panel | SCPI parser passes CTest. Physical RS-232 link pending. |
| **USB High-Speed** | USB High-Speed (480M) | **LEVEL 0 (UNSUPPORTED)** | **NO** | None | Omitted | Electrically unsupported on RP2040 without external ULPI PHY. |
