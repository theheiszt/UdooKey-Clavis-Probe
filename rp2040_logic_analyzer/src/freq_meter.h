#ifndef FREQ_METER_H
#define FREQ_METER_H

#include <Arduino.h>

struct FreqResult {
    float freq_hz;
    float period_s;
    float duty_percent;
    bool  valid;
};

// Measures frequency and duty cycle on the specified pin
FreqResult freq_measure_pin(uint8_t pin, uint32_t gate_ms = 100);

// Single measurement printed to stream
void freq_print_single(uint8_t pin, Stream& out);

// Continuous measurement running until input received on stream
void freq_run_continuous(uint8_t pin, Stream& out);

#endif // FREQ_METER_H
