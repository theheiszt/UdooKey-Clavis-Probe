#ifndef BUS_PIRATE_CORE_H
#define BUS_PIRATE_CORE_H

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pio_capture.h"
#include "hardware_shells.h"
#include "freq_meter.h"
#include "eeprom_24c.h"
#include "ws2812_driver.h"
#include "glitch_injector.h"
#include "swd_probe.h"
#include "pio_edge_analyzer.h"
#include "instrument_core.h"
#include "trigger_engine.h"
#include "frontend_control.h"
#include "interchip_link.h"
#include "instrument_mode_manager.h"

// ─── Supported Bus Pirate Modes ──────────────────────────────────────────────
enum BpMode : uint8_t {
    BP_MODE_HIZ         = 0,
    BP_MODE_1WIRE       = 1,
    BP_MODE_I2C         = 2,
    BP_MODE_SPI         = 3,
    BP_MODE_UART        = 4,
    BP_MODE_2WIRE       = 5,
    BP_MODE_3WIRE       = 6,
    BP_MODE_JTAG_SWD    = 7,
    BP_MODE_DIO         = 8,
    BP_MODE_LOGIC       = 9,
    BP_MODE_LED         = 10,
    BP_MODE_USB         = 11,
    BP_MODE_SENSOR      = 12,
    BP_MODE_COUNT       = 13
};

// ─── Bus Configuration State ─────────────────────────────────────────────────
struct BpConfig {
    BpMode   mode;
    uint32_t speed_hz;          // I2C: 100k/400k/1M, SPI: 100k-30M, UART: baud
    uint8_t  spi_polarity;      // CPOL (0 or 1)
    uint8_t  spi_phase;         // CPHA (0 or 1)
    uint8_t  spi_bits;          // 8 or 16 bits
    uint8_t  pullups_enabled;   // 1 if internal pull-ups enabled
    uint8_t  power_enabled;     // 1 if power rail enabled
    uint8_t  cs_pin;            // Active CS pin (default 5 on probe bus)
    uint8_t  bus_started;       // 1 if start condition active ([ or {)
};

extern BpConfig bp_cfg;
extern char last_cmd_output[256];
extern char active_mode_name[16];

void bp_core_init();
void bp_set_mode(BpMode new_mode, Stream* out = nullptr);
void bp_execute_command(const char* cmd_line, Stream& out);

// Hardware Measurement Utilities
float    bp_read_adc_voltage(uint8_t pin);
uint32_t bp_measure_frequency(uint8_t pin, uint32_t gate_ms = 100);
void     bp_generate_pwm(uint8_t pin, uint32_t freq_hz, uint8_t duty_percent);
void     bp_stop_pwm(uint8_t pin);

#endif // BUS_PIRATE_CORE_H
