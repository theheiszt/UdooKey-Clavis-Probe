#include "pinmux_manager.h"
#include <Arduino.h>
#include "hardware/gpio.h"
#include "acquisition_api.h"

static uint8_t s_active_profile = INSTRUMENT_PROFILE_LAB;
static PinFunction s_pin_functions[30];
static uint8_t s_sync_out_pin = PIN_SYNC_OUT; // GP9 default

void pinmux_manager_init(uint8_t profile) {
    s_active_profile = profile;

    for (int p = 0; p < 30; p++) {
        s_pin_functions[p] = PIN_FN_DEFAULT;
    }

    // Default mappings for Logic Probes
    for (int p = PIN_LA_CH0; p <= PIN_LA_CH6; p++) {
        s_pin_functions[p] = PIN_FN_LOGIC_PROBE;
        gpio_init(p);
        gpio_set_dir(p, GPIO_IN);
    }

    if (profile == INSTRUMENT_PROFILE_LAB) {
        // In Laboratory profile, GP9 defaults to SYNC_OUT for instrument synchronization
        s_pin_functions[PIN_SYNC_OUT] = PIN_FN_SYNC_OUT;
        s_sync_out_pin = PIN_SYNC_OUT;
        gpio_init(s_sync_out_pin);
        gpio_set_dir(s_sync_out_pin, GPIO_OUT);
        gpio_put(s_sync_out_pin, 0);

        // GP10 is External Trigger
        s_pin_functions[PIN_EXT_TRIG] = PIN_FN_EXT_TRIG;
        gpio_init(PIN_EXT_TRIG);
        gpio_set_dir(PIN_EXT_TRIG, GPIO_IN);
        gpio_pull_down(PIN_EXT_TRIG);
    } else {
        // Generic profile: GP9 is Logic Analyzer CH7
        s_pin_functions[PIN_LA_CH7] = PIN_FN_LOGIC_PROBE;
        gpio_init(PIN_LA_CH7);
        gpio_set_dir(PIN_LA_CH7, GPIO_IN);

        s_pin_functions[PIN_EXT_TRIG] = PIN_FN_EXT_TRIG;
        gpio_init(PIN_EXT_TRIG);
        gpio_set_dir(PIN_EXT_TRIG, GPIO_IN);
    }

    // ADC pins
    s_pin_functions[PIN_ADC0] = PIN_FN_ADC_INPUT;
    s_pin_functions[PIN_ADC1] = PIN_FN_ADC_INPUT;
    s_pin_functions[PIN_ADC2] = PIN_FN_ADC_INPUT;
}

InstrumentPinMuxRespPayload pinmux_apply(const InstrumentPinMuxPayload& req) {
    InstrumentPinMuxRespPayload resp;
    memset(&resp, 0, sizeof(resp));
    resp.pin = req.pin;

    if (req.pin >= 30) {
        resp.status_code = INST_ERR_INVALID_PARAM;
        strncpy(resp.reason, "Pin out of range (0..29)", sizeof(resp.reason) - 1);
        return resp;
    }

    // 1. Check permanently locked hardware system pins
    if ((1UL << req.pin) & RP2040_LOCKED_PINS_MASK) {
        resp.status_code = INST_ERR_PIN_CONFLICT;
        strncpy(resp.reason, "Protected system pin locked", sizeof(resp.reason) - 1);
        return resp;
    }

    // 2. Check active subsystem contention
    if (acquisition_is_running()) {
        if (req.pin >= PIN_LA_CH0 && req.pin <= PIN_LA_CH7) {
            resp.status_code = INST_ERR_PIN_CONFLICT;
            strncpy(resp.reason, "Logic capture active on pin", sizeof(resp.reason) - 1);
            return resp;
        }
    }

    // 3. Apply requested function safely
    switch ((PinFunction)req.function) {
        case PIN_FN_DEFAULT:
        case PIN_FN_GPIO_IN:
            gpio_init(req.pin);
            gpio_set_dir(req.pin, GPIO_IN);
            if (req.pull_mode == 1) gpio_pull_up(req.pin);
            else if (req.pull_mode == 2) gpio_pull_down(req.pin);
            else gpio_disable_pulls(req.pin);
            break;

        case PIN_FN_GPIO_OUT:
            gpio_init(req.pin);
            gpio_set_dir(req.pin, GPIO_OUT);
            gpio_put(req.pin, 0);
            break;

        case PIN_FN_SYNC_OUT:
            gpio_init(req.pin);
            gpio_set_dir(req.pin, GPIO_OUT);
            gpio_put(req.pin, 0);
            s_sync_out_pin = req.pin;
            break;

        case PIN_FN_EXT_TRIG:
            gpio_init(req.pin);
            gpio_set_dir(req.pin, GPIO_IN);
            gpio_pull_down(req.pin);
            break;

        case PIN_FN_LOGIC_PROBE:
            gpio_init(req.pin);
            gpio_set_dir(req.pin, GPIO_IN);
            gpio_disable_pulls(req.pin);
            break;

        case PIN_FN_ADC_INPUT:
            if (req.pin < 26 || req.pin > 28) {
                resp.status_code = INST_ERR_INVALID_PARAM;
                strncpy(resp.reason, "Pin is not ADC capable", sizeof(resp.reason) - 1);
                return resp;
            }
            break;

        default:
            gpio_init(req.pin);
            gpio_set_dir(req.pin, GPIO_IN);
            break;
    }

    s_pin_functions[req.pin] = (PinFunction)req.function;
    resp.status_code = INST_ERR_OK;
    strncpy(resp.reason, "OK", sizeof(resp.reason) - 1);
    return resp;
}

PinFunction pinmux_get_function(uint8_t pin) {
    if (pin < 30) return s_pin_functions[pin];
    return PIN_FN_DEFAULT;
}

uint8_t pinmux_get_profile() {
    return s_active_profile;
}

void pinmux_set_profile(uint8_t profile) {
    pinmux_manager_init(profile);
}

void pinmux_pulse_sync_out(uint32_t duration_us) {
    if (s_sync_out_pin < 30) {
        gpio_put(s_sync_out_pin, 1);
        if (duration_us == 1) {
            busy_wait_us_32(1);
        } else {
            delayMicroseconds(duration_us);
        }
        gpio_put(s_sync_out_pin, 0);
    }
}

uint8_t pinmux_get_sync_pin() {
    return s_sync_out_pin;
}

