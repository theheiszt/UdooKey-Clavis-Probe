#include "eeprom_24c.h"
#include <ctype.h>

static bool eeprom_wait_ready(uint8_t i2c_addr, uint32_t timeout_ms = 25) {
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        Wire.beginTransmission(i2c_addr);
        if (Wire.endTransmission() == 0) {
            return true;
        }
        delayMicroseconds(500);
    }
    return false;
}

void eeprom_probe_chip(uint8_t i2c_addr, Stream& out) {
    out.printf("\r\n[I2C EEPROM Probe] Probing address 0x%02X...\r\n", i2c_addr);
    Wire.begin();
    
    Wire.beginTransmission(i2c_addr);
    if (Wire.endTransmission() != 0) {
        out.printf("  Error: No device acknowledged at I2C address 0x%02X.\r\n", i2c_addr);
        return;
    }

    out.printf("  -> ACK received at 0x%02X!\r\n", i2c_addr);

    // Test 16-bit addressing read (24C32 - 24C512)
    Wire.beginTransmission(i2c_addr);
    Wire.write(0x00);
    Wire.write(0x00);
    bool ack_16bit = (Wire.endTransmission() == 0);

    out.printf("  Addressing Mode: %s\r\n", ack_16bit ? "16-bit Word Address (24C32 .. 24C512)" : "8-bit Byte Address (24C01 .. 24C16)");
    out.println("  Read sample (first 16 bytes):");
    eeprom_read_hex(i2c_addr, 0x0000, 16, ack_16bit, out);
}

void eeprom_read_hex(uint8_t i2c_addr, uint32_t mem_addr, uint16_t len, bool addr16, Stream& out) {
    Wire.begin();
    uint16_t offset = 0;

    while (offset < len) {
        uint16_t chunk = (len - offset > 16) ? 16 : (len - offset);
        uint32_t curr_addr = mem_addr + offset;
        uint8_t dev_addr = i2c_addr;

        if (!addr16) {
            // For 24C04/08/16, high address bits go to dev_addr
            dev_addr |= (uint8_t)((curr_addr >> 8) & 0x07);
        }

        Wire.beginTransmission(dev_addr);
        if (addr16) {
            Wire.write((uint8_t)(curr_addr >> 8));
            Wire.write((uint8_t)(curr_addr & 0xFF));
        } else {
            Wire.write((uint8_t)(curr_addr & 0xFF));
        }

        if (Wire.endTransmission(false) != 0) {
            out.printf("  [Read Error at 0x%04X]\r\n", curr_addr);
            return;
        }

        Wire.requestFrom((int)dev_addr, (int)chunk);
        uint8_t buf[16];
        uint8_t bytes_read = 0;
        while (Wire.available() && bytes_read < chunk) {
            buf[bytes_read++] = Wire.read();
        }

        // Print Hex line
        out.printf("  %04lX: ", (unsigned long)curr_addr);
        for (uint8_t i = 0; i < 16; i++) {
            if (i < bytes_read) {
                out.printf("%02X ", buf[i]);
            } else {
                out.print("   ");
            }
            if (i == 7) out.print(" ");
        }

        // Print ASCII column
        out.print(" |");
        for (uint8_t i = 0; i < bytes_read; i++) {
            char c = (char)buf[i];
            out.print(isprint(c) ? c : '.');
        }
        out.print("|\r\n");

        offset += bytes_read;
    }
}

bool eeprom_write_val(uint8_t i2c_addr, uint32_t mem_addr, uint8_t data, bool addr16, Stream& out) {
    Wire.begin();
    uint8_t dev_addr = i2c_addr;
    if (!addr16) {
        dev_addr |= (uint8_t)((mem_addr >> 8) & 0x07);
    }

    Wire.beginTransmission(dev_addr);
    if (addr16) {
        Wire.write((uint8_t)(mem_addr >> 8));
        Wire.write((uint8_t)(mem_addr & 0xFF));
    } else {
        Wire.write((uint8_t)(mem_addr & 0xFF));
    }
    Wire.write(data);
    
    if (Wire.endTransmission() != 0) {
        out.printf("  Error writing 0x%02X to 0x%04lX\r\n", data, (unsigned long)mem_addr);
        return false;
    }

    // Wait for internal write cycle to complete (ACK polling)
    if (!eeprom_wait_ready(dev_addr, 30)) {
        out.println("  Warning: EEPROM write timeout (ACK polling failed).");
        return false;
    }

    out.printf("  Write success: 0x%04lX = 0x%02X\r\n", (unsigned long)mem_addr, data);
    return true;
}

void eeprom_dump_all(uint8_t i2c_addr, uint32_t total_bytes, bool addr16, Stream& out) {
    if (total_bytes == 0) total_bytes = 256;
    out.printf("\r\n[EEPROM Dump] Dumping %lu bytes from 0x%02X...\r\n", (unsigned long)total_bytes, i2c_addr);
    eeprom_read_hex(i2c_addr, 0x0000, (uint16_t)total_bytes, addr16, out);
    out.println("Dump complete.");
}
