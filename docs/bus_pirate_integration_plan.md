# UDOO KEY — Bus Pirate Integration Plan & Architecture Roadmap

**Document ID:** DOC-BP-PLAN-20261004  
**Date:** 2026-10-04  
**Classification:** Authoritative Technical Plan & Roadmap  
**Scope:** Seamless restoration and integration of Bus Pirate capabilities into the UDOO KEY ecosystem without regressing existing baseline functionality.

---

## 1. Architectural Principles & Non-Negotiable Invariants

1. **Frozen Functional Baseline:**
   * RP2040 PIO0 SM0 logic analyzer (100 MSPS, 64 KiB circular DMA ring buffer).
   * Hardware trigger system (Edge, Level, Pattern, Ext Trig GP10).
   * SUMP / PulseView compatibility.
   * RP2040 12-bit SAR ADC voltmeter (GP26..GP28).
   * Onboard ST7735 128x128 SPI display and 5 navigation buttons.
   * Inter-MCU 1 Mbps UART link (`InstrumentProtocol` v1.0).
   * JTAG & SWD subsystem in desktop application and firmware.
2. **Mutual Exclusion on the Probe Bus (GP2..GP9):**
   * Only ONE subsystem may own the probe bus pins at any given moment.
   * Every mode transition strictly executes:
     $$\text{CURRENT\_MODE} \xrightarrow{\text{leaveMode()}} \text{Safe Hi-Z (Inputs, No Pulls)} \xrightarrow{\text{enterMode()}} \text{NEW\_MODE}$$
3. **Single Canonical Transport Protocol:**
   * All new Bus Pirate features are exposed via **`InstrumentProtocol` v1.0** framing (`0xAA55` magic, 16-bit CRC CCITT-FALSE).
   * No parallel framing or unvalidated raw serial bypassing is permitted.
4. **Unified Service Backends:**
   * The interactive CLI terminal and the GUI desktop clients bind to the exact same backend service to prevent divergent implementations.

---

## 2. Hardware Resource Ownership & Allocation

```text
┌────────────────────────────────────────────────────────────────────────┐
│                        SECO UDOO KEY HARDWARE                          │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
           ┌────────────────────────┴────────────────────────┐
           ▼                                                 ▼
┌───────────────────────────────────┐    ┌───────────────────────────────────┐
│     RP2040 (Real-Time Master)     │    │    ESP32-WROVER-E (Coprocessor)   │
├───────────────────────────────────┤    ├───────────────────────────────────┤
│ • Probe Bus GP2..GP9 (Header P1)  │    │ • Apple BLE Scan (Company 0x004C) │
│ • External Trigger GP10           │    │ • Wi-Fi Diagnostics & Captive Test│
│ • Prescaler Select GP14           │    │ • WebSockets & HTML5 WebUI Scope  │
│ • Voltmeter ADC0..ADC2 (GP26..28) │    │ • UEXT Connector P3 (SPI/GPIO)    │
│ • Local Display & Buttons         │    │ • Infrared RMT Controller         │
│ • PIO0 SM0 100MSPS Capture Engine │    │ • Status LEDs (IO32, IO33)        │
│ • SIO / PIO Bit-Banging & Timers  │    │ • Time Sync & Correlation Engine  │
│ • Factory UART0 (GP0/GP1 @ 1Mbps) │    │ • Factory UART1 (IO19/22 @ 1Mbps) │
└───────────────────────────────────┘    └───────────────────────────────────┘
```

### Probe Bus Pin Mapping (Header P1 Pins 4..12)

| Probe Channel | RP2040 Pin | SIO / DIO | I2C Mode | SPI Mode | UART Mode | 1-Wire Mode | SWD / JTAG Mode |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Probe 0** | **GP2** | DIO0 | SDA (I2C1) | SCK (SPI0) | — | — | — |
| **Probe 1** | **GP3** | DIO1 | SCL (I2C1) | MOSI (SPI0) | — | — | — |
| **Probe 2** | **GP4** | DIO2 | — | MISO (SPI0) | TX (UART1) | — | TDI |
| **Probe 3** | **GP5** | DIO3 | — | CS (SPI0) | RX (UART1) | — | TDO |
| **Probe 4** | **GP6** | DIO4 | — | — | — | DQ (1-Wire) | — |
| **Probe 5** | **GP7** | DIO5 | — | — | — | — | SWCLK / TCK |
| **Probe 6** | **GP8** | DIO6 | — | — | — | — | SWDIO / TMS |
| **Probe 7** | **GP9** | DIO7 | — | — | — | — | nTRST / AUX |

---

## 3. Protocol Opcode Space Allocation (`0x60` – `0x7F`)

The dedicated range `0x60` through `0x7F` in `shared/instrument_protocol.h` is assigned exclusively for Bus Pirate services:

| Opcode | Packet Constant | Direction | Payload Structure | Function / Description |
| :---: | :--- | :---: | :--- | :--- |
| **`0x60`** | `PKT_TYPE_GPIO_CONFIG` | Host -> Dev | `InstrumentGpioConfigPayload` | Configure pin direction, initial level, pull mode |
| **`0x61`** | `PKT_TYPE_GPIO_READ` | Host -> Dev | `InstrumentGpioReadPayload` | Request digital read of a single probe pin |
| **`0x62`** | `PKT_TYPE_GPIO_WRITE` | Host -> Dev | `InstrumentGpioWritePayload` | Write digital level to an output probe pin |
| **`0x63`** | `PKT_TYPE_GPIO_STATUS` | Dev -> Host | `InstrumentGpioStatusPayload` | Detailed status report (dir, level, pull, all pins mask) |
| **`0x64`** | `PKT_TYPE_GPIO_WRITE_MASK` | Host -> Dev | `InstrumentGpioWriteMaskPayload` | Write 8-bit direction and output values to GP2..GP9 |
| **`0x65`** | `PKT_TYPE_GPIO_READ_ALL` | Both | `InstrumentGpioReadAllPayload` | Read all 8 probe bus inputs simultaneously |
| **`0x68`** | `PKT_TYPE_UART_CONFIG` | Host -> Dev | `InstrumentUartConfigPayload` | Set baud rate, data bits, parity, stop bits |
| **`0x69`** | `PKT_TYPE_UART_TX` | Host -> Dev | `InstrumentUartDataPayload` | Transmit binary data stream over UART1 |
| **`0x6A`** | `PKT_TYPE_UART_RX` | Dev -> Host | `InstrumentUartDataPayload` | Received binary data stream from UART1 |
| **`0x6B`** | `PKT_TYPE_UART_BRIDGE_STATE`| Both | `InstrumentUartBridgePayload` | Enable/disable transparent pass-through bridge |
| **`0x6C`** | `PKT_TYPE_I2C_CONFIG` | Host -> Dev | `InstrumentI2cConfigPayload` | Set I2C bus speed (100kHz, 400kHz, 1MHz) |
| **`0x6D`** | `PKT_TYPE_I2C_SCAN_REQ` | Host -> Dev | Empty | Trigger 7-bit bus address scan (0x08..0x77) |
| **`0x6E`** | `PKT_TYPE_I2C_SCAN_RESP` | Dev -> Host | `InstrumentI2cScanRespPayload` | Bitmask / array of responding I2C addresses |
| **`0x6F`** | `PKT_TYPE_I2C_XFER` | Host -> Dev | `InstrumentI2cXferPayload` | Atomic I2C Write-then-Read transaction |
| **`0x70`** | `PKT_TYPE_SPI_CONFIG` | Host -> Dev | `InstrumentSpiConfigPayload` | Set SPI speed, mode (CPOL/CPHA), CS pin |
| **`0x71`** | `PKT_TYPE_SPI_XFER` | Both | `InstrumentSpiXferPayload` | Full-duplex SPI master transaction |
| **`0x72`** | `PKT_TYPE_SPI_FLASH_PROBE` | Host -> Dev | Empty | Read JEDEC ID (0x9F) and SFDP metadata |
| **`0x73`** | `PKT_TYPE_SPI_FLASH_RESP` | Dev -> Host | `InstrumentFlashInfoPayload` | Manufacturer, capacity, page size, speed |
| **`0x74`** | `PKT_TYPE_1WIRE_CONFIG` | Host -> Dev | `Instrument1WireConfigPayload` | Initialize 1-Wire on GP6 |
| **`0x75`** | `PKT_TYPE_1WIRE_SEARCH_REQ` | Host -> Dev | Empty | Run 1-Wire Search ROM binary tree search |
| **`0x76`** | `PKT_TYPE_1WIRE_SEARCH_RESP`| Dev -> Host | `Instrument1WireSearchRespPayload`| List of discovered 64-bit ROM IDs |
| **`0x77`** | `PKT_TYPE_1WIRE_XFER` | Both | `Instrument1WireXferPayload` | Reset pulse, match ROM, write/read bytes |
| **`0x78`** | `PKT_TYPE_BP_SYNTAX_EXEC` | Host -> Dev | `InstrumentBpSyntaxExecPayload` | Execute standard BP syntax string e.g. `[ 0x50 r:4 ]` |
| **`0x79`** | `PKT_TYPE_BP_SYNTAX_RESP` | Dev -> Host | `InstrumentBpSyntaxRespPayload` | Syntax execution response and parsed results |

---

## 4. Phase-by-Phase Implementation Roadmap

```text
┌────────────────────────────────────────────────────────────────────────┐
│                   PHASE 1: GENERIC GPIO SERVICE                        │
│   Protocol Opcodes (0x60-0x65), RP2040 gpio_hal integration,           │
│   GpioClient in desktop app, automated CTest unit test suite.          │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│              PHASE 2: BUS MODE MULTIPLEXER & INVARIANTS                │
│   InstrumentModeManager verification, safe Hi-Z transitions,           │
│   SPI0 probe/display arbiter, hardware resource validation.            │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                PHASE 3: HARDWARE PROTOCOL ENGINES                      │
│   I2C scanner/transfer, SPI flash SFDP interrogator,                   │
│   1-Wire Search ROM, UART secondary bridge integration.                │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│          PHASE 4: INTERACTIVE SYNTAX & COMMAND EXECUTION               │
│   Square-bracket bus syntax parsing ([ 0x50 r:4 ]),                    │
│   Terminal console macro execution, unified CLI backends.              │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│        PHASE 5: COPROCESSOR INTEGRATION (WEB & WIRELESS)               │
│   ESP32 WebSockets waveform streaming, HTML5 scope UI port,            │
│   nRF24L01 / IR module integration via UEXT P3 & RMT.                  │
└────────────────────────────────────────────────────────────────────────┘
```

### Detailed Phase 1 Plan (Immediate Action):
1. **Protocol Extension:**
   * Add `PKT_TYPE_GPIO_CONFIG` (`0x60`), `PKT_TYPE_GPIO_READ` (`0x61`), `PKT_TYPE_GPIO_WRITE` (`0x62`), `PKT_TYPE_GPIO_STATUS` (`0x63`), `PKT_TYPE_GPIO_WRITE_MASK` (`0x64`), `PKT_TYPE_GPIO_READ_ALL` (`0x65`) to `shared/instrument_protocol.h`.
   * Define packet payload structs with `#pragma pack(push, 1)`.
2. **RP2040 Firmware Engine:**
   * Wire protocol opcodes `0x60..0x65` in `rp2040_logic_analyzer/src/instrument_engine.cpp`.
   * Automatically transition to `InstrumentMode::DIO` when a GPIO command arrives if currently `IDLE`.
   * Route operations to the thoroughly tested `gpio_hal.cpp` routines:
     * `gpio_hal_set_dir()` with glitch-free output sequencing.
     * `gpio_hal_set_pull()` for internal pull-ups/pull-downs.
     * `gpio_hal_write()`, `gpio_hal_read()`, `gpio_hal_toggle()`.
     * `gpio_hal_write_mask()`, `gpio_hal_read_all()`.
3. **Desktop Application (`udoo-key-app`):**
   * Create `include/GpioClient.h` and `src/GpioClient.cpp`.
   * Provide clean C++20 / Qt methods:
     * `configurePin(uint8_t pin, bool isOutput, bool initialLevel, GpioPull pull)`
     * `writePin(uint8_t pin, bool level)`
     * `readPin(uint8_t pin)`
     * `writeMask(uint8_t dirMask, uint8_t valueMask)`
     * `readAll()`
   * Emit Qt signals `pinStatusChanged(const GpioPinStatus& status)` and `allPinsRead(uint8_t values, uint8_t directions)`.
4. **Verification & Tests:**
   * Add comprehensive unit test suite in `tests/TestGpioClient.cpp` (or integrated into `udoo_key_tests`).
   * Test `MockTransport` packet transmission, responses, error handling, bitmask operations, and roundtrip CRC.
   * Run CMake / Ninja build and execute `ctest` to ensure 100% test pass rate.
