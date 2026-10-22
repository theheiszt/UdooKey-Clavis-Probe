#ifndef UDOO_SPI_HAL_H
#define UDOO_SPI_HAL_H

#include <Arduino.h>
#include <SPI.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── UDOO KEY Probe Bus SPI Physical Pinout ──────────────────────────────────
// Header P1 Pins:
// SCK  : GP2 (Header P1 Pin 4)
// MOSI : GP3 (Header P1 Pin 5)
// MISO : GP4 (Header P1 Pin 6)
// CS#  : GP5 (Header P1 Pin 7, default)
#define UDOO_SPI_SCK_PIN       2
#define UDOO_SPI_MOSI_PIN      3
#define UDOO_SPI_MISO_PIN      4
#define UDOO_SPI_DEFAULT_CS    5

#define UDOO_SPI_DEFAULT_SPEED 10000000 // 10 MHz default
#define UDOO_SPI_MIN_SPEED     10000    // 10 kHz min
#define UDOO_SPI_MAX_SPEED     30000000 // 30 MHz max

typedef struct {
    uint32_t speed_hz;
    uint8_t  cpol;          // 0 = Idle Low, 1 = Idle High
    uint8_t  cpha;          // 0 = First Edge, 1 = Second Edge
    bool     lsb_first;     // false = MSB First (default), true = LSB First
    uint8_t  cs_pin;        // Active CS pin on probe bus (default GP5)
    bool     cs_asserted;   // true if CS is currently asserted (LOW)
    bool     acquired;      // true if probe SPI bus is active (TFT released)
} SpiHalConfig;

/**
 * @brief Initialize SPI HAL configuration state.
 */
void spi_hal_init(void);

/**
 * @brief Acquire the RP2040 SPI0 peripheral for the probe bus (GP2..GP5).
 * Releases the ST7735 display bus temporarily and configures probe pins.
 */
bool spi_hal_acquire(uint32_t speed_hz, uint8_t cpol, uint8_t cpha, bool lsb_first, uint8_t cs_pin);

/**
 * @brief Release the probe SPI bus back to Safe High-Z and restore display SPI0.
 */
void spi_hal_release(void);

/**
 * @brief Assert or deassert the active Chip Select (CS#) pin.
 * @param assert true = Drive CS LOW (Active), false = Drive CS HIGH (Idle)
 */
void spi_hal_set_cs(bool assert);

/**
 * @brief Get current Chip Select state.
 * @return true if CS is asserted (LOW), false if idle (HIGH)
 */
bool spi_hal_get_cs(void);

/**
 * @brief Full-duplex single byte transfer.
 * @param tx_byte Byte to transmit
 * @return Byte received simultaneously
 */
uint8_t spi_hal_transfer(uint8_t tx_byte);

/**
 * @brief Read single byte from SPI bus (clocks out dummy byte, default 0xFF).
 */
uint8_t spi_hal_read_byte(uint8_t dummy_byte);

/**
 * @brief Write single byte to SPI bus (discards received byte).
 */
void spi_hal_write_byte(uint8_t tx_byte);

/**
 * @brief Full-duplex buffer transfer.
 */
void spi_hal_transfer_buffer(const uint8_t* tx_buf, uint8_t* rx_buf, size_t len);

/**
 * @brief Reconfigure active SPI speed, CPOL, CPHA, or bit order in-flight.
 */
bool spi_hal_configure(uint32_t speed_hz, uint8_t cpol, uint8_t cpha, bool lsb_first);

/**
 * @brief Set the active CS pin on probe bus (must be GP2..GP9).
 */
bool spi_hal_set_cs_pin(uint8_t cs_pin);

/**
 * @brief Get copy of current SPI configuration.
 */
void spi_hal_get_config(SpiHalConfig* out_cfg);

#ifdef __cplusplus
}
#endif

#endif // UDOO_SPI_HAL_H
