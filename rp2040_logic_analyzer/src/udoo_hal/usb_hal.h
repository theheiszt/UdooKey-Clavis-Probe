#ifndef UDOO_USB_HAL_H
#define UDOO_USB_HAL_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── UDOO KEY USB Probing Physical Pinout ────────────────────────────────────
// Target D+ : GP2 (Header P1 Pin 4 / Probe CH0)
// Target D- : GP3 (Header P1 Pin 5 / Probe CH1)
// Target GND: Header P1 Pin 3 (GND)
//
// NOTE: An external protection network (47-ohm series damping resistors + low-cap
// ESD clamp) is required before attaching to an external live USB bus.
#define UDOO_USB_DP_PIN           2
#define UDOO_USB_DM_PIN           3

// ─── USB Physical Bus States ─────────────────────────────────────────────────
typedef enum {
    USB_STATE_SE0 = 0,   // Single-Ended Zero: D+=0, D-=0 (Reset, EOP, Disconnect)
    USB_STATE_SE1 = 1,   // Single-Ended One:  D+=1, D-=1 (Illegal / Contention / Fault)
    USB_STATE_J   = 2,   // J-State: Full-Speed (D+=1, D-=0) or Low-Speed (D+=0, D-=1)
    USB_STATE_K   = 3    // K-State: Full-Speed (D+=0, D-=1) or Low-Speed (D+=1, D-=0)
} UsbBusStateCode;

typedef enum {
    USB_SPEED_UNKNOWN      = 0,
    USB_SPEED_DISCONNECTED = 1,
    USB_SPEED_LOW          = 2, // 1.5 Mbps (Target pull-up on D-)
    USB_SPEED_FULL         = 3, // 12 Mbps  (Target pull-up on D+)
    USB_SPEED_FAULT        = 4  // Both lines pulled high (illegal SE1)
} UsbBusSpeed;

typedef struct {
    uint8_t         dp_level;      // 0 or 1
    uint8_t         dm_level;      // 0 or 1
    UsbBusStateCode state;         // SE0, SE1, J, K
    UsbBusSpeed     speed;         // Low-Speed or Full-Speed
    bool            is_idle;       // True if bus is in idle state
    bool            is_reset;      // True if SE0 sustained
    uint32_t        duration_us;   // State duration in microseconds
} UsbBusState;

// ─── USB Packet Types & PIDs ─────────────────────────────────────────────────
typedef enum {
    USB_PID_TYPE_TOKEN     = 0,
    USB_PID_TYPE_DATA      = 1,
    USB_PID_TYPE_HANDSHAKE = 2,
    USB_PID_TYPE_SPECIAL   = 3,
    USB_PID_TYPE_INVALID   = 4
} UsbPidType;

// USB 1.1 Standard PIDs (lower 4 bits are PID, upper 4 bits are ~PID)
#define USB_PID_OUT    0x01 // Token: host -> device transaction
#define USB_PID_IN     0x09 // Token: device -> host transaction
#define USB_PID_SOF    0x05 // Start of Frame token (raw PID 0b0101 -> 0xA5)
#define USB_PID_SETUP  0x0D // Token: setup transaction (raw PID 0b1101 -> 0x2D)
#define USB_PID_DATA0  0x03 // Data: even packet toggle
#define USB_PID_DATA1  0x0B // Data: odd packet toggle
#define USB_PID_DATA2  0x07 // Data: high-speed/iso
#define USB_PID_MDATA  0x0F // Data: split/iso
#define USB_PID_ACK    0x02 // Handshake: receiver accepted packet
#define USB_PID_NAK    0x0A // Handshake: receiver cannot accept data
#define USB_PID_STALL  0x0E // Handshake: endpoint halted/error
#define USB_PID_NYET   0x06 // Handshake: not yet ready
#define USB_PID_PRE    0x0C // Special: Low-speed preamble on full-speed bus

typedef struct {
    uint8_t     raw_pid;           // 8-bit PID field (PID[3:0] | ~PID[3:0])
    uint8_t     pid;               // 4-bit decoded PID
    UsbPidType  pid_type;          // Token, Data, Handshake, Special
    const char* pid_name;          // Human-readable string ("SETUP", "IN", "ACK", etc.)
    bool        pid_valid;         // True if (pid & 0x0F) == ((~raw_pid >> 4) & 0x0F)

    // Token & SOF Packet Fields
    uint8_t     addr;              // 7-bit Device Address (0..127)
    uint8_t     endp;              // 4-bit Endpoint Number (0..15)
    uint16_t    frame_num;         // 11-bit SOF Frame Number (0..2047)
    uint8_t     crc5;              // 5-bit received CRC5
    bool        crc5_valid;        // CRC5 match check

    // Data Packet Fields
    uint8_t     data[64];          // Payload bytes
    uint8_t     data_len;          // Number of payload bytes
    uint16_t    crc16;             // 16-bit received CRC16
    bool        crc16_valid;       // CRC16 match check

    // Standard Setup Control Transfer Inspection (if SETUP packet)
    bool        is_setup_desc;
    char        setup_desc[64];
} UsbPacket;

// ─── Core HAL API Functions ──────────────────────────────────────────────────

/**
 * @brief Initialize USB HAL data structures.
 */
void usb_hal_init(void);

/**
 * @brief Acquire GP2 (D+) and GP3 (D-) for USB probing and protocol sniffing.
 */
bool usb_hal_acquire(void);

/**
 * @brief Release USB probe pins and restore GP2..GP3 to Safe High-Z.
 */
void usb_hal_release(void);

/**
 * @brief Check if USB HAL currently owns probe pins.
 */
bool usb_hal_is_active(void);

/**
 * @brief Enable or disable internal weak pull-downs (~50k) on probe pins GP2 (D+) and GP3 (D-).
 * When enabled, floating/disconnected lines read SE0 (0/0) instead of floating to SE1 (1/1).
 */
void usb_hal_set_pulldowns(bool enable);

/**
 * @brief Query whether internal weak pull-downs are enabled on probe pins.
 */
bool usb_hal_get_pulldowns(void);

/**
 * @brief Probe instantaneous electrical state on target D+ and D- lines.
 */
void usb_hal_probe_state(UsbBusState* out_state);

/**
 * @brief Detect target USB bus speed based on line pull-up state.
 */
UsbBusSpeed usb_hal_detect_speed(void);

/**
 * @brief Get human-readable string for USB bus state.
 */
const char* usb_hal_state_to_str(UsbBusStateCode state);

/**
 * @brief Get human-readable string for USB speed.
 */
const char* usb_hal_speed_to_str(UsbBusSpeed speed);

/**
 * @brief Calculate USB CRC5 over 11-bit token data.
 */
uint8_t usb_hal_calc_crc5(uint16_t data);

/**
 * @brief Calculate USB CRC16 over data payload.
 */
uint16_t usb_hal_calc_crc16(const uint8_t* data, size_t len);

/**
 * @brief Decode raw PID byte into packet metadata.
 */
void usb_hal_parse_pid(uint8_t raw_pid, UsbPacket* out_pkt);

/**
 * @brief Parse 8-byte standard USB SETUP packet into human-readable description.
 */
void usb_hal_decode_setup_request(const uint8_t* setup_data, char* out_desc, size_t max_len);

/**
 * @brief Print comprehensive USB probe status table and electrical state to stream.
 */
void usb_hal_print_status(Stream& out);

/**
 * @brief Run live interactive electrical bus monitor.
 * Emits timestamped events (Reset, EOP, Connect, Disconnect, SE0, SE1).
 * Exits when user presses any key or timeout expires.
 */
void usb_hal_run_monitor(Stream& out, uint32_t timeout_ms);

/**
 * @brief Capture and decode USB packets from target bus in real-time.
 */
void usb_hal_run_sniffer(Stream& out, uint32_t max_packets, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // UDOO_USB_HAL_H
