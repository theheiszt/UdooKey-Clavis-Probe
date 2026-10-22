#include "ws2812_driver.h"
#include "hardware/sync.h"
#include "hardware/structs/sio.h"

// Cycle delay loop for Cortex-M0+
// Each loop iteration takes ~3 cycles (subs + bne)
static inline void __attribute__((always_inline)) delay_cycles(uint32_t count) {
    __asm__ volatile (
        "1: sub %0, #1 \n"
        "   bne 1b     \n"
        : "+r" (count)
        :
        : "cc"
    );
}

void ws2812_show(uint8_t pin, const uint32_t* colors, uint16_t num_leds) {
    if (!colors || num_leds == 0) return;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delayMicroseconds(60); // Reset pulse

    uint32_t mask = 1UL << pin;
    uint32_t save = save_and_disable_interrupts();

    // At 133MHz (~7.5ns/cycle):
    // T0H: 350ns -> ~46 cycles (delay 15 loops)
    // T0L: 800ns -> ~106 cycles (delay 35 loops)
    // T1H: 700ns -> ~93 cycles (delay 31 loops)
    // T1L: 600ns -> ~80 cycles (delay 26 loops)
    for (uint16_t i = 0; i < num_leds; i++) {
        uint32_t rgb = colors[i];
        // WS2812 expects GRB order: Green (bits 23..16), Red (bits 15..8), Blue (bits 7..0)
        uint8_t r = (rgb >> 16) & 0xFF;
        uint8_t g = (rgb >> 8) & 0xFF;
        uint8_t b = rgb & 0xFF;
        uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;

        for (int bit = 23; bit >= 0; bit--) {
            if (grb & (1UL << bit)) {
                // Bit 1: High for 700ns, Low for 600ns
                sio_hw->gpio_set = mask;
                delay_cycles(28);
                sio_hw->gpio_clr = mask;
                delay_cycles(22);
            } else {
                // Bit 0: High for 350ns, Low for 800ns
                sio_hw->gpio_set = mask;
                delay_cycles(12);
                sio_hw->gpio_clr = mask;
                delay_cycles(32);
            }
        }
    }

    restore_interrupts(save);
    delayMicroseconds(80); // Latch
}

void ws2812_set_color(uint8_t pin, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    ws2812_show(pin, &color, 1);
}

void ws2812_turn_off(uint8_t pin, uint16_t num_leds) {
    uint32_t black[32] = {0};
    uint16_t sent = 0;
    while (sent < num_leds) {
        uint16_t chunk = (num_leds - sent > 32) ? 32 : (num_leds - sent);
        ws2812_show(pin, black, chunk);
        sent += chunk;
    }
}

static uint32_t wheel_color(uint8_t pos) {
    pos = 255 - pos;
    if (pos < 85) {
        return ((uint32_t)(255 - pos * 3) << 16) | ((uint32_t)(pos * 3) << 8);
    }
    if (pos < 170) {
        pos -= 85;
        return ((uint32_t)(pos * 3) << 8) | (255 - pos * 3);
    }
    pos -= 170;
    return ((uint32_t)(pos * 3) << 16) | (255 - pos * 3);
}

void ws2812_show_rainbow(uint8_t pin, uint16_t num_leds, Stream& out) {
    if (num_leds == 0) num_leds = 8;
    if (num_leds > 64) num_leds = 64;

    out.printf("[WS2812] Playing rainbow pattern on GPIO %u (%u LEDs)... Press any key to stop.\r\n", pin, num_leds);
    uint32_t buffer[64];
    uint8_t start_pos = 0;

    while (!out.available()) {
        for (uint16_t i = 0; i < num_leds; i++) {
            uint8_t color_idx = (start_pos + (i * 256 / num_leds)) & 0xFF;
            buffer[i] = wheel_color(color_idx);
        }
        ws2812_show(pin, buffer, num_leds);
        start_pos = (start_pos + 4) & 0xFF;
        delay(30);
    }

    while (out.available()) out.read();
    ws2812_turn_off(pin, num_leds);
    out.println("Rainbow stopped.");
}
