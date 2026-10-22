#include "i2c_hal.h"
#include "gpio_hal.h"
#include "instrument_mode_manager.h"
#include "../terminal/terminal_vt100.h"
#include <Arduino.h>
#include <Wire.h>
#include <ctype.h>
#include <stdlib.h>

static I2cHalConfig s_i2c_cfg = {
    .speed_hz        = UDOO_I2C_DEFAULT_SPEED,
    .pullups_enabled = true,
    .active          = false
};

// ─── Known I2C Devices Database ──────────────────────────────────────────────
struct KnownDevice {
    uint8_t     addr;
    const char* part;
    const char* cat;
};

static const KnownDevice s_known_devices[] = {
    { 0x0B, "LC709203F", "Fuel Gauge" },
    { 0x0C, "MLX90393", "3-Axis Magnetometer" },
    { 0x0E, "MAG3110", "3-Axis Magnetometer" },
    { 0x10, "VEML6075 / VEML7700", "UV / Ambient Light" },
    { 0x13, "VCNL4010", "Proximity / Light" },
    { 0x18, "MCP9808", "Precision Temp Sensor" },
    { 0x19, "LSM303 (Accel)", "3-Axis Accelerometer" },
    { 0x1C, "MMA8451", "3-Axis Accelerometer" },
    { 0x1D, "ADXL345", "3-Axis Accelerometer" },
    { 0x1E, "LIS3MDL / HMC5883L", "3-Axis Magnetometer" },
    { 0x20, "MCP23017 / PCF8574", "I/O Expander (Base)" },
    { 0x21, "MCP23017 / PCF8574", "I/O Expander (+1)" },
    { 0x22, "MCP23017 / PCF8574", "I/O Expander (+2)" },
    { 0x23, "BH1750 / MCP23017", "Ambient Light / I/O Expander (+3)" },
    { 0x24, "MCP23017 / PCF8574", "I/O Expander (+4)" },
    { 0x25, "MCP23017 / PCF8574", "I/O Expander (+5)" },
    { 0x26, "MCP23017 / PCF8574", "I/O Expander (+6)" },
    { 0x27, "MCP23017 / PCF8574", "I/O Expander (+7)" },
    { 0x28, "BNO055", "9-DOF Orientation IMU" },
    { 0x29, "TCS34725 / VL53L0X", "Color Sensor / Time-of-Flight ToF" },
    { 0x36, "MAX17043 / MAX17048", "LiPo Fuel Gauge" },
    { 0x38, "AHT20 / DHT20 / PCF8574A", "Temp & Humidity / I/O Expander" },
    { 0x39, "APDS-9960 / AS7341", "Gesture & RGB Color / Spectral Sensor" },
    { 0x3C, "SSD1306 / SH1106", "OLED Display 128x64 / 128x32" },
    { 0x3D, "SSD1306 (Alt)", "OLED Display 128x64" },
    { 0x40, "INA219 / INA260 / Si7021 / HTU21D", "Power Monitor / Humidity Sensor" },
    { 0x44, "SHT31 / SHT30", "Precision Humidity & Temp Sensor" },
    { 0x45, "SHT31 (Alt)", "Precision Humidity & Temp Sensor" },
    { 0x48, "ADS1115 / TMP102 / PCF8591", "16-bit ADC / Temp / 8-bit ADC" },
    { 0x49, "ADS1115 (Alt 1)", "16-bit ADC" },
    { 0x4A, "ADS1115 (Alt 2)", "16-bit ADC" },
    { 0x4B, "ADS1115 (Alt 3)", "16-bit ADC" },
    { 0x50, "AT24Cxx (24C02..24C512) / EDID", "I2C EEPROM / Monitor DDC" },
    { 0x51, "AT24Cxx / PCF8563", "I2C EEPROM (+1) / Real-Time Clock" },
    { 0x52, "AT24Cxx", "I2C EEPROM (+2)" },
    { 0x53, "AT24Cxx", "I2C EEPROM (+3)" },
    { 0x54, "AT24Cxx", "I2C EEPROM (+4)" },
    { 0x55, "AT24Cxx", "I2C EEPROM (+5)" },
    { 0x56, "AT24Cxx", "I2C EEPROM (+6)" },
    { 0x57, "AT24Cxx / AT24MAC402", "I2C EEPROM (+7) / MAC Address Chip" },
    { 0x58, "SGP30", "Indoor Air Quality (VOC / eCO2)" },
    { 0x59, "SGP40", "VOC Index Sensor" },
    { 0x5A, "CCS811 / MLX90614 / MPR121 / DRV2605L", "Air Quality / IR Temp / Touch / Haptic" },
    { 0x5C, "LPS22HB", "Barometric Pressure Sensor" },
    { 0x5D, "LPS25HB", "Barometric Pressure Sensor" },
    { 0x60, "MCP4725 / Si5351 / MPL3115A2", "12-bit DAC / Clock Gen / Barometer" },
    { 0x68, "MPU6050 / DS1307 / DS3231 / AMG8833", "6-Axis IMU / RTC / IR Thermal Grid" },
    { 0x69, "MPU9250 (Alt) / DS3232", "9-Axis IMU / RTC" },
    { 0x6A, "LSM6DS3 / PCF8523", "6-Axis IMU / RTC" },
    { 0x6B, "LSM6DS3 (Alt)", "6-Axis IMU" },
    { 0x6F, "MCP79410", "Battery-backed RTC" },
    { 0x70, "TCA9548A / HT16K33 / PCA9685", "I2C Multiplexer / LED Matrix / PWM AllCall" },
    { 0x71, "TCA9548A (Alt 1)", "I2C Multiplexer" },
    { 0x72, "TCA9548A (Alt 2)", "I2C Multiplexer" },
    { 0x76, "BME280 / BMP280 / BMP388", "Humidity, Pressure & Temp Sensor" },
    { 0x77, "BMP180 / BME680", "Pressure / Environmental VOC Sensor" }
};

void i2c_hal_init(void) {
    s_i2c_cfg.speed_hz        = UDOO_I2C_DEFAULT_SPEED;
    s_i2c_cfg.pullups_enabled = true;
    s_i2c_cfg.active          = false;
}

bool i2c_hal_acquire(uint32_t speed_hz, bool enable_pullups) {
    if (speed_hz < UDOO_I2C_MIN_SPEED) speed_hz = UDOO_I2C_MIN_SPEED;
    if (speed_hz > UDOO_I2C_MAX_SPEED) speed_hz = UDOO_I2C_MAX_SPEED;

    s_i2c_cfg.speed_hz        = speed_hz;
    s_i2c_cfg.pullups_enabled = enable_pullups;

    // Coordinate with instrument mode manager
    ModeMgr.enterMode(InstrumentMode::I2C, speed_hz);

    Wire.setSDA(UDOO_I2C_SDA_PIN);
    Wire.setSCL(UDOO_I2C_SCL_PIN);
    Wire.begin();
    Wire.setClock(speed_hz);

    if (enable_pullups) {
        gpio_pull_up(UDOO_I2C_SDA_PIN);
        gpio_pull_up(UDOO_I2C_SCL_PIN);
    } else {
        gpio_disable_pulls(UDOO_I2C_SDA_PIN);
        gpio_disable_pulls(UDOO_I2C_SCL_PIN);
    }

    s_i2c_cfg.active = true;
    return true;
}

void i2c_hal_release(void) {
    if (!s_i2c_cfg.active) return;

    Wire.end();

    // Restore GP2 and GP3 to Safe High-Z Inputs
    gpio_set_function(UDOO_I2C_SDA_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(UDOO_I2C_SDA_PIN, GPIO_IN);
    gpio_disable_pulls(UDOO_I2C_SDA_PIN);

    gpio_set_function(UDOO_I2C_SCL_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(UDOO_I2C_SCL_PIN, GPIO_IN);
    gpio_disable_pulls(UDOO_I2C_SCL_PIN);

    s_i2c_cfg.active = false;
}

bool i2c_hal_set_speed(uint32_t speed_hz) {
    if (speed_hz < UDOO_I2C_MIN_SPEED) speed_hz = UDOO_I2C_MIN_SPEED;
    if (speed_hz > UDOO_I2C_MAX_SPEED) speed_hz = UDOO_I2C_MAX_SPEED;

    s_i2c_cfg.speed_hz = speed_hz;
    if (s_i2c_cfg.active) {
        Wire.setClock(speed_hz);
    }
    return true;
}

void i2c_hal_set_pullups(bool enable) {
    s_i2c_cfg.pullups_enabled = enable;
    if (s_i2c_cfg.active) {
        if (enable) {
            gpio_pull_up(UDOO_I2C_SDA_PIN);
            gpio_pull_up(UDOO_I2C_SCL_PIN);
        } else {
            gpio_disable_pulls(UDOO_I2C_SDA_PIN);
            gpio_disable_pulls(UDOO_I2C_SCL_PIN);
        }
    }
}

void i2c_hal_get_config(I2cHalConfig* out_cfg) {
    if (out_cfg) {
        *out_cfg = s_i2c_cfg;
    }
}

bool i2c_hal_probe_addr(uint8_t addr_7bit) {
    if (!s_i2c_cfg.active) {
        i2c_hal_acquire(s_i2c_cfg.speed_hz, s_i2c_cfg.pullups_enabled);
    }
    Wire.beginTransmission(addr_7bit);
    return (Wire.endTransmission() == 0);
}

bool i2c_hal_probe_addr_read(uint8_t addr_7bit) {
    if (!s_i2c_cfg.active) {
        i2c_hal_acquire(s_i2c_cfg.speed_hz, s_i2c_cfg.pullups_enabled);
    }
    uint8_t count = Wire.requestFrom((int)addr_7bit, 1);
    if (count > 0) {
        while (Wire.available()) Wire.read();
        return true;
    }
    return false;
}

bool i2c_hal_identify_device(uint8_t addr_7bit, const char** out_part, const char** out_cat) {
    for (size_t i = 0; i < sizeof(s_known_devices) / sizeof(s_known_devices[0]); i++) {
        if (s_known_devices[i].addr == addr_7bit) {
            if (out_part) *out_part = s_known_devices[i].part;
            if (out_cat)  *out_cat  = s_known_devices[i].cat;
            return true;
        }
    }
    if (out_part) *out_part = "Unknown Device";
    if (out_cat)  *out_cat  = "Custom / Unlisted";
    return false;
}

uint16_t i2c_hal_scan(Stream& out, bool verbose) {
    if (!s_i2c_cfg.active) {
        i2c_hal_acquire(s_i2c_cfg.speed_hz, s_i2c_cfg.pullups_enabled);
    }

    out.printf("\r\n=== I2C Bus Scan (0x08 - 0x77 | %lu kHz) ===\r\n", s_i2c_cfg.speed_hz / 1000);
    out.printf("  Pins: SDA=GP2 (P1.4), SCL=GP3 (P1.5) | Pull-ups: %s\r\n\r\n",
        s_i2c_cfg.pullups_enabled ? "Internal Weak UP (50k)" : "External Only");

    uint16_t found_count = 0;

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        bool ack_w = i2c_hal_probe_addr(addr);
        bool ack_r = i2c_hal_probe_addr_read(addr);

        if (ack_w || ack_r) {
            found_count++;
            const char* part = nullptr;
            const char* cat  = nullptr;
            bool known = i2c_hal_identify_device(addr, &part, &cat);

            const char* mode_tag = "W/R";
            if (ack_w && !ack_r) mode_tag = "WRITE ONLY";
            else if (!ack_w && ack_r) mode_tag = "READ ONLY";

            out.printf("  [0x%02X] ACK: %-10s | 8-bit: (W:0x%02X R:0x%02X) | %s%s%s\r\n",
                addr,
                mode_tag,
                (addr << 1),
                (addr << 1) | 1,
                part,
                known ? " [" : "",
                known ? cat : ""
            );
            if (known) {
                out.print("]\r\n");
            }

            if (verbose && known) {
                out.printf("         Suggested probe: 'read 0x%02X 4' or 'eeprom probe 0x%02X'\r\n", addr, addr);
            }
        }
    }

    out.println();
    if (found_count == 0) {
        out.println("  No I2C devices responded on the bus.");
        out.println("  Check wiring and pull-up resistors (2.2k - 4.7k to 3.3V recommended).");
    } else {
        out.printf("Scan complete: %u device(s) detected.\r\n", found_count);
    }
    return found_count;
}

uint8_t i2c_hal_write(uint8_t addr_7bit, const uint8_t* data, size_t len, bool send_stop) {
    if (!s_i2c_cfg.active) {
        i2c_hal_acquire(s_i2c_cfg.speed_hz, s_i2c_cfg.pullups_enabled);
    }

    Wire.beginTransmission(addr_7bit);
    if (data && len > 0) {
        Wire.write(data, len);
    }
    return Wire.endTransmission(send_stop);
}

size_t i2c_hal_read(uint8_t addr_7bit, uint8_t* buffer, size_t len, bool send_stop) {
    if (!s_i2c_cfg.active) {
        i2c_hal_acquire(s_i2c_cfg.speed_hz, s_i2c_cfg.pullups_enabled);
    }
    if (!buffer || len == 0) return 0;

    size_t count = Wire.requestFrom((int)addr_7bit, (int)len, (bool)send_stop);
    size_t bytes_read = 0;
    while (Wire.available() && bytes_read < len) {
        buffer[bytes_read++] = (uint8_t)Wire.read();
    }
    return bytes_read;
}

bool i2c_hal_exec_bp_syntax(const char* cmd_str, Stream& out) {
    if (!cmd_str) return false;
    while (isspace((unsigned char)*cmd_str)) cmd_str++;
    if (!*cmd_str) return false;

    if (!s_i2c_cfg.active) {
        i2c_hal_acquire(s_i2c_cfg.speed_hz, s_i2c_cfg.pullups_enabled);
    }

    const char* p = cmd_str;
    bool in_transaction = false;
    bool in_write = false;
    uint8_t active_addr = 0;

    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;

        char c = *p;

        if (c == '[') {
            // START Condition
            p++;
            in_transaction = true;
            if (in_write) {
                // Repeated start: send restart without stop
                Wire.endTransmission(false);
                in_write = false;
            }
            out.print("I2C START\r\n");
            continue;
        }

        if (c == ']') {
            // STOP Condition
            p++;
            if (in_write) {
                uint8_t err = Wire.endTransmission(true);
                out.printf("I2C STOP (TX status: %u - %s)\r\n", err, err == 0 ? "ACK" : "NACK");
                in_write = false;
            } else {
                out.print("I2C STOP\r\n");
            }
            in_transaction = false;
            continue;
        }

        if (c == 'r' || c == 'R') {
            // Read token: r or r:N
            p++;
            uint16_t count = 1;
            if (*p == ':') {
                p++;
                count = (uint16_t)strtoul(p, (char**)&p, 0);
                if (count == 0) count = 1;
            }

            if (in_write) {
                // End write with repeated start before read
                Wire.endTransmission(false);
                in_write = false;
            }

            out.printf("READ %u BYTE%s (0x%02X): ", count, count > 1 ? "S" : "", active_addr);
            size_t got = Wire.requestFrom((int)active_addr, (int)count);
            for (size_t i = 0; i < got; i++) {
                uint8_t b = (uint8_t)Wire.read();
                out.printf("0x%02X ", b);
            }
            out.print("\r\n");
            continue;
        }

        // Numeric literal (Hex or Dec)
        if (isxdigit((unsigned char)c) || ((c == '0') && (tolower(*(p+1)) == 'x'))) {
            char* endp = nullptr;
            uint32_t val = strtoul(p, &endp, 0);
            if (endp == p) {
                p++;
                continue;
            }
            p = endp;

            if (in_transaction && !in_write && active_addr == 0) {
                // First byte after START is the I2C address
                if (val >= 0x80) {
                    // 8-bit notation: bit 0 = R/W flag, bits 7..1 = 7-bit addr
                    active_addr = (uint8_t)(val >> 1);
                    bool is_read = (val & 0x01) != 0;
                    if (is_read) {
                        out.printf("TARGET ADDR: 0x%02X (8-bit 0x%02X READ)\r\n", active_addr, (uint8_t)val);
                        in_write = false;
                    } else {
                        Wire.beginTransmission(active_addr);
                        in_write = true;
                        out.printf("TARGET ADDR: 0x%02X (8-bit 0x%02X WRITE)\r\n", active_addr, (uint8_t)val);
                    }
                } else {
                    // 7-bit notation
                    active_addr = (uint8_t)val;
                    Wire.beginTransmission(active_addr);
                    in_write = true;
                    out.printf("TARGET ADDR: 0x%02X (7-bit WRITE)\r\n", active_addr);
                }
            } else if (in_write) {
                // Data byte to write
                Wire.write((uint8_t)val);
                out.printf("WRITE: 0x%02X (buffered)\r\n", (uint8_t)val);
            } else {
                // Writing byte outside explicit START: write single byte directly
                Wire.beginTransmission(active_addr ? active_addr : (uint8_t)val);
                if (active_addr) Wire.write((uint8_t)val);
                uint8_t err = Wire.endTransmission(true);
                out.printf("DIRECT WRITE 0x%02X (status: %u - %s)\r\n", (uint8_t)val, err, err == 0 ? "ACK" : "NACK");
            }
            continue;
        }

        // Unknown character, skip
        p++;
    }

    // Auto-close open write transaction if not stopped
    if (in_write) {
        Wire.endTransmission(true);
    }

    return true;
}
