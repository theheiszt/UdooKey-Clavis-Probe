#ifndef GLITCH_INJECTOR_H
#define GLITCH_INJECTOR_H

#include <Arduino.h>

// Fire a single precision glitch pulse on specified pin
void glitch_fire_pulse(uint8_t pin, uint32_t delay_us, uint32_t pulse_ns);

// Automated glitch timing parameter sweep
void glitch_sweep(uint8_t pin, uint32_t start_delay_us, uint32_t end_delay_us, uint32_t step_us, uint32_t pulse_ns, Stream& out);

#endif // GLITCH_INJECTOR_H
