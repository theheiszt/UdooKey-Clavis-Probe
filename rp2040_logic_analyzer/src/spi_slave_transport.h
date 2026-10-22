#ifndef SPI_SLAVE_TRANSPORT_H
#define SPI_SLAVE_TRANSPORT_H

#include <Arduino.h>
#include "spi_protocol.h"

// Initialize optional jumper-wired SPI transport in Slave Mode (GP2..GP5)
void spi_slave_transport_init();
void spi_slave_transport_stop();
bool spi_slave_transport_is_active();

// Main/worker context FSM task: handles request validation, response staging, and DMA re-arming
void spi_slave_transport_task(bool& start_requested, bool& stop_requested);

// Publish completed acquisition descriptor with trigger metadata and latency timestamps
void spi_slave_publish_capture(const uint8_t* aligned_buf, uint32_t sample_count, uint32_t sample_rate_hz,
                               uint16_t trigger_offset, uint32_t trigger_sample = 0,
                               uint8_t trigger_channel = 0, uint8_t trigger_type = 0,
                               uint32_t t_trigger_us = 0, uint32_t t_freeze_us = 0);

// Check if a valid capture descriptor is currently ready
bool spi_slave_is_capture_ready();

// Invalidate current capture descriptor (de-asserts DATA_READY level)
void spi_slave_invalidate_capture();

// Access current capture descriptor (for diagnostics/latency queries)
const CaptureDescriptor& spi_slave_get_descriptor();

// ─── Production Verification Test Helpers ────────────────────────────────────
// Test 1: Generate synthetic 00..FF ramp pattern to detect byte shifts or DMA boundary errors
void spi_slave_test_generate_synthetic(uint32_t sample_count = 8192);

// Test 2: Inject single-bit corruption into specific block to verify targeted retry recovery
void spi_slave_test_inject_corruption(uint16_t block_idx, uint16_t byte_offset = 517);

// Test 3: Simulate capture-generation race by bumping capture_id during transfer
void spi_slave_test_trigger_race();

// Backward compatibility alias
inline void spi_slave_notify_capture_ready(const uint8_t* buf, uint32_t count, uint32_t rate, uint16_t trig_pos) {
    spi_slave_publish_capture(buf, count, rate, trig_pos);
}

inline void spi_slave_clear_data_ready() {
    spi_slave_invalidate_capture();
}

// Set CLI execution response to be returned over SPI
void spi_slave_set_cmd_response(const char* response);

#endif // SPI_SLAVE_TRANSPORT_H
