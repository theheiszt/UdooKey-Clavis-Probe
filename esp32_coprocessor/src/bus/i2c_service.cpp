#include "i2c_service.h"
#include "../core/resource_manager.h"
#include <cstring>
#include <strings.h>

namespace Esp32Bus {

I2cService& I2cService::instance() {
    static I2cService s_instance;
    return s_instance;
}

bool I2cService::init(uint32_t speedHz, int8_t sdaPin, int8_t sclPin) {
    auto& rm = Esp32Core::ResourceManager::instance();
    auto err = rm.acquire(Esp32Core::Resource::I2C_BUS, Esp32Core::Owner::I2C_SERVICE);
    if (err != Esp32Core::ResourceError::OK && rm.getResourceOwner(Esp32Core::Resource::I2C_BUS) != Esp32Core::Owner::I2C_SERVICE) {
        return false;
    }

    m_speedHz = speedHz;
    m_sdaPin = sdaPin;
    m_sclPin = sclPin;

    rm.acquirePin(m_sdaPin, Esp32Core::Owner::I2C_SERVICE);
    rm.acquirePin(m_sclPin, Esp32Core::Owner::I2C_SERVICE);

    m_wire->begin(m_sdaPin, m_sclPin, m_speedHz);
    m_initialized = true;
    return true;
}

void I2cService::deinit() {
    if (m_initialized) {
        auto& rm = Esp32Core::ResourceManager::instance();
        rm.release(Esp32Core::Resource::I2C_BUS, Esp32Core::Owner::I2C_SERVICE);
        rm.releasePin(m_sdaPin, Esp32Core::Owner::I2C_SERVICE);
        rm.releasePin(m_sclPin, Esp32Core::Owner::I2C_SERVICE);
        m_initialized = false;
    }
}

void I2cService::printStatus(Print& out) {
    out.println("\r\n=== ESP32 I2C ENGINE STATUS ===");
    out.printf("  Hardware Port : Wire (SDA=IO%d, SCL=IO%d)\r\n", m_sdaPin, m_sclPin);
    out.printf("  Bus Clock     : %lu Hz (%.1f kHz)\r\n", (unsigned long)m_speedHz, m_speedHz / 1000.0f);
    out.printf("  Status        : %s\r\n", m_initialized ? "ACTIVE" : "IDLE");
    int sdaLevel = digitalRead(m_sdaPin);
    int sclLevel = digitalRead(m_sclPin);
    out.printf("  Bus Lines     : SDA=%s (%d), SCL=%s (%d)\r\n",
               sdaLevel ? "HIGH" : "LOW", sdaLevel,
               sclLevel ? "HIGH" : "LOW", sclLevel);
    if (!sdaLevel) {
        out.println("  WARNING: SDA line is held LOW! Bus recovery may be required.");
    }
    out.println("===============================");
}

bool I2cService::setSpeed(uint32_t speedHz, Print& out) {
    if (speedHz < 10000 || speedHz > 1000000) {
        out.println("Error: I2C clock out of range (10 kHz to 1 MHz).");
        return false;
    }
    m_speedHz = speedHz;
    if (m_initialized) {
        m_wire->setClock(m_speedHz);
    } else {
        init(m_speedHz, m_sdaPin, m_sclPin);
    }
    out.printf("I2C clock set to %lu Hz.\r\n", (unsigned long)m_speedHz);
    return true;
}

void I2cService::scanBus(Print& out) {
    if (!m_initialized) init();

    out.println("\r\nScanning I2C bus (0x01..0x7E)...");
    uint8_t count = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        m_wire->beginTransmission(addr);
        uint8_t err = m_wire->endTransmission();

        if (err == 0) {
            out.printf("  0x%02X (ACK)\r\n", addr);
            count++;
        } else if (err == 4) {
            out.printf("  0x%02X (BUS ERROR)\r\n", addr);
        }
    }

    out.printf("\r\nScan complete: %u device(s) found.\r\n", count);
}

bool I2cService::readData(uint8_t address, size_t count, Print& out) {
    if (!m_initialized) init();
    if (address > 127 || count == 0 || count > 128) {
        out.println("Error: Invalid address or count (1..128).");
        return false;
    }

    size_t received = m_wire->requestFrom(address, count);
    if (received == 0) {
        out.printf("Error: Device at 0x%02X NACKed read request.\r\n", address);
        return false;
    }

    out.printf("RX from 0x%02X (%u bytes): ", address, received);
    String ascii;
    while (m_wire->available()) {
        uint8_t b = (uint8_t)m_wire->read();
        out.printf("%02X ", b);
        ascii += (b >= 32 && b <= 126) ? (char)b : '.';
    }
    out.printf(" | %s\r\n", ascii.c_str());
    return true;
}

bool I2cService::writeData(uint8_t address, const char* hexStr, Print& out) {
    if (!m_initialized) init();
    if (address > 127) {
        out.println("Error: Invalid address.");
        return false;
    }

    size_t len = strlen(hexStr);
    uint8_t txBuf[128];
    size_t count = 0;

    for (size_t i = 0; i < len && count < sizeof(txBuf); ) {
        while (i < len && (isspace(hexStr[i]) || hexStr[i] == ',')) i++;
        if (i >= len) break;
        char hexByte[3] = {0};
        hexByte[0] = hexStr[i++];
        if (i < len && !isspace(hexStr[i]) && hexStr[i] != ',') {
            hexByte[1] = hexStr[i++];
        }
        txBuf[count++] = (uint8_t)strtoul(hexByte, nullptr, 16);
    }

    if (count == 0) {
        out.println("Error: No data bytes provided.");
        return false;
    }

    m_wire->beginTransmission(address);
    for (size_t i = 0; i < count; ++i) {
        m_wire->write(txBuf[i]);
    }
    uint8_t err = m_wire->endTransmission();

    if (err == 0) {
        out.printf("TX to 0x%02X (%u bytes): SUCCESS (ACK)\r\n", address, count);
        return true;
    } else {
        out.printf("TX to 0x%02X: FAILED (Code %u: %s)\r\n",
                   address, err,
                   (err == 2) ? "NACK on Address" : ((err == 3) ? "NACK on Data" : "Bus Error"));
        return false;
    }
}

bool I2cService::recoverBus(Print& out) {
    out.println("Attempting I2C bus recovery sequence (9 clock pulses)...");
    deinit();

    pinMode(m_sdaPin, INPUT_PULLUP);
    pinMode(m_sclPin, OUTPUT);

    for (int i = 0; i < 9; ++i) {
        digitalWrite(m_sclPin, LOW);
        delayMicroseconds(5);
        digitalWrite(m_sclPin, HIGH);
        delayMicroseconds(5);
    }

    // Generate STOP condition
    pinMode(m_sdaPin, OUTPUT);
    digitalWrite(m_sdaPin, LOW);
    delayMicroseconds(5);
    digitalWrite(m_sclPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(m_sdaPin, HIGH);
    delayMicroseconds(5);

    pinMode(m_sdaPin, INPUT_PULLUP);
    pinMode(m_sclPin, INPUT_PULLUP);

    bool sdaOk = digitalRead(m_sdaPin);
    bool sclOk = digitalRead(m_sclPin);

    init(m_speedHz, m_sdaPin, m_sclPin);

    if (sdaOk && sclOk) {
        out.println("Bus recovery SUCCESSFUL: Both SDA and SCL are HIGH.");
        return true;
    } else {
        out.printf("Bus recovery INCOMPLETE: SDA=%d, SCL=%d\r\n", sdaOk, sclOk);
        return false;
    }
}

void I2cService::executeCommand(const char* args, Print& out) {
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

    if (strcasecmp(cmd, "scan") == 0) {
        scanBus(out);
    } else if (strcasecmp(cmd, "speed") == 0) {
        uint32_t spd = strtoul(args, nullptr, 10);
        if (spd > 0) setSpeed(spd, out);
        else out.println("Usage: i2c speed <hz>");
    } else if (strcasecmp(cmd, "recover") == 0) {
        recoverBus(out);
    } else if (strcasecmp(cmd, "read") == 0) {
        uint32_t addr = 0, count = 1;
        if (sscanf(args, "%lx %lu", &addr, &count) >= 1) {
            readData((uint8_t)addr, count, out);
        } else {
            out.println("Usage: i2c read <address> [count]");
        }
    } else if (strcasecmp(cmd, "write") == 0) {
        uint32_t addr = 0;
        char hexData[128] = {0};
        if (sscanf(args, "%lx %127[^\r\n]", &addr, hexData) == 2) {
            writeData((uint8_t)addr, hexData, out);
        } else {
            out.println("Usage: i2c write <address> <hex_bytes>");
        }
    } else {
        out.println("Available commands: i2c status | i2c scan | i2c speed <hz> | i2c read <addr> <n> | i2c write <addr> <hex> | i2c recover");
    }
}

} // namespace Esp32Bus
