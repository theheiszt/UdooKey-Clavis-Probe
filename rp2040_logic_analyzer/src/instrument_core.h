#ifndef INSTRUMENT_CORE_H
#define INSTRUMENT_CORE_H

#include <Arduino.h>

#define MAX_EDGE_RECORDS 1024

enum EdgeType : uint8_t {
    EDGE_NONE    = 0,
    EDGE_RISING  = 1,
    EDGE_FALLING = 2,
    EDGE_BOTH    = 3
};

enum EdgeFlags : uint8_t {
    FLAG_NONE        = 0x00,
    FLAG_RUNT_PULSE  = 0x01,  // Pulse width < glitch threshold
    FLAG_LONG_PULSE  = 0x02,  // Pulse width > max threshold
    FLAG_JITTER_HIGH = 0x04,  // Period jitter > 10%
    FLAG_TIMEOUT     = 0x08,  // No transition in window
    FLAG_TRIGGER_HIT = 0x10   // Tripped the trigger condition
};

// Unified Edge Record Primitive
struct EdgeRecord {
    uint32_t timestamp;    // PIO 32-bit timestamp (7.518ns tick @ 133MHz)
    uint8_t  pin;          // Channel index (0..7)
    uint8_t  edge_type;    // 1=RISING, 2=FALLING
    uint32_t period;       // Delta t between consecutive matching edges
    uint32_t pulse_width;  // Duration of active pulse state
    uint8_t  flags;        // EdgeFlags bitmask
};

struct InstrumentStats {
    float    frequency_hz;
    float    duty_cycle_pct;
    float    period_ns;
    float    min_period_ns;
    float    max_period_ns;
    float    jitter_pk_pk_ns;
    float    jitter_rms_ns;
    float    min_high_ns;
    float    min_low_ns;
    uint32_t runt_pulse_count;
    uint32_t total_edges;
    uint32_t burst_length_us;
    bool     valid;
};

// Instrument Core Lifecycle
void instrument_core_init();

// Push raw edge data from PIO/DMA into unified edge records
void instrument_record_edge(uint8_t pin, EdgeType type, uint32_t raw_duration, bool is_high);

// Process the latest captured edge stream and update all derived analytics
void instrument_analyze(uint8_t pin, InstrumentStats& stats);

// Access edge record ring buffer
uint16_t instrument_get_edge_records(EdgeRecord* dest, uint16_t max_records);

// Derived Measurement API Getters
float    instrument_get_frequency(uint8_t pin);
float    instrument_get_duty(uint8_t pin);
float    instrument_get_period(uint8_t pin);
void     instrument_get_jitter(uint8_t pin, float& pk_pk_ns, float& rms_ns);
uint32_t instrument_get_glitch_count(uint8_t pin);

#endif // INSTRUMENT_CORE_H
