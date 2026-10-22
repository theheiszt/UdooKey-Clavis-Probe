#ifndef EEPROM_24C_H
#define EEPROM_24C_H

#include <Arduino.h>
#include <Wire.h>

struct EepromType {
    const char* name;
    uint32_t    size_bytes;
    uint16_t    page_size;
    bool        addr16;
};

// Probe 24Cxx EEPROM at specified I2C address
void eeprom_probe_chip(uint8_t i2c_addr, Stream& out);

// Read memory range and print formatted hex/ASCII dump
void eeprom_read_hex(uint8_t i2c_addr, uint32_t mem_addr, uint16_t len, bool addr16, Stream& out);

// Write single byte with ACK polling
bool eeprom_write_val(uint8_t i2c_addr, uint32_t mem_addr, uint8_t data, bool addr16, Stream& out);

// Full dump of EEPROM
void eeprom_dump_all(uint8_t i2c_addr, uint32_t total_bytes, bool addr16, Stream& out);

#endif // EEPROM_24C_H
