#ifndef PINMUX_MANAGER_H
#define PINMUX_MANAGER_H

#include <stdint.h>
#include "hardware_config.h"
#include "instrument_protocol.h"

void pinmux_manager_init(uint8_t profile = INSTRUMENT_PROFILE_LAB);

// Applies dynamic pin configuration with strict hardware conflict checking
InstrumentPinMuxRespPayload pinmux_apply(const InstrumentPinMuxPayload& req);

// Queries current function mapped to pin
PinFunction pinmux_get_function(uint8_t pin);

// Profile management
uint8_t pinmux_get_profile();
void    pinmux_set_profile(uint8_t profile);

// Pulse sync strobe out to external analyzer (default GP9 / GP10)
void    pinmux_pulse_sync_out(uint32_t duration_us = 1);

// Returns active SYNC_OUT pin
uint8_t pinmux_get_sync_pin();

#endif // PINMUX_MANAGER_H
