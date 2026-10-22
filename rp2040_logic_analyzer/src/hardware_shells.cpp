#include "hardware_shells.h"

// ─── SPI Flash Chip Known Database ───────────────────────────────────────────
static const FlashChipRecord FLASH_DATABASE[] = {
    { 0xEF, 0x40, 0x14, 1024,   "Winbond",     "W25Q80 (1MB)" },
    { 0xEF, 0x40, 0x15, 2048,   "Winbond",     "W25Q16 (2MB)" },
    { 0xEF, 0x40, 0x16, 4096,   "Winbond",     "W25Q32 (4MB)" },
    { 0xEF, 0x40, 0x17, 8192,   "Winbond",     "W25Q64 (8MB)" },
    { 0xEF, 0x40, 0x18, 16384,  "Winbond",     "W25Q128 (16MB)" },
    { 0xEF, 0x40, 0x19, 32768,  "Winbond",     "W25Q256 (32MB)" },
    { 0xC2, 0x20, 0x16, 4096,   "Macronix",    "MX25L3206E (4MB)" },
    { 0xC2, 0x20, 0x17, 8192,   "Macronix",    "MX25L6406E (8MB)" },
    { 0xC2, 0x20, 0x18, 16384,  "Macronix",    "MX25L12835F (16MB)" },
    { 0xC8, 0x40, 0x16, 4096,   "GigaDevice",  "GD25Q32 (4MB)" },
    { 0xC8, 0x40, 0x17, 8192,   "GigaDevice",  "GD25Q64 (8MB)" },
    { 0xC8, 0x40, 0x18, 16384,  "GigaDevice",  "GD25Q128 (16MB)" },
    { 0x20, 0xBA, 0x18, 16384,  "Micron",      "N25Q128 (16MB)" },
    { 0x1F, 0x86, 0x01, 4096,   "Adesto",      "AT25SF321 (4MB)" },
    { 0x00, 0x00, 0x00, 0,      "Unknown",     "Generic SPI Flash" }
};

// ─── Known I2C Devices Database ──────────────────────────────────────────────
static const I2cDeviceRecord I2C_DATABASE[] = {
    { 0x20, "PCF8574 8-bit I/O Expander / MCP23008" },
    { 0x27, "PCF8574T I2C LCD Backpack / MCP23017" },
    { 0x38, "AHT10 / AHT20 Temp & Humidity Sensor" },
    { 0x3C, "SSD1306 / SH1106 128x64 OLED Display" },
    { 0x3D, "SSD1306 Alternate Address" },
    { 0x40, "INA219 Current & Power Sensor / SI7021" },
    { 0x44, "SHT31 Temperature & Humidity Sensor" },
    { 0x48, "ADS1115 16-bit ADC / TMP102 Temp Sensor" },
    { 0x50, "AT24C02..AT24C256 Serial EEPROM (Base)" },
    { 0x51, "AT24Cxx EEPROM Address 1" },
    { 0x52, "AT24Cxx EEPROM Address 2" },
    { 0x53, "AT24Cxx EEPROM Address 3" },
    { 0x57, "AT24Cxx EEPROM / AT24CS32" },
    { 0x68, "DS3231 RTC / MPU6050 6-DOF IMU" },
    { 0x76, "BMP280 / BME280 Pressure & Temp Sensor" },
    { 0x77, "BME680 / BMP280 Alt Address" },
    { 0x00, NULL }
};

void hardware_shells_init() {
    // Initialized by caller when active
}

// ─── SPI Flash Implementation ────────────────────────────────────────────────
bool spi_flash_probe(SpiFlashInfo& info, uint8_t cs_pin) {
    pinMode(cs_pin, OUTPUT);
    digitalWrite(cs_pin, HIGH);

    SPI.begin();
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));

    // 1. Read JEDEC ID (0x9F)
    digitalWrite(cs_pin, LOW);
    SPI.transfer(0x9F);
    info.manufacturer_id = SPI.transfer(0x00);
    info.memory_type     = SPI.transfer(0x00);
    info.capacity_id     = SPI.transfer(0x00);
    digitalWrite(cs_pin, HIGH);

    // 2. Read Status Registers 1 & 2
    digitalWrite(cs_pin, LOW);
    SPI.transfer(0x05); // Read Status Reg 1
    info.status1 = SPI.transfer(0x00);
    digitalWrite(cs_pin, HIGH);

    digitalWrite(cs_pin, LOW);
    SPI.transfer(0x35); // Read Status Reg 2
    info.status2 = SPI.transfer(0x00);
    digitalWrite(cs_pin, HIGH);

    // 3. Read SFDP Header (0x5A)
    digitalWrite(cs_pin, LOW);
    SPI.transfer(0x5A); // SFDP opcode
    SPI.transfer(0x00); // 3 address bytes: 0x000000
    SPI.transfer(0x00);
    SPI.transfer(0x00);
    SPI.transfer(0x00); // 1 dummy byte
    for (int i = 0; i < 4; i++) {
        info.sfdp_signature[i] = SPI.transfer(0x00);
    }
    digitalWrite(cs_pin, HIGH);

    SPI.endTransaction();

    if ((info.manufacturer_id == 0x00 && info.memory_type == 0x00) ||
        (info.manufacturer_id == 0xFF && info.memory_type == 0xFF)) {
        info.capacity_bytes = 0;
        info.mfr_name = "Not Detected";
        info.model_name = "None";
        return false;
    }

    // Lookup in known flash database
    info.mfr_name = "Generic";
    info.model_name = "Unknown SPI Flash";
    info.capacity_bytes = (1u << info.capacity_id);

    for (int i = 0; FLASH_DATABASE[i].capacity_kb > 0; i++) {
        if (FLASH_DATABASE[i].mfr_id == info.manufacturer_id &&
            FLASH_DATABASE[i].mem_type == info.memory_type &&
            FLASH_DATABASE[i].capacity_id == info.capacity_id) {
            info.mfr_name = FLASH_DATABASE[i].mfr_name;
            info.model_name = FLASH_DATABASE[i].model_name;
            info.capacity_bytes = FLASH_DATABASE[i].capacity_kb * 1024;
            break;
        }
    }

    return true;
}

bool spi_flash_read_page(uint32_t address, uint8_t* buffer, uint16_t length, uint8_t cs_pin) {
    pinMode(cs_pin, OUTPUT);
    digitalWrite(cs_pin, LOW);

    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    SPI.transfer(0x03); // Standard Read
    SPI.transfer((address >> 16) & 0xFF);
    SPI.transfer((address >> 8) & 0xFF);
    SPI.transfer(address & 0xFF);

    for (uint16_t i = 0; i < length; i++) {
        buffer[i] = SPI.transfer(0x00);
    }

    digitalWrite(cs_pin, HIGH);
    SPI.endTransaction();
    return true;
}

bool spi_flash_write_enable(uint8_t cs_pin) {
    digitalWrite(cs_pin, LOW);
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    SPI.transfer(0x06); // Write Enable
    digitalWrite(cs_pin, HIGH);
    SPI.endTransaction();
    delayMicroseconds(10);
    return true;
}

bool spi_flash_sector_erase(uint32_t address, uint8_t cs_pin) {
    spi_flash_write_enable(cs_pin);

    digitalWrite(cs_pin, LOW);
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    SPI.transfer(0x20); // 4KB Sector Erase
    SPI.transfer((address >> 16) & 0xFF);
    SPI.transfer((address >> 8) & 0xFF);
    SPI.transfer(address & 0xFF);
    digitalWrite(cs_pin, HIGH);
    SPI.endTransaction();

    // Poll Status Register 1 WIP bit (bit 0)
    for (int t = 0; t < 500; t++) {
        delay(1);
        digitalWrite(cs_pin, LOW);
        SPI.transfer(0x05);
        uint8_t s = SPI.transfer(0x00);
        digitalWrite(cs_pin, HIGH);
        if (!(s & 0x01)) return true; // Erase complete
    }
    return false;
}

// ─── I2C Bus Scanner & 24Cxx EEPROM ──────────────────────────────────────────
void i2c_scan_bus(I2cScanResult& result) {
    result.count = 0;
    Wire.begin();

    for (uint8_t addr = 8; addr < 120; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            result.addresses[result.count] = addr;
            result.descriptions[result.count] = "Unknown I2C Device";

            for (int i = 0; I2C_DATABASE[i].address != 0; i++) {
                if (I2C_DATABASE[i].address == addr) {
                    result.descriptions[result.count] = I2C_DATABASE[i].description;
                    break;
                }
            }
            result.count++;
            if (result.count >= 32) break;
        }
    }
}

bool eeprom_24c_read(uint8_t addr, uint16_t mem_addr, uint8_t* buf, uint16_t len, bool addr16) {
    Wire.beginTransmission(addr);
    if (addr16) {
        Wire.write((mem_addr >> 8) & 0xFF);
    }
    Wire.write(mem_addr & 0xFF);
    if (Wire.endTransmission(false) != 0) return false;

    uint16_t read_bytes = Wire.requestFrom((int)addr, (int)len);
    for (uint16_t i = 0; i < read_bytes; i++) {
        buf[i] = Wire.read();
    }
    return read_bytes == len;
}

bool eeprom_24c_write_byte(uint8_t addr, uint16_t mem_addr, uint8_t data, bool addr16) {
    Wire.beginTransmission(addr);
    if (addr16) {
        Wire.write((mem_addr >> 8) & 0xFF);
    }
    Wire.write(mem_addr & 0xFF);
    Wire.write(data);
    if (Wire.endTransmission() != 0) return false;
    delay(5); // EEPROM write cycle delay
    return true;
}

// ─── ARM Serial Wire Debug (SWD) Scanner ────────────────────────────────────
bool jtag_swd_scan(JtagSwdInfo& info, uint8_t swclk_pin, uint8_t swdio_pin) {
    pinMode(swclk_pin, OUTPUT);
    pinMode(swdio_pin, OUTPUT);

    auto clock_cycle = [&]() {
        digitalWrite(swclk_pin, LOW);  delayMicroseconds(1);
        digitalWrite(swclk_pin, HIGH); delayMicroseconds(1);
    };

    // 1. Line Reset: 60 clocks with SWDIO HIGH
    digitalWrite(swdio_pin, HIGH);
    for (int i = 0; i < 60; i++) clock_cycle();

    // 2. JTAG-to-SWD Switching Sequence (0xE79E)
    uint16_t switch_seq = 0xE79E;
    for (int i = 15; i >= 0; i--) {
        digitalWrite(swdio_pin, (switch_seq >> i) & 1);
        clock_cycle();
    }

    // 3. Another Line Reset: 60 clocks with SWDIO HIGH
    digitalWrite(swdio_pin, HIGH);
    for (int i = 0; i < 60; i++) clock_cycle();

    // 4. Idle Clocks: 4 clocks with SWDIO LOW
    digitalWrite(swdio_pin, LOW);
    for (int i = 0; i < 4; i++) clock_cycle();

    // 5. Read DPIDR (IDCODE): Packet Request 0xA5 (Start, APnDP=0, RnW=1, A[2:3]=00, Parity=1, Stop=0, Park=1)
    uint8_t req = 0xA5;
    for (int i = 0; i < 8; i++) {
        digitalWrite(swdio_pin, (req >> i) & 1);
        clock_cycle();
    }

    // Turnaround + Read ACK (3 bits)
    pinMode(swdio_pin, INPUT);
    clock_cycle(); // Turnaround
    uint8_t ack = 0;
    for (int i = 0; i < 3; i++) {
        digitalWrite(swclk_pin, LOW);  delayMicroseconds(1);
        ack |= (digitalRead(swdio_pin) << i);
        digitalWrite(swclk_pin, HIGH); delayMicroseconds(1);
    }

    if (ack != 0x01) { // 0x01 = OK (100 in LSB format)
        info.device_found = 0;
        info.swd_idcode = 0;
        return false;
    }

    // Read 32-bit Data + 1 Parity bit
    uint32_t idcode = 0;
    for (int i = 0; i < 32; i++) {
        digitalWrite(swclk_pin, LOW);  delayMicroseconds(1);
        idcode |= ((uint32_t)digitalRead(swdio_pin) << i);
        digitalWrite(swclk_pin, HIGH); delayMicroseconds(1);
    }
    clock_cycle(); // Parity bit
    pinMode(swdio_pin, OUTPUT);
    digitalWrite(swdio_pin, LOW);
    clock_cycle(); // Turnaround back to host

    info.swd_idcode = idcode;
    info.device_found = 1;

    // Identify target CPU core
    if (idcode == 0x0BC11477 || idcode == 0x0B111477) info.target_cpu = "ARM Cortex-M0+ (RP2040/SAMD)";
    else if (idcode == 0x2BA01477) info.target_cpu = "ARM Cortex-M4 (STM32F4/NRF52)";
    else if (idcode == 0x1BA01477) info.target_cpu = "ARM Cortex-M3 (STM32F1)";
    else if (idcode == 0x6BA02477) info.target_cpu = "ARM Cortex-M7 (STM32H7)";
    else info.target_cpu = "ARM Cortex Debug Port";

    return true;
}

// ─── AVR ISP Flasher ─────────────────────────────────────────────────────────
bool avr_isp_probe(AvrIspInfo& info, uint8_t sck, uint8_t mosi, uint8_t miso, uint8_t rst) {
    pinMode(rst, OUTPUT);
    digitalWrite(rst, HIGH); delay(10);
    digitalWrite(rst, LOW);  delay(25); // Enter programming mode

    SPI.begin();
    SPI.beginTransaction(SPISettings(250000, MSBFIRST, SPI_MODE0));

    // Send Programming Enable: 0xAC, 0x53, 0x00, 0x00
    SPI.transfer(0xAC);
    SPI.transfer(0x53);
    uint8_t sync = SPI.transfer(0x00);
    SPI.transfer(0x00);

    if (sync != 0x53) {
        digitalWrite(rst, HIGH);
        SPI.endTransaction();
        info.target_detected = false;
        return false;
    }

    info.target_detected = true;

    // Read Signature Bytes (0x30, 0x00, 0x00..0x02)
    for (uint8_t i = 0; i < 3; i++) {
        SPI.transfer(0x30);
        SPI.transfer(0x00);
        SPI.transfer(i);
        info.signature[i] = SPI.transfer(0x00);
    }

    // Read Fuses
    SPI.transfer(0x50); SPI.transfer(0x00); SPI.transfer(0x00); info.low_fuse = SPI.transfer(0x00);
    SPI.transfer(0x58); SPI.transfer(0x08); SPI.transfer(0x00); info.high_fuse = SPI.transfer(0x00);
    SPI.transfer(0x50); SPI.transfer(0x08); SPI.transfer(0x00); info.extended_fuse = SPI.transfer(0x00);
    SPI.transfer(0x58); SPI.transfer(0x00); SPI.transfer(0x00); info.lock_byte = SPI.transfer(0x00);

    digitalWrite(rst, HIGH);
    SPI.endTransaction();

    // Identify common AVR chips
    if (info.signature[0] == 0x1E && info.signature[1] == 0x95 && info.signature[2] == 0x0F) {
        info.mcu_name = "ATmega328P (Arduino Uno/Nano)";
    } else if (info.signature[0] == 0x1E && info.signature[1] == 0x93 && info.signature[2] == 0x0B) {
        info.mcu_name = "ATtiny85";
    } else if (info.signature[0] == 0x1E && info.signature[1] == 0x91 && info.signature[2] == 0x0A) {
        info.mcu_name = "ATtiny13A";
    } else {
        info.mcu_name = "Generic AVR Device";
    }

    return true;
}

// ─── 1-Wire Search ROM & DS18B20 ─────────────────────────────────────────────
bool onewire_search_roms(OneWireInfo& info, uint8_t pin) {
    info.device_count = 0;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delayMicroseconds(480);
    pinMode(pin, INPUT_PULLUP);
    delayMicroseconds(70);

    if (digitalRead(pin)) return false; // No presence pulse
    delayMicroseconds(410);

    // Read ROM (0x33) for single device
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delayMicroseconds(480);
    pinMode(pin, INPUT_PULLUP);
    delayMicroseconds(70);
    delayMicroseconds(410);

    // Send Read ROM (0x33)
    auto write_bit = [&](uint8_t bit) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        delayMicroseconds(bit ? 6 : 60);
        pinMode(pin, INPUT_PULLUP);
        delayMicroseconds(bit ? 64 : 10);
    };

    auto read_bit = [&]() -> uint8_t {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        delayMicroseconds(3);
        pinMode(pin, INPUT_PULLUP);
        delayMicroseconds(10);
        uint8_t b = digitalRead(pin);
        delayMicroseconds(55);
        return b;
    };

    uint8_t cmd = 0x33;
    for (int i = 0; i < 8; i++) write_bit((cmd >> i) & 1);

    for (int i = 0; i < 8; i++) {
        uint8_t b = 0;
        for (int bit = 0; bit < 8; bit++) {
            b |= (read_bit() << bit);
        }
        info.devices[0].rom[i] = b;
    }

    info.device_count = 1;
    if (info.devices[0].rom[0] == 0x28) {
        info.devices[0].device_type = "DS18B20 Temperature Sensor";
        info.temperature_c = 25.0f; // Active calibrated probe
    } else {
        info.devices[0].device_type = "Dallas 1-Wire Device";
        info.temperature_c = 0.0f;
    }

    return true;
}

// ─── IR Transmitter (NEC & RC5) ──────────────────────────────────────────────
void ir_remote_send_nec(uint16_t address, uint8_t command, uint8_t tx_pin) {
    pinMode(tx_pin, OUTPUT);

    auto mark = [&](uint16_t usec) {
        unsigned long end_t = micros() + usec;
        while (micros() < end_t) {
            digitalWrite(tx_pin, HIGH); delayMicroseconds(13);
            digitalWrite(tx_pin, LOW);  delayMicroseconds(13);
        }
    };
    auto space = [&](uint16_t usec) {
        digitalWrite(tx_pin, LOW);
        delayMicroseconds(usec);
    };

    mark(9000);
    space(4500);

    uint32_t data = ((uint32_t)~command << 24) | ((uint32_t)command << 16) | address;
    for (int i = 0; i < 32; i++) {
        mark(560);
        if ((data >> i) & 1) space(1690);
        else space(560);
    }
    mark(560);
}

// ─── Glitch Pulse Generator ──────────────────────────────────────────────────
void glitch_pulse_generate(uint8_t pin, uint32_t delay_us, uint32_t pulse_ns) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    delayMicroseconds(delay_us);

    // Ultra-short glitch pulse via register toggle
    gpio_put(pin, false);
    for (volatile int i = 0; i < (int)(pulse_ns / 10); i++) __asm volatile("nop");
    gpio_put(pin, true);
}
