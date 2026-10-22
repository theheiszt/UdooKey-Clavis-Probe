#ifndef UDOO_I2C_HAL_H
#define UDOO_I2C_HAL_H

#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── UDOO KEY Probe Bus I2C Physical Pinout ──────────────────────────────────
// Header P1 Pins:
// SDA : GP2 (Header P1 Pin 4, Probe CH0)
// SCL : GP3 (Header P1 Pin 5, Probe CH1)
#define UDOO_I2C_SDA_PIN       2
#define UDOO_I2C_SCL_PIN       3

#define UDOO_I2C_DEFAULT_SPEED 400000   // 400 kHz Fast Mode default
#define UDOO_I2C_MIN_SPEED     10000    // 10 kHz min
#define UDOO_I2C_MAX_SPEED     1000000  // 1 MHz Fast Mode Plus max

typedef struct {
    uint32_t speed_hz;          // Current clock rate (e.g. 100000, 400000, 1000000)
    bool     pullups_enabled;   // true if internal RP2040 pull-ups active
    bool     active;            // true if I2C peripheral is acquired
} I2cHalConfig;

typedef struct {
    uint8_t     addr;           // 7-bit address (0x08..0x77)
    bool        write_ack;      // Acknowledged on write
    bool        read_ack;       // Acknowledged on read
    const char* part_name;      // Known device name (or NULL)
    const char* category;       // Category description (or NULL)
} I2cScanEntry;

/**
 * @brief Initialize I2C HAL data structures.
 */
void i2c_hal_init(void);

/**
 * @brief Acquire and configure RP2040 Hardware I2C1 on GP2 (SDA) and GP3 (SCL).
 * @param speed_hz Bus clock rate (10000 to 1000000 Hz, default 400000)
 * @param enable_pullups Enable RP2040 internal weak pull-ups (~50k) on SDA/SCL
 * @return true on success
 */
bool i2c_hal_acquire(uint32_t speed_hz, bool enable_pullups);

/**
 * @brief Release I2C peripheral and restore GP2 and GP3 to Safe High-Z.
 */
void i2c_hal_release(void);

/**
 * @brief Set or change bus speed dynamically.
 */
bool i2c_hal_set_speed(uint32_t speed_hz);

/**
 * @brief Enable or disable internal pull-ups on SDA/SCL.
 */
void i2c_hal_set_pullups(bool enable);

/**
 * @brief Get copy of active I2C configuration.
 */
void i2c_hal_get_config(I2cHalConfig* out_cfg);

/**
 * @brief Test single 7-bit address for ACK on Write.
 * @return true if ACK received, false if NACK or bus error
 */
bool i2c_hal_probe_addr(uint8_t addr_7bit);

/**
 * @brief Test single 7-bit address for ACK on Read.
 * @return true if ACK received, false if NACK or bus error
 */
bool i2c_hal_probe_addr_read(uint8_t addr_7bit);

/**
 * @brief Look up known device description by 7-bit address.
 * @param addr_7bit 7-bit I2C address
 * @param out_part Pointer to receive part name string pointer
 * @param out_cat Pointer to receive category string pointer
 * @return true if known device found, false if unknown
 */
bool i2c_hal_identify_device(uint8_t addr_7bit, const char** out_part, const char** out_cat);

/**
 * @brief Perform comprehensive 7-bit I2C bus scan (0x08..0x77).
 * Prints formatted table and device identification to output stream.
 * @param out Stream for printing
 * @param verbose If true, lists full device descriptions
 * @return Number of devices found
 */
uint16_t i2c_hal_scan(Stream& out, bool verbose);

/**
 * @brief Write data bytes to 7-bit I2C address.
 * @param addr_7bit 7-bit target address
 * @param data Buffer of bytes to write
 * @param len Number of bytes to write
 * @param send_stop true = send STOP condition, false = repeated START
 * @return 0 = Success (ACK), 1 = Data too long, 2 = NACK on address, 3 = NACK on data, 4 = Other error
 */
uint8_t i2c_hal_write(uint8_t addr_7bit, const uint8_t* data, size_t len, bool send_stop);

/**
 * @brief Read bytes from 7-bit I2C address.
 * @param addr_7bit 7-bit target address
 * @param buffer Output buffer
 * @param len Number of bytes to read
 * @param send_stop true = send STOP condition
 * @return Number of bytes actually read
 */
size_t i2c_hal_read(uint8_t addr_7bit, uint8_t* buffer, size_t len, bool send_stop);

/**
 * @brief Execute Bus Pirate format I2C command syntax.
 * Supports: [ (START), ] (STOP), 0xNN (Write), r:N / r (Read), etc.
 * @param cmd_str Command string to parse and execute
 * @param out Stream for output
 * @return true if successfully processed
 */
bool i2c_hal_exec_bp_syntax(const char* cmd_str, Stream& out);

#ifdef __cplusplus
}
#endif

#endif // UDOO_I2C_HAL_H
