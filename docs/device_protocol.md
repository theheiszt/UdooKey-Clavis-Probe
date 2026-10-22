# UDOO KEY — InstrumentProtocol v1.0 Specification

**Protocol Version:** 1.0 (Magic `0xAA55`)  
**Date:** 2026-10-03  
**Status:** Canonical Binary Transport Specification  
**Transports:** 1 Mbps Factory UART Link / USB CDC / Jumper-Wired SPI Mailbox  

---

## 1. Frame Structure

All packets conform to a 10-byte fixed header followed by a variable-length payload protected by CRC-16 (CCITT-FALSE).

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Magic (0xAA55)          |    Version    |     Type      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Sequence            |          Payload Length       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|            CRC-16             |   Payload Bytes (0..1024) ... |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
```

### 1.1 Header Field Definitions
* **`magic` (uint16_t, offset 0):** Always `0xAA55` (little-endian byte stream: `0x55`, `0xAA`).
* **`version` (uint8_t, offset 2):** Protocol specification version (`0x01`).
* **`type` (uint8_t, offset 3):** `InstrumentPacketType` identifier.
* **`sequence` (uint16_t, offset 4):** Monotonically increasing rolling sequence number.
* **`payload_len` (uint16_t, offset 6):** Number of payload bytes following the header (0 to 1024).
* **`crc16` (uint16_t, offset 8):** Fast CRC-16 (CCITT-FALSE) computed across the payload. Must be `0x0000` when `payload_len == 0`.

---

## 2. Complete Opcode Reference Table

| Opcode | Packet Name | Direction | Payload Struct | Description |
| :--- | :--- | :--- | :--- | :--- |
| **0x01** | `PKT_TYPE_PING` | Host ➔ Dev | None | Keepalive ping query |
| **0x02** | `PKT_TYPE_PONG` | Dev ➔ Host | None | Keepalive pong response |
| **0x03** | `PKT_TYPE_GET_STATUS` | Host ➔ Dev | None | Query acquisition and rail state |
| **0x04** | `PKT_TYPE_STATUS_RESP` | Dev ➔ Host | `InstrumentStatusPayload` | Telemetry, FIFO flags, ADC mV |
| **0x05** | `PKT_TYPE_SET_CONFIG` | Host ➔ Dev | `InstrumentConfigPayload` | Set sample rate, trigger, depth |
| **0x06** | `PKT_TYPE_CONFIG_ACK` | Dev ➔ Host | None | Acknowledge configuration |
| **0x07** | `PKT_TYPE_START_CAPTURE` | Host ➔ Dev | None | Arm hardware acquisition |
| **0x08** | `PKT_TYPE_STOP_CAPTURE` | Host ➔ Dev | None | Abort active capture |
| **0x09** | `PKT_TYPE_TRIGGER_EVENT` | Dev ➔ Host | None | Asynchronous trigger tripped notice |
| **0x0A** | `PKT_TYPE_GET_CAPTURE_INFO`| Host ➔ Dev | None | Query completed capture metadata |
| **0x0B** | `PKT_TYPE_CAPTURE_INFO_RESP`| Dev ➔ Host| `InstrumentCaptureInfoPayload`| Total samples, CRC, trigger offset |
| **0x0C** | `PKT_TYPE_GET_CAPTURE_DATA`| Host ➔ Dev | `InstrumentChunkRequestPayload` | Request specific 512B sample chunk |
| **0x0D** | `PKT_TYPE_CAPTURE_DATA_CHUNK`| Dev ➔ Host| `InstrumentChunkDataPayload` | 512-byte raw sample block |
| **0x0E** | `PKT_TYPE_GET_MEASUREMENT` | Host ➔ Dev | None | Query hardware frequency meter |
| **0x0F** | `PKT_TYPE_MEASUREMENT_RESP` | Dev ➔ Host | `InstrumentMeasurementPayload` | Frequency, period, duty cycle |
| **0x10** | `PKT_TYPE_GET_ADC` | Host ➔ Dev | None | Read analog voltage rails |
| **0x11** | `PKT_TYPE_ADC_RESP` | Dev ➔ Host | `InstrumentAdcPayload` | GP26, GP27, GP28 voltages & raw |
| **0x12** | `PKT_TYPE_CLI_CMD` | Host ➔ Dev | Text bytes | Execute terminal command |
| **0x13** | `PKT_TYPE_CLI_RESP` | Dev ➔ Host | Text bytes | Terminal ASCII output |
| **0x20** | `PKT_TYPE_SELF_TEST_REQ` | Host ➔ Dev | None | Trigger hardware self-test suite |
| **0x21** | `PKT_TYPE_SELF_TEST_RESP`| Dev ➔ Host | `InstrumentSelfTestPayload` | 10-point subsystem test results |
| **0x22** | `PKT_TYPE_GET_CAPABILITIES`| Host ➔ Dev | None | Query instrument capability matrix |
| **0x23** | `PKT_TYPE_CAPABILITIES_RESP`| Dev ➔ Host| `InstrumentCapabilitiesPayload`| Max rate, channels, profiles |
| **0x24** | `PKT_TYPE_TIME_SYNC_REQ` | Host ➔ Dev | `InstrumentTimeSyncReqPayload` | High-precision time sync probe |
| **0x25** | `PKT_TYPE_TIME_SYNC_RESP`| Dev ➔ Host | `InstrumentTimeSyncRespPayload`| Synchronized 64-bit hardware time |
| **0x26** | `PKT_TYPE_MARK_EVENT` | Host ➔ Dev | `InstrumentMarkEventPayload` | Host injects software event marker |
| **0x27** | `PKT_TYPE_EVENT_STREAM` | Dev ➔ Host | `InstrumentEventPayload` | Stream structured physical event |
| **0x28** | `PKT_TYPE_SET_PINMUX` | Host ➔ Dev | `InstrumentPinMuxPayload` | Dynamic pin remapping request |
| **0x29** | `PKT_TYPE_PINMUX_RESP` | Dev ➔ Host | `InstrumentPinMuxRespPayload` | Pin remapping confirmation/conflict |
| **0x2A** | `PKT_TYPE_RADIO_EVENT` | Dev ➔ Host | `InstrumentRadioEventPayload` | BLE Apple frame / Wi-Fi probe |
| **0x2B** | `PKT_TYPE_RADIO_CMD` | Host ➔ Dev | `InstrumentRadioCmdPayload` | Configure wireless scanner |
| **0xFF** | `PKT_TYPE_ERROR` | Dev ➔ Host | `InstrumentErrorPayload` | Error code and diagnostic string |

---

## 3. Protocol Error Codes

```cpp
enum InstrumentErrorCode : uint8_t {
    INST_ERR_OK               = 0x00, // Operation completed successfully
    INST_ERR_INVALID_COMMAND  = 0x01, // Unknown or unsupported opcode
    INST_ERR_INVALID_PARAM    = 0x02, // Parameter out of valid range
    INST_ERR_PIN_CONFLICT     = 0x03, // Requested pin is locked or active
    INST_ERR_BUFFER_OVERFLOW  = 0x04, // DMA or event FIFO overflowed
    INST_ERR_TIMEOUT          = 0x05, // Operation timed out
    INST_ERR_NOT_SUPPORTED    = 0x06, // Feature not supported on hardware
    INST_ERR_BUSY             = 0x07, // Subsystem currently busy
    INST_ERR_CRC_MISMATCH     = 0x08, // Payload CRC-16 verification failed
    INST_ERR_DEVICE_NOT_ARMED = 0x09  // Trigger occurred while unarmed
};
```

---

## 4. CRC-16 Calculation Reference

```cpp
uint16_t instrument_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
```
