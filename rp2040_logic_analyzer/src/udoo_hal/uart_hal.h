#ifndef UDOO_UART_HAL_H
#define UDOO_UART_HAL_H

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─── UDOO KEY Probe Bus UART Physical Pinout ─────────────────────────────────
// Header P1 Pins:
// TX : GP4 (Header P1 Pin 6, Probe CH2) -> Target RX
// RX : GP5 (Header P1 Pin 7, Probe CH3) <- Target TX
#define UDOO_UART_TX_PIN         4
#define UDOO_UART_RX_PIN         5

#define UDOO_UART_DEFAULT_BAUD   115200
#define UDOO_UART_MIN_BAUD       300
#define UDOO_UART_MAX_BAUD       1000000

typedef enum {
    UART_PARITY_NONE_MODE = 0,
    UART_PARITY_EVEN_MODE = 1,
    UART_PARITY_ODD_MODE  = 2
} UartHalParity;

typedef struct {
    uint32_t      baudrate;       // Active baud rate (e.g. 9600, 115200)
    uint32_t      actual_baud;    // Actual RP2040 hardware baud rate
    uint8_t       data_bits;      // 5, 6, 7, 8 (default 8)
    UartHalParity parity;         // NONE, EVEN, ODD
    uint8_t       stop_bits;      // 1 or 2
    bool          invert;         // true if signal polarity is inverted
    bool          active;         // true if Serial2 is acquired & running
} UartHalConfig;

/**
 * @brief Initialize UART HAL data structures.
 */
void uart_hal_init(void);

/**
 * @brief Acquire and configure RP2040 Hardware UART1 on GP4 (TX) and GP5 (RX).
 * @param baudrate Target baud rate (300 to 1,000,000, default 115200)
 * @param data_bits Data bits (5..8, default 8)
 * @param parity Parity mode (NONE, EVEN, ODD)
 * @param stop_bits Stop bits (1 or 2)
 * @param invert Invert TX and RX polarities
 * @return true on success
 */
bool uart_hal_acquire(uint32_t baudrate, uint8_t data_bits, UartHalParity parity, uint8_t stop_bits, bool invert);

/**
 * @brief Release UART1 peripheral and restore GP4 and GP5 to Safe High-Z.
 */
void uart_hal_release(void);

/**
 * @brief Dynamically update baud rate in-flight.
 */
bool uart_hal_set_baudrate(uint32_t baudrate);

/**
 * @brief Dynamically update framing format in-flight.
 */
bool uart_hal_set_format(uint8_t data_bits, UartHalParity parity, uint8_t stop_bits);

/**
 * @brief Get copy of active UART configuration.
 */
void uart_hal_get_config(UartHalConfig* out_cfg);

/**
 * @brief Transmit raw bytes across probe UART TX (GP4).
 */
size_t uart_hal_write(const uint8_t* data, size_t len);

/**
 * @brief Transmit null-terminated string across probe UART TX (GP4).
 */
size_t uart_hal_write_str(const char* str);

/**
 * @brief Read available bytes from probe UART RX (GP5).
 */
size_t uart_hal_read(uint8_t* buf, size_t max_len);

/**
 * @brief Number of bytes currently available in RX FIFO.
 */
int uart_hal_available(void);

/**
 * @brief Run transparent bidirectional USB CDC <-> UART bridge until exit condition.
 * Exit triggers: Ctrl+] (0x1D), Ctrl+X (0x18), or button press.
 * @param host Stream to bridge with (typically Serial)
 */
void uart_hal_bridge(Stream& host);

/**
 * @brief Live passive RX monitor / sniffer with hex/ASCII view.
 * Exits when host sends any keypress or after duration_ms (0 = indefinite).
 */
void uart_hal_monitor(Stream& host, bool hex_mode, uint32_t duration_ms);

/**
 * @brief Perform hardware loopback verification (GP4 jumpered to GP5).
 * Tests transmission and reception of pattern at current baud rate.
 * @return true if loopback passed
 */
bool uart_hal_test_loopback(Stream& out);

/**
 * @brief Measure incoming RX pulse widths on GP5 to estimate baud rate.
 * @return Estimated baud rate, or 0 if timeout / no transitions
 */
uint32_t uart_hal_detect_baud(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // UDOO_UART_HAL_H
