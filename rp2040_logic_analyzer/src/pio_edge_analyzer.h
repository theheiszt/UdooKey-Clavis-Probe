#ifndef PIO_EDGE_ANALYZER_H
#define PIO_EDGE_ANALYZER_H

#include <Arduino.h>

#define PIO_EDGE_MAX_PAIRS 512 // 512 HIGH + 512 LOW intervals = 1024 words (4KB)

struct EdgeAnalysisResult {
    float    avg_freq_hz;
    float    min_freq_hz;
    float    max_freq_hz;
    float    avg_period_ns;
    float    min_period_ns;
    float    max_period_ns;
    float    jitter_pk_pk_ns;
    float    jitter_rms_ns;
    float    avg_duty_percent;
    float    min_duty_percent;
    float    max_duty_percent;
    float    avg_high_ns;
    float    avg_low_ns;
    float    min_high_ns;
    float    min_low_ns;
    uint32_t pulse_count;
    bool     valid;
};

// Initialize PIO Edge Timing Subsystem
void pio_edge_analyzer_init();

// Capture edge intervals on ANY GPIO pin via PIO + DMA
bool pio_edge_measure(uint8_t pin, uint16_t num_pairs, uint32_t timeout_ms, EdgeAnalysisResult& result);

// Print formatted diagnostic and jitter report to stream
void pio_edge_print_report(uint8_t pin, uint16_t num_pairs, Stream& out);

// Continuous live streaming measurement
void pio_edge_run_continuous(uint8_t pin, Stream& out);

#endif // PIO_EDGE_ANALYZER_H
