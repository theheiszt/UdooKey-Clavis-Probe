#ifndef DEBUG_ENGINE_H
#define DEBUG_ENGINE_H

#include <Arduino.h>
#include <cstdint>
#include <cstddef>

namespace debug_engine {

// Probe Bus JTAG / SWD Pin Assignments (Header P1 Pins 6..12)
constexpr uint8_t PIN_JTAG_TDI  = 4; // GP4 (Probe Bus Pin 6)
constexpr uint8_t PIN_JTAG_TDO  = 5; // GP5 (Probe Bus Pin 8)
constexpr uint8_t PIN_JTAG_TCK  = 6; // GP6 (Probe Bus Pin 7)
constexpr uint8_t PIN_JTAG_TMS  = 7; // GP7 (Probe Bus Pin 9)
constexpr uint8_t PIN_JTAG_TRST = 8; // GP8 (Probe Bus Pin 10)
constexpr uint8_t PIN_JTAG_SRST = 9; // GP9 (Probe Bus Pin 12)

constexpr uint8_t PIN_SWD_CLK   = 7; // GP7 (Probe Bus Pin 9)
constexpr uint8_t PIN_SWD_DIO   = 8; // GP8 (Probe Bus Pin 10)
constexpr uint8_t PIN_SWD_RST   = 9; // GP9 (Probe Bus Pin 12)

// Initialization and pin configuration
void init();
void configure_jtag_pins();
void configure_swd_pins();
void release_pins();

// JTAG Primitives
void jtag_clock_pulse();
void jtag_set_clock_hz(uint32_t hz);
void jtag_tap_reset();
void jtag_clock_tms(bool tms);
void jtag_tms_sequence(const uint8_t* bits, size_t bit_count);
void jtag_shift(bool is_dr, const uint8_t* tdi_bytes, size_t bit_count, uint8_t* tdo_bytes, bool exit_to_idle);
size_t jtag_scan_chain(uint32_t* idcodes_out, uint8_t* ir_lens_out, size_t max_devices);
bool jtag_discover_ir_chain(uint16_t& total_ir_bits, uint8_t* captured_ir, size_t max_bytes);

// SWD Primitives
void swd_line_reset();
void swd_switch_jtag_to_swd();
bool swd_read_dp(uint8_t addr, uint32_t& data);
bool swd_write_dp(uint8_t addr, uint32_t data);
bool swd_read_ap(uint8_t addr, uint32_t& data);
bool swd_write_ap(uint8_t addr, uint32_t data);

// Binary Protocol Engine (Sync 0x5B)
bool process_debug_packet(const uint8_t* req_buf, size_t req_len, uint8_t* resp_buf, size_t max_resp_len, size_t& resp_len);
bool poll_stream(Stream& stream);

} // namespace debug_engine

#endif // DEBUG_ENGINE_H
