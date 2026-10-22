# UDOO KEY — Communications & Protocol Inventory Specification

**Document ID:** DOC-PROTO-INV-20261003  
**Status:** SPECIFICATION & REFERENCE (PHASE 1)  
**Date:** 2026-10-03  
**Target Hardware:** SECO UDOO KEY (RP2040 Dual Cortex-M0+ @ 133 MHz, REV O)  
**Firmware Version:** UDOO-KEY-OS v2.1-bp-terminal (UF2 Size: 522,752 bytes)  

---

## 1. Executive Summary

The SECO UDOO KEY provides multi-modal communications across its single USB CDC virtual COM port (enumerating via native USB 1.1 on `CN1` with jumper `JP1` closed). To ensure that the native C++ desktop application communicates reliably with the device without protocol collision, lockup, or framing errors, this document provides the complete, authoritative specification of all three protocols implemented in the firmware:

1. **UDOO Binary Logic Protocol** (`0x5A` Frame Protocol)
2. **SUMP / OpenBench Logic Sniffer Protocol** (Sigrok / PulseView standard)
3. **UDOO Universal Terminal / Bus Pirate 5 ASCII Protocol** (Interactive bench engine)

---

## 2. Protocol 1 — UDOO Binary Logic Protocol (`0x5A`)

This protocol provides low-overhead, deterministic binary control and high-speed sample streaming between the PC and the RP2040 PIO/DMA acquisition engine.

### 2.1 Packet Frame Structure

All frames begin with a 4-byte header:

```text
Byte 0        Byte 1        Byte 2 (LSB)  Byte 3 (MSB)   Byte 4 .. (4 + Length - 1)
┌───────────┬─────────────┬───────────────────────────┬───────────────────────────┐
│ Magic     │ Opcode      │ Length (16-bit unsigned)  │ Payload (Optional)        │
│ 0x5A      │ (Command ID)│ (Bytes in payload)        │                           │
└───────────┴─────────────┴───────────────────────────┴───────────────────────────┘
```

* **Magic Byte:** Strictly `0x5A` (ASCII `'Z'`).
* **Opcode:** 1-byte command identifier (`0x01` .. `0x05`).
* **Length:** 16-bit little-endian integer specifying the number of bytes in the payload immediately following the header.

### 2.2 Command Set & Data Formats

| Command Name | Opcode | Direction | Payload Length | Payload Format | Description |
| :--- | :---: | :---: | :---: | :--- | :--- |
| **`CMD_GET_INFO`** | `0x01` | Host $\rightarrow$ Device | 0 bytes | None | Requests device identification string. |
| **`CMD_GET_INFO_RESP`**| `0x01` | Device $\rightarrow$ Host | Variable | `0x5A 0x01` + ASCII string + `\r\n` | Responds with `"UDOO-KEY-LOGIC-ANALYZER-V1.0\r\n"`. |
| **`CMD_SET_CONFIG`** | `0x02` | Host $\rightarrow$ Device | 10 bytes | `<IIBB` (Little-Endian) | Configures sample rate, depth, and trigger parameters. |
| **`CMD_START_CAPTURE`**| `0x03` | Host $\rightarrow$ Device | 0 bytes | None | Arms PIO0 SM0 DMA acquisition engine. |
| **`CMD_STOP_CAPTURE`** | `0x04` | Host $\rightarrow$ Device | 0 bytes | None | Aborts running acquisition and halts DMA. |
| **`CMD_STREAM_DATA`** | `0x05` | Device $\rightarrow$ Host | $8 + N$ bytes | `<II` + $N$ raw bytes | Emits captured sample buffer and metadata upon trigger completion. |

#### `CMD_SET_CONFIG` Payload Definition (10 Bytes Little-Endian):
```c
struct SerialConfigPayload {
    uint32_t sample_rate;    // Configured sample rate in Hz (10,000 to 100,000,000)
    uint32_t sample_count;   // Capture depth in samples (1,024 to 65,536)
    uint8_t  trigger_channel;// Target trigger channel (0 to 7)
    uint8_t  trigger_mode;   // 0 = None/Auto, 1 = Rising Edge, 2 = Falling Edge
};
```

#### `CMD_STREAM_DATA` Frame Definition:
When acquisition finishes, the firmware transmits:
```text
[0x5A] [0x05] [0x04, 0x00]                      <-- Header: Magic, Opcode, Length=4
[uint32_le Length]                               <-- Number of sample bytes (N, up to 65536)
[uint32_le SampleRate]                           <-- Validated hardware clock rate in Hz
[Byte 0, Byte 1, ... Byte N-1]                   <-- Raw 8-channel packed sample bytes
```
* **Sample Format**: Each byte represents one discrete time slice across `GP2..GP9`.
  - Bit 0 = `CH0` (`GP2`)
  - Bit 1 = `CH1` (`GP3`)
  - Bit 2 = `CH2` (`GP4`)
  - Bit 3 = `CH3` (`GP5`)
  - Bit 4 = `CH4` (`GP6`)
  - Bit 5 = `CH5` (`GP7`)
  - Bit 6 = `CH6` (`GP8`)
  - Bit 7 = `CH7` (`GP9`)

---

## 3. Protocol 2 — SUMP / OpenBench Logic Sniffer Protocol

The firmware implements native SUMP Rev 2 compliance, allowing direct interoperability with external software (Sigrok, PulseView) and the native C++ application.

### 3.1 Short Commands (Single-Byte Opcodes)

| Opcode | Command Name | Description | Firmware Response |
| :---: | :--- | :--- | :--- |
| `0x00` | **`SUMP_CMD_RESET`** | Resets protocol state machine and halts active captures. | None |
| `0x01` | **`SUMP_CMD_ARM`** | Arms logic analyzer for acquisition. | Emits sample stream upon capture completion. |
| `0x02` | **`SUMP_CMD_ID`** | Queries hardware device identifier. | 4-byte string: `"1ALS"` (OpenBench Logic Sniffer). |
| `0x04` | **`SUMP_CMD_META`** | Queries device capabilities and metadata. | Variable-length Key-Value metadata block. |
| `0x05` | **`SUMP_CMD_FINISH`** | Finishes current acquisition. | None |
| `0x06` | **`SUMP_CMD_QUERY_INPUT`**| Queries current instantaneous state of 8 probe pins. | 4-byte packed bitmask of inputs. |

### 3.2 Extended Commands (5-Byte Sequences: `Opcode + 4 Data Bytes`)

| Opcode | Command Name | 4-Byte Payload Format | Description |
| :---: | :--- | :--- | :--- |
| `0x80` | **`SUMP_CMD_SET_SAMPLE_RATE`** | `divider[23:0], flags[7:0]` | Sets sampling rate clock divider: $\text{Rate} = \frac{100\,\text{MHz}}{\text{divider} + 1}$. |
| `0x81` | **`SUMP_CMD_SET_COUNTS`** | `read_delay[15:0], read_count[15:0]`| Sets pre-trigger delay count and total capture buffer depth ($4 \times (\text{read\_count} + 1)$). |
| `0x82` | **`SUMP_CMD_SET_FLAGS`** | `flags[31:0]` | Flags: Demux (bit 0), Filter (bit 1), Channel Group enable (bits 2..5). |
| `0xC0` | **`SUMP_CMD_SET_BTRG0_MASK`** | `mask[31:0]` | Trigger Stage 0 channel bitmask. |
| `0xC1` | **`SUMP_CMD_SET_BTRG0_VALUE`**| `value[31:0]` | Trigger Stage 0 expected channel match value. |
| `0xC2` | **`SUMP_CMD_SET_BTRG0_CONFIG`**| `config[31:0]` | Trigger Stage 0 delay, level vs edge, start/stop selection. |

### 3.3 SUMP Metadata Response Structure (`SUMP_CMD_META`)

When receiving `0x04`, the UDOO KEY emits:
```text
0x01, 'U', 'D', 'O', 'O', ' ', 'K', 'E', 'Y', ' ', 'L', 'o', 'g', 'i', 'c', ' ', 'A', 'n', 'a', 'l', 'y', 'z', 'e', 'r', 0x00,
0x21, 0x00, 0x01, 0x00, 0x00,   // Sample RAM: 65,536 Bytes (64 KiB ring buffer)
0x23, 0x00, 0x00, 0xE1, 0x05,   // Maximum Sample Rate: 100,000,000 Hz (100 MSPS)
0x40, 0x08,                     // Number of Probes: 8 Channels (GP2..GP9)
0x41, 0x02,                     // Protocol Version: 2
0x00                            // SUMP_META_END terminator
```

---

## 4. Protocol 3 — UDOO Universal Terminal / Bus Pirate 5 ASCII Protocol

The UDOO Universal Terminal operates as an interactive human- and machine-readable text shell with VT100 / ANSI escape sequence support and raw ASCII fallback.

### 4.1 Prompt Formats & Operating Modes

| Mode | Prompt | Primary Function | Active Hardware Allocation |
| :--- | :--- | :--- | :--- |
| **HiZ** | `HiZ> ` | Safe boot state; all probe pins floating. | Uncommitted inputs |
| **LOGIC** | `LOGIC> ` | 8-Channel 100MSPS Logic Analyzer. | PIO0 SM0 DMA Circular Engine (`GP2..GP9`) |
| **METER** | `METER> ` | Multichannel Voltmeter & Continuity. | 12-bit SAR ADC (`GP26..GP28`), PWM Buzzer (`GP20`) |
| **I2C** | `I2C> ` | I2C Master Scanner & Protocol Tools. | Hardware I2C1 (`GP2=SDA, GP3=SCL`) |
| **SPI** | `SPI> ` | SPI Master & Flash Interrogator. | Hardware SPI0 (`GP2=SCK, GP3=MOSI, GP4=MISO, GP5=CS`) |
| **UART** | `UART> ` | Serial Bridge & Monitor. | Hardware UART1 (`GP4=TX, GP5=RX`) |
| **1WIRE** | `1WIRE> ` | Dallas 1-Wire Search & Read. | Open-drain master on `GP6:DQ` |
| **DIO** | `DIO> ` | Direct Digital I/O Control. | Direct SIO control across `GP2..GP9` |
| **USB** | `USB> ` | USB Physical Probing & Sniffer. | High-Z probe inputs on `GP2(D+)` and `GP3(D-)` |
| **SENSOR**| `SENSOR> `| Universal Multi-Bus Sensor Suite. | Multi-bus discovery across I2C, SPI, 1-Wire, ADC |

### 4.2 Key Machine-Parseable ASCII Telemetry Outputs

#### Voltmeter Table (`v` in Meter Mode):
```text
+---------+------+-----------------+-----------+----------+-----------+
| Channel | GPIO | Connector       | Voltage   | Millivolt| Raw SAR   |
+---------+------+-----------------+-----------+----------+-----------+
| ADC0    | GP26 | Header P2.31    |  0.0012 V |    1.2 mV|    1 /4095|
| ADC1    | GP27 | Header P2.32    |  3.2988 V | 3298.8 mV| 4094 /4095|
| ADC2    | GP28 | Header P2.34    |  1.6504 V | 1650.4 mV| 2048 /4095|
| VSYS    | GP29 | Internal Rail   |  4.9821 V | 4982.1 mV| (Scaled)  |
| TEMP    | MUX4 | RP2040 Die      |  27.42 C  |   81.4 F | (Thermal) |
+---------+------+-----------------+-----------+----------+-----------+
```

#### I2C Scanner Table (`scan` in I2C or Sensor Mode):
```text
+------+----------------+----------------------+---------------------------+----------+
| Addr | Identified Model| Manufacturer         | Primary Sensor Category   | Signature|
+------+----------------+----------------------+---------------------------+----------+
| 0x38 | AHT10 / AHT20  | Aosong Electronics   | Temperature & Humidity    | ACK      |
| 0x68 | MPU-6050       | InvenSense / TDK     | 6-Axis Motion / IMU       | ID: 0x68 |
| 0x76 | BMP280         | Bosch Sensortec      | Pressure & Temperature    | ID: 0x58 |
+------+----------------+----------------------+---------------------------+----------+
```

#### SPI JEDEC Probe (`probe` in SPI Mode):
```text
Raw JEDEC ID Response: 0xEF 0x40 0x18
  Manufacturer:   Winbond Electronics (0xEF)
  Memory Type:    0x40
  Capacity:       16 MB (128 Mbit)
```

---

## 5. Protocol Disambiguation & Stream Demultiplexing Rules

Because the UDOO KEY utilizes a single USB CDC endpoint for CLI interaction, SUMP logic capture, and binary streaming, the PC application must implement deterministic stream classification:

```text
Incoming Serial Byte Stream
             │
             ├── Byte == 0x5A AND peek(Command) in {0x01..0x05} ──► Binary Deframer
             │                                                         │
             │                                                         ▼
             │                                                 Emit Logic Frame
             │
             ├── Mode == SUMP AND Byte in {0x00..0x06, 0x80..0xCE} ──► SUMP Engine
             │                                                         │
             │                                                         ▼
             │                                                 Emit SUMP Buffer
             │
             └── All other bytes ────────────────────────────────────► Terminal Stream
                                                                       │
                                                                       ▼
                                                               Emit ASCII / VT100
```

1. **Explicit Mode Handshake**: When the desktop application connects, it queries `CMD_GET_INFO` or issues a synchronized terminal command (`config vt100 off`).
2. **Deframing State Machine**: The binary deframer validates the 4-byte header length. If a packet timeout occurs ($> 500\,\text{ms}$) or an unexpected byte arrives mid-packet, the parser resets and drops the erroneous bytes into the ASCII terminal stream without corrupting future logic captures.
3. **Safe Reconnection**: The C++ application provides a `Reset Protocol` signal that transmits `SUMP_CMD_RESET` (`0x00`) followed by `\r\nhiz\r\n` to guarantee the firmware returns to the uncommitted `HiZ> ` baseline.
