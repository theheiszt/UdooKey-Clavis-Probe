#include "swd_probe.h"

// Low-level SWD bit-banging helpers
static inline void swd_clk_delay() {
    delayMicroseconds(1); // ~500kHz SWD clock
}

static void swd_write_bit(uint8_t swclk, uint8_t swdio, uint8_t bit) {
    digitalWrite(swclk, LOW);
    digitalWrite(swdio, bit ? HIGH : LOW);
    swd_clk_delay();
    digitalWrite(swclk, HIGH);
    swd_clk_delay();
}

static uint8_t swd_read_bit(uint8_t swclk, uint8_t swdio) {
    digitalWrite(swclk, LOW);
    swd_clk_delay();
    uint8_t bit = digitalRead(swdio);
    digitalWrite(swclk, HIGH);
    swd_clk_delay();
    return bit;
}

static void swd_write_bits(uint8_t swclk, uint8_t swdio, uint32_t val, uint8_t bits) {
    for (uint8_t i = 0; i < bits; i++) {
        swd_write_bit(swclk, swdio, (val >> i) & 1);
    }
}

static void swd_line_reset(uint8_t swclk, uint8_t swdio) {
    pinMode(swdio, OUTPUT);
    digitalWrite(swdio, HIGH);
    for (int i = 0; i < 55; i++) {
        digitalWrite(swclk, LOW);
        swd_clk_delay();
        digitalWrite(swclk, HIGH);
        swd_clk_delay();
    }
}

static void swd_switch_jtag_to_swd(uint8_t swclk, uint8_t swdio) {
    swd_line_reset(swclk, swdio);
    // Standard ARM JTAG-to-SWD 16-bit sequence: 0x79E7 (transmitted MSB first or LSB first: 0xE79E)
    // 0111 1001 1110 0111 LSB first = 0xE79E
    swd_write_bits(swclk, swdio, 0xE79E, 16);
    swd_line_reset(swclk, swdio);
    // At least 8 idle cycles
    digitalWrite(swdio, LOW);
    for (int i = 0; i < 16; i++) {
        digitalWrite(swclk, LOW);
        swd_clk_delay();
        digitalWrite(swclk, HIGH);
        swd_clk_delay();
    }
}

static bool swd_read_dp(uint8_t swclk, uint8_t swdio, uint8_t reg_addr, uint32_t& data) {
    // DP Read: APnDP=0, RnW=1
    uint8_t a2 = (reg_addr >> 2) & 1;
    uint8_t a3 = (reg_addr >> 3) & 1;
    uint8_t parity = (0 ^ 1 ^ a2 ^ a3) & 1;

    pinMode(swdio, OUTPUT);
    swd_write_bit(swclk, swdio, 1); // Start
    swd_write_bit(swclk, swdio, 0); // DP
    swd_write_bit(swclk, swdio, 1); // Read
    swd_write_bit(swclk, swdio, a2);
    swd_write_bit(swclk, swdio, a3);
    swd_write_bit(swclk, swdio, parity);
    swd_write_bit(swclk, swdio, 0); // Stop
    swd_write_bit(swclk, swdio, 1); // Park

    // Turnaround: Switch SWDIO to input
    pinMode(swdio, INPUT_PULLUP);
    digitalWrite(swclk, LOW);
    swd_clk_delay();
    digitalWrite(swclk, HIGH);
    swd_clk_delay();

    // Read ACK (3 bits, LSB first: 1, 0, 0 = OK = 1)
    uint8_t ack = 0;
    ack |= swd_read_bit(swclk, swdio);
    ack |= (swd_read_bit(swclk, swdio) << 1);
    ack |= (swd_read_bit(swclk, swdio) << 2);

    if (ack != 1) { // 1 = OK
        // Send idle cycles to recover
        for (int i = 0; i < 8; i++) {
            digitalWrite(swclk, LOW);
            swd_clk_delay();
            digitalWrite(swclk, HIGH);
            swd_clk_delay();
        }
        return false;
    }

    // Read 32-bit data
    uint32_t val = 0;
    uint8_t data_parity = 0;
    for (uint8_t i = 0; i < 32; i++) {
        uint8_t b = swd_read_bit(swclk, swdio);
        if (b) {
            val |= (1UL << i);
            data_parity ^= 1;
        }
    }
    uint8_t read_parity = swd_read_bit(swclk, swdio);

    // Turnaround + idle
    for (int i = 0; i < 8; i++) {
        digitalWrite(swclk, LOW);
        swd_clk_delay();
        digitalWrite(swclk, HIGH);
        swd_clk_delay();
    }

    if (data_parity != read_parity) return false;
    data = val;
    return true;
}

static const char* decode_arm_part(uint32_t dpidr) {
    uint32_t part = (dpidr >> 12) & 0xFFFF;
    switch (part) {
        case 0x0BC1: return "ARM Cortex-M0+ (RP2040 / SAMD21 / STM32G0)";
        case 0x0BB1: return "ARM Cortex-M0 (STM32F0 / NRF51)";
        case 0x1BA0: return "ARM Cortex-M3 (STM32F1 / LPC1768)";
        case 0x2BA0: return "ARM Cortex-M4 (STM32F4 / NRF52840 / SAM4E)";
        case 0x5BA0: return "ARM Cortex-M7 (STM32H7 / i.MX RT1060)";
        case 0x0BE1: return "ARM Cortex-M33 (STM32U5 / LPC5500)";
        default:     return "ARM Cortex-M Core (Unknown/Custom Part)";
    }
}

SwdTargetInfo swd_probe_target(uint8_t swclk_pin, uint8_t swdio_pin) {
    SwdTargetInfo info = { 0, 0, "Unknown", "Unknown", 0, false };

    pinMode(swclk_pin, OUTPUT);
    digitalWrite(swclk_pin, HIGH);
    pinMode(swdio_pin, OUTPUT);
    digitalWrite(swdio_pin, HIGH);

    swd_switch_jtag_to_swd(swclk_pin, swdio_pin);

    // Read DPIDR (Register 0x00)
    uint32_t dpidr = 0;
    if (swd_read_dp(swclk_pin, swdio_pin, 0x00, dpidr)) {
        if (dpidr != 0x00000000 && dpidr != 0xFFFFFFFF) {
            info.dpidr = dpidr;
            info.target_found = true;
            info.revision = (dpidr >> 28) & 0x0F;
            info.part_name = decode_arm_part(dpidr);

            uint16_t designer = (dpidr >> 1) & 0x7FF;
            if (designer == 0x017 || designer == 0x477) {
                info.designer_name = "ARM Ltd.";
            } else {
                info.designer_name = "Custom JEDEC";
            }
        }
    }

    pinMode(swclk_pin, INPUT);
    pinMode(swdio_pin, INPUT);
    return info;
}

void swd_print_info(uint8_t swclk_pin, uint8_t swdio_pin, Stream& out) {
    out.printf("\r\n[SWD Interrogator] Probing target on SWCLK=GP%u, SWDIO=GP%u...\r\n", swclk_pin, swdio_pin);
    SwdTargetInfo t = swd_probe_target(swclk_pin, swdio_pin);
    if (!t.target_found) {
        out.println("  No ARM Cortex SWD target responded. (Check wiring, power, & pull-up).");
        return;
    }

    out.printf("  -> Target Detected!\r\n");
    out.printf("  DPIDR      : 0x%08lX (Revision %u)\r\n", (unsigned long)t.dpidr, t.revision);
    out.printf("  Core Type  : %s\r\n", t.part_name);
    out.printf("  Designer   : %s\r\n", t.designer_name);
}
