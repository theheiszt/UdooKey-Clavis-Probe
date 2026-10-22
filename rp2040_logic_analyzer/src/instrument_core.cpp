#include "instrument_core.h"
#include "hardware/clocks.h"
#include <math.h>

static EdgeRecord edge_history[MAX_EDGE_RECORDS];
static uint16_t   edge_head = 0;
static uint16_t   edge_count = 0;
static uint32_t   current_accumulated_timestamp = 0;
static uint32_t   last_matching_edge_timestamp[8] = {0};

static InstrumentStats cached_stats[8];

void instrument_core_init() {
    edge_head = 0;
    edge_count = 0;
    current_accumulated_timestamp = 0;
    memset(edge_history, 0, sizeof(edge_history));
    memset(last_matching_edge_timestamp, 0, sizeof(last_matching_edge_timestamp));
    memset(cached_stats, 0, sizeof(cached_stats));
}

void instrument_record_edge(uint8_t pin, EdgeType type, uint32_t raw_duration, bool is_high) {
    if (pin >= 8) pin = 0;

    // Advance 64-bit continuous timestamp counter
    current_accumulated_timestamp += raw_duration;

    EdgeRecord& rec = edge_history[edge_head];
    rec.timestamp   = current_accumulated_timestamp;
    rec.pin         = pin;
    rec.edge_type   = (uint8_t)type;
    rec.pulse_width = raw_duration;
    rec.flags       = FLAG_NONE;

    // Calculate period relative to previous matching edge
    if (last_matching_edge_timestamp[pin] > 0 && rec.timestamp > last_matching_edge_timestamp[pin]) {
        rec.period = rec.timestamp - last_matching_edge_timestamp[pin];
    } else {
        rec.period = 0;
    }
    last_matching_edge_timestamp[pin] = rec.timestamp;

    // Runt pulse detection (less than 3 cycles = ~22ns)
    if (raw_duration < 4) {
        rec.flags |= FLAG_RUNT_PULSE;
    }

    edge_head = (edge_head + 1) % MAX_EDGE_RECORDS;
    if (edge_count < MAX_EDGE_RECORDS) edge_count++;
}

void instrument_analyze(uint8_t pin, InstrumentStats& stats) {
    if (pin >= 8) pin = 0;
    memset(&stats, 0, sizeof(stats));

    if (edge_count < 4) {
        stats.valid = false;
        return;
    }

    float sys_clk = (float)clock_get_hz(clk_sys);
    float ns_per_cycle = 1000000000.0f / sys_clk;

    stats.min_period_ns = 1e12f;
    stats.max_period_ns = 0.0f;
    stats.min_high_ns   = 1e12f;
    stats.min_low_ns    = 1e12f;

    float sum_period = 0.0f;
    float sum_period_sq = 0.0f;
    float sum_duty = 0.0f;
    uint32_t analyzed_cycles = 0;

    uint32_t start_time = 0;
    uint32_t end_time = 0;

    // Traverse edge history for specified pin
    for (uint16_t i = 0; i < edge_count; i++) {
        uint16_t idx = (edge_head + MAX_EDGE_RECORDS - edge_count + i) % MAX_EDGE_RECORDS;
        const EdgeRecord& r = edge_history[idx];

        if (r.pin != pin) continue;

        if (start_time == 0) start_time = r.timestamp;
        end_time = r.timestamp;
        stats.total_edges++;

        if (r.flags & FLAG_RUNT_PULSE) {
            stats.runt_pulse_count++;
        }

        if (r.period > 0) {
            float p_ns = (float)r.period * ns_per_cycle;
            if (p_ns < stats.min_period_ns) stats.min_period_ns = p_ns;
            if (p_ns > stats.max_period_ns) stats.max_period_ns = p_ns;

            sum_period += p_ns;
            sum_period_sq += (p_ns * p_ns);

            float pw_ns = (float)r.pulse_width * ns_per_cycle;
            if (r.edge_type == EDGE_RISING) {
                if (pw_ns < stats.min_high_ns) stats.min_high_ns = pw_ns;
            } else {
                if (pw_ns < stats.min_low_ns) stats.min_low_ns = pw_ns;
            }

            float d_pct = (pw_ns / p_ns) * 100.0f;
            if (d_pct > 0.0f && d_pct <= 100.0f) {
                sum_duty += d_pct;
            }
            analyzed_cycles++;
        }
    }

    if (analyzed_cycles == 0) {
        stats.valid = false;
        return;
    }

    stats.period_ns       = sum_period / (float)analyzed_cycles;
    stats.frequency_hz    = 1000000000.0f / stats.period_ns;
    stats.duty_cycle_pct  = sum_duty / (float)analyzed_cycles;
    stats.jitter_pk_pk_ns = stats.max_period_ns - stats.min_period_ns;

    float variance = (sum_period_sq / (float)analyzed_cycles) - (stats.period_ns * stats.period_ns);
    stats.jitter_rms_ns   = (variance > 0.0f) ? sqrtf(variance) : 0.0f;

    stats.burst_length_us = (uint32_t)((float)(end_time - start_time) * ns_per_cycle / 1000.0f);
    stats.valid           = true;

    cached_stats[pin] = stats;
}

uint16_t instrument_get_edge_records(EdgeRecord* dest, uint16_t max_records) {
    if (!dest || max_records == 0) return 0;
    uint16_t count = (max_records < edge_count) ? max_records : edge_count;

    for (uint16_t i = 0; i < count; i++) {
        uint16_t src_idx = (edge_head + MAX_EDGE_RECORDS - count + i) % MAX_EDGE_RECORDS;
        dest[i] = edge_history[src_idx];
    }
    return count;
}

float instrument_get_frequency(uint8_t pin) {
    if (pin >= 8) pin = 0;
    if (!cached_stats[pin].valid) {
        instrument_analyze(pin, cached_stats[pin]);
    }
    return cached_stats[pin].frequency_hz;
}

float instrument_get_duty(uint8_t pin) {
    if (pin >= 8) pin = 0;
    if (!cached_stats[pin].valid) {
        instrument_analyze(pin, cached_stats[pin]);
    }
    return cached_stats[pin].duty_cycle_pct;
}

float instrument_get_period(uint8_t pin) {
    if (pin >= 8) pin = 0;
    if (!cached_stats[pin].valid) {
        instrument_analyze(pin, cached_stats[pin]);
    }
    return cached_stats[pin].period_ns;
}

void instrument_get_jitter(uint8_t pin, float& pk_pk_ns, float& rms_ns) {
    if (pin >= 8) pin = 0;
    if (!cached_stats[pin].valid) {
        instrument_analyze(pin, cached_stats[pin]);
    }
    pk_pk_ns = cached_stats[pin].jitter_pk_pk_ns;
    rms_ns   = cached_stats[pin].jitter_rms_ns;
}

uint32_t instrument_get_glitch_count(uint8_t pin) {
    if (pin >= 8) pin = 0;
    if (!cached_stats[pin].valid) {
        instrument_analyze(pin, cached_stats[pin]);
    }
    return cached_stats[pin].runt_pulse_count;
}
