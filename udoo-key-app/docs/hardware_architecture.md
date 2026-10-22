# UDOO KEY — Universal Electronics Instrument Hardware Architecture

## 1. System Overview

The **UDOO KEY** is an expandable, dual-microcontroller benchtop electronics diagnostic instrument designed for high-performance signal analysis, hardware protocol debugging, microcontroller flashing/debugging, and wireless peripheral inspection.

```
┌────────────────────────────────────────────────────────────────────────┐
│                        UDOO KEY HARDWARE PLATFORM                      │
│                                                                        │
│   ┌────────────────────────────────┐    ┌──────────────────────────┐   │
│   │   RP2040 Primary Controller    │    │    ESP32 Coprocessor     │   │
│   │   • Dual ARM Cortex-M0+ 133MHz │    │    • Dual Xtensa LX6 240MHz  │   │
│   │   • 264 KiB SRAM               │    │    • 520 KiB SRAM + PSRAM    │   │
│   │   • PIO State Machines (0..3)  │    │    • 802.11b/g/n Wi-Fi SoftAP│   │
│   │   • 100 MSPS Logic Analyzer    │    │    • Bluetooth LE 4.2 / 5.0  │   │
│   │   • High-Z Bus Engines         │    │    • Web Telemetry & WS      │   │
│   └───────────────▲────────────────┘    └─────────────▲────────────┘   │
│                   │                                   │                │
│                   └────────── Inter-MCU Link ─────────┘                │
│                         UART @ 1,000,000 baud 8N1                      │
│                         RP2040 GP0/GP1 ◄► ESP32 IO22/IO19              │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Microcontroller Roles & Partitioning

### 2.1 Primary Acquisition Processor: Raspberry Pi RP2040
- **Deterministic Hardware Sampling**: 2 PIO blocks (8 state machines total) driving 8-channel logic capture up to 100 MSPS with zero CPU intervention.
- **DMA Ring Buffer Engine**: 64 KiB circular DMA ring buffer with configurable pre-trigger/post-trigger ratios.
- **Physical Protocol Engines**: Hardware and bit-bang implementations for UART, I2C, SPI, 1-Wire, and Bus Pirate interactive macro syntax.
- **Embedded Debugging**: Hardware JTAG IEEE 1149.1 TAP state machine and ARM CoreSight SWD interface for flashing and debugging target microcontrollers.
- **Precision Analog**: 3 calibrated SAR ADC channels (GP26, GP27, GP28) for voltage measurement, rail monitoring, and continuity testing.

### 2.2 Wireless & Networking Coprocessor: Espressif ESP32-WROOM-32E
- **Wi-Fi Reconnaissance & AP**: Concurrent SoftAP (`192.168.4.1`) for web-based scope streaming and station reconnaissance.
- **Bluetooth Low Energy (BLE)**: Passive advertisement monitoring, RSSI profiling, and generic beacon detection.
- **Coprocessor Peripherals**: Dedicated modulation engines for 38 kHz Infrared (NEC/Raw) and Nordic nRF24L01+ 2.4 GHz RF transceivers.
- **Microsecond Clock Synchronization**: Two-way deterministic time synchronization syncing ESP32 timestamps to the RP2040 monotonic timer.

---

## 3. Physical Pinout & Port Assignments

### 3.1 Digital Probe Bus (GP2 .. GP9)
All probe inputs feature high-impedance protection and dynamic pin multiplexing:

| Pin | Default Function | Secondary Capabilities | Electrical Limits |
|-----|-------------------|------------------------|-------------------|
| GP2 | CH0 / Logic Probe | I2C SDA, SPI SCK, PWM Glitch Out | 0 – 3.3V LVCMOS |
| GP3 | CH1 / Logic Probe | I2C SCL, SPI MOSI | 0 – 3.3V LVCMOS |
| GP4 | CH2 / Logic Probe | SPI MISO, UART TX | 0 – 3.3V LVCMOS |
| GP5 | CH3 / Logic Probe | SPI CS#, UART RX | 0 – 3.3V LVCMOS |
| GP6 | CH4 / Logic Probe | 1-Wire DQ | 0 – 3.3V LVCMOS |
| GP7 | CH5 / Logic Probe | Generic GPIO | 0 – 3.3V LVCMOS |
| GP8 | CH6 / Logic Probe | SWD CLK / JTAG TCK | 0 – 3.3V LVCMOS |
| GP9 | CH7 / Logic Probe | SYNC_OUT Strobe / SWD DIO / JTAG TMS | 0 – 3.3V LVCMOS |

### 3.2 Dedicated Signals & Analog Channels
| Pin | Function | Description |
|-----|----------|-------------|
| GP10 | `EXT_TRIG` | Dedicated hardware external trigger input |
| GP14 | `PRESCALER` | 74HC74 frequency prescaler control line |
| GP20 | `BUZZER` | Resonant 2.7 kHz piezo continuity buzzer output |
| GP26 | `ADC0` | Calibrated Analog Channel 0 (0 – 3.3V) |
| GP27 | `ADC1` | Calibrated Analog Channel 1 (0 – 3.3V) |
| GP28 | `ADC2` | Calibrated Analog Channel 2 (0 – 3.3V) |

> [!WARNING]
> All digital and analog inputs on the UDOO KEY are 3.3V max. Exceeding 3.6V without external level shifters or divider networks can cause permanent damage to the RP2040 and ESP32 silicon.

---

## 4. Canonical Instrument Protocol (`0xAA55`)

Communication across the inter-MCU link and between host applications utilizes the canonical `InstrumentProtocol` binary frame:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          Magic (0xAA55)       |    Version    |   Packet Type |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|        Sequence Number        |          Payload Length       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       CRC-16 (CCITT-FALSE)    |          Payload ...          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Key Packet Identifiers
- `0x01` / `0x02`: Ping / Pong
- `0x05` / `0x06`: Set Acquisition Config / Ack
- `0x07` / `0x08`: Start / Stop Capture
- `0x09`: Hardware Trigger Notification
- `0x10` / `0x11`: ADC Measurement Request / Response
- `0x20` / `0x21`: Hardware Self-Test Diagnostic Request / Response
- `0x22` / `0x23`: Instrument Capabilities Query / Response
- `0x24` / `0x25`: Microsecond Time Synchronization Request / Response
- `0x2A` / `0x2B` / `0x2C`: Radio Event / Radio Command / Wi-Fi Telemetry
- `0x2D` / `0x2E`: Wi-Fi Promiscuous Sniffer Configuration & Telemetry Status
- `0x30` – `0x33`: Infrared (IR) Engine Configuration, TX, and RX Events
- `0x40` – `0x45`: nRF24L01 2.4 GHz Transceiver Control and Packet Streaming
- `0x50` / `0x51`: PacketRecord Telemetry & PCAP Dump Streaming
- `0x60` – `0x79`: Bus Pirate GPIO, UART, I2C, SPI, 1-Wire, and Syntax Engines

---

## 5. Diagnostic Self-Test Architecture

The instrument firmware includes comprehensive internal loopback and self-verification diagnostics accessible via `PKT_TYPE_SELF_TEST_REQ` (0x20) or the terminal `selftest` command.

Subsystems verified during self-test:
1. **GPIO Loopback**: High-Z state verification and pin drive integrity.
2. **PIO State Machines**: Verifies FIFO queues, clock prescalers, and DMA trigger handshakes.
3. **DMA Controllers**: Validates circular transfers, address wrap-around, and interrupt flags.
4. **ADC Subsystem**: Checks reference voltage stability and channel calibration offsets.
5. **Inter-MCU UART Link**: Confirms bidirectional framing, CRC16 verification, and baud rate stability.
6. **Wireless Radio Transceivers**: Validates Wi-Fi stack responsiveness, BLE scanning state, and RF module status.

---

## 6. Phase 2 ESP32 Communications Instrument Architecture

The ESP32 operates as the generic communications and protocol analysis coprocessor, implementing a Bus Pirate 5 style mode system while strictly enforcing the **Hardware Reality Rule**:

### 6.1 Hardware Reality Classification

Every subsystem capability is explicitly classified into one of five states:
- `SUPPORTED`: Hardware feature electrically wired and supported by firmware.
- `AVAILABLE`: Peripheral idle and ready for user allocation.
- `ACTIVE`: Peripheral acquired and actively generating signals or capturing traffic.
- `ERROR`: Hardware initialization failure, contention, or CRC violation.
- `UNAVAILABLE`: Hardware physically not present or stack unsupported (e.g. Bluetooth Classic under NimBLE, 100 Mbps Ethernet).

### 6.2 Bus Pirate Mode Prompt System
The ESP32 interactive USB CDC and Web terminal dynamically transitions between modes via `m <mode>`:
- `HiZ>`: Safe default idle state. All user pins released to high-impedance.
- `GPIO>`: Direct digital I/O inspection and manipulation (`list`, `mode`, `read`, `write`, `toggle`).
- `UART>`: Auxiliary UART on `Serial1` with baud, framing (8N1/8E1/etc.), read/write, and live monitor.
- `SPI>`: Hardware SPI master up to 40 MHz, modes 0..3, CS polarity control, read, write, full-duplex transfer.
- `I2C>`: Hardware I2C bus scanner (0x01..0x7E), read, write, and 9-clock bus deadlock recovery.
- `WIFI>`: Network survey, AP/STA configuration, DNS resolution, ping RTT, and promiscuous capture.
- `BLE>`: NimBLE controller scan, AD structure parser, and advertisement sniffer.
- `SNIFF>`: Wireshark-compatible PCAP packet capture engine with bounded FreeRTOS queue and hex dump/streaming.
- `IR>`: 38 kHz infrared carrier transmission and pulse demodulation (NEC, Sony, Samsung, raw timings).
- `NRF24>`: Nordic nRF24L01+ 2.4 GHz packet transceiver on UEXT Header P3.

### 6.3 Standard PCAP Format Compliance
All captured RF frames (Wi-Fi 802.11, BLE LL, nRF24) are packed into standard Wireshark-compatible PCAP format:
- **Global Header (24 bytes)**: Magic `0xA1B2C3D4` (microsecond timestamps), version 2.4, snaplen 65535 octets.
  - Link Type 105: `LINKTYPE_IEEE802_11`
  - Link Type 251: `LINKTYPE_BLUETOOTH_LE_LL`
  - Link Type 147: `LINKTYPE_USER0` (Nordic nRF24L01+)
- **Record Header (16 bytes)**: 32-bit `ts_sec`, 32-bit `ts_usec`, 32-bit `incl_len`, 32-bit `orig_len`.
- **Zero-Drop ISR Architecture**: Wi-Fi promiscuous RX callback routes frames via bounded `xQueueSendFromISR` to a background worker task, strictly avoiding heap allocations inside driver interrupt context.
