#include "gpio_hal.h"
#include "hardware/gpio.h"
#include "hardware/structs/sio.h"

static const char* const s_header_pin_map[UDOO_PROBE_PIN_COUNT] = {
    "Header P1.4 (CH0)",
    "Header P1.5 (CH1)",
    "Header P1.6 (CH2)",
    "Header P1.7 (CH3)",
    "Header P1.9 (CH4)",
    "Header P1.10 (CH5)",
    "Header P1.11 (CH6)",
    "Header P1.12 (CH7)"
};

static GpioHalPullMode s_pull_modes[UDOO_PROBE_PIN_COUNT] = {
    GPIO_HAL_PULL_NONE, GPIO_HAL_PULL_NONE, GPIO_HAL_PULL_NONE, GPIO_HAL_PULL_NONE,
    GPIO_HAL_PULL_NONE, GPIO_HAL_PULL_NONE, GPIO_HAL_PULL_NONE, GPIO_HAL_PULL_NONE
};

bool gpio_hal_resolve_pin(uint8_t pin_or_idx, uint8_t* out_gpio, uint8_t* out_idx) {
    uint8_t gpio = 0xFF;
    uint8_t idx = 0xFF;

    if (pin_or_idx < UDOO_PROBE_PIN_COUNT) {
        // Provided as probe index 0..7
        idx = pin_or_idx;
        gpio = (uint8_t)(UDOO_PROBE_PIN_BASE + idx);
    } else if (pin_or_idx >= UDOO_PROBE_PIN_BASE && 
               pin_or_idx < (UDOO_PROBE_PIN_BASE + UDOO_PROBE_PIN_COUNT)) {
        // Provided as GPIO number 2..9
        gpio = pin_or_idx;
        idx = (uint8_t)(gpio - UDOO_PROBE_PIN_BASE);
    } else {
        // Out of probe bus range - reject protected system pins
        return false;
    }

    if (out_gpio) *out_gpio = gpio;
    if (out_idx)  *out_idx = idx;
    return true;
}

void gpio_hal_init(void) {
    gpio_hal_reset_all_hiz();
}

bool gpio_hal_set_dir(uint8_t pin_or_idx, bool is_output, bool initial_level) {
    uint8_t gpio = 0;
    uint8_t idx = 0;
    if (!gpio_hal_resolve_pin(pin_or_idx, &gpio, &idx)) return false;

    gpio_set_function(gpio, GPIO_FUNC_SIO);

    if (is_output) {
        // Hardware Invariant: Put value to SIO data latch BEFORE enabling output buffer
        // to prevent spurious intermediate output glitches on DUT.
        gpio_put(gpio, initial_level ? 1 : 0);
        gpio_set_dir(gpio, GPIO_OUT);
    } else {
        gpio_set_dir(gpio, GPIO_IN);
    }

    return true;
}

bool gpio_hal_write(uint8_t pin_or_idx, bool level) {
    uint8_t gpio = 0;
    uint8_t idx = 0;
    if (!gpio_hal_resolve_pin(pin_or_idx, &gpio, &idx)) return false;

    // Ensure pin is an output
    if (gpio_get_dir(gpio) != GPIO_OUT) {
        gpio_set_dir(gpio, GPIO_OUT);
    }

    gpio_put(gpio, level ? 1 : 0);
    return true;
}

int gpio_hal_read(uint8_t pin_or_idx) {
    uint8_t gpio = 0;
    uint8_t idx = 0;
    if (!gpio_hal_resolve_pin(pin_or_idx, &gpio, &idx)) return -1;

    return gpio_get(gpio) ? 1 : 0;
}

bool gpio_hal_toggle(uint8_t pin_or_idx) {
    uint8_t gpio = 0;
    uint8_t idx = 0;
    if (!gpio_hal_resolve_pin(pin_or_idx, &gpio, &idx)) return false;

    bool current = gpio_get(gpio);
    if (gpio_get_dir(gpio) != GPIO_OUT) {
        gpio_put(gpio, !current);
        gpio_set_dir(gpio, GPIO_OUT);
    } else {
        gpio_put(gpio, !current);
    }
    return true;
}

bool gpio_hal_set_pull(uint8_t pin_or_idx, GpioHalPullMode pull) {
    uint8_t gpio = 0;
    uint8_t idx = 0;
    if (!gpio_hal_resolve_pin(pin_or_idx, &gpio, &idx)) return false;

    switch (pull) {
        case GPIO_HAL_PULL_NONE:
            gpio_disable_pulls(gpio);
            s_pull_modes[idx] = GPIO_HAL_PULL_NONE;
            break;
        case GPIO_HAL_PULL_UP:
            gpio_pull_up(gpio);
            s_pull_modes[idx] = GPIO_HAL_PULL_UP;
            break;
        case GPIO_HAL_PULL_DOWN:
            gpio_pull_down(gpio);
            s_pull_modes[idx] = GPIO_HAL_PULL_DOWN;
            break;
        default:
            return false;
    }
    return true;
}

void gpio_hal_write_mask(uint8_t dir_mask, uint8_t value_mask) {
    // Write values and set directions across all 8 probe pins
    for (uint8_t idx = 0; idx < UDOO_PROBE_PIN_COUNT; idx++) {
        uint8_t gpio = (uint8_t)(UDOO_PROBE_PIN_BASE + idx);
        gpio_set_function(gpio, GPIO_FUNC_SIO);

        bool is_out = (dir_mask & (1 << idx)) != 0;
        bool val = (value_mask & (1 << idx)) != 0;

        if (is_out) {
            gpio_put(gpio, val);
            gpio_set_dir(gpio, GPIO_OUT);
        } else {
            gpio_set_dir(gpio, GPIO_IN);
        }
    }
}

uint8_t gpio_hal_read_all(void) {
    // Atomic single-cycle read of RP2040 SIO GPIO inputs for GP2..GP9
    uint32_t all_gpio = sio_hw->gpio_in;
    return (uint8_t)((all_gpio >> UDOO_PROBE_PIN_BASE) & 0xFF);
}

void gpio_hal_reset_all_hiz(void) {
    for (uint8_t idx = 0; idx < UDOO_PROBE_PIN_COUNT; idx++) {
        uint8_t gpio = (uint8_t)(UDOO_PROBE_PIN_BASE + idx);
        gpio_set_function(gpio, GPIO_FUNC_SIO);
        gpio_set_dir(gpio, GPIO_IN);
        gpio_disable_pulls(gpio);
        s_pull_modes[idx] = GPIO_HAL_PULL_NONE;
    }
}

bool gpio_hal_get_status(uint8_t pin_or_idx, GpioHalPinStatus* status) {
    if (!status) return false;

    uint8_t gpio = 0;
    uint8_t idx = 0;
    if (!gpio_hal_resolve_pin(pin_or_idx, &gpio, &idx)) return false;

    status->probe_idx       = idx;
    status->gpio_num        = gpio;
    status->is_output       = (gpio_get_dir(gpio) == GPIO_OUT);
    status->logic_level     = gpio_get(gpio);
    status->pull_mode       = s_pull_modes[idx];
    status->header_pin_desc = s_header_pin_map[idx];

    return true;
}
