#include "spi_hal.h"
#include "gpio_hal.h"
#include "instrument_mode_manager.h"
#include <SPI.h>

static SpiHalConfig s_spi_cfg = {
    .speed_hz     = UDOO_SPI_DEFAULT_SPEED,
    .cpol         = 0,
    .cpha         = 0,
    .lsb_first    = false,
    .cs_pin       = UDOO_SPI_DEFAULT_CS,
    .cs_asserted  = false,
    .acquired     = false
};

static SPISettings get_spi_settings() {
    uint8_t data_mode = SPI_MODE0;
    if (s_spi_cfg.cpol == 0 && s_spi_cfg.cpha == 0) data_mode = SPI_MODE0;
    else if (s_spi_cfg.cpol == 0 && s_spi_cfg.cpha == 1) data_mode = SPI_MODE1;
    else if (s_spi_cfg.cpol == 1 && s_spi_cfg.cpha == 0) data_mode = SPI_MODE2;
    else if (s_spi_cfg.cpol == 1 && s_spi_cfg.cpha == 1) data_mode = SPI_MODE3;

    BitOrder order = s_spi_cfg.lsb_first ? LSBFIRST : MSBFIRST;
    return SPISettings(s_spi_cfg.speed_hz, order, data_mode);
}

void spi_hal_init(void) {
    s_spi_cfg.speed_hz    = UDOO_SPI_DEFAULT_SPEED;
    s_spi_cfg.cpol        = 0;
    s_spi_cfg.cpha        = 0;
    s_spi_cfg.lsb_first   = false;
    s_spi_cfg.cs_pin      = UDOO_SPI_DEFAULT_CS;
    s_spi_cfg.cs_asserted = false;
    s_spi_cfg.acquired    = false;
}

bool spi_hal_acquire(uint32_t speed_hz, uint8_t cpol, uint8_t cpha, bool lsb_first, uint8_t cs_pin) {
    // Validate CS pin: must be within probe bus GP2..GP9
    uint8_t resolved_cs = 0;
    if (!gpio_hal_resolve_pin(cs_pin, &resolved_cs, nullptr)) {
        return false;
    }

    if (speed_hz < UDOO_SPI_MIN_SPEED) speed_hz = UDOO_SPI_MIN_SPEED;
    if (speed_hz > UDOO_SPI_MAX_SPEED) speed_hz = UDOO_SPI_MAX_SPEED;

    s_spi_cfg.speed_hz  = speed_hz;
    s_spi_cfg.cpol      = cpol ? 1 : 0;
    s_spi_cfg.cpha      = cpha ? 1 : 0;
    s_spi_cfg.lsb_first = lsb_first;
    s_spi_cfg.cs_pin    = resolved_cs;

    // Acquire SPI0 hardware from display bus and assign to probe pins GP2..GP5
    ModeMgr.acquireProbeSpi(s_spi_cfg.speed_hz, s_spi_cfg.cpol, s_spi_cfg.cpha);

    // If custom CS pin (not default GP5), configure it as output idle high
    if (s_spi_cfg.cs_pin != UDOO_SPI_DEFAULT_CS) {
        gpio_set_function(s_spi_cfg.cs_pin, GPIO_FUNC_SIO);
        gpio_put(s_spi_cfg.cs_pin, 1);
        gpio_set_dir(s_spi_cfg.cs_pin, GPIO_OUT);
    }

    s_spi_cfg.cs_asserted = false;
    s_spi_cfg.acquired    = true;

    return true;
}

void spi_hal_release(void) {
    if (!s_spi_cfg.acquired) return;

    // Deassert CS
    spi_hal_set_cs(false);

    // If custom CS pin was used, restore it to Safe High-Z
    if (s_spi_cfg.cs_pin != UDOO_SPI_DEFAULT_CS) {
        gpio_set_function(s_spi_cfg.cs_pin, GPIO_FUNC_SIO);
        gpio_set_dir(s_spi_cfg.cs_pin, GPIO_IN);
        gpio_disable_pulls(s_spi_cfg.cs_pin);
    }

    // Release probe bus SPI0 and restore ST7735 display bus
    ModeMgr.releaseProbeSpi();

    s_spi_cfg.acquired    = false;
    s_spi_cfg.cs_asserted = false;
}

void spi_hal_set_cs(bool assert) {
    if (!s_spi_cfg.acquired) return;

    if (assert) {
        digitalWrite(s_spi_cfg.cs_pin, LOW);
        s_spi_cfg.cs_asserted = true;
    } else {
        digitalWrite(s_spi_cfg.cs_pin, HIGH);
        s_spi_cfg.cs_asserted = false;
    }
}

bool spi_hal_get_cs(void) {
    return s_spi_cfg.cs_asserted;
}

uint8_t spi_hal_transfer(uint8_t tx_byte) {
    if (!s_spi_cfg.acquired) {
        // Auto-acquire with current settings if not yet acquired
        spi_hal_acquire(s_spi_cfg.speed_hz, s_spi_cfg.cpol, s_spi_cfg.cpha, s_spi_cfg.lsb_first, s_spi_cfg.cs_pin);
    }

    SPI.beginTransaction(get_spi_settings());
    uint8_t rx = SPI.transfer(tx_byte);
    SPI.endTransaction();
    return rx;
}

uint8_t spi_hal_read_byte(uint8_t dummy_byte) {
    return spi_hal_transfer(dummy_byte);
}

void spi_hal_write_byte(uint8_t tx_byte) {
    spi_hal_transfer(tx_byte);
}

void spi_hal_transfer_buffer(const uint8_t* tx_buf, uint8_t* rx_buf, size_t len) {
    if (!len) return;
    if (!s_spi_cfg.acquired) {
        spi_hal_acquire(s_spi_cfg.speed_hz, s_spi_cfg.cpol, s_spi_cfg.cpha, s_spi_cfg.lsb_first, s_spi_cfg.cs_pin);
    }

    SPI.beginTransaction(get_spi_settings());
    for (size_t i = 0; i < len; i++) {
        uint8_t tx = tx_buf ? tx_buf[i] : 0xFF;
        uint8_t rx = SPI.transfer(tx);
        if (rx_buf) rx_buf[i] = rx;
    }
    SPI.endTransaction();
}

bool spi_hal_configure(uint32_t speed_hz, uint8_t cpol, uint8_t cpha, bool lsb_first) {
    if (speed_hz < UDOO_SPI_MIN_SPEED) speed_hz = UDOO_SPI_MIN_SPEED;
    if (speed_hz > UDOO_SPI_MAX_SPEED) speed_hz = UDOO_SPI_MAX_SPEED;

    s_spi_cfg.speed_hz  = speed_hz;
    s_spi_cfg.cpol      = cpol ? 1 : 0;
    s_spi_cfg.cpha      = cpha ? 1 : 0;
    s_spi_cfg.lsb_first = lsb_first;

    return true;
}

bool spi_hal_set_cs_pin(uint8_t cs_pin) {
    uint8_t resolved_cs = 0;
    if (!gpio_hal_resolve_pin(cs_pin, &resolved_cs, nullptr)) {
        return false;
    }

    if (s_spi_cfg.acquired && s_spi_cfg.cs_pin != resolved_cs) {
        // Deassert old CS pin
        digitalWrite(s_spi_cfg.cs_pin, HIGH);
        // Configure new CS pin
        gpio_set_function(resolved_cs, GPIO_FUNC_SIO);
        gpio_put(resolved_cs, 1);
        gpio_set_dir(resolved_cs, GPIO_OUT);
    }

    s_spi_cfg.cs_pin = resolved_cs;
    return true;
}

void spi_hal_get_config(SpiHalConfig* out_cfg) {
    if (out_cfg) {
        *out_cfg = s_spi_cfg;
    }
}
