# UDOO KEY Binary Instrument Protocol Specification

**Document ID:** DOC-PROTO-MASTER-20261005  
**Status:** AUTHORITATIVE PROTOCOL SPECIFICATION  
**Date:** 2026-10-05  
**Version:** 1.0  
**Authority:** Master Specification Section 10 & `shared/instrument_protocol.h`  

---

## 1. Framing & Packet Structure

All binary communications across the inter-MCU UART link (RP2040 ◄► ESP32) and the host USB CDC interface share the identical frame structure:

```text
┌────────┬─────────┬──────┬───────┬──────────┬────────────────┬─────────┬───────┐
│ MAGIC  │ VERSION │ TYPE │ FLAGS │ SEQUENCE │ PAYLOAD_LENGTH │ PAYLOAD │ CRC16 │
│ (2 B)  │  (1 B)  │ (1 B)│ (1 B) │  (1 B)   │     (2 B)      │  (N B)  │ (2 B) │
└────────┴─────────┴──────┴───────┴──────────┴────────────────┴─────────┴───────┘
```

- **MAGIC**: `0xAA 0x55` (Frame synchronization word).
- **VERSION**: `0x01` (Protocol revision 1.0).
- **TYPE**: 8-bit packet opcode defining the request or response action.
- **FLAGS**: Bitmask flags (Bit 0: Is Response, Bit 1: Error Flag, Bit 2: Fragmented).
- **SEQUENCE**: 8-bit rolling transaction sequence number (0..255).
- **PAYLOAD_LENGTH**: 16-bit little-endian unsigned integer (0..1024 bytes).
- **PAYLOAD**: N bytes of command-specific binary data.
- **CRC16**: 16-bit CCITT CRC calculated over all bytes from `VERSION` through the end of `PAYLOAD` (Polynomial: $0\text{x}1021$, Initial: $0\text{xFFFF}$).

---

## 2. Standard Packet Types & Opcodes

| Opcode | Name | Direction | Payload Description |
| :---: | :--- | :---: | :--- |
| **`0x01`** | `PKT_TYPE_PING` | Host ➔ Device | Empty or 4-byte echo timestamp. |
| **`0x02`** | `PKT_TYPE_PONG` | Device ➔ Host | 4-byte echoed timestamp + MCU status. |
| **`0x03`** | `PKT_TYPE_GET_STATUS` | Host ➔ Device | Query active instrument mode and subsystem states. |
| **`0x04`** | `PKT_TYPE_STATUS_RESP` | Device ➔ Host | Active mode, armed status, FIFO depths, error flags. |
| **`0x05`** | `PKT_TYPE_SET_CONFIG` | Host ➔ Device | Sample rate, channels, trigger configuration. |
| **`0x06`** | `PKT_TYPE_CONFIG_ACK` | Device ➔ Host | Confirmation of applied configuration. |
| **`0x10`** | `PKT_TYPE_START_CAPTURE` | Host ➔ Device | Arm logic analyzer / USB sniffer acquisition engine. |
| **`0x11`** | `PKT_TYPE_STOP_CAPTURE` | Host ➔ Device | Abort ongoing acquisition and return to idle. |
| **`0x12`** | `PKT_TYPE_CAPTURE_INFO` | Device ➔ Host | Trigger index, sample count, timestamp, buffer status. |
| **`0x13`** | `PKT_TYPE_GET_CAPTURE_DATA` | Host ➔ Device | Request chunk of frozen SRAM sample buffer. |
| **`0x14`** | `PKT_TYPE_CAPTURE_DATA` | Device ➔ Host | Chunk offset, chunk length, raw 8-bit sample bytes. |
| **`0x20`** | `PKT_TYPE_TRIGGER_EVENT` | Device ➔ Host | Asynchronous notification that trigger condition met. |
| **`0x30`** | `PKT_TYPE_GET_MEASUREMENT` | Host ➔ Device | Request frequency, period, pulse-width measurements. |
| **`0x31`** | `PKT_TYPE_MEASUREMENT_DATA`| Device ➔ Host | Measured frequency (Hz), duty (%), edge counts. |
| **`0x32`** | `PKT_TYPE_GET_ADC` | Host ➔ Device | Request precision ADC voltages on GP26..GP28. |
| **`0x33`** | `PKT_TYPE_ADC_DATA` | Device ➔ Host | Millivolts for CH0, CH1, CH2, VSYS. |
| **`0x40`** | `PKT_TYPE_RADIO_CMD` | Host ➔ Coproc | Wireless scan / BLE / IR / nRF24 execution request. |
| **`0x41`** | `PKT_TYPE_RADIO_DATA` | Coproc ➔ Host | Live wireless event / packet / telemetry record. |
| **`0x50`** | `PKT_TYPE_PCAP_HEADER` | Coproc ➔ Host | Global 24-byte PCAP file header. |
| **`0x51`** | `PKT_TYPE_PCAP_PACKET` | Coproc ➔ Host | Standard 16-byte PCAP packet header + raw bytes. |
| **`0x60`** | `PKT_TYPE_SCOPE_CMD` | Host ➔ Coproc | HP 54520A SCPI query or control command. |
| **`0x61`** | `PKT_TYPE_SCOPE_DATA` | Coproc ➔ Host | Raw or converted waveform data payload. |
| **`0xFF`** | `PKT_TYPE_ERROR` | Either | Error code + human-readable diagnostic message. |

---

## 3. Explicit Error Handling

Every failure produces an explicit `PKT_TYPE_ERROR` packet containing:
```cpp
struct InstrumentErrorPayload {
    uint16_t error_code;       // Machine-readable enum
    uint8_t  subsystem_id;     // RP2040, ESP32, USB, RADIO, SCOPE
    char     message[64];      // Clear diagnostic explanation
};
```

Common error codes:
- `ERR_CRC_MISMATCH (0x0001)`: Received frame corrupted.
- `ERR_INVALID_MODE (0x0002)`: Requested action illegal in current mode.
- `ERR_RESOURCE_BUSY (0x0003)`: Hardware pin or peripheral in conflict.
- `ERR_CAPTURE_OVERFLOW (0x0004)`: Circular buffer acquisition overrun.
- `ERR_TRIGGER_TIMEOUT (0x0005)`: Trigger condition not detected within window.
- `ERR_INSTRUMENT_OFFLINE (0x0006)`: External oscilloscope not responding.
