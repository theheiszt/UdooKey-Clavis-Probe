#ifndef ACQUISITION_API_H
#define ACQUISITION_API_H

#include <Arduino.h>
#include "trigger_engine.h"
#include "pio_capture.h"

// ─── Unified Acquisition Configuration ───────────────────────────────────────
struct CaptureConfig {
    uint32_t            sample_rate_hz;   // e.g. 25M, 50M, 62.5M, 75M, 100M
    uint32_t            sample_count;     // 1024 to 65536 (power of 2)
    uint8_t             enabled_channels; // 8-bit mask (0xFF = GP2..GP9)
    TriggerEngineConfig trigger;          // Composable hardware trigger definition
};

// ─── Hardware Latency Profile Breakdown ──────────────────────────────────────
struct HardwareLatencyProfile {
    // 1. Hardware Trigger Resolution: sample clock interval (10.0 ns @ 100 MSPS)
    float    trigger_resolution_ns;
    // 2. Trigger Recognition Latency: sample clock cycles within synchronous PIO domain
    uint32_t trigger_recognition_clocks;
    // 3. Capture Freeze Latency: sample clock cycles from post-sample match to DMA abort
    uint32_t capture_freeze_latency_clocks;
    // 4. Software Publication Latency: buffer re-alignment, dual-CRC16 computation & DATA_READY assert
    uint32_t software_publication_latency_us;

    // Detailed Microsecond Event Timeline
    uint32_t t_physical_event_us;     // T0: Physical trigger transition
    uint32_t t_pio_detect_ns;         // T1: PIO clock domain recognition
    uint32_t t_fsm_decision_ns;       // T2: Trigger FSM decision match
    uint32_t t_post_collect_us;       // T3: Post-trigger samples collection duration
    uint32_t t_dma_freeze_us;         // T4: DMA stopped / state machine disabled
    uint32_t t_descriptor_ready_us;   // T5: Buffer re-aligned & dual CRC-16 computed
    uint32_t t_data_ready_us;         // T6: DATA_READY line asserted to ESP32
    uint32_t t_esp32_ack_us;          // T7: ESP32 response / fetch latency
};

// ─── Unified Acquisition Result ──────────────────────────────────────────────
struct CaptureResult {
    uint32_t               capture_id;
    uint32_t               sample_rate_hz;
    uint32_t               sample_count;
    uint32_t               trigger_index;
    uint32_t               timestamp_us;
    uint16_t               whole_capture_crc;
    const uint8_t*         samples;           // Chronologically ordered [PRE][TRIG][POST]
    HardwareLatencyProfile latency;
    bool                   fifo_overflow;     // True if any RX FIFO stall occurred
    uint32_t               revolutions_completed; // Circular SRAM wraps before trigger
    bool                   valid;
};

// ─── Stress Test & Pattern Test Results ───────────────────────────────────────
struct StressTestStageResult {
    uint32_t sample_rate_hz;
    uint32_t duration_ms;
    uint64_t total_samples_streamed;
    uint32_t revolutions_completed;
    float    effective_msps;
    float    crossbar_bus_load_pct;
    bool     fifo_overflow;
    bool     dma_pointer_continuous;
    bool     trigger_verified;
    bool     concurrent_display_ok;
    bool     concurrent_comm_ok;
    bool     passed;
};

struct StressTestSummary {
    uint8_t               stages_run;
    uint8_t               stages_passed;
    StressTestStageResult stages[6];
    bool                  overall_pass;
};

struct PatternIntegrityResult {
    uint64_t samples_tested;
    uint64_t samples_correct;
    uint32_t missing_samples;
    uint32_t duplicated_samples;
    bool     word_alignment_ok;
    bool     ring_wrap_ok;
    bool     trigger_alignment_ok;
    bool     passed;
};

// ─── Hardware Validation 1.0 Data Structures ─────────────────────────────────
struct FrequencySweepStage {
    uint32_t target_freq_hz;
    uint32_t actual_gen_freq_hz;
    uint32_t measured_freq_hz;
    float    expected_period_ns;
    float    mean_period_ns;
    float    min_period_ns;
    float    max_period_ns;
    float    rms_jitter_ns;
    float    error_percent;
    uint32_t edges_analyzed;
    bool     nyquist_limit; // True if >= 100 MHz @ 100 MSPS (sampler validation mode)
    bool     passed;
};

struct FrequencySweepResult {
    uint8_t             stages_count;
    uint8_t             stages_passed;
    FrequencySweepStage stages[8]; // 1M, 5M, 10M, 25M, 50M, 62.5M, 75M, 100M
    bool                passed;
};

struct ExternalTriggerResult {
    uint32_t target_index;       // e.g. 32768
    uint32_t total_iterations;   // e.g. 1000 or 10000
    uint32_t exact_hits;         // landed exactly on target_index
    uint32_t jitter_minus_1;     // landed on target_index - 1
    uint32_t jitter_plus_1;      // landed on target_index + 1
    uint32_t outliers;           // landed outside +-1 sample
    float    determinism_pct;    // percentage of exact hits
    float    max_jitter_ns;      // max deviation in ns
    float    std_dev_samples;    // standard deviation in sample units
    bool     passed;             // 100% deterministic
};

struct SystemStressResult {
    uint32_t duration_ms;
    uint64_t samples_acquired;
    uint32_t revolutions_completed;
    uint32_t fifo_overflow_count;
    uint32_t samples_dropped;
    uint32_t dma_errors;
    uint32_t trigger_errors;
    uint32_t capture_crc_errors;
    uint32_t ui_refresh_count;
    uint32_t ui_corruptions;
    uint32_t uart_packets_exchanged;
    uint32_t spi_transactions_serviced;
    bool     passed;
};

struct SelfTestItem {
    const char* name;
    bool        passed;
    const char* details;
};

struct SelfTestResult {
    uint8_t      total_tests;
    uint8_t      passed_tests;
    SelfTestItem items[17];
    bool         all_passed;
};

// ─── Acquisition Engine API Methods ──────────────────────────────────────────
void acquisition_init();
bool acquisition_configure(const CaptureConfig& cfg);
bool acquisition_arm();
void acquisition_poll();
void acquisition_abort();
bool acquisition_is_running();
bool acquisition_is_ready();
void acquisition_clear_ready();
void acquisition_publish_result(const uint8_t* buf, uint32_t samples, uint32_t rate_hz,
                                uint32_t trig_idx, uint32_t t_trig_us, uint32_t t_freeze_us,
                                uint8_t trig_ch = 0, uint8_t trig_mode = 0);
const CaptureResult& acquisition_get_result();

// ─── Automated Validation Suites ─────────────────────────────────────────────
StressTestSummary      acquisition_run_stress_test(Stream& out, bool include_10s_test = true);
PatternIntegrityResult acquisition_run_pattern_test(uint32_t test_cycles, Stream& out);
FrequencySweepResult   acquisition_run_frequency_sweep(Stream& out);
ExternalTriggerResult  acquisition_run_ext_trigger_test(uint32_t iterations, Stream& out);
SystemStressResult     acquisition_run_system_stress_test(Stream& out, uint32_t duration_ms = 3000);
SelfTestResult         acquisition_run_self_test(Stream& out);
void                   acquisition_print_latency_profile(const CaptureResult& res, Stream& out);

#endif // ACQUISITION_API_H
