#include "ir_service.h"
#include "../core/resource_manager.h"
#include "../packet/pcap_service.h"
#include <cstring>
#include <strings.h>

namespace Esp32Ir {

const char* irProtocolToString(IrProtocol proto) {
    switch (proto) {
        case IrProtocol::NEC:     return "NEC";
        case IrProtocol::SONY:    return "Sony";
        case IrProtocol::RC5:     return "RC5";
        case IrProtocol::SAMSUNG: return "Samsung";
        default:                  return "UNKNOWN (Raw)";
    }
}

IrService& IrService::instance() {
    static IrService s_instance;
    return s_instance;
}

IrService::IrService() {
    memset(m_lastTimings, 0, sizeof(m_lastTimings));
}

bool IrService::init(uint8_t txPin, uint8_t rxPin, uint16_t carrierFreqHz, uint8_t dutyPercent) {
    auto& rm = Esp32Core::ResourceManager::instance();
    rm.acquire(Esp32Core::Resource::IR_PERIPHERAL, Esp32Core::Owner::IR_SERVICE);

    m_txPin = txPin;
    m_rxPin = rxPin;
    m_carrierFreqHz = carrierFreqHz ? carrierFreqHz : 38000;
    m_dutyPercent = dutyPercent ? dutyPercent : 33;

    rm.acquirePin(m_txPin, Esp32Core::Owner::IR_SERVICE);
    rm.acquirePin(m_rxPin, Esp32Core::Owner::IR_SERVICE);

    pinMode(m_rxPin, INPUT_PULLUP);
    pinMode(m_txPin, OUTPUT);
    digitalWrite(m_txPin, LOW);

    m_initialized = true;
    return true;
}

void IrService::deinit() {
    auto& rm = Esp32Core::ResourceManager::instance();
    rm.release(Esp32Core::Resource::IR_PERIPHERAL, Esp32Core::Owner::IR_SERVICE);
    rm.releasePin(m_txPin, Esp32Core::Owner::IR_SERVICE);
    rm.releasePin(m_rxPin, Esp32Core::Owner::IR_SERVICE);
    pinMode(m_txPin, INPUT);
    m_initialized = false;
    m_rxEnabled = false;
}

void IrService::printStatus(Print& out) {
    if (!m_initialized) init();

    out.println("\r\n=== INFRARED (IR) TRANSCEIVER STATUS ===");
    out.printf("  Hardware TX   : IO%u (LEDC 38 kHz PWM Modulator)\r\n", m_txPin);
    out.printf("  Hardware RX   : IO%u (Active LOW Demodulator Input)\r\n", m_rxPin);
    out.printf("  Carrier Freq  : %u Hz (Duty: %u%%)\r\n", m_carrierFreqHz, m_dutyPercent);
    out.printf("  Decoders      : NEC, Sony, RC5, Samsung, Raw Microseconds\r\n");
    out.printf("  Last Captured : %u pulse timings\r\n", m_lastCount);
    out.println("========================================");
}

void IrService::sendRaw(const uint16_t* timingsUs, uint16_t count, uint16_t freqHz) {
    if (!m_initialized) init();

    ledcSetup(1, freqHz, 8); // Channel 1 for IR carrier
    ledcAttachPin(m_txPin, 1);
    uint32_t dutyVal = (m_dutyPercent * 255) / 100;

    for (uint16_t i = 0; i < count; i++) {
        if ((i % 2) == 0) { // Mark: Carrier ON
            ledcWrite(1, dutyVal);
        } else { // Space: Carrier OFF
            ledcWrite(1, 0);
        }
        delayMicroseconds(timingsUs[i]);
    }

    ledcWrite(1, 0);
    ledcDetachPin(m_txPin);
    pinMode(m_txPin, OUTPUT);
    digitalWrite(m_txPin, LOW);
}

void IrService::sendNec(uint16_t address, uint32_t command, Print* out) {
    uint16_t timings[68];
    uint16_t idx = 0;

    // Leader code: 9ms mark, 4.5ms space
    timings[idx++] = 9000;
    timings[idx++] = 4500;

    // 16-bit address
    for (int b = 0; b < 16; b++) {
        timings[idx++] = 560;
        timings[idx++] = (address & (1 << b)) ? 1690 : 560;
    }
    // 8-bit command
    for (int b = 0; b < 8; b++) {
        timings[idx++] = 560;
        timings[idx++] = (command & (1 << b)) ? 1690 : 560;
    }
    // 8-bit inverted command
    for (int b = 0; b < 8; b++) {
        timings[idx++] = 560;
        timings[idx++] = ((~command) & (1 << b)) ? 1690 : 560;
    }
    timings[idx++] = 560; // Stop burst

    sendRaw(timings, idx, m_carrierFreqHz);

    if (out) {
        out->printf("Sent NEC IR: Address=0x%04X, Command=0x%02X (%u pulses)\r\n", address, (uint8_t)command, idx);
    }
}

bool IrService::captureRaw(uint16_t* timingsUs, uint16_t maxCount, uint16_t& actualCount, uint32_t timeoutMs) {
    if (!m_initialized) init();

    actualCount = 0;
    uint32_t startMs = millis();

    // Demodulator idle is HIGH, active mark is LOW
    while (digitalRead(m_rxPin) == HIGH) {
        if (millis() - startMs > timeoutMs) return false;
        delayMicroseconds(5);
    }

    uint32_t lastEdge = micros();
    bool lastState = false; // Mark (LOW)

    while (actualCount < maxCount) {
        bool curState = (digitalRead(m_rxPin) == HIGH);
        if (curState != lastState) {
            uint32_t now = micros();
            uint32_t duration = now - lastEdge;
            timingsUs[actualCount++] = (duration > 65535) ? 65535 : (uint16_t)duration;
            lastEdge = now;
            lastState = curState;
        }

        // Space timeout (> 20 ms = end of burst)
        if (lastState == true && (micros() - lastEdge > 20000)) {
            break;
        }
    }
    return actualCount > 0;
}

bool IrService::decodeCaptured(const uint16_t* timingsUs, uint16_t count, IrDecodedResult& result) {
    result.protocol = IrProtocol::UNKNOWN;
    result.address = 0;
    result.command = 0;
    result.rawCount = count;

    if (count < 4) return false;

    // Check NEC (9ms mark, 4.5ms space)
    if (timingsUs[0] >= 8000 && timingsUs[0] <= 10000 &&
        timingsUs[1] >= 4000 && timingsUs[1] <= 5000 && count >= 67) {
        uint32_t data = 0;
        for (int i = 0; i < 32; i++) {
            uint16_t space = timingsUs[2 + i * 2 + 1];
            if (space > 1200) {
                data |= (1UL << i);
            }
        }
        result.protocol = IrProtocol::NEC;
        result.address = data & 0xFFFF;
        result.command = (data >> 16) & 0xFF;
        return true;
    }

    // Check Sony (2.4ms mark, 600us space)
    if (timingsUs[0] >= 2000 && timingsUs[0] <= 2800 && timingsUs[1] >= 400 && timingsUs[1] <= 800) {
        result.protocol = IrProtocol::SONY;
        return true;
    }

    // Check Samsung (4.5ms mark, 4.5ms space)
    if (timingsUs[0] >= 4000 && timingsUs[0] <= 5000 && timingsUs[1] >= 4000 && timingsUs[1] <= 5000) {
        result.protocol = IrProtocol::SAMSUNG;
        return true;
    }

    return false;
}

void IrService::captureAndDecode(Print& out) {
    out.println("Awaiting IR signal on IO26 (3s timeout)...");
    uint16_t actual = 0;
    if (captureRaw(m_lastTimings, 128, actual, 3000)) {
        m_lastCount = actual;
        IrDecodedResult res;
        bool decoded = decodeCaptured(m_lastTimings, m_lastCount, res);

        out.printf("\r\nCaptured %u pulse transitions:\r\n", m_lastCount);
        if (decoded && res.protocol != IrProtocol::UNKNOWN) {
            out.printf("  Decoded Protocol: %s\r\n", irProtocolToString(res.protocol));
            out.printf("  Decoded Address : 0x%04X\r\n", (unsigned int)res.address);
            out.printf("  Decoded Command : 0x%02X\r\n", (unsigned int)res.command);
        } else {
            out.println("  Decoded Protocol: UNKNOWN (Displaying raw microsecond pulse train)");
        }

        out.print("  Raw timings (µs): ");
        for (uint16_t i = 0; i < m_lastCount && i < 32; ++i) {
            out.printf("%u ", m_lastTimings[i]);
        }
        if (m_lastCount > 32) out.print("...");
        out.println();
    } else {
        out.println("No IR signal received (timeout).");
    }
}

void IrService::executeCommand(const char* args, Print& out) {
    while (*args && isspace(*args)) args++;
    if (!*args || strcasecmp(args, "status") == 0) {
        printStatus(out);
        return;
    }

    char cmd[16] = {0};
    int n = 0;
    while (*args && !isspace(*args) && n < 15) {
        cmd[n++] = *args++;
    }
    while (*args && isspace(*args)) args++;

    if (strcasecmp(cmd, "rx") == 0 || strcasecmp(cmd, "capture") == 0) {
        captureAndDecode(out);
    } else if (strcasecmp(cmd, "nec") == 0) {
        uint32_t addr = 0x00FF, val = 0x12;
        if (sscanf(args, "%lx %lx", &addr, &val) >= 1) {
            sendNec((uint16_t)addr, val, &out);
        } else {
            out.println("Usage: ir nec <addr> <cmd>");
        }
    } else if (strcasecmp(cmd, "raw") == 0) {
        if (m_lastCount > 0) {
            out.printf("Re-transmitting last captured pulse train (%u pulses)...\r\n", m_lastCount);
            sendRaw(m_lastTimings, m_lastCount, m_carrierFreqHz);
        } else {
            out.println("No captured pulses in buffer. Run 'ir rx' first.");
        }
    } else {
        out.println("Available commands: ir status | ir rx | ir nec <addr> <cmd> | ir raw");
    }
}

} // namespace Esp32Ir
