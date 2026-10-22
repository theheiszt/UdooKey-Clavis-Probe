#include "freq_meter.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"

FreqResult freq_measure_pin(uint8_t pin, uint32_t gate_ms) {
    FreqResult res = { 0.0f, 0.0f, 0.0f, false };
    if (gate_ms == 0) gate_ms = 100;

    // Check if pin is PWM Channel B (odd GPIOs on RP2040)
    if (pwm_gpio_to_channel(pin) == PWM_CHAN_B) {
        uint slice = pwm_gpio_to_slice_num(pin);
        
        // 1. Measure Period / Frequency using PWM_DIV_B_RISING
        pwm_set_enabled(slice, false);
        pwm_config cfg = pwm_get_default_config();
        pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);
        pwm_config_set_clkdiv(&cfg, 1.0f);
        pwm_init(slice, &cfg, false);
        gpio_set_function(pin, GPIO_FUNC_PWM);
        
        pwm_set_counter(slice, 0);
        pwm_set_enabled(slice, true);
        busy_wait_ms(1); // 1ms coarse probe
        pwm_set_enabled(slice, false);
        uint16_t coarse = pwm_get_counter(slice);

        uint32_t actual_gate = gate_ms;
        if (coarse == 0) {
            actual_gate = 100; // Low frequency, use 100ms
        } else if (coarse > 10000) {
            actual_gate = 10;  // High frequency, 10ms is plenty
        }

        pwm_set_counter(slice, 0);
        pwm_set_enabled(slice, true);
        busy_wait_ms(actual_gate);
        pwm_set_enabled(slice, false);
        uint16_t count = pwm_get_counter(slice);

        res.freq_hz = (float)count * 1000.0f / (float)actual_gate;
        if (res.freq_hz > 0.0f) {
            res.period_s = 1.0f / res.freq_hz;
            res.valid = true;
        }

        // 2. Measure Duty Cycle using PWM_DIV_B_HIGH
        if (res.valid) {
            pwm_set_enabled(slice, false);
            pwm_config cfg_duty = pwm_get_default_config();
            pwm_config_set_clkdiv_mode(&cfg_duty, PWM_DIV_B_HIGH);
            // Div = sys_clk / 1MHz for 1us per tick
            float div = (float)clock_get_hz(clk_sys) / 1000000.0f;
            pwm_config_set_clkdiv(&cfg_duty, div);
            pwm_init(slice, &cfg_duty, false);

            pwm_set_counter(slice, 0);
            pwm_set_enabled(slice, true);
            busy_wait_ms(20);
            pwm_set_enabled(slice, false);
            uint16_t high_ticks = pwm_get_counter(slice);

            // Total ticks in 20ms at 1MHz = 20000
            res.duty_percent = ((float)high_ticks / 20000.0f) * 100.0f;
            if (res.duty_percent > 100.0f) res.duty_percent = 100.0f;
        }

        gpio_set_function(pin, GPIO_FUNC_SIO);
        pinMode(pin, INPUT);
    } else {
        // Fallback for non-Channel B pins: Precision timing via pulseInLong
        pinMode(pin, INPUT);
        unsigned long th = pulseInLong(pin, HIGH, 100000); // 100ms timeout
        unsigned long tl = pulseInLong(pin, LOW, 100000);
        if (th > 0 && tl > 0) {
            unsigned long period_us = th + tl;
            res.freq_hz = 1000000.0f / (float)period_us;
            res.period_s = (float)period_us / 1000000.0f;
            res.duty_percent = ((float)th / (float)period_us) * 100.0f;
            res.valid = true;
        }
    }

    return res;
}

static void format_freq_str(float freq, char* buf, size_t sz) {
    if (freq >= 1000000.0f) {
        snprintf(buf, sz, "%.3f MHz", freq / 1000000.0f);
    } else if (freq >= 1000.0f) {
        snprintf(buf, sz, "%.3f kHz", freq / 1000.0f);
    } else {
        snprintf(buf, sz, "%.1f Hz", freq);
    }
}

static void format_period_str(float period, char* buf, size_t sz) {
    if (period >= 1.0f) {
        snprintf(buf, sz, "%.3f s", period);
    } else if (period >= 0.001f) {
        snprintf(buf, sz, "%.3f ms", period * 1000.0f);
    } else if (period >= 0.000001f) {
        snprintf(buf, sz, "%.2f us", period * 1000000.0f);
    } else {
        snprintf(buf, sz, "%.1f ns", period * 1000000000.0f);
    }
}

void freq_print_single(uint8_t pin, Stream& out) {
    out.printf("\r\n[Freq Meter] Measuring GPIO %u...\r\n", pin);
    FreqResult r = freq_measure_pin(pin, 100);
    if (!r.valid || r.freq_hz == 0.0f) {
        out.printf("  GPIO %u: No signal detected (0 Hz)\r\n", pin);
        return;
    }

    char f_str[32], p_str[32];
    format_freq_str(r.freq_hz, f_str, sizeof(f_str));
    format_period_str(r.period_s, p_str, sizeof(p_str));

    out.printf("  GPIO %u: %s | Period: %s | Duty: %.1f%%\r\n", pin, f_str, p_str, r.duty_percent);
}

void freq_run_continuous(uint8_t pin, Stream& out) {
    out.printf("\r\n[Freq Meter Continuous] Streaming GPIO %u (Press any key to stop)...\r\n", pin);
    while (!out.available()) {
        FreqResult r = freq_measure_pin(pin, 100);
        char f_str[32], p_str[32];
        if (r.valid && r.freq_hz > 0.0f) {
            format_freq_str(r.freq_hz, f_str, sizeof(f_str));
            format_period_str(r.period_s, p_str, sizeof(p_str));
            out.printf("\r  -> GPIO %u: %-12s | Period: %-10s | Duty: %5.1f%%   ", pin, f_str, p_str, r.duty_percent);
        } else {
            out.printf("\r  -> GPIO %u: [No Signal / Idle]                              ", pin);
        }
        delay(200);
    }
    // Flush keypress
    while (out.available()) out.read();
    out.println("\r\nContinuous measurement stopped.");
}
