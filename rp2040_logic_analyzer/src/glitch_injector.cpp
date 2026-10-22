#include "glitch_injector.h"
#include "hardware/sync.h"
#include "hardware/structs/sio.h"

static inline void __attribute__((always_inline)) delay_glitch_cycles(uint32_t count) {
    __asm__ volatile (
        "1: sub %0, #1 \n"
        "   bne 1b     \n"
        : "+r" (count)
        :
        : "cc"
    );
}

void glitch_fire_pulse(uint8_t pin, uint32_t delay_us, uint32_t pulse_ns) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);

    uint32_t mask = 1UL << pin;
    uint32_t save = save_and_disable_interrupts();

    if (delay_us > 0) {
        busy_wait_us(delay_us);
    }

    // At 133MHz, 1 cycle = ~7.52ns. Loop is 3 cycles = ~22.5ns
    uint32_t loops = (pulse_ns > 22) ? (pulse_ns / 22) : 1;

    sio_hw->gpio_set = mask;
    delay_glitch_cycles(loops);
    sio_hw->gpio_clr = mask;

    restore_interrupts(save);
}

void glitch_sweep(uint8_t pin, uint32_t start_delay_us, uint32_t end_delay_us, uint32_t step_us, uint32_t pulse_ns, Stream& out) {
    if (step_us == 0) step_us = 1;
    out.printf("\r\n[Glitch Sweep] Target Pin: GPIO %u | Delay: %lu..%lu us (step %lu us) | Pulse: %lu ns\r\n",
               pin, (unsigned long)start_delay_us, (unsigned long)end_delay_us, (unsigned long)step_us, (unsigned long)pulse_ns);
    
    uint32_t count = 0;
    for (uint32_t d = start_delay_us; d <= end_delay_us; d += step_us) {
        glitch_fire_pulse(pin, d, pulse_ns);
        count++;
        if (count % 10 == 0) {
            out.printf("  Fired step %lu (delay=%lu us)\r\n", (unsigned long)count, (unsigned long)d);
        }
        delay(5); // Recovery period between glitch attempts
        if (out.available()) {
            while (out.available()) out.read();
            out.println("  Glitch sweep aborted by user.");
            return;
        }
    }
    out.printf("Sweep complete: %lu glitch pulses delivered.\r\n", (unsigned long)count);
}
