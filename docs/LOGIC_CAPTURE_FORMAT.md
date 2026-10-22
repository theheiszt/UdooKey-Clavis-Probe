# UDOO KEY — Logic Analyzer Binary Capture Format

## 1. Specification Overview

When logic captures are transferred between the ESP32 coprocessor and client web applications via WebSocket or REST API (`/api/logic/capture`), they are framed using a packed binary layout.

The frame consists of:
1. **Header**: 32-byte packed binary `WebLogicCaptureHeader`.
2. **Payload**: Packed 8-bit sample bytes (1 byte per sample, where each bit corresponds to one logic channel).

```text
┌────────────────────────────────────────────────────────────────────────┐
│                   WebLogicCaptureHeader (32 Bytes)                     │
├────────────────────────────────────────────────────────────────────────┤
│ Byte 0..3   : Magic (0x53434F50 - 'SCOP' Little-Endian)                │
│ Byte 4..5   : Protocol Version (0x0001)                                │
│ Byte 6      : Channel Count (8 channels)                               │
│ Byte 7      : Flags (bit 0: valid, bit 1: trigger_fired, bit 2: ext)  │
│ Byte 8..11  : Sample Rate in Hz (e.g. 10,000,000)                      │
│ Byte 12..15 : Total Samples (e.g. 8,192 up to 65,536)                  │
│ Byte 16..19 : Trigger Offset (Index where trigger condition met)       │
│ Byte 20..23 : Pre-Trigger Sample Count                                 │
│ Byte 24..27 : Post-Trigger Sample Count                                │
│ Byte 28..31 : Monotonic Capture ID                                     │
├────────────────────────────────────────────────────────────────────────┤
│                      Raw Packed Sample Data                            │
├────────────────────────────────────────────────────────────────────────┤
│ Sample 0    : Bit 0 = CH0 (GP2) ... Bit 7 = CH7 (GP9)                  │
│ Sample 1    : Bit 0 = CH0 (GP2) ... Bit 7 = CH7 (GP9)                  │
│ ...         : ...                                                      │
│ Sample N-1  : Bit 0 = CH0 (GP2) ... Bit 7 = CH7 (GP9)                  │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Header Structure Definition (C/C++)

```cpp
#pragma pack(push, 1)
struct WebLogicCaptureHeader {
    uint32_t magic;             // 0x53434F50 ('SCOP' in ASCII, little-endian)
    uint16_t version;           // 0x0001
    uint8_t  channel_count;     // Always 8 (GP2 through GP9)
    uint8_t  flags;             // bit 0: valid, bit 1: trigger_fired, bit 2: ext_trig
    uint32_t sample_rate_hz;    // Sample rate in Hz (e.g., 10000000)
    uint32_t total_samples;     // Number of sample bytes following header
    uint32_t trigger_offset;    // Index of the trigger event in the buffer
    uint32_t pre_samples;       // Number of pre-trigger samples
    uint32_t post_samples;      // Number of post-trigger samples
    uint32_t capture_id;        // Monotonic capture counter
};
#pragma pack(pop)
```

### Field Descriptions

- **`magic` (4 bytes)**: Fixed magic identifier `0x53434F50` (ASCII `'SCOP'`). In byte order on wire: `0x50, 0x4F, 0x43, 0x53`.
- **`version` (2 bytes)**: Protocol format version. Current version: `1` (`0x0001`).
- **`channel_count` (1 byte)**: Number of digital channels packed into each sample byte. Set to `8`.
- **`flags` (1 byte)**: Bitmask indicating capture conditions:
  - Bit 0 (`0x01`): Capture buffer is valid.
  - Bit 1 (`0x02`): Trigger condition successfully detected by hardware.
  - Bit 2 (`0x04`): External trigger input (`GP10`) was utilized.
- **`sample_rate_hz` (4 bytes)**: Precise sampling frequency in Hertz.
- **`total_samples` (4 bytes)**: Length of the raw sample array in bytes (typically 1,024 to 65,536).
- **`trigger_offset` (4 bytes)**: Zero-indexed position in the buffer where the trigger condition occurred.
- **`pre_samples` (4 bytes)**: Number of valid samples recorded prior to trigger assertion.
- **`post_samples` (4 bytes)**: Number of valid samples recorded after trigger assertion.
- **`capture_id` (4 bytes)**: Incremental 32-bit counter incremented with each successful acquisition.

---

## 3. Sample Byte Bit Packing

Each byte in the payload corresponds to a single discrete time sample across all 8 logic channels:

| Bit Position | Channel | RP2040 Pin | Function |
|---|---|---|---|
| **Bit 0** (LSB) | `CH0` | `GP2` | Logic Channel 0 |
| **Bit 1** | `CH1` | `GP3` | Logic Channel 1 |
| **Bit 2** | `CH2` | `GP4` | Logic Channel 2 |
| **Bit 3** | `CH3` | `GP5` | Logic Channel 3 |
| **Bit 4** | `CH4` | `GP6` | Logic Channel 4 |
| **Bit 5** | `CH5` | `GP7` | Logic Channel 5 |
| **Bit 6** | `CH6` | `GP8` | Logic Channel 6 |
| **Bit 7** (MSB) | `CH7` | `GP9` | Logic Channel 7 |

To extract the state of channel $C$ (where $0 \le C \le 7$) at sample index $i$:
$$\text{state}(C, i) = \left( \text{sample}[i] \gg C \right) \ \& \ 1$$

---

## 4. Client-Side JavaScript Unpacking Example

```javascript
function parseCaptureBuffer(arrayBuffer) {
  const dataView = new DataView(arrayBuffer);

  // 1. Check Magic Identifier ('SCOP' = 0x53434F50)
  if (arrayBuffer.byteLength >= 32 && dataView.getUint32(0, true) === 0x53434F50) {
    const header = {
      version:       dataView.getUint16(4, true),
      channelCount:  dataView.getUint8(6),
      flags:         dataView.getUint8(7),
      sampleRateHz:  dataView.getUint32(8, true),
      totalSamples:  dataView.getUint32(12, true),
      triggerOffset: dataView.getUint32(16, true),
      preSamples:    dataView.getUint32(20, true),
      postSamples:   dataView.getUint32(24, true),
      captureId:     dataView.getUint32(28, true)
    };

    // 2. Extract sample data typed array (skipping 32-byte header)
    const samples = new Uint8Array(arrayBuffer, 32, header.totalSamples);
    return { header, samples };
  } else {
    // Fallback: Legacy unframed raw sample stream
    return {
      header: {
        sampleRateHz: 10000000,
        totalSamples: arrayBuffer.byteLength,
        triggerOffset: Math.floor(arrayBuffer.byteLength * 0.7)
      },
      samples: new Uint8Array(arrayBuffer)
    };
  }
}
```

---

## 5. Timing Reconstruction

For any sample at index $i$ ($0 \le i < \text{total\_samples}$):
- Sample period:
  $$\Delta t_{\text{sample}} = \frac{1}{\text{sample\_rate\_hz}} \quad \text{[seconds]}$$
- Time offset relative to trigger $T_0$:
  $$t(i) = (i - \text{trigger\_offset}) \times \Delta t_{\text{sample}}$$
- Time interval between two samples $A$ and $B$:
  $$\Delta t = |B - A| \times \Delta t_{\text{sample}}$$
- Equivalent frequency for an observed full cycle $\Delta t$:
  $$f = \frac{1}{\Delta t}$$
