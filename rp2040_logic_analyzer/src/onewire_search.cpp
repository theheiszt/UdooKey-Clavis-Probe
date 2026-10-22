#include "onewire_search.h"

static uint8_t s_last_discrepancy = 0;
static uint8_t s_last_family_discrepancy = 0;
static bool s_last_device_flag = false;
static uint8_t s_rom_no[8];

static uint8_t onewire_crc8(const uint8_t* addr, uint8_t len) {
    uint8_t crc = 0;
    while (len--) {
        uint8_t inbyte = *addr++;
        for (uint8_t i = 8; i; i--) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}

static bool onewire_reset(uint8_t pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delayMicroseconds(480);
    pinMode(pin, INPUT_PULLUP);
    delayMicroseconds(70);
    bool presence = (digitalRead(pin) == LOW);
    delayMicroseconds(410);
    return presence;
}

static void onewire_write_bit(uint8_t pin, uint8_t bit) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    if (bit & 1) {
        delayMicroseconds(10);
        pinMode(pin, INPUT_PULLUP);
        delayMicroseconds(55);
    } else {
        delayMicroseconds(65);
        pinMode(pin, INPUT_PULLUP);
        delayMicroseconds(5);
    }
}

static uint8_t onewire_read_bit(uint8_t pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delayMicroseconds(3);
    pinMode(pin, INPUT_PULLUP);
    delayMicroseconds(10);
    uint8_t bit = digitalRead(pin) ? 1 : 0;
    delayMicroseconds(53);
    return bit;
}

static void onewire_write_byte(uint8_t pin, uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        onewire_write_bit(pin, byte & 1);
        byte >>= 1;
    }
}

static uint8_t onewire_read_byte(uint8_t pin) {
    uint8_t byte = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (onewire_read_bit(pin)) {
            byte |= (1 << i);
        }
    }
    return byte;
}

static bool onewire_search(uint8_t pin, uint8_t* newAddr) {
    uint8_t id_bit_number = 1;
    uint8_t last_zero = 0;
    uint8_t rom_byte_number = 0;
    uint8_t rom_byte_mask = 1;
    bool search_result = false;

    if (!s_last_device_flag) {
        if (!onewire_reset(pin)) {
            s_last_discrepancy = 0;
            s_last_device_flag = false;
            return false;
        }

        onewire_write_byte(pin, 0xF0); // Search ROM command

        while (id_bit_number <= 64) {
            uint8_t id_bit = onewire_read_bit(pin);
            uint8_t cmp_id_bit = onewire_read_bit(pin);

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

                onewire_write_bit(pin, search_direction);
                id_bit_number++;
                rom_byte_mask <<= 1;
                if (rom_byte_mask == 0) {
                    rom_byte_number++;
                    rom_byte_mask = 1;
                }
            }
        }

        if (!(id_bit_number <= 64)) {
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

static const char* get_family_name(uint8_t code) {
    switch (code) {
        case 0x28: return "DS18B20 Programmable Resolution Thermometer";
        case 0x10: return "DS18S20 High-Precision Thermometer";
        case 0x22: return "DS1822 Econo Digital Thermometer";
        case 0x01: return "DS2401 Silicon Serial Number";
        case 0x2D: return "DS2431 1024-bit 1-Wire EEPROM";
        case 0x23: return "DS2433 4Kb 1-Wire EEPROM";
        case 0x29: return "DS2408 8-Channel Addressable Switch";
        default:   return "Unknown 1-Wire Device";
    }
}

void onewire_search_bus(uint8_t pin, Stream& port) {
    port.println("\r\n=== 1-Wire Bus Discovery (ROM Search 0xF0) ===");
    s_last_discrepancy = 0;
    s_last_family_discrepancy = 0;
    s_last_device_flag = false;
    memset(s_rom_no, 0, sizeof(s_rom_no));

    uint8_t addr[8];
    uint8_t count = 0;

    while (onewire_search(pin, addr)) {
        count++;
        port.printf("  Device #%d: [", count);
        for (int i = 0; i < 8; i++) {
            port.printf("%02X", addr[i]);
            if (i < 7) port.print(":");
        }
        port.print("] - ");
        uint8_t crc = onewire_crc8(addr, 7);
        if (crc == addr[7]) {
            port.printf("CRC OK | %s\r\n", get_family_name(addr[0]));
        } else {
            port.printf("CRC ERROR (calc 0x%02X != 0x%02X)\r\n", crc, addr[7]);
        }
    }

    if (count == 0) {
        port.println("  No 1-Wire devices detected on probe pin. Check pull-up resistor (4.7k to 3.3V).");
    } else {
        port.printf("  Found %d 1-Wire device(s) total.\r\n", count);
    }
}

void onewire_read_all_temps(uint8_t pin, Stream& port) {
    port.println("\r\n=== 1-Wire Multi-Sensor Temperature Read ===");
    s_last_discrepancy = 0;
    s_last_family_discrepancy = 0;
    s_last_device_flag = false;

    // First broadcast temperature conversion: Skip ROM (0xCC) -> Convert T (0x44)
    if (!onewire_reset(pin)) {
        port.println("  No 1-Wire devices responded to presence reset pulse.");
        return;
    }
    onewire_write_byte(pin, 0xCC);
    onewire_write_byte(pin, 0x44);

    delay(750); // 750ms conversion time for 12-bit DS18B20

    uint8_t addr[8];
    uint8_t count = 0;
    while (onewire_search(pin, addr)) {
        if (addr[0] == 0x28 || addr[0] == 0x10 || addr[0] == 0x22) {
            count++;
            // Match ROM
            onewire_reset(pin);
            onewire_write_byte(pin, 0x55);
            for (int i = 0; i < 8; i++) onewire_write_byte(pin, addr[i]);

            // Read Scratchpad (0xBE)
            onewire_write_byte(pin, 0xBE);
            uint8_t sp[9];
            for (int i = 0; i < 9; i++) sp[i] = onewire_read_byte(pin);

            uint8_t crc = onewire_crc8(sp, 8);
            if (crc == sp[8]) {
                int16_t raw = (sp[1] << 8) | sp[0];
                float temp_c = raw * 0.0625f;
                float temp_f = temp_c * 1.8f + 32.0f;
                port.printf("  DS18B20 #%d [%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X]: %.2f C  (%.2f F)\r\n",
                            count, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7],
                            temp_c, temp_f);
            } else {
                port.printf("  Sensor #%d: Scratchpad CRC Mismatch!\r\n", count);
            }
        }
    }
    if (count == 0) {
        port.println("  No temperature sensors found.");
    }
}
