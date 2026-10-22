# UDOO KEY — Binary Debug Protocol Specification

## 1. Protocol Overview

The UDOO KEY Binary Debug Protocol provides high-speed, packetized, batch-oriented communication between the PC desktop companion app (`udoo-key-app`) and the UDOO KEY firmware (`rp2040_logic_analyzer`).

All debug packets utilize a dedicated sync byte `0x5B` (`[` in ASCII) to coexist cleanly with SUMP (`1ALS`), binary logic packets (`0x5A`), and ASCII terminal interactions.

---

## 2. Packet Wire Framing

Each packet has a minimum length of 8 bytes:

```text
 0               1               2               3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  SYNC (0x5B)  |    COMMAND    |    SEQ_NUM    |    STATUS     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|        LEN_MSB (Payload)      |        LEN_LSB (Payload)      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     PAYLOAD DATA (N Bytes) ...                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|            CRC16_MSB          |            CRC16_LSB          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Field Definitions
- **SYNC (1 Byte)**: Always `0x5B`.
- **COMMAND (1 Byte)**: Opcode indicating the debug command. Responses have bit 7 set (`cmd | 0x80`).
- **SEQ_NUM (1 Byte)**: Transaction sequence counter (0..255) for matching asynchronous replies.
- **STATUS (1 Byte)**:
  - `0x00`: OK / Success
  - `0x01`: ERR_TIMEOUT
  - `0x02`: ERR_INVALID_PARAM
  - `0x03`: ERR_CRC_MISMATCH
  - `0x04`: ERR_TARGET_FAULT
  - `0x05`: ERR_SWD_WAIT
  - `0x06`: ERR_PARITY
  - `0x07`: ERR_UNSUPPORTED
  - `0x08`: ERR_NOT_CONNECTED
  - `0x09`: ERR_FRAMING
- **LENGTH (2 Bytes, Big-Endian)**: Length of following payload in bytes.
- **PAYLOAD (N Bytes)**: Command-specific arguments or response data.
- **CRC-16 (2 Bytes, Big-Endian)**: CRC-16-CCITT computed over bytes `[CMD ... end of PAYLOAD]`.
  - Polynomial: `0x1021`
  - Initial Value: `0xFFFF`

---

## 3. Command Set

### 3.1 System Commands
| Opcode | Name | Request Payload | Response Payload |
| :--- | :--- | :--- | :--- |
| `0x01` | `GET_STATUS` | None | `[Status: 1B] [TapState: 1B]` |
| `0x02` | `SET_TARGET_POWER`| `[PowerState: 1B]` | None |
| `0x03` | `RESET_TARGET` | `[AssertTimeMs: 2B]` | None |

### 3.2 JTAG Commands
| Opcode | Name | Request Payload | Response Payload |
| :--- | :--- | :--- | :--- |
| `0x10` | `JTAG_RESET` | None | `[CurrentTapState: 1B]` |
| `0x11` | `JTAG_SET_CLOCK` | `[FreqHz: 4B Big-Endian]` | None |
| `0x12` | `JTAG_TMS_SEQ` | `[BitCount: 1B] [TMS Bytes]` | `[FinalTapState: 1B]` |
| `0x15` | `JTAG_TRANSACTION` | `[ScanType: 1B] [Bits: 2B] [ExitState: 1B] [TDI Bytes]` | `[FinalState: 1B] [SampledBits: 2B] [TDO Bytes]` |
| `0x16` | `JTAG_SCAN_CHAIN` | None | `[DeviceCount: 1B] {IDCODE (4B LE), IRLen (1B)} x Count` |

### 3.3 SWD Commands
| Opcode | Name | Request Payload | Response Payload |
| :--- | :--- | :--- | :--- |
| `0x20` | `SWD_RESET` | None | None |
| `0x21` | `SWD_CONNECT` | None | `[DPIDR: 4B Little-Endian]` |
| `0x22` | `SWD_SET_CLOCK` | `[FreqHz: 4B Big-Endian]` | None |
| `0x23` | `SWD_READ_DP` | `[Addr A[3:2]: 1B]` | `[Data: 4B LE] [Ack: 1B]` |
| `0x24` | `SWD_WRITE_DP`| `[Addr A[3:2]: 1B] [Data: 4B LE]` | `[Ack: 1B]` |
| `0x25` | `SWD_READ_AP` | `[Addr A[7:2]: 1B]` | `[Data: 4B LE] [Ack: 1B]` |
| `0x26` | `SWD_WRITE_AP`| `[Addr A[7:2]: 1B] [Data: 4B LE]` | `[Ack: 1B]` |
| `0x27` | `SWD_MEM_READ32`| `[Address: 4B LE]` | `[Word: 4B LE] [Ack: 1B]` |
| `0x28` | `SWD_MEM_WRITE32`| `[Address: 4B LE] [Word: 4B LE]` | `[Ack: 1B]` |
