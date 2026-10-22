#ifndef UDOO_SENSOR_HAL_H
#define UDOO_SENSOR_HAL_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── UDOO KEY Sensor Physical Pinout ─────────────────────────────────────────
// I2C Sensor Bus  : GP2 (SDA), GP3 (SCL) on Header P1
// SPI Sensor Bus  : GP2 (SCK), GP3 (MOSI), GP4 (MISO), GP5 (CS) on Header P1
// 1-Wire Sensor   : GP6 (DQ) on Header P1
// Analog Transducer: ADC0 (GP26), ADC1 (GP27), ADC2 (GP28) on Header P2
// Acoustic Tone   : GP20 (PWM Piezo Buzzer) on Header P1 Pin 15

typedef struct {
    uint8_t     i2c_addr;
    const char* model;
    const char* category;
    const char* manufacturer;
    uint8_t     who_am_i_reg;
    uint8_t     expected_id;
    bool        id_verified;
    char        telemetry[64];
} I2cSensorInfo;

typedef struct {
    uint8_t     mfg_id;
    uint16_t    device_id;
    const char* mfg_name;
    const char* part_name;
    uint32_t    capacity_bytes;
    bool        verified;
} SensorSpiFlashInfo;

typedef struct {
    float r_sensor_ohms;
    float temp_c;
    float temp_f;
    bool  valid;
} NtcTempReading;

// ─── Core Sensor HAL Functions ───────────────────────────────────────────────

/**
 * @brief Initialize sensor subsystem data structures.
 */
void sensor_hal_init(void);

/**
 * @brief Acquire sensor bus hardware.
 */
bool sensor_hal_acquire(void);

/**
 * @brief Release sensor hardware and return pins to Safe High-Z.
 */
void sensor_hal_release(void);

/**
 * @brief Check if Sensor HAL is currently active.
 */
bool sensor_hal_is_active(void);

/**
 * @brief Run full automatic sensor scan across I2C, SPI, 1-Wire, and Analog.
 */
void sensor_hal_scan_all(Stream& out);

/**
 * @brief Scan 7-bit I2C bus and query known sensor registers for model identification.
 */
void sensor_hal_scan_i2c(Stream& out);

/**
 * @brief Query SPI sensor / Flash memory JEDEC ID (0x9F) and identify chip.
 */
void sensor_hal_probe_spi(Stream& out);

/**
 * @brief Search 1-Wire bus for Dallas temperature sensors (DS18B20/DS18S20) and display telemetry.
 */
void sensor_hal_scan_1wire(Stream& out);

/**
 * @brief Measure NTC Thermistor temperature using Beta equation on specified ADC channel.
 * @param ch ADC channel (0=GP26, 1=GP27, 2=GP28)
 * @param r_ref Reference pull-up resistor in ohms (default 1000.0)
 * @param r0 Nominal NTC resistance at T0 (default 10000.0 for 10k NTC)
 * @param beta Thermistor beta coefficient (default 3950.0)
 * @param t0_c Reference temperature in Celsius (default 25.0)
 */
void sensor_hal_read_ntc(Stream& out, uint8_t ch, float r_ref, float r0, float beta, float t0_c);

/**
 * @brief Read analog Light Dependent Resistor (LDR / Photocell) and estimate ambient lux.
 */
void sensor_hal_read_photo(Stream& out, uint8_t ch);

/**
 * @brief High-precision frequency and pulse measurement for tachometer/flow/encoder sensors.
 */
void sensor_hal_measure_pulse(Stream& out, uint8_t pin, uint32_t timeout_ms);

/**
 * @brief Print comprehensive Sensor mode status box.
 */
void sensor_hal_print_status(Stream& out);

#ifdef __cplusplus
}
#endif

#endif // UDOO_SENSOR_HAL_H
