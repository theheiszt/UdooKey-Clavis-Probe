#ifndef HARDWARE_SHELLS_H
#define HARDWARE_SHELLS_H

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "udoo_protocol.h"

// ─── Status Codes ────────────────────────────────────────────────────────────
#define SHELL_OK       0
#define SHELL_ERR      1
#define SHELL_TIMEOUT  2

// ─── SPI Flash Database & Structures ─────────────────────────────────────────
struct FlashChipRecord {
    uint8_t     mfr_id;       // Manufacturer JEDEC ID (0xEF, 0xC2, 0x20, 0x1F, etc.)
    uint8_t     mem_type;     // Memory Type
    uint8_t     capacity_id;  // Capacity byte
    uint32_t    capacity_kb;  // Capacity in Kilobytes
    const char* mfr_name;     // Manufacturer string
    const char* model_name;   // Model string
};

struct SpiFlashInfo {
    uint8_t     manufacturer_id;
    uint8_t     memory_type;
    uint8_t     capacity_id;
    uint32_t    capacity_bytes;
    const char* mfr_name;
    const char* model_name;
    uint8_t     status1;
    uint8_t     status2;
    uint8_t     sfdp_signature[4]; // Should be "SFDP" (0x50, 0x44, 0x46, 0x53)
};

// ─── I2C Database & Structures ───────────────────────────────────────────────
struct I2cDeviceRecord {
    uint8_t     address;
    const char* description;
};

struct I2cScanResult {
    uint8_t count;
    uint8_t addresses[32];
    const char* descriptions[32];
};

struct Eeprom24cInfo {
    uint8_t  address;
    uint16_t capacity_bytes; // e.g. 256 for 24C02, 8192 for 24C64
    uint8_t  page_size;
};

// ─── JTAG & ARM SWD Structures ───────────────────────────────────────────────
struct JtagSwdInfo {
    uint32_t jtag_idcode;
    uint32_t swd_idcode;      // ARM SW-DP IDCODE
    uint32_t ap_idcode;       // Access Point AP0 IDR
    uint8_t  device_found;
    const char* target_cpu;   // e.g. "ARM Cortex-M0+", "ARM Cortex-M4", etc.
};

// ─── AVR ISP Structures ──────────────────────────────────────────────────────
struct AvrIspInfo {
    uint8_t  signature[3];
    uint8_t  low_fuse;
    uint8_t  high_fuse;
    uint8_t  extended_fuse;
    uint8_t  lock_byte;
    uint8_t  calibration;
    const char* mcu_name;     // e.g. "ATmega328P", "ATtiny85"
    bool     target_detected;
};

// ─── 1-Wire & 3-Wire Structures ──────────────────────────────────────────────
struct OneWireDevice {
    uint8_t rom[8];           // 64-bit ROM code
    const char* device_type;  // e.g. "DS18B20 Temp", "DS2431 1K EEPROM"
};

struct OneWireInfo {
    uint8_t       device_count;
    OneWireDevice devices[8];
    float         temperature_c; // Temperature if DS18B20 present
};

struct Microwire93cInfo {
    uint16_t capacity_words;  // e.g. 64 words (128 bytes) for 93C46
    uint8_t  org_16bit;       // 1 for 16-bit words, 0 for 8-bit bytes
};

// ─── Hardware Shell Function Prototypes ──────────────────────────────────────
void hardware_shells_init();

// SPI Flash Shell (Probe Bus GP5 = CS)
bool spi_flash_probe(SpiFlashInfo& info, uint8_t cs_pin = 5);
bool spi_flash_read_sfdp(uint8_t* buffer, uint16_t len, uint8_t cs_pin = 5);
bool spi_flash_read_page(uint32_t address, uint8_t* buffer, uint16_t length, uint8_t cs_pin = 5);
bool spi_flash_write_enable(uint8_t cs_pin = 5);
bool spi_flash_sector_erase(uint32_t address, uint8_t cs_pin = 5);
bool spi_flash_page_program(uint32_t address, const uint8_t* buffer, uint16_t length, uint8_t cs_pin = 5);

// I2C Shell
void i2c_scan_bus(I2cScanResult& result);
bool eeprom_24c_read(uint8_t addr, uint16_t mem_addr, uint8_t* buf, uint16_t len, bool addr16 = false);
bool eeprom_24c_write_byte(uint8_t addr, uint16_t mem_addr, uint8_t data, bool addr16 = false);

// JTAG & SWD Shell
bool jtag_swd_scan(JtagSwdInfo& info, uint8_t swclk_pin = 18, uint8_t swdio_pin = 19);
bool swd_read_dp_register(uint8_t reg_addr, uint32_t& value, uint8_t swclk_pin = 18, uint8_t swdio_pin = 19);

// AVR ISP Flasher Shell
bool avr_isp_probe(AvrIspInfo& info, uint8_t sck = 18, uint8_t mosi = 19, uint8_t miso = 20, uint8_t rst = 21);

// 1-Wire & 3-Wire Shell
bool onewire_search_roms(OneWireInfo& info, uint8_t pin = 22);
bool onewire_read_ds18b20(float& temp_c, uint8_t pin = 22);
bool microwire_93c_read(uint8_t addr, uint16_t* buf, uint8_t count, uint8_t cs = 17, uint8_t clk = 18, uint8_t di = 19, uint8_t do_pin = 20);

// IR Universal Remote Shell
void ir_remote_send_nec(uint16_t address, uint8_t command, uint8_t tx_pin = 25);
void ir_remote_send_rc5(uint8_t address, uint8_t command, uint8_t tx_pin = 25);

// Glitcher & Pulse Injection
void glitch_pulse_generate(uint8_t pin, uint32_t delay_us, uint32_t pulse_ns);

#endif // HARDWARE_SHELLS_H
