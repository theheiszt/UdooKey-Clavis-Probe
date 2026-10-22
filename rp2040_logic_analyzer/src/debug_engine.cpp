#include "debug_engine.h"
#include <hardware/gpio.h>
#include <hardware/timer.h>

namespace debug_engine {

static uint32_t s_clock_hz = 1000000;
static uint32_t s_half_period_us = 1;

static inline void clk_delay() {
    if (s_half_period_us > 0) {
        busy_wait_us(s_half_period_us);
    }
}

// CRC-16-CCITT calculation
static uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

void init() {
    release_pins();
}

void configure_jtag_pins() {
    gpio_init(PIN_JTAG_TCK);
    gpio_set_dir(PIN_JTAG_TCK, GPIO_OUT);
    gpio_put(PIN_JTAG_TCK, 0);

    gpio_init(PIN_JTAG_TMS);
    gpio_set_dir(PIN_JTAG_TMS, GPIO_OUT);
    gpio_put(PIN_JTAG_TMS, 1);

    gpio_init(PIN_JTAG_TDI);
    gpio_set_dir(PIN_JTAG_TDI, GPIO_OUT);
    gpio_put(PIN_JTAG_TDI, 0);

    gpio_init(PIN_JTAG_TDO);
    gpio_set_dir(PIN_JTAG_TDO, GPIO_IN);
    gpio_pull_up(PIN_JTAG_TDO);

    gpio_init(PIN_JTAG_TRST);
    gpio_set_dir(PIN_JTAG_TRST, GPIO_OUT);
    gpio_put(PIN_JTAG_TRST, 1); // Not asserted

    gpio_init(PIN_JTAG_SRST);
    gpio_set_dir(PIN_JTAG_SRST, GPIO_OUT);
    gpio_put(PIN_JTAG_SRST, 1); // Not asserted
}

void configure_swd_pins() {
    gpio_init(PIN_SWD_CLK);
    gpio_set_dir(PIN_SWD_CLK, GPIO_OUT);
    gpio_put(PIN_SWD_CLK, 0);

    gpio_init(PIN_SWD_DIO);
    gpio_set_dir(PIN_SWD_DIO, GPIO_OUT);
    gpio_put(PIN_SWD_DIO, 1);

    gpio_init(PIN_SWD_RST);
    gpio_set_dir(PIN_SWD_RST, GPIO_OUT);
    gpio_put(PIN_SWD_RST, 1); // Deasserted
}

void release_pins() {
    // High-Z HiZ safe state with weak pull-ups
    const uint8_t pins[] = { PIN_JTAG_TDI, PIN_JTAG_TDO, PIN_JTAG_TCK, PIN_JTAG_TMS, PIN_JTAG_TRST, PIN_JTAG_SRST };
    for (uint8_t p : pins) {
        gpio_init(p);
        gpio_set_dir(p, GPIO_IN);
        gpio_pull_up(p);
    }
}

void jtag_clock_pulse() {
    gpio_put(PIN_JTAG_TCK, 0);
    clk_delay();
    gpio_put(PIN_JTAG_TCK, 1);
    clk_delay();
}

void jtag_set_clock_hz(uint32_t hz) {
    s_clock_hz = (hz > 0) ? hz : 1000000;
    if (s_clock_hz >= 2000000) {
        s_half_period_us = 0;
    } else {
        s_half_period_us = 500000 / s_clock_hz;
        if (s_half_period_us == 0) s_half_period_us = 1;
    }
}

void jtag_clock_tms(bool tms) {
    gpio_put(PIN_JTAG_TMS, tms ? 1 : 0);
    jtag_clock_pulse();
}

void jtag_tap_reset() {
    gpio_put(PIN_JTAG_TMS, 1);
    for (int i = 0; i < 5; ++i) {
        jtag_clock_pulse();
    }
    // Navigate to Run-Test/Idle
    gpio_put(PIN_JTAG_TMS, 0);
    jtag_clock_pulse();
}

void jtag_tms_sequence(const uint8_t* bits, size_t bit_count) {
    for (size_t i = 0; i < bit_count; ++i) {
        bool tms = (bits[i / 8] & (1 << (i % 8))) != 0;
        jtag_clock_tms(tms);
    }
}

void jtag_shift(bool is_dr, const uint8_t* tdi_bytes, size_t bit_count, uint8_t* tdo_bytes, bool exit_to_idle) {
    if (bit_count == 0) return;

    // Navigate to Shift-DR or Shift-IR from Run-Test/Idle
    if (is_dr) {
        jtag_clock_tms(true);  // Select-DR-Scan
        jtag_clock_tms(false); // Capture-DR
        jtag_clock_tms(false); // Shift-DR
    } else {
        jtag_clock_tms(true);  // Select-DR-Scan
        jtag_clock_tms(true);  // Select-IR-Scan
        jtag_clock_tms(false); // Capture-IR
        jtag_clock_tms(false); // Shift-IR
    }

    size_t byte_count = (bit_count + 7) / 8;
    if (tdo_bytes) {
        memset(tdo_bytes, 0, byte_count);
    }

    // Shift bits
    for (size_t i = 0; i < bit_count; ++i) {
        bool is_last = (i == bit_count - 1);
        if (is_last) {
            gpio_put(PIN_JTAG_TMS, 1); // Exit1-DR / Exit1-IR
        } else {
            gpio_put(PIN_JTAG_TMS, 0);
        }

        bool tdi = tdi_bytes ? ((tdi_bytes[i / 8] & (1 << (i % 8))) != 0) : false;
        gpio_put(PIN_JTAG_TDI, tdi ? 1 : 0);

        gpio_put(PIN_JTAG_TCK, 0);
        clk_delay();

        if (tdo_bytes) {
            bool tdo = gpio_get(PIN_JTAG_TDO);
            if (tdo) {
                tdo_bytes[i / 8] |= (1 << (i % 8));
            }
        }

        gpio_put(PIN_JTAG_TCK, 1);
        clk_delay();
    }

    // Transition from Exit1 to Idle if requested
    if (exit_to_idle) {
        jtag_clock_tms(true);  // Update-DR / Update-IR
        jtag_clock_tms(false); // Run-Test/Idle
    }
}

size_t jtag_scan_chain(uint32_t* idcodes_out, uint8_t* ir_lens_out, size_t max_devices) {
    configure_jtag_pins();
    jtag_tap_reset();

    // After reset, all TAPs load IDCODE (or BYPASS if no IDCODE) into DR
    // Navigate from Run-Test/Idle to Shift-DR
    jtag_clock_tms(true);  // Select-DR-Scan
    jtag_clock_tms(false); // Capture-DR
    jtag_clock_tms(false); // Shift-DR

    size_t found = 0;
    while (found < max_devices) {
        uint32_t id = 0;
        for (int b = 0; b < 32; ++b) {
            gpio_put(PIN_JTAG_TMS, 0);
            gpio_put(PIN_JTAG_TDI, 0);

            gpio_put(PIN_JTAG_TCK, 0);
            clk_delay();
            bool tdo = gpio_get(PIN_JTAG_TDO);
            if (tdo) {
                id |= (1u << b);
            }
            gpio_put(PIN_JTAG_TCK, 1);
            clk_delay();
        }

        // Check if ID is valid (LSB must be 1, and not all 1s or all 0s)
        if ((id & 1) == 1 && id != 0xFFFFFFFF && id != 0) {
            idcodes_out[found] = id;
            if (ir_lens_out) ir_lens_out[found] = 4; // Default standard ARM 4-bit IR
            found++;
        } else {
            break;
        }
    }

    // Return to Run-Test/Idle
    jtag_clock_tms(true);  // Exit1-DR
    jtag_clock_tms(true);  // Update-DR
    jtag_clock_tms(false); // Run-Test/Idle

    return found;
}

bool jtag_discover_ir_chain(uint16_t& total_ir_bits, uint8_t* captured_ir, size_t max_bytes) {
    configure_jtag_pins();
    jtag_tap_reset();

    // Navigate to Shift-IR: TLR -> RunTestIdle -> Select-DR -> Select-IR -> Capture-IR -> Shift-IR
    jtag_clock_tms(false); // Run-Test/Idle
    jtag_clock_tms(true);  // Select-DR-Scan
    jtag_clock_tms(true);  // Select-IR-Scan
    jtag_clock_tms(false); // Capture-IR (loads '...01' into all TAPs)
    jtag_clock_tms(false); // Shift-IR

    if (captured_ir && max_bytes > 0) {
        memset(captured_ir, 0, max_bytes);
    }

    // Phase 1: Flush chain with 256 bits of '1' while recording captured IR pattern
    size_t record_bits = max_bytes * 8;
    if (record_bits > 128) record_bits = 128;

    for (size_t i = 0; i < 256; ++i) {
        gpio_put(PIN_JTAG_TMS, 0);
        gpio_put(PIN_JTAG_TDI, 1);

        gpio_put(PIN_JTAG_TCK, 0);
        clk_delay();
        bool tdo = gpio_get(PIN_JTAG_TDO);
        if (captured_ir && i < record_bits && tdo) {
            captured_ir[i / 8] |= (1 << (i % 8));
        }
        gpio_put(PIN_JTAG_TCK, 1);
        clk_delay();
    }

    // Phase 2: Shift in a single '0' followed by '1's, count cycles until TDO reads '0'
    uint16_t count = 0;
    bool found_zero = false;
    for (uint16_t i = 0; i < 512; ++i) {
        gpio_put(PIN_JTAG_TMS, 0);
        gpio_put(PIN_JTAG_TDI, (i == 0) ? 0 : 1);

        gpio_put(PIN_JTAG_TCK, 0);
        clk_delay();
        bool tdo = gpio_get(PIN_JTAG_TDO);
        gpio_put(PIN_JTAG_TCK, 1);
        clk_delay();

        if (!tdo && !found_zero) {
            count = i + 1;
            found_zero = true;
            break;
        }
    }

    total_ir_bits = found_zero ? count : 0;

    // Return to Run-Test/Idle
    jtag_clock_tms(true);  // Exit1-IR
    jtag_clock_tms(true);  // Update-IR
    jtag_clock_tms(false); // Run-Test/Idle

    return found_zero;
}

// ---------------- SWD Primitives ----------------

static void swd_write_bit(bool bit) {
    gpio_put(PIN_SWD_CLK, 0);
    gpio_put(PIN_SWD_DIO, bit ? 1 : 0);
    clk_delay();
    gpio_put(PIN_SWD_CLK, 1);
    clk_delay();
}

static bool swd_read_bit() {
    gpio_put(PIN_SWD_CLK, 0);
    clk_delay();
    bool bit = gpio_get(PIN_SWD_DIO);
    gpio_put(PIN_SWD_CLK, 1);
    clk_delay();
    return bit;
}

void swd_line_reset() {
    configure_swd_pins();
    gpio_set_dir(PIN_SWD_DIO, GPIO_OUT);
    gpio_put(PIN_SWD_DIO, 1);
    for (int i = 0; i < 55; ++i) {
        gpio_put(PIN_SWD_CLK, 0);
        clk_delay();
        gpio_put(PIN_SWD_CLK, 1);
        clk_delay();
    }
}

void swd_switch_jtag_to_swd() {
    swd_line_reset();

    // 0xE79E (16 bits LSB first)
    uint16_t seq = 0xE79E;
    for (int i = 0; i < 16; ++i) {
        swd_write_bit((seq >> i) & 1);
    }

    swd_line_reset();

    // Idle cycles
    gpio_put(PIN_SWD_DIO, 0);
    for (int i = 0; i < 16; ++i) {
        gpio_put(PIN_SWD_CLK, 0);
        clk_delay();
        gpio_put(PIN_SWD_CLK, 1);
        clk_delay();
    }
}

bool swd_read_dp(uint8_t addr, uint32_t& data) {
    uint8_t a2 = (addr >> 2) & 1;
    uint8_t a3 = (addr >> 3) & 1;
    uint8_t parity = (0 ^ 1 ^ a2 ^ a3) & 1;

    gpio_set_dir(PIN_SWD_DIO, GPIO_OUT);
    swd_write_bit(1);      // Start
    swd_write_bit(0);      // APnDP (0=DP)
    swd_write_bit(1);      // RnW (1=Read)
    swd_write_bit(a2);     // A[2]
    swd_write_bit(a3);     // A[3]
    swd_write_bit(parity); // Parity
    swd_write_bit(0);      // Stop
    swd_write_bit(1);      // Park

    // Turnaround: Switch SWDIO to input
    gpio_set_dir(PIN_SWD_DIO, GPIO_IN);
    gpio_pull_up(PIN_SWD_DIO);
    gpio_put(PIN_SWD_CLK, 0);
    clk_delay();
    gpio_put(PIN_SWD_CLK, 1);
    clk_delay();

    // Read ACK (3 bits)
    uint8_t ack = 0;
    ack |= swd_read_bit() ? 1 : 0;
    ack |= (swd_read_bit() ? 1 : 0) << 1;
    ack |= (swd_read_bit() ? 1 : 0) << 2;

    if (ack != 1) { // 1 = OK
        return false;
    }

    // Read 32-bit data
    uint32_t val = 0;
    uint8_t calc_parity = 0;
    for (int i = 0; i < 32; ++i) {
        bool b = swd_read_bit();
        if (b) {
            val |= (1u << i);
            calc_parity ^= 1;
        }
    }

    // Read parity bit
    bool rcv_parity = swd_read_bit();
    if (rcv_parity != (calc_parity != 0)) {
        return false;
    }

    // Turnaround
    gpio_set_dir(PIN_SWD_DIO, GPIO_OUT);
    gpio_put(PIN_SWD_DIO, 0);
    gpio_put(PIN_SWD_CLK, 0);
    clk_delay();
    gpio_put(PIN_SWD_CLK, 1);
    clk_delay();

    data = val;
    return true;
}

bool swd_write_dp(uint8_t addr, uint32_t data) {
    uint8_t a2 = (addr >> 2) & 1;
    uint8_t a3 = (addr >> 3) & 1;
    uint8_t parity = (0 ^ 0 ^ a2 ^ a3) & 1;

    gpio_set_dir(PIN_SWD_DIO, GPIO_OUT);
    swd_write_bit(1);      // Start
    swd_write_bit(0);      // APnDP (0=DP)
    swd_write_bit(0);      // RnW (0=Write)
    swd_write_bit(a2);     // A[2]
    swd_write_bit(a3);     // A[3]
    swd_write_bit(parity); // Parity
    swd_write_bit(0);      // Stop
    swd_write_bit(1);      // Park

    // Turnaround: Switch SWDIO to input
    gpio_set_dir(PIN_SWD_DIO, GPIO_IN);
    gpio_pull_up(PIN_SWD_DIO);
    gpio_put(PIN_SWD_CLK, 0);
    clk_delay();
    gpio_put(PIN_SWD_CLK, 1);
    clk_delay();

    // Read ACK (3 bits)
    uint8_t ack = 0;
    ack |= swd_read_bit() ? 1 : 0;
    ack |= (swd_read_bit() ? 1 : 0) << 1;
    ack |= (swd_read_bit() ? 1 : 0) << 2;

    if (ack != 1) {
        return false;
    }

    // Turnaround: Switch SWDIO to output
    gpio_set_dir(PIN_SWD_DIO, GPIO_OUT);
    gpio_put(PIN_SWD_CLK, 0);
    clk_delay();
    gpio_put(PIN_SWD_CLK, 1);
    clk_delay();

    // Send 32-bit data
    uint8_t calc_parity = 0;
    for (int i = 0; i < 32; ++i) {
        bool b = (data >> i) & 1;
        swd_write_bit(b);
        calc_parity ^= b ? 1 : 0;
    }
    swd_write_bit(calc_parity != 0);

    return true;
}

bool swd_read_ap(uint8_t addr, uint32_t& data) {
    // AP read sequence requires setting DP SELECT to APBANKSEL, then reading AP, then reading DP RDBUFF
    uint8_t apbank = addr & 0xF0;
    if (!swd_write_dp(0x08, apbank)) return false;

    uint8_t a2 = (addr >> 2) & 1;
    uint8_t a3 = (addr >> 3) & 1;
    uint8_t parity = (1 ^ 1 ^ a2 ^ a3) & 1;

    gpio_set_dir(PIN_SWD_DIO, GPIO_OUT);
    swd_write_bit(1);      // Start
    swd_write_bit(1);      // APnDP (1=AP)
    swd_write_bit(1);      // RnW (1=Read)
    swd_write_bit(a2);
    swd_write_bit(a3);
    swd_write_bit(parity);
    swd_write_bit(0);
    swd_write_bit(1);

    // Turnaround
    gpio_set_dir(PIN_SWD_DIO, GPIO_IN);
    gpio_pull_up(PIN_SWD_DIO);
    gpio_put(PIN_SWD_CLK, 0);
    clk_delay();
    gpio_put(PIN_SWD_CLK, 1);
    clk_delay();

    uint8_t ack = 0;
    ack |= swd_read_bit() ? 1 : 0;
    ack |= (swd_read_bit() ? 1 : 0) << 1;
    ack |= (swd_read_bit() ? 1 : 0) << 2;
    if (ack != 1) return false;

    // Discard initial turnaround read data (posted read)
    for (int i = 0; i < 33; ++i) swd_read_bit();

    // Read DP RDBUFF (0x0C) to capture actual result
    return swd_read_dp(0x0C, data);
}

bool swd_write_ap(uint8_t addr, uint32_t data) {
    uint8_t apbank = addr & 0xF0;
    if (!swd_write_dp(0x08, apbank)) return false;

    uint8_t a2 = (addr >> 2) & 1;
    uint8_t a3 = (addr >> 3) & 1;
    uint8_t parity = (1 ^ 0 ^ a2 ^ a3) & 1;

    gpio_set_dir(PIN_SWD_DIO, GPIO_OUT);
    swd_write_bit(1);      // Start
    swd_write_bit(1);      // APnDP (1=AP)
    swd_write_bit(0);      // RnW (0=Write)
    swd_write_bit(a2);
    swd_write_bit(a3);
    swd_write_bit(parity);
    swd_write_bit(0);
    swd_write_bit(1);

    // Turnaround
    gpio_set_dir(PIN_SWD_DIO, GPIO_IN);
    gpio_pull_up(PIN_SWD_DIO);
    gpio_put(PIN_SWD_CLK, 0);
    clk_delay();
    gpio_put(PIN_SWD_CLK, 1);
    clk_delay();

    uint8_t ack = 0;
    ack |= swd_read_bit() ? 1 : 0;
    ack |= (swd_read_bit() ? 1 : 0) << 1;
    ack |= (swd_read_bit() ? 1 : 0) << 2;
    if (ack != 1) return false;

    // Turnaround
    gpio_set_dir(PIN_SWD_DIO, GPIO_OUT);
    gpio_put(PIN_SWD_CLK, 0);
    clk_delay();
    gpio_put(PIN_SWD_CLK, 1);
    clk_delay();

    uint8_t calc_parity = 0;
    for (int i = 0; i < 32; ++i) {
        bool b = (data >> i) & 1;
        swd_write_bit(b);
        calc_parity ^= b ? 1 : 0;
    }
    swd_write_bit(calc_parity != 0);

    return true;
}

// ---------------- Packet Processing Engine ----------------

bool process_debug_packet(const uint8_t* req_buf, size_t req_len, uint8_t* resp_buf, size_t max_resp_len, size_t& resp_len) {
    if (req_len < 8 || req_buf[0] != 0x5B) {
        return false;
    }

    uint8_t cmd = req_buf[1];
    uint8_t seq = req_buf[2];
    uint16_t payload_len = (static_cast<uint16_t>(req_buf[4]) << 8) | req_buf[5];

    if (req_len != static_cast<size_t>(payload_len + 8)) {
        return false;
    }

    // Verify CRC
    uint16_t expected_crc = (static_cast<uint16_t>(req_buf[6 + payload_len]) << 8) | req_buf[7 + payload_len];
    uint16_t actual_crc = crc16(&req_buf[1], 5 + payload_len);
    if (expected_crc != actual_crc) {
        return false;
    }

    const uint8_t* payload = &req_buf[6];

    // Build response payload
    uint8_t resp_payload[256];
    size_t resp_payload_len = 0;
    uint8_t status = 0; // OK

    switch (cmd) {
        case 0x01: { // GET_STATUS
            resp_payload[0] = 0x01; // Connected
            resp_payload[1] = 0x00; // Idle state
            resp_payload_len = 2;
            break;
        }
        case 0x10: { // JTAG_RESET
            configure_jtag_pins();
            jtag_tap_reset();
            resp_payload[0] = 0x00; // Test-Logic-Reset
            resp_payload_len = 1;
            break;
        }
        case 0x11: { // JTAG_SET_CLOCK
            if (payload_len >= 4) {
                uint32_t hz = (static_cast<uint32_t>(payload[0]) << 24) |
                              (static_cast<uint32_t>(payload[1]) << 16) |
                              (static_cast<uint32_t>(payload[2]) << 8)  |
                              payload[3];
                jtag_set_clock_hz(hz);
            }
            break;
        }
        case 0x12: { // JTAG_TMS_SEQ
            if (payload_len >= 1) {
                uint8_t bits = payload[0];
                jtag_tms_sequence(&payload[1], bits);
                resp_payload[0] = 0x01; // Current state approximation
                resp_payload_len = 1;
            }
            break;
        }
        case 0x15: { // JTAG_TRANSACTION
            if (payload_len >= 4 && payload[0] != 2) {
                bool is_dr = (payload[0] == 1);
                uint16_t bits = (static_cast<uint16_t>(payload[1]) << 8) | payload[2];
                // bool exit_state = (payload[3] != 0);
                const uint8_t* tdi = &payload[4];

                uint8_t tdo[64] = {0};
                size_t byte_count = (bits + 7) / 8;
                if (byte_count > sizeof(tdo)) byte_count = sizeof(tdo);

                jtag_shift(is_dr, tdi, bits, tdo, true);

                resp_payload[0] = 0x01; // RunTestIdle
                resp_payload[1] = (bits >> 8) & 0xFF;
                resp_payload[2] = bits & 0xFF;
                memcpy(&resp_payload[3], tdo, byte_count);
                resp_payload_len = 3 + byte_count;
            } else if (payload_len >= 2 && payload[0] == 2) {
                // Deterministic batch execution
                uint8_t op_count = payload[1];
                size_t offset = 2;
                uint8_t shift_results = 0;
                resp_payload[0] = 0x01; // Final state: RunTestIdle
                resp_payload[1] = 0;    // Shift result count (placeholder)
                resp_payload_len = 2;

                for (uint8_t op = 0; op < op_count && offset < payload_len; ++op) {
                    uint8_t op_type = payload[offset++];
                    if (op_type == 0x01) { // TapReset
                        jtag_tap_reset();
                    } else if (op_type == 0x02) { // GotoState
                        if (offset < payload_len) {
                            uint8_t st = payload[offset++];
                            if (st == 0x00) jtag_tap_reset();
                            else if (st == 0x01) { jtag_clock_tms(false); }
                        }
                    } else if (op_type == 0x03) { // ClockTms
                        if (offset < payload_len) {
                            uint8_t bit_cnt = payload[offset++];
                            size_t bytes = (bit_cnt + 7) / 8;
                            jtag_tms_sequence(&payload[offset], bit_cnt);
                            offset += bytes;
                        }
                    } else if (op_type == 0x04 || op_type == 0x05) { // ShiftIr / ShiftDr
                        if (offset + 3 <= payload_len) {
                            uint16_t bit_cnt = (static_cast<uint16_t>(payload[offset]) << 8) | payload[offset + 1];
                            offset += 3;
                            size_t byte_cnt = (bit_cnt + 7) / 8;
                            const uint8_t* tdi = (offset + byte_cnt <= payload_len) ? &payload[offset] : nullptr;
                            offset += byte_cnt;

                            uint8_t tdo[64] = {0};
                            size_t copy_len = byte_cnt > sizeof(tdo) ? sizeof(tdo) : byte_cnt;
                            jtag_shift(op_type == 0x05, tdi, bit_cnt, tdo, true);

                            if (resp_payload_len + 2 + copy_len <= max_resp_len) {
                                resp_payload[resp_payload_len++] = (bit_cnt >> 8) & 0xFF;
                                resp_payload[resp_payload_len++] = bit_cnt & 0xFF;
                                memcpy(&resp_payload[resp_payload_len], tdo, copy_len);
                                resp_payload_len += copy_len;
                                shift_results++;
                            }
                        }
                    } else if (op_type == 0x06) { // ClockIdle
                        if (offset + 2 <= payload_len) {
                            uint16_t cycles = (static_cast<uint16_t>(payload[offset]) << 8) | payload[offset + 1];
                            offset += 2;
                            for (uint16_t c = 0; c < cycles; ++c) {
                                jtag_clock_tms(false);
                            }
                        }
                    }
                }
                resp_payload[1] = shift_results;
            }
            break;
        }
        case 0x16: { // JTAG_SCAN_CHAIN
            uint32_t ids[8];
            uint8_t irs[8];
            size_t count = jtag_scan_chain(ids, irs, 8);

            resp_payload[0] = static_cast<uint8_t>(count);
            resp_payload_len = 1;
            for (size_t i = 0; i < count; ++i) {
                resp_payload[resp_payload_len++] = ids[i] & 0xFF;
                resp_payload[resp_payload_len++] = (ids[i] >> 8) & 0xFF;
                resp_payload[resp_payload_len++] = (ids[i] >> 16) & 0xFF;
                resp_payload[resp_payload_len++] = (ids[i] >> 24) & 0xFF;
                resp_payload[resp_payload_len++] = irs[i];
            }
            break;
        }
        case 0x17: { // JTAG_DISCOVER_IR
            uint16_t total_ir = 0;
            uint8_t captured[32] = {0};
            if (jtag_discover_ir_chain(total_ir, captured, sizeof(captured))) {
                resp_payload[0] = (total_ir >> 8) & 0xFF;
                resp_payload[1] = total_ir & 0xFF;
                size_t byte_count = (total_ir + 7) / 8;
                if (byte_count > sizeof(captured)) byte_count = sizeof(captured);
                memcpy(&resp_payload[2], captured, byte_count);
                resp_payload_len = 2 + byte_count;
            } else {
                status = 0x04; // ERR_TARGET_FAULT
            }
            break;
        }
        case 0x20: { // SWD_RESET
            swd_line_reset();
            break;
        }
        case 0x21: { // SWD_CONNECT
            swd_switch_jtag_to_swd();
            uint32_t dpidr = 0;
            if (swd_read_dp(0x00, dpidr)) {
                resp_payload[0] = dpidr & 0xFF;
                resp_payload[1] = (dpidr >> 8) & 0xFF;
                resp_payload[2] = (dpidr >> 16) & 0xFF;
                resp_payload[3] = (dpidr >> 24) & 0xFF;
                resp_payload_len = 4;
            } else {
                status = 0x04; // ERR_TARGET_FAULT
            }
            break;
        }
        case 0x23: { // SWD_READ_DP
            if (payload_len >= 1) {
                uint32_t val = 0;
                if (swd_read_dp(payload[0], val)) {
                    resp_payload[0] = val & 0xFF;
                    resp_payload[1] = (val >> 8) & 0xFF;
                    resp_payload[2] = (val >> 16) & 0xFF;
                    resp_payload[3] = (val >> 24) & 0xFF;
                    resp_payload[4] = 0x01; // Ack Ok
                    resp_payload_len = 5;
                } else {
                    status = 0x04;
                }
            }
            break;
        }
        case 0x24: { // SWD_WRITE_DP
            if (payload_len >= 5) {
                uint32_t val = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);
                if (swd_write_dp(payload[0], val)) {
                    resp_payload[0] = 0x01; // Ack Ok
                    resp_payload_len = 1;
                } else {
                    status = 0x04;
                }
            }
            break;
        }
        case 0x25: { // SWD_READ_AP
            if (payload_len >= 1) {
                uint32_t val = 0;
                if (swd_read_ap(payload[0], val)) {
                    resp_payload[0] = val & 0xFF;
                    resp_payload[1] = (val >> 8) & 0xFF;
                    resp_payload[2] = (val >> 16) & 0xFF;
                    resp_payload[3] = (val >> 24) & 0xFF;
                    resp_payload[4] = 0x01; // Ack Ok
                    resp_payload_len = 5;
                } else {
                    status = 0x04;
                }
            }
            break;
        }
        case 0x26: { // SWD_WRITE_AP
            if (payload_len >= 5) {
                uint32_t val = payload[1] | (payload[2] << 8) | (payload[3] << 16) | (payload[4] << 24);
                if (swd_write_ap(payload[0], val)) {
                    resp_payload[0] = 0x01; // Ack Ok
                    resp_payload_len = 1;
                } else {
                    status = 0x04;
                }
            }
            break;
        }
        default:
            status = 0x07; // ERR_UNSUPPORTED
            break;
    }

    if (max_resp_len < 8 + resp_payload_len) {
        return false;
    }

    resp_buf[0] = 0x5B;           // SYNC
    resp_buf[1] = cmd | 0x80;     // Response command
    resp_buf[2] = seq;            // Sequence number
    resp_buf[3] = status;         // Status code
    resp_buf[4] = (resp_payload_len >> 8) & 0xFF;
    resp_buf[5] = resp_payload_len & 0xFF;

    if (resp_payload_len > 0) {
        memcpy(&resp_buf[6], resp_payload, resp_payload_len);
    }

    uint16_t c = crc16(&resp_buf[1], 5 + resp_payload_len);
    resp_buf[6 + resp_payload_len] = (c >> 8) & 0xFF;
    resp_buf[7 + resp_payload_len] = c & 0xFF;

    resp_len = 8 + resp_payload_len;
    return true;
}

bool poll_stream(Stream& stream) {
    if (!stream.available()) return false;

    // Peek for SYNC byte
    if (stream.peek() != 0x5B) {
        return false; // Not a debug packet, leave for terminal/SUMP
    }

    uint8_t header[6];
    if (stream.readBytes(header, 6) != 6) {
        return false;
    }

    uint16_t payload_len = (static_cast<uint16_t>(header[4]) << 8) | header[5];
    uint8_t packet[300];
    memcpy(packet, header, 6);

    size_t to_read = payload_len + 2; // payload + 2 CRC bytes
    if (to_read > sizeof(packet) - 6) {
        return false;
    }

    if (stream.readBytes(&packet[6], to_read) != to_read) {
        return false;
    }

    uint8_t resp[300];
    size_t resp_len = 0;
    if (process_debug_packet(packet, 6 + to_read, resp, sizeof(resp), resp_len)) {
        stream.write(resp, resp_len);
        stream.flush();
        return true;
    }

    return false;
}

} // namespace debug_engine
