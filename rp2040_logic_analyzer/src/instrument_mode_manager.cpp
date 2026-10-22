#include "instrument_mode_manager.h"
#include "bus_pirate_core.h"
#include "pio_capture.h"
#include "spi_slave_transport.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "udoo_hal/gpio_hal.h"
#include <SPI.h>
#include <Wire.h>

InstrumentModeManager::InstrumentModeManager()
    : current_mode_(InstrumentMode::IDLE),
      active_speed_(0),
      probe_spi_active_(false) {
}

InstrumentModeManager& InstrumentModeManager::instance() {
    static InstrumentModeManager inst;
    return inst;
}

InstrumentModeManager& ModeMgr = InstrumentModeManager::instance();

void InstrumentModeManager::init() {
    leaveMode();
}

const char* InstrumentModeManager::getModeName(InstrumentMode mode) {
    switch (mode) {
        case InstrumentMode::IDLE:           return "IDLE (Safe Hi-Z)";
        case InstrumentMode::LOGIC_ANALYZER: return "LOGIC_ANALYZER";
        case InstrumentMode::SPI:            return "SPI";
        case InstrumentMode::I2C:            return "I2C";
        case InstrumentMode::UART:           return "UART";
        case InstrumentMode::ONE_WIRE:       return "1-WIRE";
        case InstrumentMode::SWD:            return "SWD";
        case InstrumentMode::PWM_GENERATOR:  return "PWM_GENERATOR";
        case InstrumentMode::GLITCH:         return "GLITCH";
        case InstrumentMode::EEPROM:         return "EEPROM";
        case InstrumentMode::FLASH:          return "FLASH";
        case InstrumentMode::INTERCHIP_SPI:  return "INTERCHIP_SPI (Optional Jumper Transport)";
        case InstrumentMode::DIO:            return "DIO (Digital I/O)";
        case InstrumentMode::USB_SNIFFER:    return "USB_SNIFFER (Probing & Protocol Sniffing)";
        case InstrumentMode::SENSOR_TEST:    return "SENSOR_TEST (Multi-Bus Sensor Testing)";
        default:                             return "UNKNOWN";
    }
}

const char* InstrumentModeManager::getCurrentModeName() const {
    return getModeName(current_mode_);
}

bool InstrumentModeManager::validatePinOwnership(InstrumentMode mode, char* err_buf, size_t err_len) {
    // Protected System Pin Map:
    // GP0, GP1   : UART0 Link to ESP32 (Hardwired on PCB)
    // GP10       : Dedicated EXT TRIG (Hardware input)
    // GP11..13   : Display 2 (CTRL) SPI CS/DC/RST
    // GP14       : 74HC74 Hardware Prescaler Control
    // GP15..17   : Display 1 (DISP) SPI CS/DC/RST
    // GP18..20   : Display SPI0 Bus (SCK=18, MOSI=19, MISO=20)
    // GP21, GP22 : Touch Controller (CS=21, IRQ=22)
    // GP23..25,29: Internal Ghost Pins (SMPS PS, VBUS sense, Onboard LED, VSYS/3)
    // GP26..28   : External ADC Voltmeters (ADC0, ADC1, ADC2)
    //
    // Physical Probe Bus: STRICTLY GP2 .. GP9.
    // Any mode trying to allocate outside GP2..GP9 (except optional EXT_TRIG on GP10) is rejected.

    if (mode == current_mode_ || mode == InstrumentMode::IDLE) {
        return true;
    }

    // Sections 4 & 5: Mutual exclusivity between LOGIC_ANALYZER, INTERCHIP_SPI, and external SPI probe
    if (mode == InstrumentMode::LOGIC_ANALYZER) {
        if (current_mode_ == InstrumentMode::INTERCHIP_SPI) {
            if (err_buf && err_len > 0) {
                snprintf(err_buf, err_len, "GP2..GP5 currently owned by INTERCHIP_SPI transport. Run 'transport uart' first.");
            }
            return false;
        }
        if (probe_spi_active_ || current_mode_ == InstrumentMode::SPI || current_mode_ == InstrumentMode::FLASH) {
            if (err_buf && err_len > 0) {
                snprintf(err_buf, err_len, "GP2..GP5 currently owned by external SPI probe. Exit SPI mode first.");
            }
            return false;
        }
    } else if (mode == InstrumentMode::INTERCHIP_SPI) {
        if (current_mode_ == InstrumentMode::LOGIC_ANALYZER && pio_capture_is_running()) {
            if (err_buf && err_len > 0) {
                snprintf(err_buf, err_len, "GP2..GP9 currently owned by active LOGIC_ANALYZER acquisition. Stop capture first.");
            }
            return false;
        }
        if (probe_spi_active_ || current_mode_ == InstrumentMode::SPI || current_mode_ == InstrumentMode::FLASH) {
            if (err_buf && err_len > 0) {
                snprintf(err_buf, err_len, "GP2..GP5 currently owned by external SPI probe. Exit SPI mode first.");
            }
            return false;
        }
    } else if (mode == InstrumentMode::SPI || mode == InstrumentMode::FLASH) {
        if (current_mode_ == InstrumentMode::INTERCHIP_SPI) {
            if (err_buf && err_len > 0) {
                snprintf(err_buf, err_len, "GP2..GP5 currently owned by INTERCHIP_SPI transport. Run 'transport uart' first.");
            }
            return false;
        }
        if (current_mode_ == InstrumentMode::LOGIC_ANALYZER && pio_capture_is_running()) {
            if (err_buf && err_len > 0) {
                snprintf(err_buf, err_len, "GP2..GP9 currently owned by active LOGIC_ANALYZER acquisition. Stop capture first.");
            }
            return false;
        }
    }

    return true;
}

void InstrumentModeManager::leaveMode() {
    // 1. Stop conflicting hardware peripherals and DMA/PIO engines
    switch (current_mode_) {
        case InstrumentMode::LOGIC_ANALYZER:
            pio_capture_stop();
            break;

        case InstrumentMode::INTERCHIP_SPI:
            spi_slave_transport_stop();
            break;

        case InstrumentMode::SPI:
        case InstrumentMode::FLASH:
            releaseProbeSpi();
            break;

        case InstrumentMode::I2C:
        case InstrumentMode::EEPROM:
            Wire.end();
            break;

        case InstrumentMode::UART:
            Serial2.end();
            break;

        case InstrumentMode::PWM_GENERATOR:
            bp_stop_pwm(PROBE_PIN_GP2);
            break;

        case InstrumentMode::GLITCH:
            pinMode(PROBE_PIN_GP2, INPUT);
            break;

        case InstrumentMode::SWD:
            pinMode(PROBE_PIN_GP7, INPUT);
            pinMode(PROBE_PIN_GP8, INPUT);
            break;

        case InstrumentMode::ONE_WIRE:
            pinMode(PROBE_PIN_GP6, INPUT);
            break;

        case InstrumentMode::DIO:
            gpio_hal_reset_all_hiz();
            break;

        default:
            break;
    }

    // 2. Restore all probe bus pins (GP2..GP9) to Safe High-Z Inputs
    for (uint8_t pin = PROBE_PIN_BASE; pin < PROBE_PIN_BASE + PROBE_PIN_COUNT; pin++) {
        gpio_set_function(pin, GPIO_FUNC_SIO);
        pinMode(pin, INPUT);
        gpio_disable_pulls(pin);
    }

    current_mode_ = InstrumentMode::IDLE;
    active_speed_ = 0;
}

bool InstrumentModeManager::enterMode(InstrumentMode mode, uint32_t speed_param, Stream* out) {
    char err_buf[128] = {0};
    if (!validatePinOwnership(mode, err_buf, sizeof(err_buf))) {
        if (out) out->printf("\r\n[ModeMgr ERR] Cannot enter %s: %s\r\n", getModeName(mode), err_buf);
        return false;
    }

    // Stop previous mode and return bus to Safe High-Z
    leaveMode();

    active_speed_ = speed_param;

    switch (mode) {
        case InstrumentMode::IDLE:
            // Already safe high-Z from leaveMode()
            break;

        case InstrumentMode::LOGIC_ANALYZER: {
            // Configure GP2..GP9 as PIO input channels
            for (uint8_t p = PROBE_PIN_BASE; p < PROBE_PIN_BASE + PROBE_PIN_COUNT; p++) {
                pio_gpio_init(pio0, p);
                gpio_set_dir(p, GPIO_IN);
                gpio_disable_pulls(p);
            }
            break;
        }

        case InstrumentMode::SPI:
        case InstrumentMode::FLASH: {
            uint32_t spd = speed_param ? speed_param : 10000000;
            acquireProbeSpi(spd);
            break;
        }

        case InstrumentMode::I2C:
        case InstrumentMode::EEPROM: {
            uint32_t spd = speed_param ? speed_param : 400000;
            Wire.setSDA(PROBE_PIN_GP2);
            Wire.setSCL(PROBE_PIN_GP3);
            Wire.begin();
            Wire.setClock(spd);
            break;
        }

        case InstrumentMode::UART: {
            uint32_t baud = speed_param ? speed_param : 115200;
            Serial2.setTX(PROBE_PIN_GP4);
            Serial2.setRX(PROBE_PIN_GP5);
            Serial2.begin(baud);
            break;
        }

        case InstrumentMode::ONE_WIRE: {
            pinMode(PROBE_PIN_GP6, INPUT_PULLUP);
            break;
        }

        case InstrumentMode::SWD: {
            pinMode(PROBE_PIN_GP7, OUTPUT);
            digitalWrite(PROBE_PIN_GP7, LOW); // SWCLK idle low
            pinMode(PROBE_PIN_GP8, INPUT_PULLUP); // SWDIO with pull-up
            break;
        }

        case InstrumentMode::PWM_GENERATOR: {
            uint32_t freq = speed_param ? speed_param : 1000;
            bp_generate_pwm(PROBE_PIN_GP2, freq, 50);
            break;
        }

        case InstrumentMode::GLITCH: {
            pinMode(PROBE_PIN_GP2, OUTPUT);
            digitalWrite(PROBE_PIN_GP2, LOW);
            break;
        }

        case InstrumentMode::INTERCHIP_SPI: {
            // Configure GP2..GP5 as hardware SPI0 slave for Inter-Chip Transport to ESP32 UEXT
            spi_slave_transport_init();
            break;
        }

        case InstrumentMode::DIO: {
            gpio_hal_init();
            break;
        }

        case InstrumentMode::USB_SNIFFER: {
            pinMode(PROBE_PIN_GP2, INPUT);
            gpio_disable_pulls(PROBE_PIN_GP2);
            pinMode(PROBE_PIN_GP3, INPUT);
            gpio_disable_pulls(PROBE_PIN_GP3);
            break;
        }

        case InstrumentMode::SENSOR_TEST: {
            // Sensors initialized on demand per bus
            break;
        }

        default:
            return false;
    }

    current_mode_ = mode;

    if (out) {
        out->printf("\r\n[ModeMgr] Entered Mode: %s\r\n", getCurrentModeName());
        printPinout(*out);
    }

    return true;
}

void InstrumentModeManager::acquireProbeSpi(uint32_t speed_hz, uint8_t cpol, uint8_t cpha) {
    if (probe_spi_active_) return;

    // Switch SPI0 to probe pins GP2 (SCK), GP3 (MOSI), GP4 (MISO)
    SPI.end();
    SPI.setSCK(PROBE_PIN_GP2);
    SPI.setTX(PROBE_PIN_GP3);
    SPI.setRX(PROBE_PIN_GP4);

    pinMode(PROBE_PIN_GP5, OUTPUT);
    digitalWrite(PROBE_PIN_GP5, HIGH); // CS idle high

    SPI.begin();
    probe_spi_active_ = true;
}

void InstrumentModeManager::releaseProbeSpi() {
    if (!probe_spi_active_) return;

    SPI.end();

    // Release probe CS pin to Safe High-Z
    gpio_set_function(PROBE_PIN_GP5, GPIO_FUNC_SIO);
    pinMode(PROBE_PIN_GP5, INPUT);
    gpio_disable_pulls(PROBE_PIN_GP5);

    // Restore SPI0 to Dual ST7735 Display Bus (GP18, GP19, GP20)
    SPI.setSCK(TFT_SCK);
    SPI.setTX(TFT_MOSI);
    SPI.setRX(TFT_MISO);
    SPI.begin();

    probe_spi_active_ = false;
}

const char* InstrumentModeManager::getPinFunction(uint8_t gpio_pin) const {
    switch (current_mode_) {
        case InstrumentMode::IDLE:
            return (gpio_pin >= 2 && gpio_pin <= 9) ? "HIGH-Z (Safe Input)" : "SYSTEM";

        case InstrumentMode::LOGIC_ANALYZER:
            switch (gpio_pin) {
                case 2: return "LA Ch0 (D0)";
                case 3: return "LA Ch1 (D1)";
                case 4: return "LA Ch2 (D2)";
                case 5: return "LA Ch3 (D3)";
                case 6: return "LA Ch4 (D4)";
                case 7: return "LA Ch5 (D5)";
                case 8: return "LA Ch6 (D6)";
                case 9: return "LA Ch7 (D7)";
                case 10: return "EXT TRIG";
                default: return "SYSTEM";
            }

        case InstrumentMode::SPI:
        case InstrumentMode::FLASH:
            switch (gpio_pin) {
                case 2: return "SPI SCK";
                case 3: return "SPI MOSI";
                case 4: return "SPI MISO";
                case 5: return "SPI CS#";
                default: return (gpio_pin >= 6 && gpio_pin <= 9) ? "HIGH-Z" : "SYSTEM";
            }

        case InstrumentMode::I2C:
        case InstrumentMode::EEPROM:
            switch (gpio_pin) {
                case 2: return "I2C SDA";
                case 3: return "I2C SCL";
                default: return (gpio_pin >= 4 && gpio_pin <= 9) ? "HIGH-Z" : "SYSTEM";
            }

        case InstrumentMode::UART:
            switch (gpio_pin) {
                case 4: return "UART TX (Serial2)";
                case 5: return "UART RX (Serial2)";
                default: return (gpio_pin >= 2 && gpio_pin <= 9) ? "HIGH-Z" : "SYSTEM";
            }

        case InstrumentMode::ONE_WIRE:
            switch (gpio_pin) {
                case 6: return "1-WIRE DQ (Pull-Up)";
                default: return (gpio_pin >= 2 && gpio_pin <= 9) ? "HIGH-Z" : "SYSTEM";
            }

        case InstrumentMode::SWD:
            switch (gpio_pin) {
                case 7: return "SWD SWCLK";
                case 8: return "SWD SWDIO";
                default: return (gpio_pin >= 2 && gpio_pin <= 9) ? "HIGH-Z" : "SYSTEM";
            }

        case InstrumentMode::PWM_GENERATOR:
            switch (gpio_pin) {
                case 2: return "PWM OUT (Slice 1A)";
                default: return (gpio_pin >= 3 && gpio_pin <= 9) ? "HIGH-Z" : "SYSTEM";
            }

        case InstrumentMode::GLITCH:
            switch (gpio_pin) {
                case 2: return "GLITCH OUT";
                default: return (gpio_pin >= 3 && gpio_pin <= 9) ? "HIGH-Z" : "SYSTEM";
            }

        case InstrumentMode::INTERCHIP_SPI:
            switch (gpio_pin) {
                case 2: return "MCU SPI SCK (Jumper to ESP32 IO14)";
                case 3: return "MCU SPI MOSI (Jumper to ESP32 IO12)";
                case 4: return "MCU SPI MISO (Jumper to ESP32 IO35)";
                case 5: return "MCU SPI CS# (Jumper to ESP32 IO15)";
                default: return (gpio_pin >= 6 && gpio_pin <= 9) ? "HIGH-Z" : "SYSTEM";
            }

        case InstrumentMode::DIO:
            switch (gpio_pin) {
                case 2: return "DIO Probe 0 (Header P1.4)";
                case 3: return "DIO Probe 1 (Header P1.5)";
                case 4: return "DIO Probe 2 (Header P1.6)";
                case 5: return "DIO Probe 3 (Header P1.7)";
                case 6: return "DIO Probe 4 (Header P1.9)";
                case 7: return "DIO Probe 5 (Header P1.10)";
                case 8: return "DIO Probe 6 (Header P1.11)";
                case 9: return "DIO Probe 7 (Header P1.12)";
                default: return "SYSTEM";
            }

        default:
            return "UNALLOCATED";
    }
}

void InstrumentModeManager::printStatus(Stream& out) const {
    out.println("\r\n==================================================");
    out.printf(" Instrument Mode Manager: [%s]\r\n", getCurrentModeName());
    if (active_speed_ > 0) {
        out.printf(" Active Speed/Clock     : %lu Hz\r\n", (unsigned long)active_speed_);
    }
    out.printf(" Probe SPI Active       : %s\r\n", probe_spi_active_ ? "YES (GP2..GP5)" : "NO (TFT Bus GP18..GP20)");
    out.println("==================================================");
}

void InstrumentModeManager::printPinout(Stream& out) const {
    out.println("--- Physical Instrument Bus Pin Assignment (GP2-GP9) ---");
    for (uint8_t p = 2; p <= 9; p++) {
        out.printf("  Pin GP%u (P1.%02u) : %s\r\n", p, p, getPinFunction(p));
    }
    out.println("  Pin GP10 (P1.10) : EXT TRIG (Dedicated External Trigger)");
    out.println("  Pin GP14 (P2.14) : PRESCALER (74HC74 Divide Ratio Control)");
    out.println("  Pin GP26..GP28   : ADC0, ADC1, ADC2 (0-3.3V Pure Analog Input)");
    out.println("---------------------------------------------------------");
}
