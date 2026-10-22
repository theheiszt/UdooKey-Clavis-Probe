#pragma once

#include <Arduino.h>

namespace Esp32Rf {

class Nrf24Service {
public:
    static Nrf24Service& instance();

    bool init();
    void deinit();
    bool isConnected() const { return m_connected; }

    void printStatus(Print& out);
    bool setChannel(uint8_t channel, Print& out);
    bool transmitPacket(const uint8_t* addr, const uint8_t* data, size_t len, Print& out);
    void receivePacket(Print& out);
    void startSniff(uint8_t channel, Print& out);
    void stopSniff(Print& out);

    void executeCommand(const char* args, Print& out);

private:
    Nrf24Service();

    uint8_t spiXfer(uint8_t byteOut);
    uint8_t readReg(uint8_t reg);
    void writeReg(uint8_t reg, uint8_t val);
    void writeBuf(uint8_t reg, const uint8_t* buf, uint8_t len);
    void readBuf(uint8_t reg, uint8_t* buf, uint8_t len);

    uint8_t m_cePin{4};
    uint8_t m_csnPin{15};
    uint8_t m_sckPin{14};
    uint8_t m_misoPin{35};
    uint8_t m_mosiPin{12};

    uint8_t m_channel{76};
    bool    m_connected{false};
    bool    m_sniffing{false};
};

} // namespace Esp32Rf
