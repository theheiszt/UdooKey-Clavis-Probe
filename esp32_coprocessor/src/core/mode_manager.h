#pragma once

#include <Arduino.h>
#include <cstdint>

namespace Esp32Core {

enum class CoprocessorMode : uint8_t {
    HIZ = 0,
    GPIO,
    UART,
    SPI,
    I2C,
    WIFI,
    BLE,
    SNIFF,
    IR,
    NRF24,
    COUNT
};

const char* modeToString(CoprocessorMode mode);
const char* modeToPrompt(CoprocessorMode mode);
CoprocessorMode parseMode(const char* str);

class ModeManager {
public:
    static ModeManager& instance();

    CoprocessorMode currentMode() const { return m_currentMode; }
    bool setMode(CoprocessorMode mode, Print* out = nullptr);
    const char* prompt() const;

private:
    ModeManager();
    CoprocessorMode m_currentMode{CoprocessorMode::HIZ};
};

} // namespace Esp32Core
