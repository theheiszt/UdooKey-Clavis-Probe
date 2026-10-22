# UDOO KEY Dual-MCU Universal Electronics Instrument Architecture

**Document ID:** DOC-ARCH-MASTER-20261005  
**Status:** AUTHORITATIVE SYSTEM ARCHITECTURE  
**Date:** 2026-10-05  
**Authority:** Master Specification Sections 1–10  

---

## 1. System Topology & Dual-MCU Fusion

The UDOO KEY Universal Electronics Instrument fuses two heterogeneous microcontrollers into a single unified instrument:

```text
                         UDOO KEY
                            │
              ┌─────────────┴─────────────┐
              │                           │
           RP2040                       ESP32
              │                           │
       Deterministic                 Application /
       instrumentation               communications
              │                           │
       ┌──────┴───────┐             ┌─────┴─────────┐
       │      │       │             │     │         │
      PIO    DMA    ADC            Wi-Fi BLE       UI
       │      │       │             │     │         │
       └──────┬───────┘             └─────┬─────────┘
              │                           │
       Logic analyzer              Network analysis
       USB analyzer                Bluetooth
       Digital capture             IR
       Trigger engine              nRF24
       Measurements                PCAP
       JTAG/SWD                    External instruments
       Bus timing                  Terminal
              │                           │
              └───────────UART────────────┘
                   (GP0/GP1 ◄► IO19/IO22)
                            │
                   Instrument Protocol
                            │
                            ▼
                 C++ Desktop Application
               (Protocol Analyzer Workspace)
```

---

## 2. Microcontroller Division of Ownership

### 2.1 RP2040 — Deterministic Real-Time Engine
The RP2040 owns all subsystems requiring cycle-exact deterministic timing:
- **8-Channel Digital Acquisition**: PIO state machines sampling GP2..GP9 into circular DMA buffers.
- **64 KiB Continuous Circular Buffer**: Up to 100 MSPS sampling rate with configurable pre/post-trigger ratios.
- **Hardware Trigger Engine**: Edge (rising/falling/either), Level (high/low), 8-bit Pattern+Mask, Glitch detection, and external trigger input (GP10).
- **USB Physical Analysis**: Bus line probing (D+/D-), speed detection (Low-Speed / Full-Speed), NRZI decoding, bit unstuffing, token/data/handshake decoding.
- **Digital Measurements**: Hardware frequency counting (PWM counter), period, pulse width, duty cycle, edge counters.
- **Analog Voltmeter**: 3-channel 12-bit SAR ADC (GP26..GP28) with 16x oversampling and hardware transfer calibration.
- **Bus Pirate Multi-Mode Controller**: Hardware-timed SPI, I2C, UART, 1-Wire, DIO engines.

### 2.2 ESP32 — Application, Communications & RF Engine
The ESP32 owns all high-level connectivity and peripheral interfaces:
- **Wi-Fi Subsystem**: Station/AP management, RSSI telemetry, active scanner, promiscuous 802.11 packet capture.
- **Bluetooth / BLE Subsystem**: Generic BLE advertisement scanner, RSSI tracking, service UUID extraction.
- **Infrared (IR) Subsystem**: RMT transmitter and demodulated receiver, raw timing pulses and protocol decoders (NEC, Sony, RC5).
- **nRF24L01+ 2.4GHz RF Subsystem**: SPI communication via UEXT, RF channel scanning, packet transmission and reception.
- **Packet & PCAP Engine**: Decoding 802.11, Ethernet, ARP, IPv4, IPv6, UDP, TCP, ICMP, DHCP, DNS; real-time PCAP stream export.
- **External Instrument Bridge**: Interface management and SCPI command translation for external oscilloscopes (HP 54520A).
- **Resource Manager**: Mutex-guarded hardware resource locking preventing pin, bus, and peripheral contention.

---

## 3. Inter-MCU Communication Architecture

- **Physical Link**: Dedicated on-board PCB traces:
  - RP2040 UART0: GP0 (RX), GP1 (TX)
  - ESP32 UART1: GPIO19 (TX), GPIO22 (RX)
- **Baud Rate**: 1,000,000 baud (1 Mbps), 8 data bits, no parity, 1 stop bit.
- **Framing**: Binary packet protocol with CCITT CRC16 verification (`shared/instrument_protocol.h`).
- **Zero Real-Time Interference**: RP2040 DMA captures directly into SRAM independently of inter-MCU serial transmission. Serial transmission occurs asynchronously upon trigger capture freeze.

---

## 4. Desktop Companion Architecture

The C++ Desktop Application (`udoo-key-app`) is built on modern Qt 6.8+ and MSVC 64-bit:
- **Transport Abstraction**: `SerialTransport` (direct hardware USB CDC) and `MockTransport` (unit test simulation).
- **Protocol Clients**: `BinaryLogicClient`, `SumpClient`, `ModeClient`, `GpioClient`, `BusToolsClient`, `WirelessClient`, `PacketClient`.
- **Workspaces**:
  - Logic Analyzer (multi-channel waveform display, zoom, cursors, SUMP/Sigrok compatibility).
  - Bus Pirate Terminal (VT100 interactive console, syntax execution, macro engine).
  - USB Protocol Analyzer (3-level hierarchy: Transactions -> Packets -> Raw Symbols).
  - Wireless & RF (Wi-Fi telemetry, BLE discovery, live PCAP export).
  - External Oscilloscope (HP 54520A SCPI control, waveform retrieval, cross-trigger correlation).
