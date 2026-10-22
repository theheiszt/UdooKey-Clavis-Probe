#pragma once

#include <Arduino.h>

namespace Esp32Bus {

class GpioService {
public:
    static GpioService& instance();

    void listPins(Print& out);
    bool setPinMode(uint8_t pin, const char* modeStr, Print& out);
    bool readPin(uint8_t pin, Print& out);
    bool writePin(uint8_t pin, uint8_t level, Print& out);
    bool togglePin(uint8_t pin, Print& out);
    void executeCommand(const char* args, Print& out);

private:
    GpioService() = default;
};

} // namespace Esp32Bus
