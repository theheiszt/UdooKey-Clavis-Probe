#include "frontend_control.h"

static bool    protection_enabled = true;
static uint8_t prescaler_ratio    = 1; // 1:1 direct

void frontend_init() {
    if (PIN_FRONTEND_PROT_EN >= 0) {
        pinMode(PIN_FRONTEND_PROT_EN, OUTPUT);
        digitalWrite(PIN_FRONTEND_PROT_EN, HIGH); // Safe default: protection enabled
    }

    if (PIN_FRONTEND_PRESCALER >= 0) {
        pinMode(PIN_FRONTEND_PRESCALER, OUTPUT);
        digitalWrite(PIN_FRONTEND_PRESCALER, LOW);  // 1:1 direct pass-through
    }
}

void frontend_set_protection(bool enable) {
    protection_enabled = enable;
    if (PIN_FRONTEND_PROT_EN >= 0) {
        digitalWrite(PIN_FRONTEND_PROT_EN, enable ? HIGH : LOW);
    }
}

bool frontend_get_protection() {
    return protection_enabled;
}

void frontend_set_prescaler(uint8_t ratio) {
    if (ratio != 1 && ratio != 2 && ratio != 4) ratio = 1;
    prescaler_ratio = ratio;
    // 74HC74 hardware mux select line
    digitalWrite(PIN_FRONTEND_PRESCALER, (ratio > 1) ? HIGH : LOW);
}

uint8_t frontend_get_prescaler() {
    return prescaler_ratio;
}

float frontend_adjust_frequency(float raw_freq) {
    return raw_freq * (float)prescaler_ratio;
}

void frontend_print_status(Stream& out) {
    out.printf("\r\n=== Hardware Front-End Status ===\r\n");
    out.printf("  Input Protection / Clamping : [%s]\r\n", protection_enabled ? "ENABLED (Safe)" : "BYPASS (Direct Low-C)");
    out.printf("  74HC74 Hardware Prescaler    : [1:%u Division]\r\n", prescaler_ratio);
    if (prescaler_ratio > 1) {
        out.printf("  (Frequencies multiplied by %ux to compensate prescaler)\r\n", prescaler_ratio);
    }
}
