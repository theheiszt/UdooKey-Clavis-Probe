#ifndef SPIFLASH_FULL_H
#define SPIFLASH_FULL_H

#include <Arduino.h>
#include <SPI.h>

void flash_full_probe(uint8_t cs_pin, Stream& port);
void flash_full_status(uint8_t cs_pin, Stream& port);
void flash_full_sfdp(uint8_t cs_pin, Stream& port);
void flash_full_read_hex(uint8_t cs_pin, uint32_t addr, uint32_t len, Stream& port);
void flash_full_dump(uint8_t cs_pin, uint32_t pages, Stream& port);
void flash_full_write_byte(uint8_t cs_pin, uint32_t addr, uint8_t data, Stream& port);
void flash_full_erase_sector(uint8_t cs_pin, uint32_t addr, Stream& port);
void flash_full_erase_block(uint8_t cs_pin, uint32_t addr, Stream& port);
void flash_full_erase_chip(uint8_t cs_pin, Stream& port);
void flash_full_protect(uint8_t cs_pin, bool enable, Stream& port);

#endif // SPIFLASH_FULL_H
