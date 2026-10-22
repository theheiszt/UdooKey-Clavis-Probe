#pragma once

#include <Arduino.h>

namespace Esp32Bus {

class UartService {
public:
    static UartService& instance();

    bool init(uint32_t baud = 115200, uint32_t config = SERIAL_8N1, int8_t rxPin = 16, int8_t txPin = 17);
    void deinit();
    bool isInitialized() const { return m_initialized; }

    void printStatus(Print& out);
    bool setBaud(uint32_t baud, Print& out);
    bool setConfig(const char* cfgStr, Print& out);
    void readData(Print& out);
    bool writeData(const char* dataStr, bool isHex, Print& out);
    void startMonitor(Print& out);
    void stopMonitor(Print& out);
    bool isMonitoring() const { return m_monitoring; }
    void pollMonitor(Print& out);

    void executeCommand(const char* args, Print& out);

private:
    UartService() = default;

    HardwareSerial* m_serial{&Serial1};
    uint32_t m_baud{115200};
    uint32_t m_config{SERIAL_8N1};
    char m_configStr[8]{"8N1"};
    int8_t m_rxPin{16};
    int8_t m_txPin{17};
    bool m_initialized{false};
    bool m_monitoring{false};
};

} // namespace Esp32Bus
