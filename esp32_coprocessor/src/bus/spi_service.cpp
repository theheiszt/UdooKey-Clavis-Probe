#include "spi_service.h"
#include "../core/resource_manager.h"
#include <cstring>
#include <strings.h>

namespace Esp32Bus {

SpiService& SpiService::instance() {
    static SpiService s_instance;
    return s_instance;
}

bool SpiService::init(uint32_t speedHz, uint8_t mode, int8_t csPin) {
    auto& rm = Esp32Core::ResourceManager::instance();
    auto err = rm.acquire(Esp32Core::Resource::SPI_BUS, Esp32Core::Owner::SPI_SERVICE);
    if (err != Esp32Core::ResourceError::OK && rm.getResourceOwner(Esp32Core::Resource::SPI_BUS) != Esp32Core::Owner::SPI_SERVICE) {
        return false;
    }

    m_speedHz = speedHz;
    m_mode = (mode <= 3) ? mode : 0;
    m_csPin = csPin;

    if (m_csPin >= 0) {
        rm.acquirePin(m_csPin, Esp32Core::Owner::SPI_SERVICE);
        pinMode(m_csPin, OUTPUT);
        digitalWrite(m_csPin, HIGH);
    }

    m_spi->begin(m_sckPin, m_misoPin, m_mosiPin, m_csPin);
    m_initialized = true;
    return true;
}

void SpiService::deinit() {
    if (m_initialized) {
        m_spi->end();
        auto& rm = Esp32Core::ResourceManager::instance();
        rm.release(Esp32Core::Resource::SPI_BUS, Esp32Core::Owner::SPI_SERVICE);
        if (m_csPin >= 0) {
            rm.releasePin(m_csPin, Esp32Core::Owner::SPI_SERVICE);
            pinMode(m_csPin, INPUT);
        }
        m_initialized = false;
    }
}

void SpiService::printStatus(Print& out) {
    out.println("\r\n=== ESP32 GENERIC SPI STATUS ===");
    out.printf("  Hardware Port : HSPI/VSPI (SCK=IO%d, MOSI=IO%d, MISO=IO%d)\r\n", m_sckPin, m_mosiPin, m_misoPin);
    out.printf("  Chip Select   : IO%d (Active LOW)\r\n", m_csPin);
    out.printf("  Bus Clock     : %lu Hz (%.2f MHz)\r\n", (unsigned long)m_speedHz, m_speedHz / 1000000.0f);
    out.printf("  SPI Mode      : Mode %u (CPOL=%u, CPHA=%u)\r\n", m_mode, (m_mode >> 1) & 1, m_mode & 1);
    out.printf("  Status        : %s\r\n", m_initialized ? "ACTIVE" : "IDLE");
    out.println("================================");
}

bool SpiService::setSpeed(uint32_t speedHz, Print& out) {
    if (speedHz < 1000 || speedHz > 40000000) {
        out.println("Error: Speed out of range (1 kHz to 40 MHz).");
        return false;
    }
    m_speedHz = speedHz;
    out.printf("SPI clock set to %lu Hz.\r\n", (unsigned long)m_speedHz);
    return true;
}

bool SpiService::setMode(uint8_t mode, Print& out) {
    if (mode > 3) {
        out.println("Error: Invalid SPI mode. Must be 0, 1, 2, or 3.");
        return false;
    }
    m_mode = mode;
    out.printf("SPI mode set to Mode %u.\r\n", m_mode);
    return true;
}

bool SpiService::setCsPin(int8_t csPin, Print& out) {
    auto& rm = Esp32Core::ResourceManager::instance();
    if (csPin >= 0 && (rm.isPinProtected(csPin) || rm.isPinInputOnly(csPin))) {
        out.printf("Error: Pin %d cannot be used as CS output.\r\n", csPin);
        return false;
    }
    if (m_csPin >= 0) {
        rm.releasePin(m_csPin, Esp32Core::Owner::SPI_SERVICE);
    }
    m_csPin = csPin;
    if (m_csPin >= 0) {
        rm.acquirePin(m_csPin, Esp32Core::Owner::SPI_SERVICE);
        pinMode(m_csPin, OUTPUT);
        digitalWrite(m_csPin, HIGH);
    }
    out.printf("SPI CS pin set to IO%d.\r\n", m_csPin);
    return true;
}

void SpiService::readBytes(size_t count, Print& out) {
    if (!m_initialized) init();
    if (count == 0 || count > 512) {
        out.println("Error: Read count must be between 1 and 512.");
        return;
    }

    uint8_t spiModeVal = (m_mode == 0) ? SPI_MODE0 : ((m_mode == 1) ? SPI_MODE1 : ((m_mode == 2) ? SPI_MODE2 : SPI_MODE3));
    m_spi->beginTransaction(SPISettings(m_speedHz, MSBFIRST, spiModeVal));
    if (m_csPin >= 0) digitalWrite(m_csPin, LOW);

    out.printf("RX (%u bytes): ", count);
    for (size_t i = 0; i < count; ++i) {
        uint8_t b = m_spi->transfer(0xFF);
        out.printf("%02X ", b);
    }
    out.println();

    if (m_csPin >= 0) digitalWrite(m_csPin, HIGH);
    m_spi->endTransaction();
}

void SpiService::writeBytes(const char* hexStr, Print& out) {
    if (!m_initialized) init();
    size_t len = strlen(hexStr);
    uint8_t txBuf[256];
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
        out.println("Error: No hex bytes specified to write.");
        return;
    }

    uint8_t spiModeVal = (m_mode == 0) ? SPI_MODE0 : ((m_mode == 1) ? SPI_MODE1 : ((m_mode == 2) ? SPI_MODE2 : SPI_MODE3));
    m_spi->beginTransaction(SPISettings(m_speedHz, MSBFIRST, spiModeVal));
    if (m_csPin >= 0) digitalWrite(m_csPin, LOW);

    out.print("TX: ");
    for (size_t i = 0; i < count; ++i) {
        m_spi->transfer(txBuf[i]);
        out.printf("%02X ", txBuf[i]);
    }
    out.printf("(%u bytes written)\r\n", count);

    if (m_csPin >= 0) digitalWrite(m_csPin, HIGH);
    m_spi->endTransaction();
}

void SpiService::transferBytes(const char* hexStr, Print& out) {
    if (!m_initialized) init();
    size_t len = strlen(hexStr);
    uint8_t txBuf[256];
    uint8_t rxBuf[256];
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
        out.println("Error: No hex bytes specified for transfer.");
        return;
    }

    uint8_t spiModeVal = (m_mode == 0) ? SPI_MODE0 : ((m_mode == 1) ? SPI_MODE1 : ((m_mode == 2) ? SPI_MODE2 : SPI_MODE3));
    m_spi->beginTransaction(SPISettings(m_speedHz, MSBFIRST, spiModeVal));
    if (m_csPin >= 0) digitalWrite(m_csPin, LOW);

    for (size_t i = 0; i < count; ++i) {
        rxBuf[i] = m_spi->transfer(txBuf[i]);
    }

    if (m_csPin >= 0) digitalWrite(m_csPin, HIGH);
    m_spi->endTransaction();

    out.print("TX: ");
    for (size_t i = 0; i < count; ++i) out.printf("%02X ", txBuf[i]);
    out.println();

    out.print("RX: ");
    for (size_t i = 0; i < count; ++i) out.printf("%02X ", rxBuf[i]);
    out.println();
}

void SpiService::executeCommand(const char* args, Print& out) {
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

    if (strcasecmp(cmd, "speed") == 0) {
        uint32_t spd = strtoul(args, nullptr, 10);
        if (spd > 0) setSpeed(spd, out);
        else out.println("Usage: spi speed <hz>");
    } else if (strcasecmp(cmd, "mode") == 0) {
        uint8_t m = atoi(args);
        setMode(m, out);
    } else if (strcasecmp(cmd, "cs") == 0) {
        int8_t cs = atoi(args);
        setCsPin(cs, out);
    } else if (strcasecmp(cmd, "read") == 0) {
        size_t c = strtoul(args, nullptr, 10);
        readBytes(c ? c : 4, out);
    } else if (strcasecmp(cmd, "write") == 0) {
        writeBytes(args, out);
    } else if (strcasecmp(cmd, "xfer") == 0) {
        transferBytes(args, out);
    } else {
        out.println("Available commands: spi status | spi speed <hz> | spi mode <0-3> | spi cs <pin> | spi read <n> | spi write <hex> | spi xfer <hex>");
    }
}

} // namespace Esp32Bus
