#ifndef INSTRUMENT_MODE_MANAGER_H
#define INSTRUMENT_MODE_MANAGER_H

#include <Arduino.h>

// ─── Instrument Probe Bus Physical Definitions (GP2 .. GP9) ──────────────────
#define PROBE_PIN_BASE        2
#define PROBE_PIN_COUNT       8

// Pin Mappings on Probe Bus:
#define PROBE_PIN_GP2         2  // LA Ch0 | SPI SCK  | I2C SDA | PWM Out / Glitch Out
#define PROBE_PIN_GP3         3  // LA Ch1 | SPI MOSI | I2C SCL
#define PROBE_PIN_GP4         4  // LA Ch2 | SPI MISO | UART TX
#define PROBE_PIN_GP5         5  // LA Ch3 | SPI CS   | UART RX
#define PROBE_PIN_GP6         6  // LA Ch4 | 1-Wire DQ
#define PROBE_PIN_GP7         7  // LA Ch5 | SWD SWCLK
#define PROBE_PIN_GP8         8  // LA Ch6 | SWD SWDIO
#define PROBE_PIN_GP9         9  // LA Ch7 | AUX / DIO

// Dedicated Hardware Instrument Pins (outside probe bus):
#ifndef PIN_EXT_TRIG
#define PIN_EXT_TRIG          10 // External trigger input (GPIO 10)
#endif
#ifndef PIN_FRONTEND_PRESCALER
#define PIN_FRONTEND_PRESCALER 14 // 74HC74 hardware prescaler ratio (GPIO 14)
#endif

// ─── Instrument Modes ────────────────────────────────────────────────────────
enum class InstrumentMode : uint8_t {
    IDLE            = 0,
    LOGIC_ANALYZER  = 1,
    SPI             = 2,
    I2C             = 3,
    UART            = 4,
    ONE_WIRE        = 5,
    SWD             = 6,
    PWM_GENERATOR   = 7,
    GLITCH          = 8,
    EEPROM          = 9,
    FLASH           = 10,
    INTERCHIP_SPI   = 11,
    DIO             = 12,
    USB_SNIFFER     = 13,
    SENSOR_TEST     = 14
};

class InstrumentModeManager {
public:
    static InstrumentModeManager& instance();

    void init();

    // Mode Transitions enforcing:
    // enterMode() -> validate pin ownership -> stop conflicting peripheral -> configure PIO/SIO -> configure GPIO -> initialize mode
    // leaveMode() -> stop DMA/PIO -> restore pins -> release resources
    bool enterMode(InstrumentMode mode, uint32_t speed_param = 0, Stream* out = nullptr);
    void leaveMode();

    InstrumentMode getCurrentMode() const { return current_mode_; }
    const char*    getCurrentModeName() const;
    static const char* getModeName(InstrumentMode mode);

    // Pin Ownership & Validation
    bool validatePinOwnership(InstrumentMode mode, char* err_buf = nullptr, size_t err_len = 0);
    const char* getPinFunction(uint8_t gpio_pin) const;

    // SPI Display / Probe Arbiter
    // Because RP2040 SPI0 is shared between displays (GP18-20) and probe bus (GP2-5),
    // acquireProbeSpi() configures SPI0 for probe bus, and releaseProbeSpi() restores SPI0 for TFT.
    void acquireProbeSpi(uint32_t speed_hz = 10000000, uint8_t cpol = 0, uint8_t cpha = 0);
    void releaseProbeSpi();
    bool isProbeSpiActive() const { return probe_spi_active_; }

    // Diagnostics & Pinout reporting
    void printStatus(Stream& out) const;
    void printPinout(Stream& out) const;

private:
    InstrumentModeManager();
    InstrumentMode current_mode_;
    uint32_t       active_speed_;
    bool           probe_spi_active_;
};

extern InstrumentModeManager& ModeMgr;

#endif // INSTRUMENT_MODE_MANAGER_H
