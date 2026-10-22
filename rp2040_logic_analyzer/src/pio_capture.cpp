#include "pio_capture.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "logic_analyzer.pio.h"

alignas(65536) uint8_t capture_buffer[MAX_CAPTURE_SAMPLES];
uint8_t aligned_capture_buffer[MAX_CAPTURE_SAMPLES] __attribute__((aligned(4)));

volatile bool capture_running  = false;
volatile bool capture_complete = false;

static PIO   pio_inst           = pio0;
static uint  sm_inst            = 0;
static int   dma_chan           = -1;
static uint  pio_offset         = 0;
static float current_clkdiv     = 1.0f;
static uint32_t active_samples  = 8192;
static uint32_t last_eval_pos   = 0;
static uint32_t start_time_ms   = 0;
static uint32_t t_trigger_micros = 0;
static uint32_t t_freeze_micros  = 0;
static LogicAnalyzerConfig active_cfg;

void pio_capture_init() {
    pio_offset = pio_add_program(pio_inst, &logic_analyzer_program);
    dma_chan   = dma_claim_unused_channel(true);
    trigger_engine_init();
}

bool pio_capture_start(const LogicAnalyzerConfig& cfg) {
    if (capture_running) return false;

    active_cfg = cfg;
    capture_complete = false;

    // Power-of-2 clamping for hardware DMA circular ring wrapping
    uint32_t samples = cfg.sample_count;
    if (samples > MAX_CAPTURE_SAMPLES) samples = MAX_CAPTURE_SAMPLES;
    if (samples < 16) samples = 16;
    active_cfg.sample_count = samples;

    // Round up to nearest power of 2
    uint32_t p2 = 1024;
    uint8_t ring_bits = 10;
    while (p2 < samples && ring_bits < 16) {
        p2 <<= 1;
        ring_bits++;
    }
    active_samples = p2;

    // Backwards compatibility mapping for legacy trigger fields
    if (active_cfg.trigger.mode == TRIG_MODE_AUTO && active_cfg.trigger_mode != TRIG_IMMEDIATE) {
        active_cfg.trigger.channel = active_cfg.trigger_channel;
        switch (active_cfg.trigger_mode) {
            case TRIG_RISING:  active_cfg.trigger.mode = TRIG_MODE_EDGE_RISING; break;
            case TRIG_FALLING: active_cfg.trigger.mode = TRIG_MODE_EDGE_FALLING; break;
            case TRIG_HIGH:    active_cfg.trigger.mode = TRIG_MODE_LEVEL_HIGH; break;
            case TRIG_LOW:     active_cfg.trigger.mode = TRIG_MODE_LEVEL_LOW; break;
            default: break;
        }
    }

    // Default pre-trigger percentage: 70% (Section 11)
    if (active_cfg.trigger.pre_trigger_pct == 0 && active_cfg.trigger.mode != TRIG_MODE_AUTO) {
        active_cfg.trigger.pre_trigger_pct = CAPTURE_PRE_TRIGGER_DEFAULT_PCT;
    }

    trigger_engine_configure(active_cfg.trigger);
    trigger_engine_arm(active_cfg.sample_count, active_cfg.trigger.pre_trigger_pct, active_samples);

    // Calculate PIO clock divider
    uint32_t system_clk = clock_get_hz(clk_sys);
    uint32_t req_rate   = (active_cfg.sample_rate_hz > 0) ? active_cfg.sample_rate_hz : 10000000;
    current_clkdiv      = (float)system_clk / (float)req_rate;
    if (current_clkdiv < 1.0f) current_clkdiv = 1.0f;

    // Clear RAM buffer
    memset(capture_buffer, 0, active_samples);

    // Initialize PIO state machine
    logic_analyzer_init(pio_inst, sm_inst, pio_offset, CAPTURE_PIN_BASE, CAPTURE_PIN_COUNT, current_clkdiv);

    // Configure DMA channel with hardware ring wrapping
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio_inst, sm_inst, false));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32); // 32-bit transfer for 32-bit autopush (4 samples/DREQ)
    channel_config_set_ring(&c, true, ring_bits);          // Automatic circular ring wrap

    dma_channel_configure(
        dma_chan,
        &c,
        capture_buffer,                 // Destination circular buffer
        &pio_inst->rxf[sm_inst],       // Source (PIO RX FIFO)
        0xFFFFFFFF,                     // Continuous rolling capture
        false                           // Don't start yet
    );

    last_eval_pos   = 0;
    start_time_ms   = millis();
    capture_running = true;

    // Clear FIFOs and FIFO overflow flag before launch
    pio_sm_clear_fifos(pio_inst, sm_inst);
    pio_capture_clear_fifo_overflow();
    dma_channel_start(dma_chan);
    pio_sm_set_enabled(pio_inst, sm_inst, true);

    return true;
}

void pio_capture_task() {
    if (!capture_running || dma_chan < 0) return;

    uint32_t cur_write = dma_hw->ch[dma_chan].write_addr;
    uint32_t cur_pos   = (cur_write - (uint32_t)capture_buffer) & (active_samples - 1);
    TriggerState ts    = trigger_engine_get_state();

    if (ts == TRIG_STATE_ARMED) {
        if (active_cfg.trigger.mode == TRIG_MODE_AUTO) {
            // Wait until circular buffer has at least pre-trigger history filled
            uint32_t pre_needed = trigger_engine_get_pre_samples();
            if (cur_pos >= pre_needed || (millis() - start_time_ms) > 10) {
                t_trigger_micros = micros();
                trigger_engine_fire(cur_pos);
            }
        } else {
            // Evaluate stream from last inspected position to cur_pos
            uint32_t match_idx = 0;
            if (trigger_engine_evaluate_stream(capture_buffer, last_eval_pos, cur_pos,
                                               active_cfg.sample_rate_hz, active_samples, match_idx)) {
                // Trigger condition met! State is now TRIG_STATE_TRIGGERED
                t_trigger_micros = micros();
            }
        }
        last_eval_pos = cur_pos;
    } else if (ts == TRIG_STATE_TRIGGERED) {
        // Condition already tripped! Wait until post-trigger samples are captured
        uint32_t trig_pos = trigger_engine_get_trigger_pos();
        uint32_t post_collected = (cur_pos + active_samples - trig_pos) & (active_samples - 1);

        if (post_collected >= trigger_engine_get_post_samples()) {
            // Post-trigger capture is complete! Stop acquisition
            pio_capture_stop();
            t_freeze_micros = micros();
            trigger_engine_set_complete();

            // Re-align the circular buffer into [PRE-TRIGGER][TRIGGER][POST-TRIGGER]
            trigger_engine_align_buffer(capture_buffer, aligned_capture_buffer, active_cfg.sample_count, active_samples);

            capture_complete = true;
            capture_running  = false;
        }
    }
}

void pio_capture_stop() {
    if (capture_running) {
        pio_sm_set_enabled(pio_inst, sm_inst, false);
        dma_channel_abort(dma_chan);
        capture_running = false;
        if (t_freeze_micros == 0) t_freeze_micros = micros();
    }
}

bool pio_capture_is_running() {
    return capture_running;
}

bool pio_capture_is_complete() {
    return capture_complete;
}

const uint8_t* pio_capture_get_aligned_buffer() {
    return aligned_capture_buffer;
}

uint32_t pio_capture_get_sample_count() {
    return active_cfg.sample_count;
}

uint32_t pio_capture_get_trigger_offset() {
    return trigger_engine_get_pre_samples();
}

uint32_t get_actual_sample_rate() {
    return (uint32_t)(clock_get_hz(clk_sys) / current_clkdiv);
}

uint32_t pio_capture_get_t_trigger_us() {
    return t_trigger_micros;
}

uint32_t pio_capture_get_t_freeze_us() {
    return t_freeze_micros;
}

bool pio_capture_check_fifo_overflow() {
    uint32_t rxstall_mask = 1u << (PIO_FDEBUG_RXSTALL_LSB + sm_inst);
    return (pio_inst->fdebug & rxstall_mask) != 0;
}

void pio_capture_clear_fifo_overflow() {
    uint32_t rxstall_mask = 1u << (PIO_FDEBUG_RXSTALL_LSB + sm_inst);
    pio_inst->fdebug = rxstall_mask; // Clear sticky RXSTALL bit by writing 1
}

Benchmark100MspsResult pio_capture_run_100msps_benchmark(uint32_t target_rotations) {
    Benchmark100MspsResult res = {0};
    res.sample_rate_hz = 100000000;
    res.sample_count   = MAX_CAPTURE_SAMPLES; // 65,536 samples = 64KB

    LogicAnalyzerConfig cfg = {
        .sample_rate_hz   = 100000000,
        .sample_count     = MAX_CAPTURE_SAMPLES,
        .trigger_channel  = 0,
        .trigger_mode     = TRIG_IMMEDIATE,
        .enabled_channels = 0xFF,
        .trigger = {
            .mode            = TRIG_MODE_AUTO,
            .channel         = 0,
            .param           = 0,
            .pattern_mask    = 0xFF,
            .pattern_value   = 0x00,
            .qual_channel    = 1,
            .qual_level      = 1,
            .pre_trigger_pct = 50,
            .stage_count     = 0,
            .stages          = {}
        }
    };

    uint32_t sys_clk = clock_get_hz(clk_sys);
    // At 100 MSPS with 32-bit DMA transfers, DREQ = 25 Mwords/sec
    res.bus_utilization_pct = (25000000.0f / (float)sys_clk) * 100.0f;

    if (!pio_capture_start(cfg)) {
        return res;
    }

    pio_capture_clear_fifo_overflow();

    uint32_t t_start_bench = micros();
    uint32_t last_waddr = dma_hw->ch[dma_chan].write_addr;
    uint32_t revs = 0;

    // Monitor circular DMA ring wrapping for target_rotations full revolutions
    while (revs < target_rotations && (micros() - t_start_bench) < 200000) { // 200ms timeout
        uint32_t cur_waddr = dma_hw->ch[dma_chan].write_addr;
        if (cur_waddr < last_waddr) {
            revs++;
        }
        last_waddr = cur_waddr;
    }

    uint32_t t_trig = micros();
    uint32_t cur_pos = (dma_hw->ch[dma_chan].write_addr - (uint32_t)capture_buffer) & (MAX_CAPTURE_SAMPLES - 1);
    trigger_engine_fire(cur_pos);

    // Run capture task until post-trigger buffer freeze completes
    while (capture_running && (micros() - t_trig) < 50000) {
        pio_capture_task();
    }

    uint32_t t_end = micros();

    res.revolutions_completed  = revs;
    res.total_samples_streamed = (revs * MAX_CAPTURE_SAMPLES) + (MAX_CAPTURE_SAMPLES / 2);
    res.elapsed_us             = t_end - t_start_bench;
    if (res.elapsed_us > 0) {
        res.effective_msps     = ((float)res.total_samples_streamed / (float)res.elapsed_us);
    }
    res.fifo_overflow          = pio_capture_check_fifo_overflow();
    res.trigger_latency_us     = t_trig - t_start_bench;
    res.freeze_latency_us      = t_end - t_trig;
    res.buffer_aligned_ok      = capture_complete;

    return res;
}
