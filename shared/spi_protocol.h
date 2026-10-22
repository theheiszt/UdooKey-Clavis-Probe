#ifndef SPI_PROTOCOL_H
#define SPI_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define SPI_PROTO_MAGIC       0x5350 // 'S', 'P' (SPI Pirate)
#define SPI_DEFAULT_FREQ_HZ   10000000 // 10 MHz SPI Clock
#define SPI_BLOCK_SIZE        1024   // 1024 samples per block

// ─── Transport Architecture Definitions ──────────────────────────────────────
#define FACTORY_MCU_TRANSPORT            "Native Factory UART (GP0/GP1 <-> GPIO22/GPIO19)"
#define OPTIONAL_HIGH_SPEED_MCU_TRANSPORT "Optional Jumper-Wired 10 MHz SPI (GP2..GP5 <-> UEXT)"

// ─── Factory Native UART Transport Pin Mappings (Hardwired on PCB) ───────────
#ifndef RP2040_PIN_UART_TX
#define RP2040_PIN_UART_TX    0  // RP2040 GP0 TX -> ESP32 GPIO22 RX
#endif
#ifndef RP2040_PIN_UART_RX
#define RP2040_PIN_UART_RX    1  // RP2040 GP1 RX <- ESP32 GPIO19 TX
#endif
#ifndef ESP32_PIN_UART_RX
#define ESP32_PIN_UART_RX     22 // ESP32 GPIO22 RX <- RP2040 GP0 TX
#endif
#ifndef ESP32_PIN_UART_TX
#define ESP32_PIN_UART_TX     19 // ESP32 GPIO19 TX -> RP2040 GP1 RX
#endif

// ─── Hardware RP2040 Reset Control from ESP32 (Hardwired on PCB) ─────────────
#ifndef ESP32_PIN_RPI_RESET
#define ESP32_PIN_RPI_RESET   23 // LOW = RP2040 reset/disabled, HIGH = RP2040 running
#endif

// ─── Hardware Inter-MCU SWD Link (Hardwired on PCB, reserved for debug) ──────
#ifndef ESP32_PIN_SWDIO
#define ESP32_PIN_SWDIO       2  // ESP32 IO2  <-> RP2040 SWDIO
#endif
#ifndef ESP32_PIN_SWCLK
#define ESP32_PIN_SWCLK       4  // ESP32 IO4  <-> RP2040 SWCLK
#endif
#ifndef ESP32_PIN_SWD_OE
#define ESP32_PIN_SWD_OE      5  // ESP32 IO5  <-> External SWD header routing/access
#endif

// ─── Optional Jumper-Wired 10 MHz SPI Pin Mappings (Probe Bus <-> ESP32 UEXT) ─
// Wiring:
//   ESP32 UEXT GPIO14 (SCK)  <--> RP2040 GP2 (SCK)
//   ESP32 UEXT GPIO12 (MOSI) <--> RP2040 GP3 (MOSI)
//   ESP32 UEXT GPIO35 (MISO) <--> RP2040 GP4 (MISO)
//   ESP32 UEXT GPIO15 (CS#)  <--> RP2040 GP5 (CS#)
//   GND                      <--> GND
#ifndef RP2040_PIN_SPI_SCK
#define RP2040_PIN_SPI_SCK    2  // GP2: Inter-chip SPI SCK (Input)
#endif
#ifndef RP2040_PIN_SPI_MOSI
#define RP2040_PIN_SPI_MOSI   3  // GP3: Inter-chip SPI MOSI (Input)
#endif
#ifndef RP2040_PIN_SPI_MISO
#define RP2040_PIN_SPI_MISO   4  // GP4: Inter-chip SPI MISO (Output)
#endif
#ifndef RP2040_PIN_SPI_CS
#define RP2040_PIN_SPI_CS     5  // GP5: Inter-chip SPI CS# (Input)
#endif
#ifndef RP2040_PIN_DATA_READY
#define RP2040_PIN_DATA_READY 255 // Disabled by default to protect GP18..GP22 (Display/Touch subsystem)
#endif

#ifndef ESP32_PIN_SPI_SCK
#define ESP32_PIN_SPI_SCK     14 // ESP32 UEXT Pin: SPI SCK
#endif
#ifndef ESP32_PIN_SPI_MOSI
#define ESP32_PIN_SPI_MOSI    12 // ESP32 UEXT Pin: SPI MOSI
#endif
#ifndef ESP32_PIN_SPI_MISO
#define ESP32_PIN_SPI_MISO    35 // ESP32 UEXT Pin: SPI MISO (Input only)
#endif
#ifndef ESP32_PIN_SPI_CS
#define ESP32_PIN_SPI_CS      15 // ESP32 UEXT Pin: SPI CS#
#endif
#ifndef ESP32_PIN_DATA_READY
#define ESP32_PIN_DATA_READY  255 // Disabled by default (in-band status flag used)
#endif

// ─── Packet Types ────────────────────────────────────────────────────────────
enum SpiPacketType : uint8_t {
    SPI_PKT_NONE             = 0x00,
    SPI_PKT_PING             = 0x01, // ESP32 -> RP2040: Ping check
    SPI_PKT_PONG             = 0x02, // RP2040 -> ESP32: Pong reply
    SPI_PKT_GET_STATUS       = 0x03, // ESP32 -> RP2040: Query status
    SPI_PKT_STATUS_RESP      = 0x04, // RP2040 -> ESP32: Status response
    SPI_PKT_GET_CAPTURE_INFO = 0x05, // ESP32 -> RP2040: Query capture parameters
    SPI_PKT_CAPTURE_INFO     = 0x06, // RP2040 -> ESP32: Capture metadata
    SPI_PKT_GET_BLOCK        = 0x07, // ESP32 -> RP2040: Request specific block (sequence #)
    SPI_PKT_CAPTURE_BLOCK    = 0x08, // RP2040 -> ESP32: Block payload with CRC
    SPI_PKT_CMD_EXEC         = 0x09, // ESP32 -> RP2040: Execute CLI / Bus Pirate command
    SPI_PKT_CMD_RESP         = 0x0A, // RP2040 -> ESP32: Command execution output
    SPI_PKT_LA_START         = 0x0B, // ESP32 -> RP2040: Start logic capture
    SPI_PKT_LA_STOP          = 0x0C, // ESP32 -> RP2040: Stop logic capture
    SPI_PKT_CAPTURE_ACK      = 0x0D, // ESP32 -> RP2040: Capture received & verified OK
};

// ─── Packet Flags ────────────────────────────────────────────────────────────
enum SpiPacketFlags : uint8_t {
    SPI_FLAG_NONE          = 0x00,
    SPI_FLAG_LAST_BLOCK    = 0x01, // Final block of multi-block capture
    SPI_FLAG_CAPTURE_READY = 0x02, // Fresh capture waiting in SRAM
    SPI_FLAG_BUSY          = 0x04, // Acquisition currently running
    SPI_FLAG_ERROR         = 0x08  // Requested block / command invalid
};

// ─── Packed Wire Headers ─────────────────────────────────────────────────────
#pragma pack(push, 1)

struct SpiPacketHeader {
    uint16_t magic;          // 0x5350
    uint8_t  type;           // SpiPacketType
    uint8_t  flags;          // SpiPacketFlags bitmask
    uint16_t sequence;       // Block sequence (0, 1, 2...)
    uint16_t total_blocks;   // Total blocks in capture (e.g. 8)
    uint16_t payload_len;    // Payload length in bytes (e.g. 1024)
    uint32_t capture_id;     // Generation counter (prevents stale / race reads)
    uint32_t sample_index;   // Starting sample index of this block
    uint32_t sample_rate;    // Sample rate in Hz
    uint16_t trigger_offset; // Trigger marker index in total capture
    uint16_t block_crc;      // Level 1: CRC-16 of generation + sequence + block payload
};

struct SpiCaptureInfoPayload {
    uint32_t capture_id;        // Monotonic generation number
    uint32_t total_samples;     // Total samples (e.g. 8192, 16384, 65536)
    uint32_t sample_rate_hz;    // Sample rate in Hz
    uint16_t block_size;        // Fixed block size (1024)
    uint16_t total_blocks;      // Total blocks
    uint32_t trigger_offset;    // Trigger marker index in buffer
    uint32_t trigger_sample;    // Sample value at trigger match
    uint16_t whole_capture_crc; // Level 2: CRC-16 across entire assembled capture
    uint8_t  trigger_channel;   // Trigger channel (0-7 or 10=EXT)
    uint8_t  trigger_type;      // Trigger mode enum
    uint8_t  capture_ready;     // 1 if capture is available in SRAM
    uint8_t  active_mode;       // Active Bus Pirate mode
};

struct SpiStatusPayload {
    uint32_t capture_id;
    uint8_t  capture_running;
    uint8_t  capture_ready;
    uint8_t  active_mode;
    uint8_t  trigger_state;
    uint32_t sample_rate_hz;
    uint32_t sample_count;
};

#pragma pack(pop)

// ─── Capture Descriptor (In-Memory Interface between Acquisition & Transport) ──
struct CaptureDescriptor {
    uint32_t       capture_id;        // Monotonic generation counter
    const uint8_t* buffer;            // Aligned SRAM acquisition buffer
    uint32_t       total_samples;     // Total samples
    uint32_t       sample_rate_hz;    // Hardware sample rate
    uint32_t       trigger_offset;    // Trigger position in buffer
    uint32_t       trigger_sample;    // Raw GPIO sample value at trigger point
    uint16_t       total_blocks;      // Number of 1024-byte blocks
    uint16_t       whole_capture_crc; // Level 2 whole-capture checksum
    uint8_t        trigger_channel;   // Hardware channel that tripped trigger
    uint8_t        trigger_type;      // Edge, Qual, Runt, Pattern, etc.
    bool           valid;             // Descriptor validity flag

    // Hardware Latency Profiler Timestamps (microseconds)
    uint32_t       t_trigger_us;      // Timestamp when trigger condition met
    uint32_t       t_dma_freeze_us;   // Timestamp when ring DMA was halted
    uint32_t       t_desc_ready_us;   // Timestamp when descriptor & CRCs finalized
    uint32_t       t_data_ready_us;   // Timestamp when DATA_READY line asserted
};

// ─── Level 1 CRC-16: Enforces Header + Payload Integrity ──────────────────────
// Includes capture_id, sequence, and payload into the CRC calculation so that
// any stale-block or sequence-swap error is mathematically rejected!
static inline uint16_t spi_calc_block_crc16(uint32_t capture_id, uint16_t sequence, const uint8_t* data, size_t len) {
    uint8_t meta[6];
    meta[0] = (uint8_t)(capture_id >> 0);
    meta[1] = (uint8_t)(capture_id >> 8);
    meta[2] = (uint8_t)(capture_id >> 16);
    meta[3] = (uint8_t)(capture_id >> 24);
    meta[4] = (uint8_t)(sequence >> 0);
    meta[5] = (uint8_t)(sequence >> 8);

    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < 6; i++) {
        crc ^= (uint16_t)meta[i] << 8;
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

// ─── Level 2 CRC-16: Whole-Capture & General Checksum ─────────────────────────
static inline uint16_t spi_calc_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

#endif // SPI_PROTOCOL_H
