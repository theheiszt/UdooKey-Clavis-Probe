#pragma once

#include <Arduino.h>
#include <SPI.h>

namespace Esp32Bus {

class SpiService {
public:
    static SpiService& instance();

    bool init(uint32_t speedHz = 1000000, uint8_t mode = 0, int8_t csPin = 15);
    void deinit();
    bool isInitialized() const { return m_initialized; }

    void printStatus(Print& out);
    bool setSpeed(uint32_t speedHz, Print& out);
    bool setMode(uint8_t mode, Print& out);
    bool setCsPin(int8_t csPin, Print& out);
    void readBytes(size_t count, Print& out);
    void writeBytes(const char* hexStr, Print& out);
    void transferBytes(const char* hexStr, Print& out);

    void executeCommand(const char* args, Print& out);

private:
    SpiService() = default;

    SPIClass* m_spi{&SPI};
    uint32_t m_speedHz{1000000};
    uint8_t  m_mode{0}; // SPI_MODE0
    int8_t   m_csPin{15};
    int8_t   m_sckPin{14};
    int8_t   m_mosiPin{12};
    int8_t   m_misoPin{35};
    bool     m_initialized{false};
};

} // namespace Esp32Bus
