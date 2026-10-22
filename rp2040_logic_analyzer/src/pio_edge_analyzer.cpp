#include "pio_edge_analyzer.h"
#include "pio_edge_timer.pio.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include <math.h>

static PIO  edge_pio     = pio1;
static uint edge_sm      = 0;
static int  edge_dma_ch  = -1;
static uint edge_offset  = 0;
static bool edge_inited  = false;

static uint32_t edge_raw_buffer[PIO_EDGE_MAX_PAIRS * 2] __attribute__((aligned(4)));

void pio_edge_analyzer_init() {
    if (edge_inited) return;
    edge_offset = pio_add_program(edge_pio, &pio_edge_timer_program);
    edge_dma_ch = dma_claim_unused_channel(true);
    edge_inited = true;
}

bool pio_edge_measure(uint8_t pin, uint16_t num_pairs, uint32_t timeout_ms, EdgeAnalysisResult& result) {
    if (!edge_inited) pio_edge_analyzer_init();
    if (num_pairs == 0) num_pairs = 64;
    if (num_pairs > PIO_EDGE_MAX_PAIRS) num_pairs = PIO_EDGE_MAX_PAIRS;
    if (timeout_ms == 0) timeout_ms = 500;

    memset(&result, 0, sizeof(result));

    // Clear buffer
    memset(edge_raw_buffer, 0, num_pairs * 2 * sizeof(uint32_t));

    // Initialize PIO state machine for input pin with divider 1.0 (full 133MHz clock)
    pio_edge_timer_init(edge_pio, edge_sm, edge_offset, pin, 1.0f);

    // Configure DMA channel
    dma_channel_config c = dma_channel_get_default_config(edge_dma_ch);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(edge_pio, edge_sm, false));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);

    dma_channel_configure(
        edge_dma_ch,
        &c,
        edge_raw_buffer,               // Destination
        &edge_pio->rxf[edge_sm],       // Source (PIO RX FIFO)
        num_pairs * 2,                 // Total words (HIGH + LOW pairs)
        true                           // Start DMA immediately
    );

    // Drain any leftover data in FIFO before enabling SM
    while (!pio_sm_is_rx_fifo_empty(edge_pio, edge_sm)) {
        (void)edge_pio->rxf[edge_sm];
    }

    // Enable PIO State Machine
    pio_sm_set_enabled(edge_pio, edge_sm, true);

    // Wait for DMA completion or timeout
    uint32_t start_ms = millis();
    bool completed = false;
    while (millis() - start_ms < timeout_ms) {
        if (!dma_channel_is_busy(edge_dma_ch)) {
            completed = true;
            break;
        }
        yield();
    }

    // Stop PIO and abort DMA if timed out
    pio_sm_set_enabled(edge_pio, edge_sm, false);
    if (!completed) {
        dma_channel_abort(edge_dma_ch);
        // Reset pin function
        gpio_set_function(pin, GPIO_FUNC_SIO);
        pinMode(pin, INPUT);
        return false;
    }

    // Reset pin function
    gpio_set_function(pin, GPIO_FUNC_SIO);
    pinMode(pin, INPUT);

    // Conversion constants
    float sys_clk = (float)clock_get_hz(clk_sys);
    float ns_per_cycle = 1000000000.0f / sys_clk;

    // Process edge interval measurements
    // Skip the first pair to avoid partial-cycle artifacts
    uint16_t valid_pulses = 0;
    float sum_period = 0.0f;
    float sum_period_sq = 0.0f;
    float sum_duty = 0.0f;
    float sum_high = 0.0f;
    float sum_low = 0.0f;

    result.min_period_ns = 1e12f;
    result.max_period_ns = 0.0f;
    result.min_freq_hz   = 1e12f;
    result.max_freq_hz   = 0.0f;
    result.min_duty_percent = 100.0f;
    result.max_duty_percent = 0.0f;
    result.min_high_ns = 1e12f;
    result.min_low_ns  = 1e12f;

    for (uint16_t i = 1; i < num_pairs; i++) {
        uint32_t raw_high = edge_raw_buffer[i * 2];
        uint32_t raw_low  = edge_raw_buffer[i * 2 + 1];

        // Filter out idle / overflow states
        if (raw_high == 0 || raw_low == 0 || raw_high > 0x7FFFFFFF || raw_low > 0x7FFFFFFF) {
            continue;
        }

        // Calibrate timing: 2 cycles per tick + state transition overhead
        float high_ns = (raw_high * 2.0f + 4.0f) * ns_per_cycle;
        float low_ns  = (raw_low  * 2.0f + 5.0f) * ns_per_cycle;
        float period_ns = high_ns + low_ns;
        float freq_hz   = 1000000000.0f / period_ns;
        float duty_pct  = (high_ns / period_ns) * 100.0f;

        if (period_ns < result.min_period_ns) result.min_period_ns = period_ns;
        if (period_ns > result.max_period_ns) result.max_period_ns = period_ns;
        if (freq_hz < result.min_freq_hz) result.min_freq_hz = freq_hz;
        if (freq_hz > result.max_freq_hz) result.max_freq_hz = freq_hz;
        if (duty_pct < result.min_duty_percent) result.min_duty_percent = duty_pct;
        if (duty_pct > result.max_duty_percent) result.max_duty_percent = duty_pct;
        if (high_ns < result.min_high_ns) result.min_high_ns = high_ns;
        if (low_ns < result.min_low_ns) result.min_low_ns = low_ns;

        sum_period += period_ns;
        sum_period_sq += (period_ns * period_ns);
        sum_duty += duty_pct;
        sum_high += high_ns;
        sum_low  += low_ns;
        valid_pulses++;
    }

    if (valid_pulses == 0) {
        return false;
    }

    result.pulse_count      = valid_pulses;
    result.avg_period_ns    = sum_period / (float)valid_pulses;
    result.avg_freq_hz      = 1000000000.0f / result.avg_period_ns;
    result.avg_duty_percent = sum_duty / (float)valid_pulses;
    result.avg_high_ns      = sum_high / (float)valid_pulses;
    result.avg_low_ns       = sum_low  / (float)valid_pulses;
    result.jitter_pk_pk_ns  = result.max_period_ns - result.min_period_ns;

    // RMS Jitter (Standard Deviation of Period)
    float variance = (sum_period_sq / (float)valid_pulses) - (result.avg_period_ns * result.avg_period_ns);
    result.jitter_rms_ns = (variance > 0.0f) ? sqrtf(variance) : 0.0f;
    result.valid = true;

    return true;
}

static void format_time_unit(float ns, char* buf, size_t sz) {
    if (ns >= 1000000000.0f) {
        snprintf(buf, sz, "%.4f s", ns / 1000000000.0f);
    } else if (ns >= 1000000.0f) {
        snprintf(buf, sz, "%.3f ms", ns / 1000000.0f);
    } else if (ns >= 1000.0f) {
        snprintf(buf, sz, "%.3f us", ns / 1000.0f);
    } else {
        snprintf(buf, sz, "%.2f ns", ns);
    }
}

static void format_freq_unit(float hz, char* buf, size_t sz) {
    if (hz >= 1000000000.0f) {
        snprintf(buf, sz, "%.4f GHz", hz / 1000000000.0f);
    } else if (hz >= 1000000.0f) {
        snprintf(buf, sz, "%.4f MHz", hz / 1000000.0f);
    } else if (hz >= 1000.0f) {
        snprintf(buf, sz, "%.3f kHz", hz / 1000.0f);
    } else {
        snprintf(buf, sz, "%.2f Hz", hz);
    }
}

void pio_edge_print_report(uint8_t pin, uint16_t num_pairs, Stream& out) {
    out.printf("\r\n=== PIO+DMA Edge & Interval Analyzer (GPIO %u) ===\r\n", pin);
    out.printf("  Capturing %u edge pairs into DMA ring buffer...\r\n", num_pairs);

    EdgeAnalysisResult r;
    if (!pio_edge_measure(pin, num_pairs, 500, r)) {
        out.println("  [Timeout] No edge transitions detected or signal below 2 Hz.");
        out.println("  Make sure probe is connected to a toggling digital signal.");
        return;
    }

    char f_avg[32], f_min[32], f_max[32];
    char p_avg[32], p_min[32], p_max[32], j_pk[32], j_rms[32];
    char t_hi[32], t_lo[32], min_hi[32], min_lo[32];

    format_freq_unit(r.avg_freq_hz, f_avg, sizeof(f_avg));
    format_freq_unit(r.min_freq_hz, f_min, sizeof(f_min));
    format_freq_unit(r.max_freq_hz, f_max, sizeof(f_max));

    format_time_unit(r.avg_period_ns, p_avg, sizeof(p_avg));
    format_time_unit(r.min_period_ns, p_min, sizeof(p_min));
    format_time_unit(r.max_period_ns, p_max, sizeof(p_max));
    format_time_unit(r.jitter_pk_pk_ns, j_pk, sizeof(j_pk));
    format_time_unit(r.jitter_rms_ns, j_rms, sizeof(j_rms));

    format_time_unit(r.avg_high_ns, t_hi, sizeof(t_hi));
    format_time_unit(r.avg_low_ns, t_lo, sizeof(t_lo));
    format_time_unit(r.min_high_ns, min_hi, sizeof(min_hi));
    format_time_unit(r.min_low_ns, min_lo, sizeof(min_lo));

    out.printf("  Analyzed Pulses   : %lu complete cycles (Zero CPU load during capture)\r\n", (unsigned long)r.pulse_count);
    out.printf("  Average Frequency : %s\r\n", f_avg);
    out.printf("  Frequency Spread  : %s .. %s\r\n", f_min, f_max);
    out.printf("  Average Period    : %s\r\n", p_avg);
    out.printf("  Period Range      : %s .. %s\r\n", p_min, p_max);
    out.printf("  Duty Cycle (Avg)  : %.2f%% (Spread: %.1f%% .. %.1f%%)\r\n",
               r.avg_duty_percent, r.min_duty_percent, r.max_duty_percent);
    out.printf("  Pulse Times (H/L) : High: %s | Low: %s\r\n", t_hi, t_lo);
    out.printf("  Peak-to-Peak Jitter: %s (Max - Min Period)\r\n", j_pk);
    out.printf("  RMS Jitter (sigma): %s (Period Standard Deviation)\r\n", j_rms);
    out.printf("  Shortest Pulses   : Min High: %s | Min Low: %s\r\n", min_hi, min_lo);

    // Anomaly / Glitch Detection Warning
    if (r.jitter_pk_pk_ns > (r.avg_period_ns * 0.15f)) {
        out.println("  [!] Notice: Significant period jitter detected (>15% of period).");
    }
    if (r.min_high_ns < (r.avg_high_ns * 0.25f) || r.min_low_ns < (r.avg_low_ns * 0.25f)) {
        out.println("  [!] Warning: Runt pulse / Glitch detected (<25% of average width).");
    }
    out.println("=================================================");
}

void pio_edge_run_continuous(uint8_t pin, Stream& out) {
    out.printf("\r\n[PIO+DMA Live Stream] GPIO %u (Press any key to stop)...\r\n", pin);
    while (!out.available()) {
        EdgeAnalysisResult r;
        if (pio_edge_measure(pin, 128, 200, r) && r.valid) {
            char f_str[32], j_str[32];
            format_freq_unit(r.avg_freq_hz, f_str, sizeof(f_str));
            format_time_unit(r.jitter_rms_ns, j_str, sizeof(j_str));
            out.printf("\r  -> GPIO %u: %-13s | Duty: %5.1f%% | RMS Jitter: %-10s   ",
                       pin, f_str, r.avg_duty_percent, j_str);
        } else {
            out.printf("\r  -> GPIO %u: [No Signal / Idle]                              ", pin);
        }
        delay(150);
    }
    while (out.available()) out.read();
    out.println("\r\nLive stream stopped.");
}
