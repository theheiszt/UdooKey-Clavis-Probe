#include "spi_slave_transport.h"
#include "bus_pirate_core.h"
#include "pio_capture.h"
#include "trigger_engine.h"
#include <hardware/spi.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>

enum TransportSlaveFsmState {
    SLAVE_STATE_IDLE,           // DMA RX actively listening for master request
    SLAVE_STATE_REQ_RECEIVED,   // CS edge latched a request; waiting for worker task
    SLAVE_STATE_RESP_STAGED,    // Worker task staged response; DMA TX armed for master read
    SLAVE_STATE_RESP_COMPLETED  // CS edge latched master read completion; waiting for worker task
};

static volatile TransportSlaveFsmState slave_fsm_state = SLAVE_STATE_IDLE;
static volatile bool cs_event_pending = false;
static volatile uint32_t latched_rx_count = 0;
static int16_t last_transferred_seq = -1;

static uint8_t rx_req_buf[128];
static uint8_t tx_resp_buf[sizeof(SpiPacketHeader) + SPI_BLOCK_SIZE];
static const uint8_t dummy_tx_byte = 0x00;
static uint8_t dummy_rx_byte = 0x00;

static int spi_dma_tx = -1;
static int spi_dma_rx = -1;

// Capture Descriptor & Generation Tracking
static CaptureDescriptor current_descriptor = {0, nullptr, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0, 0, 0};
static uint32_t next_capture_id = 1;
static uint16_t precomputed_block_crcs[64];

// Test & Fault Injection State
static uint8_t  synthetic_test_buffer[8192];
static bool     corrupt_inject_active = false;
static uint16_t corrupt_block_target  = 0;
static uint16_t corrupt_byte_target   = 517;

// Deferred command execution
static volatile bool start_req_pending = false;
static volatile bool stop_req_pending  = false;
static volatile bool cmd_req_pending   = false;
static char pending_cmd_str[64] = {0};
static char cmd_resp_str[128]   = {0};
static bool s_mcu_spi_active    = false;

bool spi_slave_transport_is_active() {
    return s_mcu_spi_active;
}

static inline void set_data_ready_pin(bool asserted) {
#if defined(RP2040_PIN_DATA_READY) && (RP2040_PIN_DATA_READY != 255)
    digitalWrite(RP2040_PIN_DATA_READY, asserted ? HIGH : LOW);
#else
    (void)asserted;
#endif
}

static void arm_rx_listen() {
    while (spi_is_readable(spi0)) {
        (void)spi_get_hw(spi0)->dr;
    }

    dma_channel_config rx_c = dma_channel_get_default_config(spi_dma_rx);
    channel_config_set_transfer_data_size(&rx_c, DMA_SIZE_8);
    channel_config_set_dreq(&rx_c, spi_get_dreq(spi0, false));
    channel_config_set_read_increment(&rx_c, false);
    channel_config_set_write_increment(&rx_c, true);
    dma_channel_configure(spi_dma_rx, &rx_c, rx_req_buf, &spi_get_hw(spi0)->dr, sizeof(rx_req_buf), true);

    dma_channel_config tx_c = dma_channel_get_default_config(spi_dma_tx);
    channel_config_set_transfer_data_size(&tx_c, DMA_SIZE_8);
    channel_config_set_dreq(&tx_c, spi_get_dreq(spi0, true));
    channel_config_set_read_increment(&tx_c, false);
    channel_config_set_write_increment(&tx_c, false);
    dma_channel_configure(spi_dma_tx, &tx_c, &spi_get_hw(spi0)->dr, &dummy_tx_byte, sizeof(rx_req_buf), true);
}

static void arm_tx_response(uint16_t resp_len) {
    while (spi_is_readable(spi0)) {
        (void)spi_get_hw(spi0)->dr;
    }

    dma_channel_config tx_c = dma_channel_get_default_config(spi_dma_tx);
    channel_config_set_transfer_data_size(&tx_c, DMA_SIZE_8);
    channel_config_set_dreq(&tx_c, spi_get_dreq(spi0, true));
    channel_config_set_read_increment(&tx_c, true);
    channel_config_set_write_increment(&tx_c, false);
    dma_channel_configure(spi_dma_tx, &tx_c, &spi_get_hw(spi0)->dr, tx_resp_buf, resp_len, true);

    dma_channel_config rx_c = dma_channel_get_default_config(spi_dma_rx);
    channel_config_set_transfer_data_size(&rx_c, DMA_SIZE_8);
    channel_config_set_dreq(&rx_c, spi_get_dreq(spi0, false));
    channel_config_set_read_increment(&rx_c, false);
    channel_config_set_write_increment(&rx_c, false);
    dma_channel_configure(spi_dma_rx, &rx_c, &dummy_rx_byte, &spi_get_hw(spi0)->dr, resp_len, true);
}

// ─── Minimalist CS Rising Edge ISR (<15 cycles) ─────────────────────────────
// Absolutely NO memory copies, packet parsing, or CRC calculations in ISR context!
static void on_spi_cs_rise() {
    dma_channel_abort(spi_dma_rx);
    dma_channel_abort(spi_dma_tx);

    if (slave_fsm_state == SLAVE_STATE_RESP_STAGED) {
        slave_fsm_state = SLAVE_STATE_RESP_COMPLETED;
    } else {
        latched_rx_count = sizeof(rx_req_buf) - dma_channel_hw_addr(spi_dma_rx)->transfer_count;
        slave_fsm_state = SLAVE_STATE_REQ_RECEIVED;
    }

    cs_event_pending = true;
}

static void process_master_request(bool& start_requested, bool& stop_requested) {
    if (latched_rx_count < sizeof(SpiPacketHeader)) {
        arm_rx_listen();
        slave_fsm_state = SLAVE_STATE_IDLE;
        return;
    }

    SpiPacketHeader* req = (SpiPacketHeader*)rx_req_buf;
    if (req->magic != SPI_PROTO_MAGIC) {
        arm_rx_listen();
        slave_fsm_state = SLAVE_STATE_IDLE;
        return;
    }

    uint16_t resp_len = sizeof(SpiPacketHeader);
    SpiPacketHeader* resp = (SpiPacketHeader*)tx_resp_buf;
    memset(resp, 0, sizeof(SpiPacketHeader));
    resp->magic      = SPI_PROTO_MAGIC;
    resp->capture_id = current_descriptor.capture_id;

    switch (req->type) {
        case SPI_PKT_PING:
            resp->type = SPI_PKT_PONG;
            resp->payload_len = 0;
            break;

        case SPI_PKT_GET_STATUS: {
            resp->type = SPI_PKT_STATUS_RESP;
            resp->payload_len = sizeof(SpiStatusPayload);
            SpiStatusPayload* stat = (SpiStatusPayload*)(tx_resp_buf + sizeof(SpiPacketHeader));
            stat->capture_id      = current_descriptor.capture_id;
            stat->capture_running = capture_running ? 1 : 0;
            stat->capture_ready   = current_descriptor.valid ? 1 : 0;
            stat->active_mode     = bp_cfg.mode;
            stat->trigger_state   = (uint8_t)trigger_engine_get_state();
            stat->sample_rate_hz  = current_descriptor.sample_rate_hz;
            stat->sample_count    = current_descriptor.total_samples;
            resp->block_crc = spi_calc_block_crc16(current_descriptor.capture_id, 0, (const uint8_t*)stat, sizeof(SpiStatusPayload));
            resp_len += sizeof(SpiStatusPayload);
            break;
        }

        case SPI_PKT_GET_CAPTURE_INFO: {
            resp->type = SPI_PKT_CAPTURE_INFO;
            resp->payload_len = sizeof(SpiCaptureInfoPayload);
            SpiCaptureInfoPayload* info = (SpiCaptureInfoPayload*)(tx_resp_buf + sizeof(SpiPacketHeader));
            info->capture_id        = current_descriptor.capture_id;
            info->total_samples     = current_descriptor.total_samples;
            info->sample_rate_hz    = current_descriptor.sample_rate_hz;
            info->block_size        = SPI_BLOCK_SIZE;
            info->total_blocks      = current_descriptor.total_blocks;
            info->trigger_offset    = current_descriptor.trigger_offset;
            info->trigger_sample    = current_descriptor.trigger_sample;
            info->whole_capture_crc = current_descriptor.whole_capture_crc; // Level 2 CRC across entire acquisition!
            info->trigger_channel   = current_descriptor.trigger_channel;
            info->trigger_type      = current_descriptor.trigger_type;
            info->capture_ready     = current_descriptor.valid ? 1 : 0;
            info->active_mode       = bp_cfg.mode;
            resp->block_crc = spi_calc_block_crc16(current_descriptor.capture_id, 0, (const uint8_t*)info, sizeof(SpiCaptureInfoPayload));
            resp_len += sizeof(SpiCaptureInfoPayload);
            break;
        }

        case SPI_PKT_GET_BLOCK: {
            uint16_t seq = req->sequence;
            if (current_descriptor.valid && seq < current_descriptor.total_blocks && current_descriptor.buffer) {
                resp->type           = SPI_PKT_CAPTURE_BLOCK;
                resp->sequence       = seq;
                resp->total_blocks   = current_descriptor.total_blocks;
                resp->capture_id     = current_descriptor.capture_id;
                uint32_t offset      = seq * SPI_BLOCK_SIZE;
                uint16_t blk_len     = SPI_BLOCK_SIZE;
                if (offset + blk_len > current_descriptor.total_samples) {
                    blk_len = current_descriptor.total_samples - offset;
                }

                resp->payload_len    = blk_len;
                resp->sample_index   = offset;
                resp->sample_rate    = current_descriptor.sample_rate_hz;
                resp->trigger_offset = current_descriptor.trigger_offset;
                resp->flags          = (seq == current_descriptor.total_blocks - 1) ? SPI_FLAG_LAST_BLOCK : SPI_FLAG_NONE;
                resp->block_crc      = precomputed_block_crcs[seq]; // Level 1 CRC (generation + sequence + payload)!

                memcpy(tx_resp_buf + sizeof(SpiPacketHeader), current_descriptor.buffer + offset, blk_len);

                // Test 2: Intentional fault injection if activated by user/test runner
                if (corrupt_inject_active && seq == corrupt_block_target && corrupt_byte_target < blk_len) {
                    tx_resp_buf[sizeof(SpiPacketHeader) + corrupt_byte_target] ^= 0x01;
                    corrupt_inject_active = false; // Corrupt exactly once; subsequent retry will succeed!
                }

                resp_len += blk_len;
                last_transferred_seq = seq;
            } else {
                resp->type = SPI_PKT_CAPTURE_BLOCK;
                resp->flags = SPI_FLAG_ERROR;
                resp->payload_len = 0;
            }
            break;
        }

        case SPI_PKT_CAPTURE_ACK: {
            // Master explicitly acknowledged full capture verification!
            current_descriptor.valid = false;
            digitalWrite(RP2040_PIN_DATA_READY, LOW);
            resp->type = SPI_PKT_STATUS_RESP;
            break;
        }

        case SPI_PKT_LA_START:
            start_req_pending = true;
            spi_slave_invalidate_capture();
            resp->type = SPI_PKT_STATUS_RESP;
            resp->flags = SPI_FLAG_BUSY;
            break;

        case SPI_PKT_LA_STOP:
            stop_req_pending = true;
            resp->type = SPI_PKT_STATUS_RESP;
            break;

        case SPI_PKT_CMD_EXEC: {
            uint16_t clen = req->payload_len;
            if (clen > 0 && clen < sizeof(pending_cmd_str) && latched_rx_count >= sizeof(SpiPacketHeader) + clen) {
                memcpy(pending_cmd_str, rx_req_buf + sizeof(SpiPacketHeader), clen);
                pending_cmd_str[clen] = '\0';
                cmd_req_pending = true;
            }
            resp->type = SPI_PKT_CMD_RESP;
            uint16_t rlen = strlen(cmd_resp_str);
            resp->payload_len = rlen;
            if (rlen > 0) {
                memcpy(tx_resp_buf + sizeof(SpiPacketHeader), cmd_resp_str, rlen);
                resp_len += rlen;
                resp->block_crc = spi_calc_block_crc16(0, 0, (const uint8_t*)cmd_resp_str, rlen);
            }
            break;
        }

        default:
            resp->flags = SPI_FLAG_ERROR;
            break;
    }

    arm_tx_response(resp_len);
    slave_fsm_state = SLAVE_STATE_RESP_STAGED;
}

void spi_slave_transport_init() {
    if (s_mcu_spi_active) return;

    pinMode(RP2040_PIN_DATA_READY, OUTPUT);
    digitalWrite(RP2040_PIN_DATA_READY, LOW);

    spi_init(spi0, SPI_DEFAULT_FREQ_HZ);
    spi_set_slave(spi0, true);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(RP2040_PIN_SPI_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(RP2040_PIN_SPI_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(RP2040_PIN_SPI_MISO, GPIO_FUNC_SPI);
    gpio_set_function(RP2040_PIN_SPI_CS,   GPIO_FUNC_SPI);

    if (spi_dma_tx < 0) spi_dma_tx = dma_claim_unused_channel(true);
    if (spi_dma_rx < 0) spi_dma_rx = dma_claim_unused_channel(true);

    arm_rx_listen();
    slave_fsm_state = SLAVE_STATE_IDLE;

    attachInterrupt(digitalPinToInterrupt(RP2040_PIN_SPI_CS), on_spi_cs_rise, RISING);
    s_mcu_spi_active = true;
}

void spi_slave_transport_stop() {
    if (!s_mcu_spi_active) return;

    detachInterrupt(digitalPinToInterrupt(RP2040_PIN_SPI_CS));

    if (spi_dma_rx >= 0) dma_channel_abort(spi_dma_rx);
    if (spi_dma_tx >= 0) dma_channel_abort(spi_dma_tx);

    spi_deinit(spi0);

    // Release probe bus pins (GP2..GP5) to SIO Safe High-Z
    gpio_set_function(RP2040_PIN_SPI_SCK,  GPIO_FUNC_SIO);
    gpio_set_function(RP2040_PIN_SPI_MOSI, GPIO_FUNC_SIO);
    gpio_set_function(RP2040_PIN_SPI_MISO, GPIO_FUNC_SIO);
    gpio_set_function(RP2040_PIN_SPI_CS,   GPIO_FUNC_SIO);

    pinMode(RP2040_PIN_SPI_SCK,  INPUT);
    pinMode(RP2040_PIN_SPI_MOSI, INPUT);
    pinMode(RP2040_PIN_SPI_MISO, INPUT);
    pinMode(RP2040_PIN_SPI_CS,   INPUT);

    slave_fsm_state = SLAVE_STATE_IDLE;
    cs_event_pending = false;
    current_descriptor.valid = false;
    digitalWrite(RP2040_PIN_DATA_READY, LOW);
    s_mcu_spi_active = false;
}

// ─── Main/Worker Context FSM Task ───────────────────────────────────────────
void spi_slave_transport_task(bool& start_requested, bool& stop_requested) {
    if (!s_mcu_spi_active) return;

    if (cs_event_pending) {
        cs_event_pending = false;

        if (slave_fsm_state == SLAVE_STATE_RESP_COMPLETED) {
            // Master completed read transaction
            if (last_transferred_seq >= 0 && current_descriptor.valid) {
                if ((uint16_t)last_transferred_seq == current_descriptor.total_blocks - 1) {
                    // All blocks transferred! Level handshake drops
                    digitalWrite(RP2040_PIN_DATA_READY, LOW);
                    current_descriptor.valid = false;
                }
            }
            last_transferred_seq = -1;

            arm_rx_listen();
            slave_fsm_state = SLAVE_STATE_IDLE;
        } else if (slave_fsm_state == SLAVE_STATE_REQ_RECEIVED) {
            process_master_request(start_requested, stop_requested);
        }
    }

    if (start_req_pending) {
        start_req_pending = false;
        start_requested = true;
    }
    if (stop_req_pending) {
        stop_req_pending = false;
        stop_requested = true;
    }
    if (cmd_req_pending) {
        cmd_req_pending = false;
        bp_execute_command(pending_cmd_str, Serial);
        strncpy(cmd_resp_str, last_cmd_output, sizeof(cmd_resp_str) - 1);
        cmd_resp_str[sizeof(cmd_resp_str) - 1] = '\0';
    }
}

// ─── Publish Capture Descriptor to Transport Layer ──────────────────────────
void spi_slave_publish_capture(const uint8_t* aligned_buf, uint32_t sample_count, uint32_t sample_rate_hz,
                               uint16_t trigger_offset, uint32_t trigger_sample,
                               uint8_t trigger_channel, uint8_t trigger_type,
                               uint32_t t_trigger_us, uint32_t t_freeze_us) {
    if (!aligned_buf || sample_count == 0) return;

    current_descriptor.capture_id        = next_capture_id++;
    current_descriptor.buffer            = aligned_buf;
    current_descriptor.total_samples     = sample_count;
    current_descriptor.sample_rate_hz    = sample_rate_hz;
    current_descriptor.trigger_offset    = trigger_offset;
    current_descriptor.trigger_sample    = trigger_sample;
    current_descriptor.trigger_channel   = trigger_channel;
    current_descriptor.trigger_type      = trigger_type;
    current_descriptor.total_blocks      = (sample_count + SPI_BLOCK_SIZE - 1) / SPI_BLOCK_SIZE;
    if (current_descriptor.total_blocks > 64) current_descriptor.total_blocks = 64;

    current_descriptor.t_trigger_us      = t_trigger_us;
    current_descriptor.t_dma_freeze_us   = t_freeze_us;

    // 1. Calculate Level 1 Per-Block CRCs (covers capture_id + sequence + payload)
    for (uint16_t b = 0; b < current_descriptor.total_blocks; b++) {
        uint32_t off = b * SPI_BLOCK_SIZE;
        uint16_t len = SPI_BLOCK_SIZE;
        if (off + len > sample_count) len = sample_count - off;
        precomputed_block_crcs[b] = spi_calc_block_crc16(current_descriptor.capture_id, b, aligned_buf + off, len);
    }

    // 2. Calculate Level 2 Whole-Capture CRC-16 across entire assembled acquisition
    current_descriptor.whole_capture_crc = spi_calc_crc16(aligned_buf, sample_count);
    current_descriptor.t_desc_ready_us   = micros();
    current_descriptor.valid             = true;

    // 3. Assert Level-Triggered DATA_READY handshake:
    // Indicates: "A complete, valid capture descriptor is available in SRAM"
    digitalWrite(RP2040_PIN_DATA_READY, HIGH);
    current_descriptor.t_data_ready_us   = micros();
}

bool spi_slave_is_capture_ready() {
    return current_descriptor.valid;
}

void spi_slave_invalidate_capture() {
    current_descriptor.valid = false;
    digitalWrite(RP2040_PIN_DATA_READY, LOW);
}

const CaptureDescriptor& spi_slave_get_descriptor() {
    return current_descriptor;
}

// ─── Production Verification Test Helpers ────────────────────────────────────
void spi_slave_test_generate_synthetic(uint32_t sample_count) {
    if (sample_count > sizeof(synthetic_test_buffer)) sample_count = sizeof(synthetic_test_buffer);

    // Test 1: Fill with repeating 00 01 02 ... FF ramp pattern
    for (uint32_t i = 0; i < sample_count; i++) {
        synthetic_test_buffer[i] = (uint8_t)(i & 0xFF);
    }

    spi_slave_publish_capture(synthetic_test_buffer, sample_count, 10000000, 0, 0, 0, 0);
}

void spi_slave_test_inject_corruption(uint16_t block_idx, uint16_t byte_offset) {
    corrupt_inject_active = true;
    corrupt_block_target  = block_idx;
    corrupt_byte_target   = byte_offset;
}

void spi_slave_test_trigger_race() {
    current_descriptor.capture_id++; // Force generation mismatch
}

void spi_slave_set_cmd_response(const char* response) {
    if (!response) return;
    strncpy(cmd_resp_str, response, sizeof(cmd_resp_str) - 1);
    cmd_resp_str[sizeof(cmd_resp_str) - 1] = '\0';
}
