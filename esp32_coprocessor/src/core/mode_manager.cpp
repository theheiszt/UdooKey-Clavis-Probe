#include "mode_manager.h"
#include "resource_manager.h"
#include <cstring>
#include <strings.h>

namespace Esp32Core {

const char* modeToString(CoprocessorMode mode) {
    switch (mode) {
        case CoprocessorMode::HIZ:   return "HiZ";
        case CoprocessorMode::GPIO:  return "GPIO";
        case CoprocessorMode::UART:  return "UART";
        case CoprocessorMode::SPI:   return "SPI";
        case CoprocessorMode::I2C:   return "I2C";
        case CoprocessorMode::WIFI:  return "WIFI";
        case CoprocessorMode::BLE:   return "BLE";
        case CoprocessorMode::SNIFF: return "SNIFF";
        case CoprocessorMode::IR:    return "IR";
        case CoprocessorMode::NRF24: return "NRF24";
        default:                     return "UNKNOWN";
    }
}

const char* modeToPrompt(CoprocessorMode mode) {
    switch (mode) {
        case CoprocessorMode::HIZ:   return "HiZ> ";
        case CoprocessorMode::GPIO:  return "GPIO> ";
        case CoprocessorMode::UART:  return "UART> ";
        case CoprocessorMode::SPI:   return "SPI> ";
        case CoprocessorMode::I2C:   return "I2C> ";
        case CoprocessorMode::WIFI:  return "WIFI> ";
        case CoprocessorMode::BLE:   return "BLE> ";
        case CoprocessorMode::SNIFF: return "SNIFF> ";
        case CoprocessorMode::IR:    return "IR> ";
        case CoprocessorMode::NRF24: return "NRF24> ";
        default:                     return "UDOO> ";
    }
}

CoprocessorMode parseMode(const char* str) {
    if (!str || !*str) return CoprocessorMode::HIZ;
    if (strcasecmp(str, "hiz") == 0 || strcmp(str, "0") == 0) return CoprocessorMode::HIZ;
    if (strcasecmp(str, "gpio") == 0 || strcmp(str, "1") == 0) return CoprocessorMode::GPIO;
    if (strcasecmp(str, "uart") == 0 || strcmp(str, "2") == 0) return CoprocessorMode::UART;
    if (strcasecmp(str, "spi") == 0 || strcmp(str, "3") == 0) return CoprocessorMode::SPI;
    if (strcasecmp(str, "i2c") == 0 || strcmp(str, "4") == 0) return CoprocessorMode::I2C;
    if (strcasecmp(str, "wifi") == 0 || strcmp(str, "5") == 0) return CoprocessorMode::WIFI;
    if (strcasecmp(str, "ble") == 0 || strcmp(str, "6") == 0) return CoprocessorMode::BLE;
    if (strcasecmp(str, "sniff") == 0 || strcasecmp(str, "packet") == 0 || strcmp(str, "7") == 0) return CoprocessorMode::SNIFF;
    if (strcasecmp(str, "ir") == 0 || strcmp(str, "8") == 0) return CoprocessorMode::IR;
    if (strcasecmp(str, "nrf24") == 0 || strcasecmp(str, "rf24") == 0 || strcmp(str, "9") == 0) return CoprocessorMode::NRF24;
    return CoprocessorMode::COUNT;
}

ModeManager& ModeManager::instance() {
    static ModeManager s_instance;
    return s_instance;
}

ModeManager::ModeManager()
    : m_currentMode(CoprocessorMode::HIZ) {
}

bool ModeManager::setMode(CoprocessorMode mode, Print* out) {
    if (mode >= CoprocessorMode::COUNT) {
        if (out) out->println("Error: Unknown mode. Valid modes: hiz, gpio, uart, spi, i2c, wifi, ble, sniff, ir, nrf24");
        return false;
    }

    if (m_currentMode == mode) {
        if (out) out->printf("Mode already set to %s\r\n", modeToString(mode));
        return true;
    }

    // When returning to HiZ, release any user-claimed resources
    if (mode == CoprocessorMode::HIZ) {
        ResourceManager::instance().resetUserResources();
    }

    m_currentMode = mode;
    if (out) {
        out->printf("Mode changed to %s. Safe Bus Pirate environment active.\r\n", modeToString(mode));
    }
    return true;
}

const char* ModeManager::prompt() const {
    return modeToPrompt(m_currentMode);
}

} // namespace Esp32Core
