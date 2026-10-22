#include "onewire_hal.h"
#include "instrument_mode_manager.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include <Arduino.h>

static OneWireHalConfig s_ow_cfg = {
    UDOO_ONEWIRE_DEFAULT_PIN, // GP6
    true,                     // pull-up enabled
    false                     // active
};

// ─── Dallas 1-Wire Binary Search ROM State Variables ─────────────────────────
static uint8_t s_last_discrepancy = 0;
static uint8_t s_last_family_discrepancy = 0;
static bool    s_last_device_flag = false;
static uint8_t s_rom_no[8] = {0};

void onewire_hal_init(void) {
    s_ow_cfg.pin = UDOO_ONEWIRE_DEFAULT_PIN;
    s_ow_cfg.pullup_enabled = true;
    s_ow_cfg.active = false;
    s_last_discrepancy = 0;
    s_last_family_discrepancy = 0;
    s_last_device_flag = false;
    memset(s_rom_no, 0, sizeof(s_rom_no));
}

bool onewire_hal_acquire(uint8_t pin, bool pullup) {
    // Validate probe bus range (GP2..GP9 on Header P1)
    if (pin < 2 || pin > 9) {
        return false;
    }

    if (s_ow_cfg.active && s_ow_cfg.pin != pin) {
        onewire_hal_release();
    }

    // Inform Instrument Mode Manager
    ModeMgr.enterMode(InstrumentMode::ONE_WIRE, 0);

    s_ow_cfg.pin = pin;
    s_ow_cfg.pullup_enabled = pullup;
    s_ow_cfg.active = true;

    // Configure open-drain GPIO
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_put(pin, 0);           // Output data latch held LOW
    gpio_set_dir(pin, GPIO_IN); // Bus released (tri-stated / floating)

    if (pullup) {
        gpio_pull_up(pin);
    } else {
        gpio_disable_pulls(pin);
    }

    // Reset binary search state
    s_last_discrepancy = 0;
    s_last_family_discrepancy = 0;
    s_last_device_flag = false;
    memset(s_rom_no, 0, sizeof(s_rom_no));

    return true;
}

void onewire_hal_release(void) {
    if (s_ow_cfg.active) {
        gpio_set_function(s_ow_cfg.pin, GPIO_FUNC_SIO);
        gpio_set_dir(s_ow_cfg.pin, GPIO_IN);
        gpio_disable_pulls(s_ow_cfg.pin);
        s_ow_cfg.active = false;
    }
    ModeMgr.leaveMode();
}

bool onewire_hal_set_pin(uint8_t pin) {
    if (pin < 2 || pin > 9) return false;
    if (s_ow_cfg.pin == pin && s_ow_cfg.active) return true;

    bool was_active = s_ow_cfg.active;
    bool pull = s_ow_cfg.pullup_enabled;

    if (was_active) {
        // Restore old pin
        gpio_set_function(s_ow_cfg.pin, GPIO_FUNC_SIO);
        gpio_set_dir(s_ow_cfg.pin, GPIO_IN);
        gpio_disable_pulls(s_ow_cfg.pin);
    }

    s_ow_cfg.pin = pin;

    if (was_active) {
        gpio_set_function(pin, GPIO_FUNC_SIO);
        gpio_put(pin, 0);
        gpio_set_dir(pin, GPIO_IN);
        if (pull) {
            gpio_pull_up(pin);
        } else {
            gpio_disable_pulls(pin);
        }
        s_last_discrepancy = 0;
        s_last_family_discrepancy = 0;
        s_last_device_flag = false;
        memset(s_rom_no, 0, sizeof(s_rom_no));
    }

    return true;
}

void onewire_hal_set_pullup(bool enable) {
    s_ow_cfg.pullup_enabled = enable;
    if (s_ow_cfg.active) {
        if (enable) {
            gpio_pull_up(s_ow_cfg.pin);
        } else {
            gpio_disable_pulls(s_ow_cfg.pin);
        }
    }
}

void onewire_hal_get_config(OneWireHalConfig* out_cfg) {
    if (out_cfg) {
        *out_cfg = s_ow_cfg;
    }
}

bool onewire_hal_reset(void) {
    if (!s_ow_cfg.active) return false;

    // Pull DQ bus LOW for 480 microseconds
    gpio_set_dir(s_ow_cfg.pin, GPIO_OUT);
    delayMicroseconds(480);

    // Release bus and enter INPUT mode
    gpio_set_dir(s_ow_cfg.pin, GPIO_IN);
    delayMicroseconds(70);

    // Sample presence pulse (slave pulls LOW during 60-240us window)
    bool presence = (gpio_get(s_ow_cfg.pin) == 0);

    // Wait remainder of reset time slot (410us)
    delayMicroseconds(410);

    return presence;
}

void onewire_hal_write_bit(uint8_t bit) {
    if (!s_ow_cfg.active) return;

    uint32_t flags = save_and_disable_interrupts();
    // Drive bus LOW
    gpio_set_dir(s_ow_cfg.pin, GPIO_OUT);

    if (bit & 1) {
        // Write-1 slot: hold LOW for 10us, then release
        delayMicroseconds(10);
        gpio_set_dir(s_ow_cfg.pin, GPIO_IN);
        restore_interrupts(flags);
        delayMicroseconds(55);
    } else {
        // Write-0 slot: hold LOW for 65us, then release
        delayMicroseconds(65);
        gpio_set_dir(s_ow_cfg.pin, GPIO_IN);
        restore_interrupts(flags);
        delayMicroseconds(5);
    }
}

uint8_t onewire_hal_read_bit(void) {
    if (!s_ow_cfg.active) return 0;

    uint32_t flags = save_and_disable_interrupts();
    // Initiate read slot: pull LOW for 3us
    gpio_set_dir(s_ow_cfg.pin, GPIO_OUT);
    delayMicroseconds(3);

    // Release bus and wait 10us before sampling
    gpio_set_dir(s_ow_cfg.pin, GPIO_IN);
    delayMicroseconds(10);

    // Sample bus level
    uint8_t bit = gpio_get(s_ow_cfg.pin) ? 1 : 0;
    restore_interrupts(flags);

    // Wait remainder of 60us slot + recovery
    delayMicroseconds(53);

    return bit;
}

void onewire_hal_write_byte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        onewire_hal_write_bit(byte & 1);
        byte >>= 1;
    }
}

uint8_t onewire_hal_read_byte(void) {
    uint8_t byte = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (onewire_hal_read_bit()) {
            byte |= (1 << i);
        }
    }
    return byte;
}

uint8_t onewire_hal_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t inbyte = data[i];
        for (uint8_t b = 0; b < 8; b++) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) {
                crc ^= 0x8C;
            }
            inbyte >>= 1;
        }
    }
    return crc;
}

const char* onewire_hal_get_family_desc(uint8_t family_code) {
    switch (family_code) {
        case 0x01: return "DS2401/DS1990A Silicon Serial Number";
        case 0x02: return "DS1991 MultiKey iButton";
        case 0x04: return "DS2404 Real Time Clock";
        case 0x05: return "DS2405 Addressable Switch";
        case 0x09: return "DS2502 1Kb Add-Only Memory";
        case 0x0B: return "DS2505 16Kb Add-Only Memory";
        case 0x10: return "DS18S20 High-Precision Thermometer";
        case 0x12: return "DS2406/DS2407 Dual Addressable Switch";
        case 0x14: return "DS1971 256-bit EEPROM iButton";
        case 0x1D: return "DS2423 4Kb RAM with Counter";
        case 0x20: return "DS2450 4-Channel 16-bit ADC";
        case 0x22: return "DS1822 Econo Digital Thermometer";
        case 0x23: return "DS2433 4Kb EEPROM";
        case 0x24: return "DS2415 Real-Time Clock";
        case 0x26: return "DS2438 Smart Battery Monitor";
        case 0x28: return "DS18B20 Programmable Resolution Thermometer";
        case 0x29: return "DS2408 8-Channel Addressable Switch";
        case 0x2C: return "DS2890 1-Channel Digital Potentiometer";
        case 0x2D: return "DS2431 1024-bit 1-Wire EEPROM";
        case 0x30: return "DS2760 High-Precision Li+ Battery Monitor";
        case 0x33: return "DS1961S 1Kb SHA-1 EEPROM";
        case 0x3A: return "DS2413 2-Channel Addressable Switch";
        case 0x42: return "DS28EA00 Digital Temp Sensor with Sequence";
        case 0xEE: return "DS28E07 1024-bit Radiation Resistant EEPROM";
        default:   return "Generic / Unrecognized 1-Wire Device";
    }
}

static bool onewire_search_step(uint8_t* newAddr) {
    uint8_t id_bit_number = 1;
    uint8_t last_zero = 0;
    uint8_t rom_byte_number = 0;
    uint8_t rom_byte_mask = 1;
    bool search_result = false;

    if (!s_last_device_flag) {
        if (!onewire_hal_reset()) {
            s_last_discrepancy = 0;
            s_last_device_flag = false;
            return false;
        }

        onewire_hal_write_byte(0xF0); // Search ROM command

        while (id_bit_number <= 64) {
            uint8_t id_bit = onewire_hal_read_bit();
            uint8_t cmp_id_bit = onewire_hal_read_bit();

            if (id_bit == 1 && cmp_id_bit == 1) {
                // No devices responded
                break;
            } else {
                uint8_t search_direction = 0;
                if (id_bit != cmp_id_bit) {
                    search_direction = id_bit;
                } else {
                    if (id_bit_number < s_last_discrepancy) {
                        search_direction = ((s_rom_no[rom_byte_number] & rom_byte_mask) > 0);
                    } else {
                        search_direction = (id_bit_number == s_last_discrepancy);
                    }

                    if (search_direction == 0) {
                        last_zero = id_bit_number;
                        if (last_zero < 9) {
                            s_last_family_discrepancy = last_zero;
                        }
                    }
                }

                if (search_direction == 1) {
                    s_rom_no[rom_byte_number] |= rom_byte_mask;
                } else {
                    s_rom_no[rom_byte_number] &= ~rom_byte_mask;
                }

                onewire_hal_write_bit(search_direction);
                id_bit_number++;
                rom_byte_mask <<= 1;
                if (rom_byte_mask == 0) {
                    rom_byte_number++;
                    rom_byte_mask = 1;
                }
            }
        }

        if (id_bit_number > 64) {
            s_last_discrepancy = last_zero;
            if (s_last_discrepancy == 0) {
                s_last_device_flag = true;
            }
            search_result = true;
        }
    }

    if (!search_result || !s_rom_no[0]) {
        s_last_discrepancy = 0;
        s_last_device_flag = false;
        search_result = false;
    } else {
        for (int i = 0; i < 8; i++) newAddr[i] = s_rom_no[i];
    }
    return search_result;
}

uint16_t onewire_hal_search_bus(Stream& out) {
    out.println("\r\n=== Dallas 1-Wire Bus Discovery (ROM Search 0xF0) ===");
    s_last_discrepancy = 0;
    s_last_family_discrepancy = 0;
    s_last_device_flag = false;
    memset(s_rom_no, 0, sizeof(s_rom_no));

    uint8_t addr[8];
    uint16_t count = 0;

    while (onewire_search_step(addr)) {
        count++;
        out.printf("  Device #%u: [", (unsigned int)count);
        for (int i = 0; i < 8; i++) {
            out.printf("%02X", addr[i]);
            if (i < 7) out.print(":");
        }
        out.print("] - ");
        uint8_t crc = onewire_hal_crc8(addr, 7);
        if (crc == addr[7]) {
            out.printf("CRC OK | %s\r\n", onewire_hal_get_family_desc(addr[0]));
        } else {
            out.printf("CRC ERROR (calc 0x%02X != 0x%02X)\r\n", crc, addr[7]);
        }
    }

    if (count == 0) {
        out.printf("  No 1-Wire devices detected on GP%u.\r\n", s_ow_cfg.pin);
        out.println("  Check pull-up resistor (4.7k to 3.3V recommended) and probe hookup.");
    } else {
        out.printf("  Found %u 1-Wire device(s) total.\r\n", (unsigned int)count);
    }

    return count;
}

uint16_t onewire_hal_read_all_temps(Stream& out) {
    out.println("\r\n=== Dallas 1-Wire Multi-Sensor Temperature Read ===");

    // Step 1: Broadcast temperature conversion: Skip ROM (0xCC) -> Convert T (0x44)
    if (!onewire_hal_reset()) {
        out.printf("  No 1-Wire devices responded to presence reset pulse on GP%u.\r\n", s_ow_cfg.pin);
        return 0;
    }

    onewire_hal_write_byte(0xCC); // Skip ROM
    onewire_hal_write_byte(0x44); // Convert T

    out.println("  Conversion command broadcast (0xCC 0x44)... Waiting 750ms...");
    delay(750); // 750ms conversion time for 12-bit DS18B20

    // Step 2: Enumerate sensors and read scratchpads
    s_last_discrepancy = 0;
    s_last_family_discrepancy = 0;
    s_last_device_flag = false;
    memset(s_rom_no, 0, sizeof(s_rom_no));

    uint8_t addr[8];
    uint16_t count = 0;

    while (onewire_search_step(addr)) {
        // Temperature sensor family codes:
        // 0x28 = DS18B20, 0x10 = DS18S20, 0x22 = DS1822
        if (addr[0] == 0x28 || addr[0] == 0x10 || addr[0] == 0x22) {
            count++;

            // Match ROM
            onewire_hal_reset();
            onewire_hal_write_byte(0x55);
            for (int i = 0; i < 8; i++) onewire_hal_write_byte(addr[i]);

            // Read Scratchpad (0xBE)
            onewire_hal_write_byte(0xBE);
            uint8_t sp[9];
            for (int i = 0; i < 9; i++) sp[i] = onewire_hal_read_byte();

            uint8_t crc = onewire_hal_crc8(sp, 8);
            if (crc == sp[8]) {
                float temp_c = 0.0f;
                if (addr[0] == 0x10) {
                    // DS18S20: 9-bit standard resolution, high precision via count registers
                    int16_t raw = (int16_t)((sp[1] << 8) | sp[0]);
                    temp_c = (raw >> 1) * 1.0f - 0.25f + ((float)(sp[7] - sp[6]) / (float)sp[7]);
                } else {
                    // DS18B20 & DS1822: 12-bit (0.0625 deg C / LSB)
                    int16_t raw = (int16_t)((sp[1] << 8) | sp[0]);
                    temp_c = raw * 0.0625f;
                }
                float temp_f = temp_c * 1.8f + 32.0f;

                out.printf("  Sensor #%u [%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X] (%s):\r\n",
                    (unsigned int)count,
                    addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7],
                    (addr[0] == 0x28) ? "DS18B20" : ((addr[0] == 0x10) ? "DS18S20" : "DS1822")
                );
                out.printf("    Temperature: %.2f C  (%.2f F) [CRC OK]\r\n", temp_c, temp_f);
            } else {
                out.printf("  Sensor #%u [%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X]: Scratchpad CRC Mismatch!\r\n",
                    (unsigned int)count,
                    addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]
                );
            }
        }
    }

    if (count == 0) {
        out.println("  No Dallas temperature sensors detected on bus.");
    } else {
        out.printf("  Read %u temperature sensor(s) successfully.\r\n", (unsigned int)count);
    }

    return count;
}

bool onewire_hal_read_single_rom(uint8_t out_rom[8]) {
    if (!onewire_hal_reset()) return false;

    // Issue Read ROM command (0x33)
    onewire_hal_write_byte(0x33);

    for (int i = 0; i < 8; i++) {
        out_rom[i] = onewire_hal_read_byte();
    }

    uint8_t crc = onewire_hal_crc8(out_rom, 7);
    return (crc == out_rom[7] && out_rom[0] != 0 && out_rom[0] != 0xFF);
}
