#ifndef PIO_CAPTURE_H
#define PIO_CAPTURE_H

#include <Arduino.h>
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "trigger_engine.h"

#define CAPTURE_PIN_BASE    2     // GPIO 2 .. GPIO 9 (8 Channels: Ch0 - Ch7)
#define CAPTURE_PIN_COUNT   8
#define MAX_CAPTURE_SAMPLES 65536 // 64k sample RAM buffer (aligned to 64KB for DMA ring wrapping)

// Legacy TriggerMode enum for backwards compatibility
enum TriggerMode : uint8_t {
    TRIG_IMMEDIATE = 0,
    TRIG_RISING    = 1,
    TRIG_FALLING   = 2,
    TRIG_HIGH      = 3,
    TRIG_LOW       = 4
};

struct LogicAnalyzerConfig {
    uint32_t            sample_rate_hz;   // e.g., 100,000 to 100,000,000 Hz
    uint32_t            sample_count;     // 1024, 2048, 4096, 8192, 16384, 32768, 65536
    uint8_t             trigger_channel;  // 0 to 7 (or PIN_EXT_TRIG)
    uint8_t             trigger_mode;     // Legacy TriggerMode
    uint8_t             enabled_channels; // Bitmask of active channels (0xFF)
    TriggerEngineConfig trigger;          // First-class hardware trigger configuration
};

// Circular DMA SRAM ring buffer
extern uint8_t capture_buffer[MAX_CAPTURE_SAMPLES];

// Chronologically aligned buffer: [PRE-TRIGGER][TRIGGER][POST-TRIGGER]
extern uint8_t aligned_capture_buffer[MAX_CAPTURE_SAMPLES];

extern volatile bool capture_running;
extern volatile bool capture_complete;

void pio_capture_init();
bool pio_capture_start(const LogicAnalyzerConfig& cfg);
void pio_capture_task();
void pio_capture_stop();

bool pio_capture_is_running();
bool pio_capture_is_complete();
const uint8_t* pio_capture_get_aligned_buffer();
uint32_t pio_capture_get_sample_count();
uint32_t pio_capture_get_trigger_offset();
uint32_t get_actual_sample_rate();
uint32_t pio_capture_get_t_trigger_us();
uint32_t pio_capture_get_t_freeze_us();

// Hardware Diagnostic & Benchmark APIs
bool pio_capture_check_fifo_overflow();
void pio_capture_clear_fifo_overflow();

struct Benchmark100MspsResult {
    uint32_t sample_rate_hz;
    uint32_t sample_count;
    uint32_t revolutions_completed;
    uint32_t total_samples_streamed;
    uint32_t elapsed_us;
    float    effective_msps;
    float    bus_utilization_pct;
    bool     fifo_overflow;
    uint32_t trigger_latency_us;
    uint32_t freeze_latency_us;
    bool     buffer_aligned_ok;
};

Benchmark100MspsResult pio_capture_run_100msps_benchmark(uint32_t target_rotations = 10);

#endif // PIO_CAPTURE_H
