#include "spi_master_transport.h"
#include <SPI.h>

static SPIClass spi_bus(VSPI);
static SPISettings spi_settings(SPI_DEFAULT_FREQ_HZ, MSBFIRST, SPI_MODE0);

static uint32_t stats_total_retries = 0;
static uint32_t stats_generation_mismatches = 0;
static uint32_t stats_crc_errors = 0;

static void spi_send_request(const SpiPacketHeader& req, const uint8_t* payload = nullptr, uint16_t payload_len = 0) {
    digitalWrite(ESP32_PIN_SPI_CS, LOW);
    spi_bus.beginTransaction(spi_settings);
    spi_bus.transferBytes((const uint8_t*)&req, NULL, sizeof(SpiPacketHeader));
    if (payload && payload_len > 0) {
        spi_bus.transferBytes(payload, NULL, payload_len);
    }
    spi_bus.endTransaction();
    digitalWrite(ESP32_PIN_SPI_CS, HIGH);
}

static bool spi_read_response(SpiPacketHeader& resp, uint8_t* payload = nullptr, uint16_t payload_len = 0) {
    digitalWrite(ESP32_PIN_SPI_CS, LOW);
    spi_bus.beginTransaction(spi_settings);
    spi_bus.transferBytes(NULL, (uint8_t*)&resp, sizeof(SpiPacketHeader));
    if (payload && payload_len > 0) {
        spi_bus.transferBytes(NULL, payload, payload_len);
    }
    spi_bus.endTransaction();
    digitalWrite(ESP32_PIN_SPI_CS, HIGH);
    return (resp.magic == SPI_PROTO_MAGIC);
}

static bool s_spi_master_active = false;

void spi_master_transport_init() {
    if (s_spi_master_active) return;
    pinMode(ESP32_PIN_SPI_CS, OUTPUT);
    digitalWrite(ESP32_PIN_SPI_CS, HIGH);

#if defined(ESP32_PIN_DATA_READY) && (ESP32_PIN_DATA_READY != 255)
    pinMode(ESP32_PIN_DATA_READY, INPUT_PULLDOWN);
#endif

    spi_bus.begin(ESP32_PIN_SPI_SCK, ESP32_PIN_SPI_MISO, ESP32_PIN_SPI_MOSI, ESP32_PIN_SPI_CS);
    s_spi_master_active = true;
}

void spi_master_transport_stop() {
    if (!s_spi_master_active) return;
    spi_bus.end();
    pinMode(ESP32_PIN_SPI_CS, INPUT);
    s_spi_master_active = false;
}

bool spi_master_transport_is_active() {
    return s_spi_master_active;
}

void rp2040_reset_assert() {
    pinMode(ESP32_PIN_RPI_RESET, OUTPUT);
    digitalWrite(ESP32_PIN_RPI_RESET, LOW); // LOW = RP2040 execution disabled
}

void rp2040_reset_release() {
    pinMode(ESP32_PIN_RPI_RESET, OUTPUT);
    digitalWrite(ESP32_PIN_RPI_RESET, HIGH); // HIGH = RP2040 execution restored
}

bool rp2040_reboot(uint32_t hold_ms) {
    rp2040_reset_assert();
    delay(hold_ms);
    rp2040_reset_release();
    delay(100);
    return true;
}

bool spi_master_ping() {
    SpiPacketHeader req = {};
    req.magic = SPI_PROTO_MAGIC;
    req.type  = SPI_PKT_PING;

    spi_send_request(req);
    delayMicroseconds(30);

    SpiPacketHeader resp = {};
    if (spi_read_response(resp) && resp.type == SPI_PKT_PONG) {
        return true;
    }
    return false;
}

bool spi_master_get_status(SpiStatusPayload& status) {
    SpiPacketHeader req = {};
    req.magic = SPI_PROTO_MAGIC;
    req.type  = SPI_PKT_GET_STATUS;

    spi_send_request(req);
    delayMicroseconds(30);

    SpiPacketHeader resp = {};
    if (spi_read_response(resp, (uint8_t*)&status, sizeof(SpiStatusPayload)) &&
        resp.type == SPI_PKT_STATUS_RESP && resp.payload_len == sizeof(SpiStatusPayload)) {
        uint16_t crc = spi_calc_block_crc16(status.capture_id, 0, (const uint8_t*)&status, sizeof(SpiStatusPayload));
        return (crc == resp.block_crc);
    }
    return false;
}

bool spi_master_is_data_ready() {
    if (!s_spi_master_active) return false;
#if defined(ESP32_PIN_DATA_READY) && (ESP32_PIN_DATA_READY != 255)
    if (digitalRead(ESP32_PIN_DATA_READY) == HIGH) return true;
#endif
    SpiStatusPayload status = {};
    if (spi_master_get_status(status)) {
        return status.capture_ready;
    }
    return false;
}

bool spi_master_acknowledge_capture() {
    SpiPacketHeader req = {};
    req.magic = SPI_PROTO_MAGIC;
    req.type  = SPI_PKT_CAPTURE_ACK;

    spi_send_request(req);
    delayMicroseconds(30);

    SpiPacketHeader resp = {};
    return spi_read_response(resp);
}

bool spi_master_fetch_complete_capture(uint8_t* dest_buffer, uint32_t max_len, SpiCaptureInfoPayload& info) {
    if (!dest_buffer || max_len == 0) return false;

    // Step 1: Query capture descriptor metadata from RP2040
    SpiPacketHeader req = {};
    req.magic = SPI_PROTO_MAGIC;
    req.type  = SPI_PKT_GET_CAPTURE_INFO;

    spi_send_request(req);
    delayMicroseconds(30);

    SpiPacketHeader resp = {};
    if (!spi_read_response(resp, (uint8_t*)&info, sizeof(SpiCaptureInfoPayload)) ||
        resp.type != SPI_PKT_CAPTURE_INFO || resp.payload_len != sizeof(SpiCaptureInfoPayload)) {
        return false;
    }

    // Validate Level 1 CRC of metadata payload (pre-seeded with capture_id)
    if (spi_calc_block_crc16(info.capture_id, 0, (const uint8_t*)&info, sizeof(SpiCaptureInfoPayload)) != resp.block_crc) {
        return false;
    }

    // Explicit Invariant Check:
    if (info.capture_id == 0 || info.total_samples == 0 || info.total_samples > max_len || info.total_blocks == 0) {
        return false;
    }

    uint32_t current_gen_id = info.capture_id;

    // Step 2: Sequentially fetch 1024-byte blocks with explicit invariants and Level 1 CRC
    for (uint16_t seq = 0; seq < info.total_blocks; seq++) {
        bool block_valid = false;
        for (int retry = 0; retry < 3 && !block_valid; retry++) {
            memset(&req, 0, sizeof(req));
            req.magic      = SPI_PROTO_MAGIC;
            req.type       = SPI_PKT_GET_BLOCK;
            req.sequence   = seq;
            req.capture_id = current_gen_id;

            spi_send_request(req);
            delayMicroseconds(30);

            uint32_t offset = seq * SPI_BLOCK_SIZE;
            uint16_t expected_len = SPI_BLOCK_SIZE;
            if (offset + expected_len > info.total_samples) {
                expected_len = info.total_samples - offset;
            }

            memset(&resp, 0, sizeof(resp));
            if (spi_read_response(resp, dest_buffer + offset, expected_len)) {
                // Check generation invariant
                if (resp.capture_id != current_gen_id && resp.capture_id != 0) {
                    stats_generation_mismatches++;
                }

                // Production Invariants:
                // 1. Generation number must match descriptor (response.capture_id == descriptor.capture_id)
                // 2. Block sequence must match requested sequence
                // 3. Block sequence must be < total_blocks (response.block_index < descriptor.total_blocks)
                // 4. Payload length must be <= SPI_BLOCK_SIZE (response.payload_length <= SPI_BLOCK_SIZE)
                // 5. No error flag set
                if (resp.type == SPI_PKT_CAPTURE_BLOCK &&
                    resp.capture_id == current_gen_id &&
                    resp.sequence == seq &&
                    resp.sequence < info.total_blocks &&
                    resp.payload_len <= SPI_BLOCK_SIZE &&
                    resp.payload_len == expected_len &&
                    !(resp.flags & SPI_FLAG_ERROR)) {

                    // Level 1: Verify Per-Block CRC-16 (covers capture_id + sequence + payload)
                    uint16_t calc_crc = spi_calc_block_crc16(current_gen_id, seq, dest_buffer + offset, expected_len);
                    if (calc_crc == resp.block_crc) {
                        block_valid = true;
                        if (retry > 0) stats_total_retries += retry;
                    } else {
                        stats_crc_errors++;
                    }
                } else {
                    stats_crc_errors++;
                }
            } else {
                stats_crc_errors++;
            }
        }
        if (!block_valid) {
            return false; // Transfer failed after retries
        }
    }

    // Step 3: Level 2 Whole-Capture CRC-16 verification across entire assembled acquisition
    uint16_t calc_whole_crc = spi_calc_crc16(dest_buffer, info.total_samples);
    if (calc_whole_crc != info.whole_capture_crc) {
        Serial.printf("[ESP32 SPI] Whole-capture CRC mismatch: expected 0x%04X, calculated 0x%04X!\n",
                      info.whole_capture_crc, calc_whole_crc);
        return false;
    }

    // Step 4: Gracefully acknowledge capture reception to release slave DATA_READY level
    spi_master_acknowledge_capture();

    return true;
}

bool spi_master_start_capture(uint32_t rate_hz, uint32_t sample_count) {
    SpiPacketHeader req = {};
    req.magic        = SPI_PROTO_MAGIC;
    req.type         = SPI_PKT_LA_START;
    req.sample_rate  = rate_hz;
    req.sample_index = sample_count;

    spi_send_request(req);
    delayMicroseconds(30);

    SpiPacketHeader resp = {};
    return spi_read_response(resp);
}

bool spi_master_stop_capture() {
    SpiPacketHeader req = {};
    req.magic = SPI_PROTO_MAGIC;
    req.type  = SPI_PKT_LA_STOP;

    spi_send_request(req);
    delayMicroseconds(30);

    SpiPacketHeader resp = {};
    return spi_read_response(resp);
}

bool spi_master_exec_command(const char* cmd, char* resp_buf, size_t max_resp_len) {
    if (!cmd) return false;
    uint16_t len = strlen(cmd);

    SpiPacketHeader req = {};
    req.magic       = SPI_PROTO_MAGIC;
    req.type        = SPI_PKT_CMD_EXEC;
    req.payload_len = len;

    spi_send_request(req, (const uint8_t*)cmd, len);
    delayMicroseconds(50);

    SpiPacketHeader resp = {};
    if (resp_buf && max_resp_len > 0) {
        resp_buf[0] = '\0';
    }

    uint16_t read_len = (max_resp_len > 0) ? (max_resp_len - 1) : 0;
    if (spi_read_response(resp, (uint8_t*)resp_buf, read_len)) {
        if (resp.payload_len < max_resp_len) {
            resp_buf[resp.payload_len] = '\0';
        } else if (max_resp_len > 0) {
            resp_buf[max_resp_len - 1] = '\0';
        }
        return true;
    }
    return false;
}

// ─── Production Verification Test Helpers ────────────────────────────────────
bool spi_master_test_verify_synthetic(const uint8_t* buf, uint32_t len, uint32_t& first_err_offset) {
    if (!buf || len == 0) return false;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t expected = (uint8_t)(i & 0xFF);
        if (buf[i] != expected) {
            first_err_offset = i;
            return false;
        }
    }
    return true;
}

bool spi_master_run_benchmark(uint32_t iterations, SpiBenchmarkReport& report) {
    memset(&report, 0, sizeof(report));
    if (iterations == 0) iterations = 10;
    static uint8_t bench_buf[8192];

    uint32_t start_retries    = stats_total_retries;
    uint32_t start_mismatches = stats_generation_mismatches;
    uint32_t start_crc_errs   = stats_crc_errors;

    uint32_t t_start = millis();

    for (uint32_t i = 0; i < iterations; i++) {
        // Request RP2040 to generate a fresh 8192-sample synthetic test capture
        char resp[64] = {0};
        spi_master_exec_command("test synth 8192", resp, sizeof(resp));

        // Await DATA_READY with 150 ms timeout
        uint32_t t_wait = millis();
        while (!spi_master_is_data_ready() && (millis() - t_wait < 150)) {
            delayMicroseconds(100);
        }

        if (!spi_master_is_data_ready()) {
            report.crc_errors++;
            continue;
        }

        SpiCaptureInfoPayload info;
        if (spi_master_fetch_complete_capture(bench_buf, sizeof(bench_buf), info)) {
            report.total_transfers++;
            report.total_bytes += info.total_samples;
        } else {
            report.crc_errors++;
        }
    }

    report.total_time_ms = millis() - t_start;
    if (report.total_time_ms > 0) {
        report.effective_mb_s = ((float)report.total_bytes / (1024.0f * 1024.0f)) / ((float)report.total_time_ms / 1000.0f);
    }
    report.retries               = stats_total_retries - start_retries;
    report.generation_mismatches = stats_generation_mismatches - start_mismatches;
    if (report.crc_errors == 0 && (stats_crc_errors > start_crc_errs)) {
        report.crc_errors = stats_crc_errors - start_crc_errs;
    }

    return (report.total_transfers > 0 && report.crc_errors == 0);
}

void spi_master_get_stats(uint32_t& retries, uint32_t& mismatches, uint32_t& crc_errors) {
    retries    = stats_total_retries;
    mismatches = stats_generation_mismatches;
    crc_errors = stats_crc_errors;
}

void spi_master_reset_stats() {
    stats_total_retries         = 0;
    stats_generation_mismatches = 0;
    stats_crc_errors            = 0;
}

bool spi_master_run_mcu_spi_test(McuSpiTestReport& report) {
    memset(&report, 0, sizeof(report));
    report.clock_hz = SPI_DEFAULT_FREQ_HZ;

    if (!s_spi_master_active) {
        spi_master_transport_init();
    }

    // 1. Initial Ping check (5 retries to verify physical link)
    bool link_ok = false;
    for (int p = 0; p < 5; p++) {
        report.tx_packets++;
        if (spi_master_ping()) {
            report.rx_packets++;
            link_ok = true;
            break;
        }
        delay(10);
    }

    if (!link_ok) {
        report.timeouts++;
        report.passed = false;
        return false;
    }

    // 2. Query status
    report.tx_packets++;
    SpiStatusPayload status = {};
    if (spi_master_get_status(status)) {
        report.rx_packets++;
    } else {
        report.crc_failures++;
    }

    // 3. Query capture info metadata
    report.tx_packets++;
    SpiCaptureInfoPayload info = {};
    SpiPacketHeader req = {};
    req.magic = SPI_PROTO_MAGIC;
    req.type  = SPI_PKT_GET_CAPTURE_INFO;
    spi_send_request(req);
    delayMicroseconds(30);

    SpiPacketHeader resp = {};
    if (spi_read_response(resp, (uint8_t*)&info, sizeof(info)) && resp.type == SPI_PKT_CAPTURE_INFO) {
        report.rx_packets++;
        uint16_t expected_crc = spi_calc_block_crc16(info.capture_id, 0, (const uint8_t*)&info, sizeof(info));
        if (expected_crc != resp.block_crc) {
            report.crc_failures++;
        }
    } else {
        report.timeouts++;
    }

    // 4. Repeated block transfers and verification up to 1000 total packets
    uint8_t blk_buf[SPI_BLOCK_SIZE];
    uint16_t total_blks = (info.total_blocks > 0 && info.total_blocks <= 64) ? info.total_blocks : 8;

    while (report.tx_packets < 1000) {
        uint16_t seq = (report.tx_packets) % total_blks;
        memset(&req, 0, sizeof(req));
        req.magic      = SPI_PROTO_MAGIC;
        req.type       = SPI_PKT_GET_BLOCK;
        req.sequence   = seq;
        req.capture_id = info.capture_id;

        report.tx_packets++;
        spi_send_request(req);
        delayMicroseconds(30);

        memset(&resp, 0, sizeof(resp));
        if (spi_read_response(resp, blk_buf, SPI_BLOCK_SIZE)) {
            report.rx_packets++;
            if (resp.sequence != seq) {
                report.sequence_errors++;
            }
            uint16_t blk_crc = spi_calc_block_crc16(resp.capture_id, resp.sequence, blk_buf, resp.payload_len);
            if (blk_crc != resp.block_crc) {
                report.crc_failures++;
            }
            if (seq == 0 && resp.payload_len > 0) {
                for (uint16_t i = 0; i < resp.payload_len; i++) {
                    if (blk_buf[i] != (uint8_t)(i & 0xFF)) {
                        report.data_mismatches++;
                        break;
                    }
                }
            }
        } else {
            report.timeouts++;
        }
    }

    report.passed = (report.crc_failures == 0 &&
                     report.sequence_errors == 0 &&
                     report.timeouts == 0 &&
                     report.data_mismatches == 0 &&
                     report.rx_packets == report.tx_packets);
    return report.passed;
}
