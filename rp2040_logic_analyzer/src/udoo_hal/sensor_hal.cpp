#include "udoo_hal/sensor_hal.h"
#include "udoo_hal/i2c_hal.h"
#include "udoo_hal/spi_hal.h"
#include "udoo_hal/onewire_hal.h"
#include "udoo_hal/meter_hal.h"
#include "terminal/terminal_transport.h"
#include "terminal/terminal_vt100.h"
#include "adc_subsystem.h"
#include <math.h>

static bool s_sensor_active = false;

void sensor_hal_init(void) {
    s_sensor_active = false;
}

bool sensor_hal_acquire(void) {
    s_sensor_active = true;
    return true;
}

void sensor_hal_release(void) {
    if (s_sensor_active) {
        i2c_hal_release();
        spi_hal_release();
        onewire_hal_release();
        meter_hal_release();
        s_sensor_active = false;
    }
}

bool sensor_hal_is_active(void) {
    return s_sensor_active;
}

void sensor_hal_print_status(Stream& out) {
    vt100_set_color(VT100_COLOR_INFO);
    out.print("\r\n=== Universal Sensor Testing & Identification Suite ===\r\n");
    vt100_set_color(VT100_COLOR_RESET);
    out.print("  I2C Sensor Bus:    GP2 (SDA), GP3 (SCL) [Header P1 Pins 4 & 5]\r\n");
    out.print("  SPI Sensor Bus:    GP2 (SCK), GP3 (MOSI), GP4 (MISO), GP5 (CS) [P1]\r\n");
    out.print("  Dallas 1-Wire:     GP6 (DQ with 50k internal pull-up) [Header P1 Pin 9]\r\n");
    out.print("  Analog Inputs:     ADC0=GP26 (P2.31) | ADC1=GP27 (P2.32) | ADC2=GP28 (P2.34)\r\n");
    out.print("  Acoustic Feedback: GP20 (PWM Piezo Buzzer, 2.7 kHz resonant driver)\r\n");
    out.printf("  Subsystem State:   %s\r\n", s_sensor_active ? "Active" : "Idle");
    vt100_set_color(VT100_COLOR_INFO);
    out.print("=======================================================\r\n\r\n");
    vt100_set_color(VT100_COLOR_RESET);
}

static bool i2c_read_reg_byte(uint8_t addr, uint8_t reg, uint8_t* val) {
    if (i2c_hal_write(addr, &reg, 1, false) != 0) return false;
    return (i2c_hal_read(addr, val, 1, true) == 1);
}

void sensor_hal_scan_i2c(Stream& out) {
    out.print("\r\n=== Scanning I2C Bus (GP2=SDA, GP3=SCL) for Known Sensors ===\r\n");
    out.print("+------+----------------+----------------------+---------------------------+----------+\r\n");
    out.print("| Addr | Identified Model| Manufacturer         | Primary Sensor Category   | Signature|\r\n");
    out.print("+------+----------------+----------------------+---------------------------+----------+\r\n");

    // Acquire I2C hardware at 100 kHz for robust sensor interrogation
    i2c_hal_acquire(100000, true);

    uint8_t found = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        uint8_t dummy = 0;
        if (i2c_hal_read(addr, &dummy, 1, true) == 1) {
            found++;
            const char* model = "Generic I2C Device";
            const char* mfg   = "Unknown Vendor";
            const char* cat   = "Digital Peripheral";
            char sig_buf[16]  = "ACK";

            // Interrogate known signature registers
            if (addr == 0x76 || addr == 0x77) {
                uint8_t chip_id = 0;
                if (i2c_read_reg_byte(addr, 0xD0, &chip_id)) {
                    if (chip_id == 0x58) {
                        model = "BMP280"; mfg = "Bosch Sensortec"; cat = "Pressure & Temperature";
                        snprintf(sig_buf, sizeof(sig_buf), "ID: 0x%02X", chip_id);
                    } else if (chip_id == 0x60) {
                        model = "BME280"; mfg = "Bosch Sensortec"; cat = "Humidity, Pressure & Temp";
                        snprintf(sig_buf, sizeof(sig_buf), "ID: 0x%02X", chip_id);
                    } else if (chip_id == 0x61) {
                        model = "BME680"; mfg = "Bosch Sensortec"; cat = "Gas, Humidity, Press & Temp";
                        snprintf(sig_buf, sizeof(sig_buf), "ID: 0x%02X", chip_id);
                    }
                }
            } else if (addr == 0x68 || addr == 0x69) {
                uint8_t who_am_i = 0;
                if (i2c_read_reg_byte(addr, 0x75, &who_am_i)) {
                    if (who_am_i == 0x68) {
                        model = "MPU-6050"; mfg = "InvenSense / TDK"; cat = "6-Axis Motion / IMU";
                        snprintf(sig_buf, sizeof(sig_buf), "ID: 0x%02X", who_am_i);
                    } else if (who_am_i == 0x70) {
                        model = "MPU-6500"; mfg = "InvenSense / TDK"; cat = "6-Axis Motion / IMU";
                        snprintf(sig_buf, sizeof(sig_buf), "ID: 0x%02X", who_am_i);
                    } else if (who_am_i == 0x71) {
                        model = "MPU-9250"; mfg = "InvenSense / TDK"; cat = "9-Axis Motion & Compass";
                        snprintf(sig_buf, sizeof(sig_buf), "ID: 0x%02X", who_am_i);
                    }
                } else if (addr == 0x68) {
                    model = "DS3231 / DS1307"; mfg = "Maxim Integrated"; cat = "Precision RTC Clock";
                }
            } else if (addr == 0x23) {
                model = "BH1750"; mfg = "ROHM Semiconductor"; cat = "Ambient Light Sensor (Lux)";
            } else if (addr == 0x38 || addr == 0x39) {
                model = "AHT10 / AHT20"; mfg = "Aosong Electronics"; cat = "Temperature & Humidity";
            } else if (addr == 0x44 || addr == 0x45) {
                model = "SHT30 / SHT31"; mfg = "Sensirion"; cat = "Precision Temp & Humidity";
            } else if (addr == 0x40 || addr == 0x41) {
                model = "INA219 / INA226"; mfg = "Texas Instruments"; cat = "High-Side Current & Power";
            } else if (addr == 0x18 || addr == 0x19) {
                model = "MCP9808"; mfg = "Microchip"; cat = "Precision Digital Temp (+-0.25C)";
            } else if (addr >= 0x48 && addr <= 0x4B) {
                model = "ADS1115 / TMP102"; mfg = "Texas Instruments"; cat = "16-Bit ADC / Temp Sensor";
            } else if (addr == 0x3C || addr == 0x3D) {
                model = "SSD1306 / SH1106"; mfg = "Solomon Systech"; cat = "OLED Graphic Display";
            } else if (addr >= 0x50 && addr <= 0x57) {
                model = "24Cxx EEPROM"; mfg = "Microchip / Atmel"; cat = "Non-Volatile Memory";
            }

            out.printf("| 0x%02X | %-14s | %-20s | %-25s | %-8s |\r\n",
                addr, model, mfg, cat, sig_buf);
        }
    }

    out.print("+------+----------------+----------------------+---------------------------+----------+\r\n");
    if (found == 0) {
        out.print("No I2C sensors detected. Ensure target is powered and connected to GP2(SDA)/GP3(SCL).\r\n\r\n");
    } else {
        out.printf("Identified %u active I2C device(s) on bus.\r\n\r\n", found);
    }
}

void sensor_hal_probe_spi(Stream& out) {
    out.print("\r\n=== Interrogating SPI Sensor / Flash (GP2=SCK, GP3=MOSI, GP4=MISO, GP5=CS) ===\r\n");

    // Acquire SPI at 10 MHz
    spi_hal_acquire(10000000, 0, 0, false, 5);

    // Send JEDEC Read ID command (0x9F)
    uint8_t tx[4] = { 0x9F, 0x00, 0x00, 0x00 };
    uint8_t rx[4] = { 0 };

    spi_hal_set_cs(true);
    spi_hal_transfer_buffer(tx, rx, 4);
    spi_hal_set_cs(false);

    uint8_t mfg_id  = rx[1];
    uint8_t mem_type = rx[2];
    uint8_t cap_id   = rx[3];

    out.printf("Raw JEDEC ID Response: 0x%02X 0x%02X 0x%02X\r\n", mfg_id, mem_type, cap_id);

    if ((mfg_id == 0x00 && mem_type == 0x00) || (mfg_id == 0xFF && mem_type == 0xFF)) {
        out.print("No SPI Flash / Sensor response detected (Line floating or unpowered).\r\n\r\n");
        return;
    }

    const char* mfg_name = "Unknown Vendor";
    if (mfg_id == 0xEF) mfg_name = "Winbond Electronics";
    else if (mfg_id == 0xC2) mfg_name = "Macronix (MXIC)";
    else if (mfg_id == 0x20) mfg_name = "Micron / Numonyx";
    else if (mfg_id == 0x1F) mfg_name = "Adesto Technologies";
    else if (mfg_id == 0xBF) mfg_name = "Microchip / SST";
    else if (mfg_id == 0x01) mfg_name = "Spansion / Cypress";
    else if (mfg_id == 0xC8) mfg_name = "GigaDevice";

    uint32_t capacity_bytes = 0;
    if (cap_id >= 0x10 && cap_id <= 0x20) {
        capacity_bytes = 1UL << cap_id;
    }

    vt100_set_color(VT100_COLOR_VALUE);
    out.printf("  Manufacturer:   %s (0x%02X)\r\n", mfg_name, mfg_id);
    out.printf("  Memory Type:    0x%02X\r\n", mem_type);
    if (capacity_bytes > 0) {
        out.printf("  Capacity:       %lu MB (%lu Mbit)\r\n",
            (unsigned long)(capacity_bytes / (1024 * 1024)),
            (unsigned long)(capacity_bytes * 8 / (1024 * 1024)));
    }
    vt100_set_color(VT100_COLOR_RESET);
    out.print("------------------------------------------------------------------------\r\n\r\n");
}

void sensor_hal_scan_1wire(Stream& out) {
    out.print("\r\n=== Scanning 1-Wire Bus (GP6:DQ) for Temperature Sensors ===\r\n");
    onewire_hal_acquire(UDOO_ONEWIRE_DEFAULT_PIN, true);
    onewire_hal_search_bus(out);
    out.print("Reading connected Dallas temperature sensors...\r\n");
    onewire_hal_read_all_temps(out);
    out.print("\r\n");
}

void sensor_hal_read_ntc(Stream& out, uint8_t ch, float r_ref, float r0, float beta, float t0_c) {
    if (ch > 2) ch = 0;
    if (r_ref <= 0.0f) r_ref = 1000.0f;
    if (r0 <= 0.0f) r0 = 10000.0f;
    if (beta <= 0.0f) beta = 3950.0f;
    if (t0_c <= 0.0f) t0_c = 25.0f;

    meter_hal_acquire();
    float v_meas = meter_hal_read_channel(ch, 32);
    float v_source = 3.300f;

    out.printf("\r\n=== NTC Thermistor Transducer on ADC%u (GP%u) ===\r\n", ch, 26 + ch);
    out.printf("  Configuration: R_ref=%.1f ohms, R0=%.1f ohms, Beta=%.1f, T0=%.1f C\r\n",
        r_ref, r0, beta, t0_c);
    out.printf("  Measured ADC:  %.4f V\r\n", v_meas);

    if (v_meas <= 0.005f) {
        vt100_set_color(VT100_COLOR_WARNING);
        out.print("  Sensor State:  SHORT CIRCUIT TO GND (0 ohms)\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }
    if (v_meas >= v_source - 0.005f) {
        vt100_set_color(VT100_COLOR_WARNING);
        out.print("  Sensor State:  OPEN CIRCUIT / DISCONNECTED\r\n");
        vt100_set_color(VT100_COLOR_RESET);
        return;
    }

    // Resistance calculation across voltage divider:
    // R_ntc = R_ref * (V_meas / (V_source - V_meas))
    float r_ntc = r_ref * (v_meas / (v_source - v_meas));

    // Steinhart-Hart / Beta equation:
    // 1 / T_K = 1 / T0_K + (1 / Beta) * ln(R_ntc / R0)
    float t0_k = t0_c + 273.15f;
    float inv_t_k = (1.0f / t0_k) + (1.0f / beta) * logf(r_ntc / r0);
    float t_k = 1.0f / inv_t_k;
    float t_c = t_k - 273.15f;
    float t_f = t_c * 1.8f + 32.0f;

    vt100_set_color(VT100_COLOR_VALUE);
    out.printf("  NTC Resistance: %.1f ohms (%.2f k-ohms)\r\n", r_ntc, r_ntc / 1000.0f);
    out.printf("  Temperature:    %.2f C  (%.2f F)\r\n", t_c, t_f);
    vt100_set_color(VT100_COLOR_RESET);
    out.print("==================================================\r\n\r\n");
}

void sensor_hal_read_photo(Stream& out, uint8_t ch) {
    if (ch > 2) ch = 0;

    meter_hal_acquire();
    float v_meas = meter_hal_read_channel(ch, 32);
    float v_source = 3.300f;
    float r_ref = 10000.0f; // Standard 10k LDR pull-up

    out.printf("\r\n=== Photocell / Light-Dependent Resistor (LDR) on ADC%u (GP%u) ===\r\n", ch, 26 + ch);
    out.printf("  Measured Voltage: %.4f V\r\n", v_meas);

    if (v_meas < 0.05f) {
        out.print("  Illumination:     EXTREMELY BRIGHT / SATURATED LIGHT (> 2,000 Lux)\r\n\r\n");
        return;
    }
    if (v_meas > v_source - 0.05f) {
        out.print("  Illumination:     TOTAL DARKNESS (< 1 Lux)\r\n\r\n");
        return;
    }

    float r_ldr = r_ref * (v_meas / (v_source - v_meas));
    // Empirical LDR curve: Lux approx = 500 / (R_ldr in k-ohms) ^ 1.4
    float r_k = r_ldr / 1000.0f;
    float lux = 500.0f / powf(r_k, 1.4f);

    vt100_set_color(VT100_COLOR_VALUE);
    out.printf("  LDR Resistance:   %.1f ohms (%.2f k-ohms)\r\n", r_ldr, r_k);
    out.printf("  Estimated Lux:    %.1f Lux\r\n", lux);
    vt100_set_color(VT100_COLOR_RESET);
    out.print("==================================================================\r\n\r\n");
}

void sensor_hal_measure_pulse(Stream& out, uint8_t pin, uint32_t timeout_ms) {
    if (pin < 2 || pin > 9) pin = 2;
    if (timeout_ms == 0) timeout_ms = 1000;

    pinMode(pin, INPUT);

    out.printf("\r\n=== Pulse / Tachometer / Flow Sensor Measurement on GP%u ===\r\n", pin);
    out.print("Measuring frequency, period, and pulse widths...\r\n");

    // Wait for edge transitions
    uint32_t high_us = pulseIn(pin, HIGH, timeout_ms * 1000);
    uint32_t low_us  = pulseIn(pin, LOW, timeout_ms * 1000);

    if (high_us == 0 && low_us == 0) {
        out.print("No pulse transitions detected (Signal is static DC).\r\n\r\n");
        return;
    }

    uint32_t period_us = high_us + low_us;
    float freq_hz = period_us > 0 ? (1000000.0f / (float)period_us) : 0.0f;
    float duty_pct = period_us > 0 ? ((float)high_us * 100.0f / (float)period_us) : 0.0f;

    vt100_set_color(VT100_COLOR_VALUE);
    out.printf("  High Time:    %lu us\r\n", (unsigned long)high_us);
    out.printf("  Low Time:     %lu us\r\n", (unsigned long)low_us);
    out.printf("  Period:       %lu us\r\n", (unsigned long)period_us);
    out.printf("  Frequency:    %.2f Hz\r\n", freq_hz);
    out.printf("  Duty Cycle:   %.1f %%\r\n", duty_pct);
    vt100_set_color(VT100_COLOR_RESET);
    out.print("============================================================\r\n\r\n");
}

void sensor_hal_scan_all(Stream& out) {
    out.print("\r\n=======================================================\r\n");
    out.print("           AUTONOMOUS MULTI-BUS SENSOR SCAN            \r\n");
    out.print("=======================================================\r\n");

    sensor_hal_scan_i2c(out);
    sensor_hal_scan_1wire(out);
    sensor_hal_probe_spi(out);

    out.print("-------------------------------------------------------\r\n");
    out.print("Multi-bus sensor scan complete.\r\n\r\n");
}
