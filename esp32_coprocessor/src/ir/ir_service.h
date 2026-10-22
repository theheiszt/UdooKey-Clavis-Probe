#pragma once

#include <Arduino.h>

namespace Esp32Ir {

enum class IrProtocol : uint8_t {
    UNKNOWN = 0,
    NEC,
    SONY,
    RC5,
    SAMSUNG
};

const char* irProtocolToString(IrProtocol proto);

struct IrDecodedResult {
    IrProtocol protocol;
    uint32_t   address;
    uint32_t   command;
    uint16_t   rawCount;
};

class IrService {
public:
    static IrService& instance();

    bool init(uint8_t txPin = 25, uint8_t rxPin = 26, uint16_t carrierFreqHz = 38000, uint8_t dutyPercent = 33);
    void deinit();

    void printStatus(Print& out);
    void sendRaw(const uint16_t* timingsUs, uint16_t count, uint16_t freqHz = 38000);
    void sendNec(uint16_t address, uint32_t command, Print* out = nullptr);
    bool captureRaw(uint16_t* timingsUs, uint16_t maxCount, uint16_t& actualCount, uint32_t timeoutMs = 3000);
    bool decodeCaptured(const uint16_t* timingsUs, uint16_t count, IrDecodedResult& result);

    void captureAndDecode(Print& out);
    void executeCommand(const char* args, Print& out);

private:
    IrService();

    uint8_t  m_txPin{25};
    uint8_t  m_rxPin{26};
    uint16_t m_carrierFreqHz{38000};
    uint8_t  m_dutyPercent{33};
    bool     m_initialized{false};
    bool     m_rxEnabled{false};

    uint16_t m_lastTimings[128];
    uint16_t m_lastCount{0};
};

} // namespace Esp32Ir
