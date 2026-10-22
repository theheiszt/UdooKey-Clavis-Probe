#ifndef UDOO_ONEWIRE_HAL_H
#define UDOO_ONEWIRE_HAL_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── UDOO KEY Probe Bus 1-Wire Physical Pinout ───────────────────────────────
// Header P1 Pin 8 maps to RP2040 GP6 (Probe CH4)
#define UDOO_ONEWIRE_DEFAULT_PIN    6

typedef struct {
    uint8_t pin;                // Active physical GPIO (default GP6)
    bool    pullup_enabled;     // true if internal weak pull-up active
    bool    active;             // true if 1-Wire mode is active
} OneWireHalConfig;

typedef struct {
    uint8_t     rom[8];         // 64-bit ROM ID (Family, Serial, CRC)
    bool        crc_valid;      // CRC8 matches
    const char* family_name;    // Known device description
} OneWireDeviceEntry;

/**
 * @brief Initialize 1-Wire HAL state.
 */
void onewire_hal_init(void);

/**
 * @brief Acquire probe pin for Dallas 1-Wire bus.
 * @param pin Physical GPIO pin (default GP6)
 * @param pullup Enable RP2040 internal weak pull-up (~50k)
 * @return true on success
 */
bool onewire_hal_acquire(uint8_t pin, bool pullup);

/**
 * @brief Release 1-Wire bus and restore probe pin to Safe High-Z.
 */
void onewire_hal_release(void);

/**
 * @brief Reconfigure active probe pin.
 */
bool onewire_hal_set_pin(uint8_t pin);

/**
 * @brief Toggle internal weak pull-up resistor.
 */
void onewire_hal_set_pullup(bool enable);

/**
 * @brief Get copy of active 1-Wire configuration.
 */
void onewire_hal_get_config(OneWireHalConfig* out_cfg);

/**
 * @brief Issue 1-Wire Reset pulse (480us) and detect Presence pulse.
 * @return true if device presence pulse detected (ACK), false if line held high (NACK)
 */
bool onewire_hal_reset(void);

/**
 * @brief Transmit single bit across 1-Wire bus.
 */
void onewire_hal_write_bit(uint8_t bit);

/**
 * @brief Sample single bit from 1-Wire bus.
 */
uint8_t onewire_hal_read_bit(void);

/**
 * @brief Transmit 8 bits (LSB first) across 1-Wire bus.
 */
void onewire_hal_write_byte(uint8_t byte);

/**
 * @brief Receive 8 bits (LSB first) from 1-Wire bus.
 */
uint8_t onewire_hal_read_byte(void);

/**
 * @brief Calculate Dallas 1-Wire CRC8 polynomial (X^8 + X^5 + X^4 + 1, 0x8C).
 */
uint8_t onewire_hal_crc8(const uint8_t* data, size_t len);

/**
 * @brief Look up human-readable description for 1-Wire family code.
 */
const char* onewire_hal_get_family_desc(uint8_t family_code);

/**
 * @brief Perform full 1-Wire ROM search algorithm (0xF0) and print report.
 * @param out Stream to print report
 * @return Number of devices found on bus
 */
uint16_t onewire_hal_search_bus(Stream& out);

/**
 * @brief Broadcast temperature conversion and read all DS18B20/DS18S20/DS1822 sensors.
 * @param out Stream to print temperature readings
 * @return Number of temperature sensors read
 */
uint16_t onewire_hal_read_all_temps(Stream& out);

/**
 * @brief Read 64-bit ROM for single device on bus (0x33 Read ROM command).
 * @param out_rom Buffer of 8 bytes
 * @return true if valid ROM read and CRC verified
 */
bool onewire_hal_read_single_rom(uint8_t out_rom[8]);

#ifdef __cplusplus
}
#endif

#endif // UDOO_ONEWIRE_HAL_H
