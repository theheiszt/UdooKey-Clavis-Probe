# UDOO KEY — Apple Hardware Capability Matrix

**Document Version:** 1.0.0  
**Date:** 2026-10-03  
**Status:** Official Engineering Specification  
**Architecture:** RP2040 Acquisition Core + ESP32 Telemetry Engine  

---

## 1. Architectural Capability Summary

The UDOO KEY operates as a **physical-layer diagnostic companion instrument** for Apple hardware, accessories, and wireless ecosystems. It explicitly distinguishes between hardware physical-layer capabilities provided by its dual MCUs and software diagnostics executed on the host PC.

```
┌────────────────────────────────────────────────────────────────────────┐
│                        HOST PC (udoo-key-app)                          │
│   • Apple Device Discovery (USBMux, libimobiledevice, devicectl)       │
│   • Descriptor Analysis, Apple Proprietary Protocol Decoding           │
│   • Timeline Correlation: Host Software Logs + UDOO Physical Traces    │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ 1 Mbps UART / USB CDC
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                        UDOO KEY DUAL-MCU CORE                          │
│                                                                        │
│   RP2040 Real-Time Acquisition:       ESP32 Wireless Telemetry:        │
│   • 8-Ch 100 MSPS Logic Analyzer      • Apple BLE Continuity Scanner   │
│   • 10 ns Hardware Triggering         • AirDrop / AirPods Detection    │
│   • SYNC_OUT Analyzer Strobe Pulse    • Find My Beacon Logging         │
│   • 3-Ch Analog Voltmeter (VBUS/ADC)  • 802.11 Wi-Fi Recon Probes      │
│   • Target ARM SWD / JTAG Probe       • Microsecond Clock Sync Link    │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Definitive Feature Matrix

| Subsystem / Capability | Hardware Authority | Operational Limit | Apple Diagnostic Role | Status |
| :--- | :--- | :--- | :--- | :--- |
| **Logic Analyzer Bus** | RP2040 PIO0 SM0 | 8 Channels (GP2..GP9), up to 100 MSPS | Sideband capture (CC1, CC2, VBUS_EN, I2C, UART) | **Supported** |
| **Capture Buffer Depth** | RP2040 SRAM | 65,536 Samples (64 KiB) Ring DMA | High-speed transient capture before/after trigger | **Supported** |
| **Trigger Modes** | RP2040 Trigger Engine | Auto, Edge Rise/Fall, High/Low, Qualified, Pattern, Pulse Width | Trigger on CC transition, VBUS glitch, or accessory packet | **Supported** |
| **External Analyzer Sync** | RP2040 GP9 / GP10 | 1 µs hardware strobe pulse (`SYNC_OUT`) | Strobe external Beagle USB / Total Phase protocol analyzers | **Supported** |
| **External Trigger In** | RP2040 GP10 (`PIN_EXT_TRIG`) | < 10 ns hardware trigger comparator | Trigger logic capture from external test fixture or analyzer | **Supported** |
| **Analog Voltage Rails** | RP2040 SAR ADC0..ADC2 | 0 – 3.3V (Scaled via divider for 5V/9V/15V/20V) | VBUS step timing, inrush current shunt monitoring | **Supported** |
| **USB 2.0 HS (480M) PHY Sniffing** | RP2040 USB Controller | USB 1.1 Full-Speed (12M) Device/Host PHY | Direct packet sniffing of 480 Mbps differential pairs | **NOT SUPPORTED (Honest declaration)** |
| **USB 3.x SuperSpeed (5G) PHY** | RP2040 USB Controller | No 5 Gbps SERDES transceiver | Differential SuperSpeed packet sniffing | **NOT SUPPORTED (Honest declaration)** |
| **Target SWD / JTAG Probe** | RP2040 SWD Engine | JTAG TAP & ARM CoreSight SWD (DPIDR, AP access) | Accessory target microcontroller debug (cables, docks) | **Supported** |
| **Inter-MCU Reset & Flash** | ESP32 GPIO23, SWD | ESP32 controls RP2040 RUN & SWD lines | Self-recovery, firmware updates, in-system debug | **Supported** |
| **Apple BLE Proximity Scan** | ESP32 NimBLE Engine | 2.4 GHz BLE passive & active scan | Apple Company ID (0x004C) frame extraction & logging | **Supported** |
| **AirDrop Detection** | ESP32 NimBLE Engine | Apple Type 0x05 advertisement | Detect active nearby AirDrop transmissions | **Supported** |
| **AirPods / Proximity Pairing**| ESP32 NimBLE Engine | Apple Type 0x07 advertisement | Detect AirPods case open/pairing telemetry | **Supported** |
| **Find My Offline Beacon** | ESP32 NimBLE Engine | Apple Type 0x12 advertisement | Detect Find My / AirTag offline beaconing | **Supported** |
| **Nearby Action / Continuity** | ESP32 NimBLE Engine | Apple Type 0x10 advertisement | Detect Universal Control / Continuity Camera | **Supported** |
| **Wi-Fi Reconnaissance** | ESP32 802.11 b/g/n | Channels 1..14, RSSI, BSSID, SSID | Correlate Wi-Fi roaming and packet channel conditions | **Supported** |
| **Network Latency Probes** | ESP32 TCP/IP Stack | DNS resolution & ICMP / TCP handshake | Probe `captive.apple.com` and Apple APNs endpoints | **Supported** |
| **Microsecond Timebase Sync** | RP2040 64-bit HW Timer | 1.0 µs resolution, ping-pong RTT sync | Synchronize ESP32 radio events with RP2040 logic traces | **Supported** |
| **Host Marker Ingestion** | Protocol Opcode 0x26 | Monotonic ID + 32-byte label | Host injects software event markers into hardware capture | **Supported** |

---

## 3. Physical Signal Multiplexing for Apple Accessory Testing

When configured for `PROFILE_APPLE_DIAGNOSTICS`, the UDOO KEY GPIO header lines are assigned to the following canonical roles:

```
[P1 Header Pin 4]  GP2: APPLE_CC1          ──► USB Type-C CC1 / Lightning ID0
[P1 Header Pin 5]  GP3: APPLE_CC2          ──► USB Type-C CC2 / Lightning ID1
[P1 Header Pin 6]  GP4: APPLE_VBUS_EN      ──► VBUS Enable / Power switch line
[P1 Header Pin 7]  GP5: APPLE_ACC_SDA      ──► Accessory I2C SDA / Single-Wire bus
[P1 Header Pin 9]  GP6: APPLE_ACC_SCL      ──► Accessory I2C SCL
[P1 Header Pin 10] GP7: APPLE_ACC_RX       ──► Accessory UART RX / SWD DIO
[P1 Header Pin 11] GP8: APPLE_ACC_TX       ──► Accessory UART TX / SWD CLK
[P1 Header Pin 12] GP9: APPLE_SYNC_STROBE  ──► Pulse Out to External Protocol Analyzer
[P1 Header Pin 14] GP10: EXT_TRIG          ◄── Trigger In from External Fixture
[P2 Header Pin 11] GP26: ADC0 (VBUS)       ──► Attenuated VBUS Rail (0..20V via divider)
[P2 Header Pin 12] GP27: ADC1 (SHUNT)      ──► Current Shunt Sense Amplifier
[P2 Header Pin 14] GP28: ADC2 (AUX_V)      ──► Accessory Auxiliary Rail
```
