#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace Esp32Bus {

class I2cService {
public:
    static I2cService& instance();

    bool init(uint32_t speedHz = 100000, int8_t sdaPin = 21, int8_t sclPin = 26);
    void deinit();
    bool isInitialized() const { return m_initialized; }

    void printStatus(Print& out);
    bool setSpeed(uint32_t speedHz, Print& out);
    void scanBus(Print& out);
    bool readData(uint8_t address, size_t count, Print& out);
    bool writeData(uint8_t address, const char* hexStr, Print& out);
    bool recoverBus(Print& out);

    void executeCommand(const char* args, Print& out);

private:
    I2cService() = default;

    TwoWire* m_wire{&Wire};
    uint32_t m_speedHz{100000};
    int8_t   m_sdaPin{21};
    int8_t   m_sclPin{26};
    bool     m_initialized{false};
};

} // namespace Esp32Bus
