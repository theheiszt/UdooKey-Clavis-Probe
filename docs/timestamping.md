# UDOO KEY — Deterministic Hardware Timestamping & Clock Synchronization

**Document Version:** 1.0.0  
**Date:** 2026-10-03  
**Status:** Canonical Timing Architecture Specification  
**Authority:** RP2040 64-Bit Hardware Timer & Dual-MCU Ping-Pong RTT Synchronizer  

---

## 1. Timing Architecture Overview

High-speed physical acquisition (100 MSPS) and wireless packet capture require nanosecond-to-microsecond alignment between three independent clock domains:

```
┌────────────────────────┐      ┌────────────────────────┐      ┌────────────────────────┐
│     HOST DESKTOP       │      │    ESP32 COPROCESSOR   │      │    RP2040 ACQUISITION  │
│  (udoo-key-app / OS)   │      │ (Wireless & Web probe) │      │  (Timing Authority)    │
│                        │      │                        │      │                        │
│ • std::chrono (ns)     │◄────►│ • esp_timer_get_time() │◄────►│ • time_us_64() (1 µs)  │
│ • QDateTime wall clock │ UART │ • 240 MHz Xtensa LX6   │ UART │ • 133 MHz Cortex-M0+   │
│ • Host Markers (0x26)  │ /USB │ • BLE / Wi-Fi events   │1 Mbps│ • PIO sample clocks    │
└────────────────────────┘      └────────────────────────┘      └────────────────────────┘
```

The **Raspberry Pi RP2040 serves as the primary hardware timing authority**. All digital logic samples, trigger positions, and analog voltage conversions are timestamped against the RP2040 64-bit hardware timer.

---

## 2. Microsecond Hardware Clock Sources

### 2.1 RP2040 64-Bit Hardware Timer
* **Hardware:** Native 64-bit monotonic timer counter clocked at 1.000000 MHz.
* **Resolution:** Exactly 1.000 µs per tick.
* **Jitter / Stability:** ±20 ppm (governed by 12.000 MHz fundamental-mode crystal oscillator).
* **Rollover Period:** $2^{64}$ microseconds $\approx 584,942$ years. Rollover handling is inherently impossible during operational instrument lifetimes.
* **Acquisition Timestamp:** Captured via `time_us_64()` at the instant of trigger detection (`trigger_fire_timestamp_us`).

### 2.2 ESP32 High-Resolution Timer (`esp_timer`)
* **Hardware:** 64-bit microsecond counter based on the 40 MHz crystal oscillator.
* **Resolution:** 1.0 µs.
* **Offset Correction:** Continuously corrected relative to RP2040 via the Ping-Pong RTT algorithm.

---

## 3. Inter-MCU Ping-Pong Clock Synchronization

To correlate asynchronous wireless events (Apple Continuity BLE advertisements, Wi-Fi probe requests) with RP2040 logic analyzer traces, the ESP32 maintains a calibrated offset against the RP2040:

```
    ESP32                                     RP2040
      │                                         │
      │─── PKT_TYPE_TIME_SYNC_REQ (t_send) ────►│
      │                                         │ (Records t_device = time_us_64())
      │◄─── PKT_TYPE_TIME_SYNC_RESP (t_device) ─│
      │                                         │
 (t_recv)
```

### 3.1 Mathematical Formulation
1. Let $t_{\text{send}}$ be the local ESP32 timestamp when the request packet is transmitted.
2. Let $t_{\text{device}}$ be the deterministic RP2040 timestamp captured upon receipt of the packet.
3. Let $t_{\text{recv}}$ be the local ESP32 timestamp when the response packet is parsed.
4. The symmetrical round-trip propagation latency is:
   $$\text{RTT} = t_{\text{recv}} - t_{\text{send}}$$
5. Assuming symmetric transmission over the hardwired 1 Mbps UART link, the clock offset $\Delta t$ is:
   $$\Delta t = t_{\text{device}} - \left(t_{\text{send}} + \frac{\text{RTT}}{2}\right)$$
6. Any local ESP32 event at timestamp $t_{\text{esp}}$ is converted into the unified RP2040 timebase via:
   $$t_{\text{synced\_us}} = t_{\text{esp}} + \Delta t$$

---

## 4. Hardware Strobe Synchronization (`SYNC_OUT` & `EXT_TRIG`)

Because the RP2040 does not possess a 480 Mbps USB High-Speed PHY, physical synchronization with external dedicated USB protocol analyzers (e.g. Total Phase Beagle USB 480/5000, Teledyne LeCroy, Ellisys) is achieved through dedicated hardware trigger lines:

1. **`SYNC_OUT` (GP9 / GP10):**
   * Configured as high-speed push-pull output.
   * Whenever a trigger condition is met or a host software marker is received, the RP2040 drives `SYNC_OUT` HIGH for 1.0 µs and returns to LOW.
   * Connected to the external analyzer's digital Trigger In connector to freeze and align packet captures.
2. **`EXT_TRIG` (GP10):**
   * Connected to external test fixture or analyzer Trigger Out.
   * Enables immediate logic analyzer acquisition within < 10 ns of external electrical events.

---

## 5. Host Timeline Alignment (`TimestampSynchronizer`)

On the PC side, `TimestampSynchronizer` anchors the device's monotonic timebase to the host operating system's wall clock (`QDateTime`). When host-level software events occur (e.g. `ioregistry` device insertion, `log stream` messages), `AppleHardwareBridge` injects `PKT_TYPE_MARK_EVENT` (0x26), guaranteeing that host logs and hardware logic analyzer waveforms align on a common nanosecond timeline.
