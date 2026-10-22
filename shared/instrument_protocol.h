#ifndef INSTRUMENT_PROTOCOL_H
#define INSTRUMENT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/*
 * ==============================================================================
 * UDOO KEY Dual-MCU Diagnostic Instrument — Transport-Neutral Packet Protocol
 * ==============================================================================
 *
 * Implements Section 19 of the Master Specification:
 *   - Clearly versioned protocol
 *   - Transport-neutral framing: magic, version, type, sequence, length, payload, CRC
 *   - Complete support for PING, STATUS, CONFIG, CAPTURE, TRIGGER, DATA, ADC,
 *     MEASUREMENTS, CLI, and ERROR handling.
 *   - Operates over the 1 Mbps Native Factory UART link (and optional transports).
 * ==============================================================================
 */

#define INSTRUMENT_PROTO_MAGIC       0xAA55
#define INSTRUMENT_PROTO_VERSION     0x01
#define INSTRUMENT_CHUNK_SIZE        512   // Chunk size for 64 KiB buffer streaming

// ─── Packet Types ────────────────────────────────────────────────────────────
enum InstrumentPacketType : uint8_t {
    // Basic Liveness & Status
    PKT_TYPE_PING                = 0x01, // ESP32 -> RP2040: Keepalive / Ping
    PKT_TYPE_PONG                = 0x02, // RP2040 -> ESP32: Pong response
    PKT_TYPE_GET_STATUS          = 0x03, // ESP32 -> RP2040: Query state
    PKT_TYPE_STATUS_RESP         = 0x04, // RP2040 -> ESP32: Status payload

    // Configuration & Control
    PKT_TYPE_SET_CONFIG          = 0x05, // ESP32 -> RP2040: Set acquisition config
    PKT_TYPE_CONFIG_ACK          = 0x06, // RP2040 -> ESP32: Config applied successfully
    PKT_TYPE_START_CAPTURE       = 0x07, // ESP32 -> RP2040: Arm/start acquisition
    PKT_TYPE_STOP_CAPTURE        = 0x08, // ESP32 -> RP2040: Abort acquisition
    PKT_TYPE_TRIGGER_EVENT       = 0x09, // RP2040 -> ESP32: Asynchronous trigger notification

    // Capture Metadata & Buffer Transfer
    PKT_TYPE_GET_CAPTURE_INFO    = 0x0A, // ESP32 -> RP2040: Request capture info
    PKT_TYPE_CAPTURE_INFO_RESP   = 0x0B, // RP2040 -> ESP32: Capture metadata & CRC
    PKT_TYPE_GET_CAPTURE_DATA    = 0x0C, // ESP32 -> RP2040: Request specific chunk
    PKT_TYPE_CAPTURE_DATA_CHUNK  = 0x0D, // RP2040 -> ESP32: 512-byte payload chunk

    // Hardware Measurements & ADC
    PKT_TYPE_GET_MEASUREMENT     = 0x0E, // ESP32 -> RP2040: Request frequency/period
    PKT_TYPE_MEASUREMENT_RESP    = 0x0F, // RP2040 -> ESP32: Measurement results
    PKT_TYPE_GET_ADC             = 0x10, // ESP32 -> RP2040: Read 3 ADC voltages
    PKT_TYPE_ADC_RESP            = 0x11, // RP2040 -> ESP32: ADC results in mV

    // Interactive CLI Commands
    PKT_TYPE_CLI_CMD             = 0x12, // ESP32 -> RP2040: Execute text command
    PKT_TYPE_CLI_RESP            = 0x13, // RP2040 -> ESP32: Text output

    // Diagnostics & Error Handling
    PKT_TYPE_SELF_TEST_REQ       = 0x20, // Request hardware diagnostic suite
    PKT_TYPE_SELF_TEST_RESP      = 0x21, // Diagnostic test results
    
    // Advanced Instrument Capabilities & Coprocessor Telemetry
    PKT_TYPE_GET_CAPABILITIES    = 0x22, // Query instrument capabilities
    PKT_TYPE_CAPABILITIES_RESP   = 0x23, // Instrument capabilities response
    PKT_TYPE_TIME_SYNC_REQ       = 0x24, // High-precision time sync request
    PKT_TYPE_TIME_SYNC_RESP      = 0x25, // High-precision time sync response
    PKT_TYPE_MARK_EVENT          = 0x26, // Ingest host-side software marker
    PKT_TYPE_EVENT_STREAM        = 0x27, // Stream structured instrument event
    PKT_TYPE_SET_PINMUX          = 0x28, // Dynamic pin function assignment
    PKT_TYPE_PINMUX_RESP         = 0x29, // Pin assignment status / conflict
    PKT_TYPE_RADIO_EVENT         = 0x2A, // ESP32 BLE / Wi-Fi diagnostic event
    PKT_TYPE_RADIO_CMD           = 0x2B, // Radio command (scan, probe, filter)
    PKT_TYPE_RADIO_WIFI_DIAG     = 0x2C, // Wi-Fi diagnostic telemetry (SSID, DNS, RTT, loss)
    PKT_TYPE_WIFI_SNIFF_CONFIG   = 0x2D, // Host -> Dev: Wi-Fi promiscuous sniffer channel & filter
    PKT_TYPE_WIFI_SNIFF_STATUS   = 0x2E, // Dev -> Host: Wi-Fi sniffer active, channel, counts

    // Bus Pirate Infrared (IR) Engine (0x30 - 0x33)
    PKT_TYPE_IR_CONFIG           = 0x30, // Host -> Dev: Configure IR TX/RX pins, carrier frequency
    PKT_TYPE_IR_TX               = 0x31, // Host -> Dev: Transmit IR command or raw pulse timings
    PKT_TYPE_IR_RX_EVENT         = 0x32, // Dev -> Host: Captured IR packet or demodulated symbol
    PKT_TYPE_IR_RX_CONFIG        = 0x33, // Host -> Dev: Enable/disable IR receiver listening

    // Bus Pirate nRF24L01 2.4 GHz Radio Module (0x40 - 0x45)
    PKT_TYPE_NRF24_CONFIG        = 0x40, // Host -> Dev: Channel, datarate, RF power, address width
    PKT_TYPE_NRF24_TX            = 0x41, // Host -> Dev: Send packet to remote RF24 node
    PKT_TYPE_NRF24_TX_STATUS     = 0x42, // Dev -> Host: TX delivery status (ACK ok / max retries)
    PKT_TYPE_NRF24_RX_CONFIG     = 0x43, // Host -> Dev: Open RX pipe address and payload size
    PKT_TYPE_NRF24_RX_EVENT      = 0x44, // Dev -> Host: Received packet from pipe
    PKT_TYPE_NRF24_STATUS        = 0x45, // Both: Query or report radio state and FIFO flags

    // Bus Pirate Packet & PCAP Analysis Service (0x50 - 0x53)
    PKT_TYPE_PACKET_RECORD       = 0x50, // Dev -> Host: Structured PacketRecord (Wi-Fi/BLE/nRF24)
    PKT_TYPE_PCAP_DUMP_REQ       = 0x51, // Host -> Dev: Request PCAP export / stream

    // Bus Pirate Generic GPIO Service (0x60 - 0x67)
    PKT_TYPE_GPIO_CONFIG         = 0x60, // Host -> Dev: Configure pin direction, initial level, pull mode
    PKT_TYPE_GPIO_READ           = 0x61, // Host -> Dev: Read digital level of single pin
    PKT_TYPE_GPIO_WRITE          = 0x62, // Host -> Dev: Write digital level to output pin
    PKT_TYPE_GPIO_STATUS         = 0x63, // Dev -> Host: Single pin status response
    PKT_TYPE_GPIO_WRITE_MASK     = 0x64, // Host -> Dev: Set directions & values for all 8 probe pins
    PKT_TYPE_GPIO_READ_ALL       = 0x65, // Host -> Dev / Dev -> Host: Read all 8 probe pins simultaneously
    PKT_TYPE_MODE_SET            = 0x66, // Host -> Dev: Request mode change (InstrumentModeId + speed)
    PKT_TYPE_MODE_STATUS         = 0x67, // Both: Query or report active mode, speed, and pin mappings

    // Bus Pirate UART Bridge Service (0x68 - 0x6B)
    PKT_TYPE_UART_CONFIG         = 0x68, // Host -> Dev: Configure baud rate, data bits, parity, stop bits
    PKT_TYPE_UART_DATA           = 0x69, // Both: Bidirectional binary data stream
    PKT_TYPE_UART_STATUS         = 0x6A, // Both: Query/report UART state and buffered RX count

    // Bus Pirate I2C Engine (0x6C - 0x6F)
    PKT_TYPE_I2C_CONFIG          = 0x6C, // Host -> Dev: Configure I2C speed and internal pull-ups
    PKT_TYPE_I2C_SCAN_REQ        = 0x6D, // Host -> Dev: Request 7-bit bus address scan (0x08..0x77)
    PKT_TYPE_I2C_SCAN_RESP       = 0x6E, // Dev -> Host: Discovered I2C addresses
    PKT_TYPE_I2C_XFER            = 0x6F, // Both: Atomic Write-then-Read I2C transaction

    // Bus Pirate SPI Engine & Flash Probe (0x70 - 0x73)
    PKT_TYPE_SPI_CONFIG          = 0x70, // Host -> Dev: Configure SPI speed, mode (CPOL/CPHA), CS pin
    PKT_TYPE_SPI_XFER            = 0x71, // Both: Full-duplex SPI buffer transfer
    PKT_TYPE_SPI_FLASH_PROBE     = 0x72, // Host -> Dev: Read JEDEC ID (0x9F) and SFDP metadata
    PKT_TYPE_SPI_FLASH_RESP      = 0x73, // Dev -> Host: Flash manufacturer, capacity, and model string

    // Bus Pirate 1-Wire Engine & Search (0x74 - 0x77)
    PKT_TYPE_1WIRE_CONFIG        = 0x74, // Host -> Dev: Configure 1-Wire pin and pull-up
    PKT_TYPE_1WIRE_SEARCH_REQ    = 0x75, // Host -> Dev: Trigger 1-Wire ROM search
    PKT_TYPE_1WIRE_SEARCH_RESP   = 0x76, // Dev -> Host: Discovered 64-bit ROM IDs
    PKT_TYPE_1WIRE_XFER          = 0x77, // Both: Reset, Match ROM, write/read transfer

    // Bus Pirate Interactive Syntax & Command Execution (0x78 - 0x79)
    PKT_TYPE_BP_SYNTAX_EXEC      = 0x78, // Host -> Dev: Execute standard BP syntax string e.g. [ 0x50 r:4 ]
    PKT_TYPE_BP_SYNTAX_RESP      = 0x79, // Dev -> Host: Syntax execution response and parsed results

    PKT_TYPE_ERROR               = 0xFF  // Error report
};

// ─── Frame Header (10 Bytes) ─────────────────────────────────────────────────
#pragma pack(push, 1)
struct InstrumentPacketHeader {
    uint16_t magic;        // Must equal INSTRUMENT_PROTO_MAGIC (0xAA55)
    uint8_t  version;      // Protocol version (0x01)
    uint8_t  type;         // InstrumentPacketType
    uint16_t sequence;     // Rolling sequence number
    uint16_t payload_len;  // Length of subsequent payload bytes (0 to 1024)
    uint16_t crc16;        // CRC-16 (CCITT-FALSE) over payload (0 if payload_len == 0)
};

// ─── Status & Telemetry Payloads ─────────────────────────────────────────────
struct InstrumentStatusPayload {
    uint8_t  acquisition_state; // 0=IDLE, 1=ARMED, 2=TRIGGERED, 3=COMPLETE
    uint8_t  active_mode;       // Instrument mode (Logic, Freq, ADC, etc.)
    uint32_t sample_rate_hz;    // Current sample rate
    uint32_t total_samples;     // Configured buffer depth
    uint32_t trigger_offset;    // Trigger sample index in buffer
    uint16_t adc0_mv;           // GP26 voltage in mV
    uint16_t adc1_mv;           // GP27 voltage in mV
    uint16_t adc2_mv;           // GP28 voltage in mV
    uint8_t  prescaler_ratio;   // Active prescaler (1, 2, or 4)
    uint8_t  fifo_overflow;     // 1 if PIO RX FIFO experienced overflow
    uint64_t uptime_us;         // Monotonic 64-bit hardware timer timestamp (microseconds)
};

// ─── Configuration Payload ───────────────────────────────────────────────────
struct InstrumentConfigPayload {
    uint32_t sample_rate_hz;    // Requested sample rate (e.g. 10000000)
    uint32_t sample_count;      // Requested samples (up to 65536)
    uint8_t  trigger_channel;   // Channel 0..7 or 10 (EXT_TRIG)
    uint8_t  trigger_mode;      // 0=Auto, 1=Rising, 2=Falling, 3=Any, 4=High, 5=Low, 9=Pattern
    uint8_t  pattern_mask;      // Pattern mask for pattern trigger
    uint8_t  pattern_value;     // Pattern value for pattern trigger
    uint32_t trigger_param;     // Pulse width threshold (ns) or duration
    uint8_t  pre_trigger_pct;   // Configurable pre-trigger ratio (0..100%, default 70)
};

// ─── Capture Metadata Payload ────────────────────────────────────────────────
struct InstrumentCaptureInfoPayload {
    uint32_t capture_id;        // Monotonically increasing capture index
    uint32_t sample_rate_hz;    // Exact sample rate used
    uint32_t total_samples;     // Total valid samples captured (up to 65536)
    uint32_t trigger_offset;    // Exact index where trigger occurred
    uint32_t pre_samples;       // Number of pre-trigger samples preserved
    uint32_t post_samples;      // Number of post-trigger samples captured
    uint16_t whole_buffer_crc;  // CRC-16 over entire aligned capture buffer
    uint32_t t_trigger_us;      // Timestamp of trigger event (legacy 32-bit us)
    uint64_t t_trigger_us_64;   // Monotonic 64-bit hardware timer timestamp of trigger (microseconds)
};

// ─── Capture Data Streaming Chunk Payloads ───────────────────────────────────
struct InstrumentChunkRequestPayload {
    uint16_t chunk_index;       // 0-indexed chunk number (0 to 127 for 64 KiB @ 512B/chunk)
    uint16_t chunk_size;        // Requested chunk size (typically 512)
};

struct InstrumentChunkDataPayload {
    uint16_t chunk_index;       // Current chunk index
    uint16_t total_chunks;      // Total chunks in complete capture
    uint16_t chunk_len;         // Number of data bytes in this chunk
    uint8_t  data[INSTRUMENT_CHUNK_SIZE]; // Binary sample data
};

// ─── Analog Voltmeter Payload ────────────────────────────────────────────────
struct InstrumentAdcPayload {
    uint16_t adc0_mv;           // GP26 voltage in mV (0 to 3300 mV)
    uint16_t adc1_mv;           // GP27 voltage in mV (0 to 3300 mV)
    uint16_t adc2_mv;           // GP28 voltage in mV (0 to 3300 mV)
    uint16_t raw0;              // Raw 12-bit ADC reading (0 to 4095)
    uint16_t raw1;              // Raw 12-bit ADC reading (0 to 4095)
    uint16_t raw2;              // Raw 12-bit ADC reading (0 to 4095)
};

// ─── Hardware Measurement Payload ────────────────────────────────────────────
struct InstrumentMeasurementPayload {
    uint8_t  channel;           // Evaluated channel (0..7 or EXT_TRIG)
    float    frequency_hz;      // Measured frequency in Hz (prescaler-adjusted)
    float    period_ns;         // Measured period in ns
    float    duty_cycle_pct;    // Measured duty cycle (0.0 to 100.0%)
    float    pulse_width_ns;    // High pulse width in ns
    uint32_t edge_count;        // Total counted edges in measurement window
    uint8_t  valid;             // 1 if signal detected and stable, 0 otherwise
};

// ─── Error Payload ───────────────────────────────────────────────────────────
struct InstrumentErrorPayload {
    uint8_t  error_code;
    char     message[64];
};

// ─── Diagnostic Self-Test Payload ────────────────────────────────────────────
struct InstrumentSelfTestPayload {
    uint8_t  gpio_ok;
    uint8_t  uart_ok;
    uint8_t  spi_ok;
    uint8_t  adc_ok;
    uint8_t  buttons_ok;
    uint8_t  display_ok;
    uint8_t  pio_ok;
    uint8_t  dma_ok;
    uint8_t  trigger_ok;
    char     details[128];
};

// ─── Protocol Error Codes ───────────────────────────────────────────────────
enum InstrumentErrorCode : uint8_t {
    INST_ERR_OK                   = 0x00,
    INST_ERR_INVALID_COMMAND      = 0x01,
    INST_ERR_INVALID_PARAM        = 0x02,
    INST_ERR_PIN_CONFLICT         = 0x03,
    INST_ERR_BUFFER_OVERFLOW      = 0x04,
    INST_ERR_TIMEOUT              = 0x05,
    INST_ERR_NOT_SUPPORTED        = 0x06,
    INST_ERR_BUSY                 = 0x07,
    INST_ERR_CRC_MISMATCH         = 0x08,
    INST_ERR_DEVICE_NOT_ARMED     = 0x09
};

// ─── Pin Function Multiplexing ───────────────────────────────────────────────
enum PinFunction : uint8_t {
    PIN_FN_DEFAULT           = 0x00, // Safe Hi-Z input
    PIN_FN_LOGIC_PROBE       = 0x01, // Logic Analyzer Channel (GP2..GP9)
    PIN_FN_EXT_TRIG          = 0x02, // Hardware Trigger Input (GP10)
    PIN_FN_SYNC_OUT          = 0x03, // Analyzer Sync Strobe Output (GP9/GP10)
    PIN_FN_JTAG_TCK          = 0x04, // JTAG TCK
    PIN_FN_JTAG_TMS          = 0x05, // JTAG TMS
    PIN_FN_JTAG_TDI          = 0x06, // JTAG TDI
    PIN_FN_JTAG_TDO          = 0x07, // JTAG TDO
    PIN_FN_SWD_SWCLK         = 0x08, // SWD SWCLK
    PIN_FN_SWD_SWDIO         = 0x09, // SWD SWDIO
    PIN_FN_ADC_INPUT         = 0x0A, // Analog ADC input (GP26..GP28)
    PIN_FN_GPIO_IN           = 0x0B, // Generic GPIO In
    PIN_FN_GPIO_OUT          = 0x0C, // Generic GPIO Out
    PIN_FN_I2C_SDA           = 0x0D, // I2C SDA (GP2)
    PIN_FN_I2C_SCL           = 0x0E, // I2C SCL (GP3)
    PIN_FN_SPI_SCK           = 0x0F, // SPI SCK (GP2)
    PIN_FN_SPI_MOSI          = 0x10, // SPI MOSI (GP3)
    PIN_FN_SPI_MISO          = 0x11, // SPI MISO (GP4)
    PIN_FN_SPI_CS            = 0x12, // SPI CS# (GP5)
    PIN_FN_UART_TX           = 0x13, // UART TX (GP4)
    PIN_FN_UART_RX           = 0x14, // UART RX (GP5)
    PIN_FN_ONEWIRE_DQ        = 0x15, // 1-Wire DQ (GP6)
    PIN_FN_PWM_OUT           = 0x16  // PWM / Glitch Output (GP2)
};

// ─── Instrument Event Types ──────────────────────────────────────────────────
enum InstrumentEventType : uint8_t {
    EVENT_TYPE_NONE          = 0x00,
    EVENT_TYPE_LOGIC_EDGE    = 0x01, // Digital edge on logic channel
    EVENT_TYPE_TRIGGER_FIRED = 0x02, // Hardware trigger satisfied
    EVENT_TYPE_HOST_MARKER   = 0x03, // Software marker injected by host
    EVENT_TYPE_ADC_THRESHOLD = 0x04, // ADC rail cross/drop threshold
    EVENT_TYPE_BUFFER_OVERRUN= 0x05, // Sample or event buffer overrun
    EVENT_TYPE_BLE_DETECT    = 0x06, // BLE proximity advertisement
    EVENT_TYPE_WIFI_PROBE    = 0x07, // Wi-Fi scan / network probe event
    EVENT_TYPE_SWD_FAULT     = 0x08, // Target CPU fault or halt
    EVENT_TYPE_SYNC_STROBE   = 0x09, // SYNC_OUT strobe pulsed
    EVENT_TYPE_CAPTURE_START = 0x0A, // Acquisition armed / started
    EVENT_TYPE_CAPTURE_DONE  = 0x0B, // Acquisition completed
    EVENT_TYPE_VBUS_CHANGE   = 0x0C, // VBUS rail attach/detach/brownout
    EVENT_TYPE_CC_ORIENTATION= 0x0D  // USB-C CC1/CC2 orientation detected
};

// ─── Instrument Operating Profiles ──────────────────────────────────────────
#define INSTRUMENT_PROFILE_GENERIC   0
#define INSTRUMENT_PROFILE_LAB       1
#define INSTRUMENT_PROFILE_APPLE_DIAGNOSTICS 1 // Backward compatibility alias

// ─── Device Capabilities Payload (0x23) ──────────────────────────────────────
struct InstrumentCapabilitiesPayload {
    uint8_t  protocol_version;       // 0x01
    uint8_t  instrument_profile;     // 0=Generic, 1=Electronics Diagnostic Lab
    uint8_t  logic_channels;         // 8 channels (GP2..GP9)
    uint8_t  adc_channels;           // 3 external + 1 internal
    uint32_t max_sample_rate_hz;     // 100,000,000 Hz
    uint32_t min_sample_rate_hz;     // 5,000 Hz
    uint32_t buffer_size_bytes;      // 65,536 bytes
    uint32_t trigger_modes_mask;     // Bitmask of supported trigger types
    uint32_t timestamp_tick_hz;      // 1,000,000 Hz (1 µs)
    uint8_t  jtag_swd_supported;     // 1
    uint8_t  radio_diag_supported;   // 1 (ESP32 BLE & Wi-Fi)
    uint8_t  usb_phy_sniff_supported;// 0 (Truthful! NOT supported on RP2040 12 Mbps PHY)
    uint8_t  external_sync_supported;// 1 (SYNC_OUT & EXT_TRIG)
    uint8_t  variable_pretrigger;    // 1 (0..100% pre/post trigger ratio supported)
    uint8_t  pinmux_supported;       // 1 (Dynamic safe pinmux)
    uint8_t  sync_strobe_pin;        // Active sync strobe pin (GP9 or GP10)
    uint8_t  ext_trig_pin;           // Active external trigger pin (GP10)
    char     board_name[32];         // "UDOO KEY Dual-MCU"
    char     firmware_version[16];   // "2.1.0-lab"
    char     mcu_primary[24];        // "RP2040 (133 MHz M0+)"
    char     mcu_coprocessor[24];    // "ESP32 (240 MHz LX6)"
};

// ─── Time Synchronization Payloads (0x24 / 0x25) ─────────────────────────────
struct InstrumentTimeSyncReqPayload {
    uint32_t host_request_id;        // Echoed back by device
    uint64_t host_timestamp_us;      // Host clock (µs or ns)
};

struct InstrumentTimeSyncRespPayload {
    uint32_t host_request_id;        // Echoed request ID
    uint64_t host_timestamp_us;      // Echoed host timestamp
    uint64_t device_timestamp_us;    // Deterministic hardware timer at reception
    uint32_t clock_drift_ppm;        // Calculated clock drift in PPM
};

// ─── Host Marker Event Ingestion Payload (0x26) ─────────────────────────────
struct InstrumentMarkEventPayload {
    uint64_t host_timestamp_us;      // Host timestamp
    uint16_t marker_id;              // Monotonic marker ID
    uint16_t marker_type;            // 1=USB, 2=BLE, 3=WiFi, 4=SysLog, 5=User
    char     label[32];              // Human-readable identifier (e.g. "USB_ENUM_START")
};

// ─── Structured Instrument Event Stream Payload (0x27) ──────────────────────
struct InstrumentEventPayload {
    uint64_t timestamp_us;           // Hardware 64-bit microsecond timestamp
    uint8_t  event_type;             // InstrumentEventType
    uint8_t  source_channel;         // Channel 0..7 or pin number
    uint16_t raw_value;              // Logic state, ADC mV, or RSSI
    uint32_t event_flags;            // Trigger flags, overrun indicators
    char     tag[16];                // Short identifier (e.g. "EXT_TRIG", "OVERRUN")
};

// ─── Dynamic Pin-Mux Payloads (0x28 / 0x29) ──────────────────────────────────
struct InstrumentPinMuxPayload {
    uint8_t pin;                     // Target GP0..GP29
    uint8_t function;                // PinFunction
    uint8_t pull_mode;               // 0=None, 1=PullUp, 2=PullDown
    uint8_t reserved;
};

struct InstrumentPinMuxRespPayload {
    uint8_t pin;
    uint8_t status_code;             // InstrumentErrorCode
    char    reason[32];              // Explanation if ERR_PIN_CONFLICT
};

// ─── Radio Diagnostics Telemetry Payloads (0x2A / 0x2B / 0x2C) ──────────────
struct InstrumentRadioEventPayload {
    uint64_t esp32_timestamp_us;     // ESP32 local microsecond timestamp
    uint64_t synced_device_ts_us;    // Correlated RP2040 microsecond timestamp
    uint8_t  radio_type;             // 1=BLE, 2=WiFi
    int8_t   rssi;                   // Signal strength in dBm
    uint8_t  channel;                // BLE channel (37..39) or Wi-Fi channel (1..14)
    union {
        uint8_t  adv_type;           // Advertisement type indicator
        uint8_t  apple_type;         // Backward compatibility
    };
    union {
        uint8_t  adv_flags;          // Subtype / status flags
        uint8_t  apple_sub_type;     // Backward compatibility
    };
    uint8_t  mac[6];                 // Device MAC address
    uint8_t  data_len;               // Length of diagnostic data
    uint8_t  data[32];               // Manufacturer or probe payload
};

struct InstrumentRadioCmdPayload {
    uint8_t cmd_type;                // 1=Start BLE scan, 2=Stop BLE scan, 3=WiFi Recon, 4=Ping, 5=WiFi Diag
    uint8_t scan_duration_s;         // Duration in seconds
    union {
        uint8_t filter_mode;         // 0 = All devices, 1 = Filtered
        uint8_t apple_only;          // Backward compatibility
    };
    uint8_t target_ip[4];            // Target IPv4 address for ping probe
};

struct InstrumentRadioWifiDiagPayload {
    uint64_t synced_device_ts_us;    // Correlated 64-bit device timestamp
    int8_t   rssi;                   // Current Wi-Fi signal strength in dBm
    uint8_t  channel;                // Channel 1..14
    uint8_t  auth_mode;              // 0=Open, 1=WEP, 2=WPA, 3=WPA2, 4=WPA3
    uint8_t  status_flags;           // Bit 0: Connected, Bit 1: Internet Reachable, Bit 2: DNS OK
    char     ssid[32];               // Connected or scanned SSID
    uint8_t  bssid[6];               // Access Point MAC
    uint8_t  local_ip[4];            // Station IPv4 address
    uint8_t  gateway_ip[4];          // Gateway IPv4 address
    uint32_t dns_latency_us;         // DNS resolution latency to probe target
    uint32_t gateway_rtt_us;         // Gateway ping round-trip time in microseconds
    uint8_t  packet_loss_pct;        // Packet loss percentage (0-100%)
    uint8_t  reserved[7];
};

// ─── Wi-Fi Promiscuous Sniffer Payloads (0x2D / 0x2E) ───────────────────────
struct InstrumentWifiSniffConfigPayload {
    uint8_t enabled;        // 1=Enable, 0=Disable
    uint8_t channel;        // 1..14
    uint8_t filter_type;    // 0=All, 1=Management, 2=Control, 3=Data
    uint8_t reserved;
};

struct InstrumentWifiSniffStatusPayload {
    uint8_t  active;        // 1=Sniffing active, 0=Idle
    uint8_t  channel;       // Active channel
    uint8_t  filter_type;   // Active filter
    uint8_t  reserved;
    uint32_t captured_count;// Frames captured
    uint32_t dropped_count; // Frames dropped (queue overrun)
};

// ─── Packet & PCAP Stream Payloads (0x50 / 0x51) ────────────────────────────
struct InstrumentPacketRecordPayload {
    uint64_t timestamp_us;   // Frame microsecond timestamp
    uint8_t  source;         // 1=WiFi 802.11, 2=BLE Adv, 3=nRF24
    uint8_t  channel;        // RF channel
    int8_t   rssi;           // Signal strength in dBm
    uint8_t  link_type;      // DLT (105=802.11, 251=BLE LL, 147=USER0)
    uint16_t captured_length;// Length of data in this payload
    uint16_t original_length;// Wire frame length
    uint8_t  data[256];      // Packet octets
};

struct InstrumentPcapDumpReqPayload {
    uint8_t  stream_binary;  // 1=Stream raw PCAP file over transport
    uint8_t  clear_after;    // 1=Clear ring buffer after dump
    uint16_t max_packets;    // 0=All available
};

// ─── Bus Pirate Generic GPIO Service Payloads (0x60 - 0x65) ──────────────────
enum GpioPullMode : uint8_t {
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP   = 1,
    GPIO_PULL_DOWN = 2
};

struct InstrumentGpioConfigPayload {
    uint8_t pin_or_idx;     // Probe index 0..7 or physical GPIO 2..9
    uint8_t is_output;      // 1 = Output, 0 = Input
    uint8_t initial_level;  // 1 = High, 0 = Low (applied before dir change)
    uint8_t pull_mode;      // GpioPullMode (0=None, 1=PullUp, 2=PullDown)
};

struct InstrumentGpioReadPayload {
    uint8_t pin_or_idx;     // Probe index 0..7 or physical GPIO 2..9
};

struct InstrumentGpioWritePayload {
    uint8_t pin_or_idx;     // Probe index 0..7 or physical GPIO 2..9
    uint8_t level;          // 1 = High, 0 = Low
};

struct InstrumentGpioStatusPayload {
    uint8_t pin_or_idx;     // Requested probe index / pin
    uint8_t gpio_num;       // Resolved physical GPIO (2..9)
    uint8_t is_output;      // 1 = Output, 0 = Input
    uint8_t logic_level;    // 1 = High, 0 = Low
    uint8_t pull_mode;      // GpioPullMode
    uint8_t all_pins_state; // Current 8-bit bus logic levels
};

struct InstrumentGpioWriteMaskPayload {
    uint8_t dir_mask;       // Bitmask of pins to make outputs (bit 0 = Probe 0, etc.)
    uint8_t value_mask;     // Bitmask of output values
};

struct InstrumentGpioReadAllPayload {
    uint8_t values_mask;    // Logic levels of all 8 probe pins (bit 0..7)
    uint8_t directions_mask;// Direction of all 8 probe pins (1=Output, 0=Input)
};

// ─── Instrument Operating Modes (0x66 - 0x67) ────────────────────────────────
enum InstrumentModeId : uint8_t {
    INST_MODE_IDLE            = 0,
    INST_MODE_LOGIC_ANALYZER  = 1,
    INST_MODE_SPI             = 2,
    INST_MODE_I2C             = 3,
    INST_MODE_UART            = 4,
    INST_MODE_ONE_WIRE        = 5,
    INST_MODE_SWD             = 6,
    INST_MODE_PWM_GENERATOR   = 7,
    INST_MODE_GLITCH          = 8,
    INST_MODE_EEPROM          = 9,
    INST_MODE_FLASH           = 10,
    INST_MODE_INTERCHIP_SPI   = 11,
    INST_MODE_DIO             = 12,
    INST_MODE_USB_SNIFFER     = 13,
    INST_MODE_SENSOR_TEST     = 14
};

struct InstrumentModeSetPayload {
    uint8_t  mode;          // Target InstrumentModeId
    uint32_t speed_hz;      // Speed or baud rate in Hz (e.g. 400000 for I2C, 115200 for UART, 0=default)
};

struct InstrumentModeStatusPayload {
    uint8_t  active_mode;       // Current InstrumentModeId
    uint8_t  previous_mode;     // Previous InstrumentModeId
    uint8_t  probe_spi_active;  // 1 if probe SPI active (GP2..GP5), 0 if TFT display bus (GP18..GP20)
    uint8_t  safe_hiz_verified; // 1 if all probe pins returned to Hi-Z during last transition
    uint32_t active_speed_hz;   // Configured speed/baud rate in Hz
    char     mode_name[24];     // Null-terminated mode name string
    uint8_t  pin_functions[8];  // PinFunction for GP2..GP9
};

// ─── Bus Pirate UART Bridge Payloads (0x68 - 0x6A) ───────────────────────────
struct InstrumentUartConfigPayload {
    uint32_t baudrate;    // 300 to 1000000
    uint8_t  data_bits;   // 5..8 (default 8)
    uint8_t  parity;      // 0=None, 1=Even, 2=Odd
    uint8_t  stop_bits;   // 1 or 2
    uint8_t  invert;      // 0=Normal, 1=Inverted
};

struct InstrumentUartDataPayload {
    uint16_t len;
    uint8_t  data[256];
};

struct InstrumentUartStatusPayload {
    uint32_t actual_baud;
    uint8_t  data_bits;
    uint8_t  parity;
    uint8_t  stop_bits;
    uint16_t rx_buffered;
};

// ─── Bus Pirate I2C Engine Payloads (0x6C - 0x6F) ────────────────────────────
struct InstrumentI2cConfigPayload {
    uint32_t speed_hz;        // Clock speed (100000, 400000, 1000000)
    uint8_t  enable_pullups;  // 1 = enable internal pull-ups, 0 = disable
};

struct InstrumentI2cScanRespPayload {
    uint8_t  device_count;
    uint8_t  addresses[32];   // Up to 32 detected 7-bit addresses
};

struct InstrumentI2cXferPayload {
    uint8_t  addr_7bit;       // 7-bit target address
    uint8_t  send_stop;       // 1 = send STOP condition, 0 = repeated START
    uint16_t write_len;       // Bytes to write (0..256)
    uint16_t read_len;        // Bytes to read (0..256)
    uint8_t  status;          // 0=Success, 1=Data too long, 2=NACK on address, 3=NACK on data, 4=Other
    uint8_t  data[256];       // Write bytes on request; Read bytes on response
};

// ─── Bus Pirate SPI Engine & Flash Probe Payloads (0x70 - 0x73) ──────────────
struct InstrumentSpiConfigPayload {
    uint32_t speed_hz;        // Clock speed (10000..30000000)
    uint8_t  cpol;            // 0 = Idle Low, 1 = Idle High
    uint8_t  cpha;            // 0 = Leading edge, 1 = Trailing edge
    uint8_t  lsb_first;       // 0 = MSB first, 1 = LSB first
    uint8_t  cs_pin;          // Active CS pin (default 5 for GP5)
};

struct InstrumentSpiXferPayload {
    uint8_t  assert_cs;       // 1 = Assert CS before and deassert after, 0 = manual CS
    uint16_t data_len;        // 1..512 bytes
    uint8_t  data[512];       // MOSI on request; MISO on response
};

struct InstrumentSpiFlashProbePayload {
    uint8_t  cs_pin;          // CS pin (default 5)
};

struct InstrumentFlashInfoPayload {
    uint8_t  mfg_id;          // Manufacturer ID (e.g. 0xEF for Winbond)
    uint8_t  memory_type;     // Memory Type (e.g. 0x40)
    uint8_t  capacity_id;     // Capacity ID (e.g. 0x18 for 16MB)
    uint32_t capacity_bytes;  // Decoded size in bytes (e.g. 16777216)
    char     part_name[32];   // e.g. "W25Q128FV"
};

// ─── Bus Pirate 1-Wire Engine & Search Payloads (0x74 - 0x77) ────────────────
struct Instrument1WireConfigPayload {
    uint8_t  pin;             // GPIO pin (default 6 for GP6)
    uint8_t  enable_pullup;   // 1 = enable internal pullup
};

struct Instrument1WireSearchRespPayload {
    uint8_t  device_count;
    uint8_t  rom_ids[8][8];   // Up to 8 discovered 64-bit ROM IDs
};

struct Instrument1WireXferPayload {
    uint8_t  issue_reset;     // 1 = issue reset pulse first
    uint8_t  presence_ok;     // 1 if presence pulse detected on bus
    uint16_t write_len;       // Bytes to write
    uint16_t read_len;        // Bytes to read
    uint8_t  data[64];        // Write bytes on request; read bytes on response
};

// ─── Bus Pirate Interactive Syntax & Command Execution Payloads (0x78 - 0x79)
struct InstrumentBpSyntaxExecPayload {
    char syntax[256];         // Null-terminated syntax string e.g. "[ 0x50 0x00 ] [ 0x51 r:4 ]" or "macro 1"
};

struct InstrumentBpSyntaxRespPayload {
    uint8_t  status;          // 0 = OK, 1 = Parse Error, 2 = Execution Error
    uint16_t result_len;      // Length of result text (0..512)
    char     result[512];     // Execution output text
};

// ─── Infrared (IR) Engine Payloads (0x30 - 0x33) ────────────────────────────
enum InstrumentIrProtocol : uint8_t {
    IR_PROTO_RAW     = 0,
    IR_PROTO_NEC     = 1,
    IR_PROTO_SONY    = 2,
    IR_PROTO_RC5     = 3,
    IR_PROTO_RC6     = 4,
    IR_PROTO_SAMSUNG = 5
};

struct InstrumentIrConfigPayload {
    uint8_t  tx_pin;          // ESP32 IO25 default
    uint8_t  rx_pin;          // ESP32 IO26 default
    uint16_t carrier_freq_hz; // 38000 default (38 kHz)
    uint8_t  duty_percent;    // 33% default
};

struct InstrumentIrTxPayload {
    uint8_t  protocol;        // InstrumentIrProtocol
    uint16_t address;         // Device / Manufacturer address
    uint32_t command;         // Command code (e.g. 0x12 for Power)
    uint8_t  repeat_count;    // Number of retransmissions
    uint16_t raw_len;         // Number of pulse durations if protocol == RAW (up to 128)
    uint16_t raw_timings_us[128]; // Alternating Mark/Space durations in microseconds
};

struct InstrumentIrRxEventPayload {
    uint8_t  protocol;        // InstrumentIrProtocol
    uint16_t address;
    uint32_t command;
    uint8_t  repeat;          // 1 if repeat frame
    uint64_t timestamp_us;    // Monotonic timestamp
    uint16_t raw_len;
    uint16_t raw_timings_us[128];
};

struct InstrumentIrRxConfigPayload {
    uint8_t  enabled;         // 1 = start listening, 0 = stop listening
    uint8_t  filter_unknown;  // 1 = only emit recognized protocols
};

// ─── nRF24L01 Radio Module Payloads (0x40 - 0x45) ───────────────────────────
enum InstrumentNrf24Datarate : uint8_t {
    NRF24_RATE_250KBPS = 0,
    NRF24_RATE_1MBPS   = 1,
    NRF24_RATE_2MBPS   = 2
};

enum InstrumentNrf24Power : uint8_t {
    NRF24_PA_MIN  = 0, // -18 dBm
    NRF24_PA_LOW  = 1, // -12 dBm
    NRF24_PA_HIGH = 2, // -6 dBm
    NRF24_PA_MAX  = 3  // 0 dBm
};

enum InstrumentNrf24Crc : uint8_t {
    NRF24_CRC_DISABLED = 0,
    NRF24_CRC_8BIT     = 1,
    NRF24_CRC_16BIT    = 2
};

struct InstrumentNrf24ConfigPayload {
    uint8_t  channel;         // RF Channel: 0..125 (2400 MHz + channel)
    uint8_t  datarate;        // InstrumentNrf24Datarate
    uint8_t  power_level;     // InstrumentNrf24Power
    uint8_t  crc_mode;        // InstrumentNrf24Crc
    uint8_t  addr_width;      // 3, 4, or 5 bytes
    uint8_t  auto_ack;        // 1 = enable Enhanced ShockBurst auto-ack
};

struct InstrumentNrf24TxPayload {
    uint8_t  target_addr[5];  // Destination address
    uint8_t  length;          // Payload length: 1..32 bytes
    uint8_t  data[32];        // Payload bytes
};

struct InstrumentNrf24TxStatusPayload {
    uint8_t  status;          // 0 = Success (ACK received or noack sent), 1 = Max retries reached, 2 = Hardware error
    uint8_t  retries_count;   // Retransmit count
};

struct InstrumentNrf24RxConfigPayload {
    uint8_t  pipe_index;      // 0..5
    uint8_t  enabled;         // 1 = open pipe, 0 = close pipe
    uint8_t  pipe_addr[5];    // Pipe listening address
    uint8_t  payload_size;    // 1..32 or 0 for dynamic payload
};

struct InstrumentNrf24RxEventPayload {
    uint8_t  pipe_index;      // 0..5
    uint8_t  length;          // 1..32
    uint8_t  data[32];
    uint64_t timestamp_us;
};

struct InstrumentNrf24StatusPayload {
    uint8_t  is_connected;    // 1 if nRF24 SPI communications verified
    uint8_t  carrier_detected;// 1 if RPD (Received Power Detector) active
    uint8_t  rx_fifo_empty;
    uint8_t  tx_fifo_full;
    uint8_t  active_channel;
};

#pragma pack(pop)

// ─── Fast CRC-16 (CCITT-FALSE, Poly 0x1021, Init 0xFFFF) ─────────────────────
static inline uint16_t instrument_crc16(const uint8_t* data, size_t len) {
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

// ─── 64-Bit Monotonic Timestamp Helper for RP2040 ────────────────────────────
#if defined(PICO_TIME_H) || defined(ARDUINO_ARCH_RP2040)
#if defined(ARDUINO_ARCH_RP2040)
#include "pico/time.h"
#endif
static inline uint64_t device_timestamp_us() {
    return time_us_64();
}
#endif

#endif // INSTRUMENT_PROTOCOL_H
