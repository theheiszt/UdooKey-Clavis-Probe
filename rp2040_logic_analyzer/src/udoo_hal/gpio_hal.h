#ifndef UDOO_GPIO_HAL_H
#define UDOO_GPIO_HAL_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── UDOO KEY Probe Bus Pin Mappings ─────────────────────────────────────────
// Header P1 Pins 4..12 map strictly to RP2040 GP2..GP9.
// Protected system pins (GP0, GP1, GP10..GP29) CANNOT be touched by this HAL.
#define UDOO_PROBE_PIN_BASE     2
#define UDOO_PROBE_PIN_COUNT    8

typedef enum {
    GPIO_HAL_PULL_NONE = 0,
    GPIO_HAL_PULL_UP   = 1,
    GPIO_HAL_PULL_DOWN = 2
} GpioHalPullMode;

typedef struct {
    uint8_t         probe_idx;        // Probe index 0..7
    uint8_t         gpio_num;         // Physical RP2040 GPIO 2..9
    bool            is_output;        // true = OUTPUT, false = INPUT
    bool            logic_level;      // true = HIGH (1), false = LOW (0)
    GpioHalPullMode pull_mode;        // NONE, UP, or DOWN
    const char*     header_pin_desc;  // Physical connector description (e.g., "P1.4")
} GpioHalPinStatus;

/**
 * @brief Initialize all probe pins (GP2..GP9) into safe High-Z input state.
 * Disables internal pull-ups and pull-downs.
 */
void gpio_hal_init(void);

/**
 * @brief Translate either a probe index (0..7) or GPIO number (2..9) into valid pin mappings.
 * @param pin_or_idx Probe index (0..7) or GPIO pin number (2..9)
 * @param out_gpio Pointer to receive physical GPIO number (2..9), nullable
 * @param out_idx Pointer to receive probe index (0..7), nullable
 * @return true if valid probe pin, false if invalid or protected pin
 */
bool gpio_hal_resolve_pin(uint8_t pin_or_idx, uint8_t* out_gpio, uint8_t* out_idx);

/**
 * @brief Configure direction of a probe pin.
 * Invariant: If setting to OUTPUT, initial_level is written to data register
 * BEFORE driving the direction register to prevent output glitching.
 */
bool gpio_hal_set_dir(uint8_t pin_or_idx, bool is_output, bool initial_level);

/**
 * @brief Drive logic level on a probe pin (must be configured as OUTPUT).
 */
bool gpio_hal_write(uint8_t pin_or_idx, bool level);

/**
 * @brief Read digital logic level from a probe pin.
 * @return 0 or 1 on success, -1 on invalid pin
 */
int gpio_hal_read(uint8_t pin_or_idx);

/**
 * @brief Invert the output logic level of a probe pin.
 */
bool gpio_hal_toggle(uint8_t pin_or_idx);

/**
 * @brief Set pull resistor configuration on a probe pin.
 */
bool gpio_hal_set_pull(uint8_t pin_or_idx, GpioHalPullMode pull);

/**
 * @brief Write multi-bit pattern to probe bus (similar to Bus Pirate dio_write).
 * @param dir_mask Bitmask of pins to make outputs (bit 0 = Probe 0 / GP2, bit 7 = Probe 7 / GP9)
 * @param value_mask Bitmask of output values
 */
void gpio_hal_write_mask(uint8_t dir_mask, uint8_t value_mask);

/**
 * @brief Read all 8 probe pins simultaneously.
 * @return 8-bit integer with bit 0 = Probe 0 (GP2) through bit 7 = Probe 7 (GP9)
 */
uint8_t gpio_hal_read_all(void);

/**
 * @brief Restore all probe pins (GP2..GP9) to high-impedance inputs without pulls.
 */
void gpio_hal_reset_all_hiz(void);

/**
 * @brief Retrieve detailed pin status for diagnostics.
 */
bool gpio_hal_get_status(uint8_t pin_or_idx, GpioHalPinStatus* status);

#ifdef __cplusplus
}
#endif

#endif // UDOO_GPIO_HAL_H
