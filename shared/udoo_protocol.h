/*
 * udoo_protocol.h — UDOO Key iPhone Pentest Protocol v5.0
 *
 * Dual-chip comms protocol: RP2040 HID Engine <-> ESP32 C2 Brain
 *
 * Packet namespace:
 *   0x01-0x0F  System / Heartbeat / Abort
 *   0x10-0x1F  DFU / BootROM exploit coordination
 *   0x20-0x2F  Passcode brute-force (HID keyboard)
 *   0x30-0x3F  Activation bypass commands
 *   0x40-0x4F  Captive portal / DNS hijack / WebSheet
 *   0x50-0x5F  AFC filesystem injection
 *   0x60-0x6F  lockdownd flag manipulation
 *   0x70-0x7F  BadUSB payloads
 *   0x80-0x8F  LED control
 *   0xF0-0xFF  Telemetry & sensor
 *
 * iOS 26.3 attack surface (from live iPad8,10 testing 2026-04-06):
 *   - Factory activation commands REMOVED in 26.2+
 *   - Only 6 mobileactivationd commands remain
 *   - Activation nonce enforced (FairPlay DRM local lock check)
 *   - WiFi captive portal → WebSheet JS execution WORKS
 *   - HTTPS cert pinning blocks albert.apple.com MITM
 *   - FDR-LOCAL private keys extractable from IPSW
 *   - AFC r/w to /var/mobile/Media/ works
 *   - 25/27 lockdownd flags persist across reboot
 *   - VU#346053 XML injection unpatched (needs HTTPS bypass)
 */

#ifndef UDOO_PROTOCOL_H
#define UDOO_PROTOCOL_H

#include <stdint.h>

#define PROTOCOL_SYNC_BYTE  0xAA
#define PROTOCOL_VERSION    0x05

// ─── Packet Types ────────────────────────────────────────────────────────────
enum PacketType : uint8_t {
    // System (0x01-0x0F)
    PKT_HEARTBEAT        = 0x01,
    PKT_ACK              = 0x02,
    PKT_ABORT            = 0x03,
    PKT_MODE_CHANGE      = 0x04,   // RP2040 tells ESP32 which mode is active

    // DFU / BootROM exploit (0x10-0x1F)
    PKT_DFU_TRIGGER      = 0x10,
    PKT_DFU_STATUS       = 0x11,
    PKT_PAYLOAD_REQ      = 0x12,
    PKT_PAYLOAD_CHUNK    = 0x13,

    // Passcode brute-force (0x20-0x2F)
    PKT_BRUTE_START      = 0x20,
    PKT_BRUTE_STATUS     = 0x21,
    PKT_BRUTE_SUCCESS    = 0x22,
    PKT_BRUTE_PAUSE      = 0x23,

    // Activation bypass (0x30-0x3F)
    PKT_ACTIV_CMD        = 0x30,   // ESP32 → RP2040: send activation command via USB
    PKT_ACTIV_RESP       = 0x31,   // RP2040 → ESP32: activation command response
    PKT_ACTIV_NONCE      = 0x32,   // Captured nonce data
    PKT_ACTIV_SESSION    = 0x33,   // Session blob (CollectionBlob, HandshakeReq)
    PKT_ACTIV_DRM        = 0x34,   // DRM handshake data relay

    // Captive portal / DNS hijack / WebSheet (0x40-0x4F)
    PKT_PORTAL_START     = 0x40,   // Start captive portal attack
    PKT_PORTAL_STATUS    = 0x41,   // Portal status (DNS up, client connected, etc.)
    PKT_PORTAL_CLIENT    = 0x42,   // A device connected to WiFi AP
    PKT_PORTAL_WEBSHEET  = 0x43,   // WebSheet JS callback received
    PKT_PORTAL_HTTPS     = 0x44,   // HTTPS interception attempt status
    PKT_PORTAL_STOP      = 0x45,   // Stop captive portal

    // AFC filesystem injection (0x50-0x5F)
    PKT_AFC_WRITE        = 0x50,   // Write file via AFC
    PKT_AFC_STATUS       = 0x51,   // AFC operation result
    PKT_AFC_LIST         = 0x52,   // List AFC-accessible paths

    // lockdownd flag manipulation (0x60-0x6F)
    PKT_LOCKD_SET        = 0x60,   // Set lockdownd flag
    PKT_LOCKD_STATUS     = 0x61,   // Flag set confirmation
    PKT_LOCKD_BATCH      = 0x62,   // Set all 25 known-good flags at once

    // BadUSB payloads (0x70-0x7F)
    PKT_BADUSB_FIRE      = 0x70,   // Fire a BadUSB payload by ID
    PKT_BADUSB_CUSTOM    = 0x71,   // Custom typed string payload
    PKT_MACRO_CONFIG     = 0x72,   // Legacy macro config compat

    // LED control (0x80-0x8F)
    PKT_LED_SET          = 0x80,   // Set individual LED state
    PKT_LED_PATTERN      = 0x81,   // Set LED animation pattern

    // Logic Analyzer (0x90-0x9F)
    PKT_LA_CONFIG        = 0x90,   // Configure sample rate, trigger, depth
    PKT_LA_START         = 0x91,   // Start logic capture
    PKT_LA_STOP          = 0x92,   // Stop logic capture
    PKT_LA_STREAM        = 0x93,   // Binary stream of sample buffer

    // Bit-Pirate Hardware Protocol Tools (0xA0-0xAF)
    PKT_PIRATE_I2C_SCAN  = 0xA0,   // Trigger I2C bus scan
    PKT_PIRATE_SPI_XFER  = 0xA1,   // SPI register transfer
    PKT_PIRATE_PWM_SET   = 0xA2,   // Set PWM frequency / duty cycle
    PKT_PIRATE_TERMINAL  = 0xA3,   // Bit Pirate VT100 interactive command
    PKT_PIRATE_SPI_FLASH = 0xA4,   // SPI Flash JEDEC ID probe & read/write
    PKT_PIRATE_JTAG_SWD  = 0xA5,   // JTAG TAP scan & ARM SWD IDCODE reader
    PKT_PIRATE_AVR_PROG  = 0xA6,   // AVR ISP / PDI / UPDI flasher shell
    PKT_PIRATE_ONEWIRE   = 0xA7,   // 1-Wire ROM search & DS18B20 temp reading
    PKT_PIRATE_IR_REMOTE = 0xA8,   // IR NEC/RC5 remote transmitter & receiver
    PKT_PIRATE_WIFI_RECON= 0xA9,   // WiFi AP reconnaissance & probe monitor

    // Telemetry (0xF0-0xFF)
    PKT_TELEMETRY        = 0xF0,
    PKT_SENSOR_ALERT     = 0xF1,
};

// ─── Attack Modes ────────────────────────────────────────────────────────────
enum AttackMode : uint8_t {
    MODE_IDLE            = 0x00,
    MODE_PIN_BRUTE       = 0x01,
    MODE_BADUSB          = 0x02,
    MODE_DFU_EXPLOIT     = 0x03,
    MODE_CAPTIVE_PORTAL  = 0x04,   // ESP32 DNS hijack + WebSheet JS injection
    MODE_USB_ACTIVATION  = 0x05,   // Send the 6 remaining activation commands
    MODE_AFC_INJECT      = 0x06,   // Write files via AFC sandbox
    MODE_LOCKD_FLAGS     = 0x07,   // Set 25 persistent lockdownd flags
    MODE_USB_RECON       = 0x08,   // Enumerate device, capture nonces
    MODE_COUNT           = 0x09,
};

// ─── Attack Stages ───────────────────────────────────────────────────────────
enum AttackStage : uint8_t {
    STAGE_IDLE           = 0x00,
    STAGE_DFU_WAIT       = 0x01,
    STAGE_DFU_DETECTED   = 0x02,
    STAGE_EXPLOIT_SEND   = 0x03,
    STAGE_EXPLOIT_DONE   = 0x04,
    STAGE_BRUTE_ACTIVE   = 0x05,
    STAGE_BRUTE_SUCCESS  = 0x06,
    STAGE_PORTAL_DNS_UP  = 0x07,
    STAGE_PORTAL_CLIENT  = 0x08,  // iPad connected to WiFi AP
    STAGE_PORTAL_JS_EXEC = 0x09,  // WebSheet JS executing
    STAGE_PORTAL_HTTPS   = 0x0A,  // HTTPS MITM attempt
    STAGE_ACTIV_SEND     = 0x0B,
    STAGE_ACTIV_NONCE    = 0x0C,  // Nonce captured
    STAGE_ACTIV_FAIL     = 0x0D,  // Nonce barrier hit
    STAGE_AFC_WRITING    = 0x0E,
    STAGE_LOCKD_SETTING  = 0x0F,
    STAGE_COMPLETE       = 0xFF,
};

// ─── LED Identifiers ─────────────────────────────────────────────────────────
// Physical layout (from mockup): 4 LEDs at bottom of device case
enum LedId : uint8_t {
    LED_ATTACK   = 0,   // Attack status: solid=active, blink=working, off=idle
    LED_WIFI     = 1,   // WiFi/Portal: solid=AP up, blink=client connected
    LED_USB      = 2,   // USB: solid=target detected, blink=data transfer
    LED_SENSOR   = 3,   // Sensor/Alert: solid=armed, red-blink=abort triggered
};

enum LedState : uint8_t {
    LED_OFF      = 0,
    LED_SOLID    = 1,
    LED_BLINK    = 2,
    LED_FAST     = 3,   // Fast blink (4Hz)
    LED_PULSE    = 4,   // Slow pulse (1Hz fade)
};

// ─── Activation Commands (the 6 that still work on iOS 26.3) ─────────────────
enum ActivationCmd : uint8_t {
    ACMD_GET_STATE       = 0,  // GetActivationStateRequest → "Unactivated"
    ACMD_CREATE_SESSION  = 1,  // CreateTunnel1SessionInfoRequest → CollectionBlob
    ACMD_CREATE_INFO     = 2,  // CreateActivationInfoRequest → fails (DRM locked)
    ACMD_HANDLE_INFO     = 3,  // HandleActivationInfoRequest → nonce check
    ACMD_HANDLE_SESSION  = 4,  // HandleActivationInfoWithSessionRequest → nonce check
    ACMD_DEACTIVATE      = 5,  // DeactivateRequest → {} (already unactivated)
};

// ─── Wire Structures ─────────────────────────────────────────────────────────
#pragma pack(push, 1)

struct PacketHeader {
    uint8_t  sync;
    uint8_t  type;
    uint16_t payload_len;
};

// DFU status
struct DfuStatusPayload {
    uint8_t  stage;
    uint8_t  usb_detected;
    uint16_t vid;           // Apple = 0x05AC
    uint16_t pid;           // DFU = 0x1227
    uint8_t  exploit_step;
};

// Payload chunk delivery (chunked transfer, max 240B per chunk)
struct PayloadChunkPayload {
    uint32_t offset;
    uint16_t chunk_len;
    uint8_t  is_last;
    uint8_t  data[240];
};

// Brute-force config (ESP32 → RP2040)
struct BruteConfig {
    uint32_t start_code;
    uint32_t end_code;
    uint16_t delay_ms;
    uint16_t lockout_delay;
    uint8_t  lockout_every;
    uint8_t  digits;       // 4 or 6
    uint8_t  light_abort;  // 1 = abort on LDR spike
    uint8_t  pad;
};

// Brute-force status (RP2040 → ESP32)
struct BruteStatus {
    uint32_t current_attempt;
    uint32_t total_attempts;
    uint8_t  stage;
    uint8_t  light_value_hi;
    uint8_t  light_value_lo;
    uint8_t  locked_out;
};

// Activation command request/response
struct ActivationCmdPayload {
    uint8_t  command;       // ActivationCmd enum
    uint8_t  status;        // 0=pending 1=sent 2=success 3=nonce_fail 4=error
    int32_t  error_code;    // Apple error code (e.g., -1 = invalid nonce)
    uint16_t data_len;      // Length of attached data (session blob, nonce, etc.)
    // Followed by data_len bytes of response data
};

// Captive portal status
struct PortalStatus {
    uint8_t  stage;         // AttackStage enum
    uint8_t  dns_up;        // DNS server running
    uint8_t  http_up;       // HTTP captive portal serving
    uint8_t  https_up;      // HTTPS intercept attempt
    uint8_t  clients;       // Number of WiFi clients connected
    uint8_t  websheet_hit;  // WebSheet loaded our JS payload
    uint8_t  js_callback;   // JS called back to our server
    uint8_t  https_attempt; // HTTPS MITM attempted
    uint32_t http_requests; // Total HTTP requests seen
    uint32_t dns_queries;   // Total DNS queries intercepted
};

// AFC write request (ESP32 → RP2040 → target via USB)
struct AfcWritePayload {
    uint8_t  path_len;      // Length of target path string
    uint16_t data_len;      // Length of data to write
    uint8_t  overwrite;     // 1 = overwrite existing
    // Followed by: path_len bytes of path + data_len bytes of data
};

// lockdownd flag set request
struct LockdownFlagPayload {
    uint8_t  domain_len;    // Length of domain string
    uint8_t  key_len;       // Length of key string
    uint8_t  value_type;    // 0=bool_true 1=bool_false 2=string
    uint8_t  value_len;     // Length of value string (if type=2)
    // Followed by: domain_len + key_len + value_len bytes
};

// LED control
struct LedControlPayload {
    uint8_t  led_id;        // LedId enum
    uint8_t  state;         // LedState enum
    uint8_t  r, g, b;       // RGB color (if LED is addressable, else ignored)
};

// Full telemetry (RP2040 → ESP32, every 500ms)
struct TelemetryPayload {
    uint32_t uptime_ms;
    uint32_t total_keystrokes;
    uint32_t macros_executed;
    uint32_t brute_attempts;
    uint16_t light_level;
    uint8_t  is_dark;
    uint8_t  attack_mode;   // Current AttackMode
    uint8_t  attack_stage;  // Current AttackStage
    uint8_t  usb_connected;
    uint8_t  led_states[4]; // Current LED states
    uint8_t  pad;
};

#pragma pack(pop)

#endif // UDOO_PROTOCOL_H
