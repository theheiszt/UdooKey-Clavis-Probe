#ifndef SPI_MASTER_TRANSPORT_H
#define SPI_MASTER_TRANSPORT_H

#include <Arduino.h>
#include "spi_protocol.h"

struct SpiBenchmarkReport {
    uint32_t total_transfers;
    uint32_t total_bytes;
    uint32_t total_time_ms;
    float    effective_mb_s;
    uint32_t crc_errors;
    uint32_t retries;
    uint32_t generation_mismatches;
};

struct McuSpiTestReport {
    uint32_t clock_hz;
    uint32_t tx_packets;
    uint32_t rx_packets;
    uint32_t crc_failures;
    uint32_t sequence_errors;
    uint32_t timeouts;
    uint32_t data_mismatches;
    bool     passed;
};

// Transport Lifecycle Control
void spi_master_transport_init();
void spi_master_transport_stop();
bool spi_master_transport_is_active();

// RP2040 Hardware Reset Control (ESP32 IO23)
void rp2040_reset_assert();
void rp2040_reset_release();
bool rp2040_reboot(uint32_t hold_ms = 50);

// Ping RP2040 slave, returns true if pong received with correct magic
bool spi_master_ping();

// Query status from RP2040
bool spi_master_get_status(SpiStatusPayload& status);

// Check if RP2040 has capture data ready (hardware level-triggered DATA_READY pin)
bool spi_master_is_data_ready();

// Fetch capture info and all 1024-byte blocks into dest_buffer:
// - Validates explicit invariants (capture_id, sequence, payload_len <= SPI_BLOCK_SIZE)
// - Validates Level 1 per-block CRC-16 (covering capture_id, sequence, and payload)
// - Validates Level 2 whole-capture CRC-16 across assembled acquisition buffer
// - Gracefully issues CAPTURE_ACK to release slave DATA_READY level
bool spi_master_fetch_complete_capture(uint8_t* dest_buffer, uint32_t max_len, SpiCaptureInfoPayload& info);

// Explicitly acknowledge capture reception, dropping slave DATA_READY level
bool spi_master_acknowledge_capture();

// Send acquisition control commands
bool spi_master_start_capture(uint32_t rate_hz = 10000000, uint32_t sample_count = 8192);
bool spi_master_stop_capture();

// Execute a CLI / Bus Pirate command via SPI mailbox and receive response
bool spi_master_exec_command(const char* cmd, char* resp_buf, size_t max_resp_len);

// ─── Production Verification Test Helpers ────────────────────────────────────
// Verify Test 1 synthetic 00..FF ramp pattern (detects byte shifts, dropped bytes, DMA boundaries)
bool spi_master_test_verify_synthetic(const uint8_t* buf, uint32_t len, uint32_t& first_err_offset);

// Run automated multi-capture benchmark report
bool spi_master_run_benchmark(uint32_t iterations, SpiBenchmarkReport& report);

// Run Section 15 1000-packet comprehensive inter-MCU SPI self-test
bool spi_master_run_mcu_spi_test(McuSpiTestReport& report);

// Query and reset cumulative transport stats
void spi_master_get_stats(uint32_t& retries, uint32_t& mismatches, uint32_t& crc_errors);
void spi_master_reset_stats();

#endif // SPI_MASTER_TRANSPORT_H
