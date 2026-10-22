#include "uart_service.h"
#include "../core/resource_manager.h"
#include <cstring>
#include <strings.h>

namespace Esp32Bus {

UartService& UartService::instance() {
    static UartService s_instance;
    return s_instance;
}

bool UartService::init(uint32_t baud, uint32_t config, int8_t rxPin, int8_t txPin) {
    auto& rm = Esp32Core::ResourceManager::instance();
    auto err = rm.acquire(Esp32Core::Resource::UART_USER, Esp32Core::Owner::UART_SERVICE);
    if (err != Esp32Core::ResourceError::OK && rm.getResourceOwner(Esp32Core::Resource::UART_USER) != Esp32Core::Owner::UART_SERVICE) {
        return false;
    }

    if (rxPin >= 0) rm.acquirePin(rxPin, Esp32Core::Owner::UART_SERVICE);
    if (txPin >= 0) rm.acquirePin(txPin, Esp32Core::Owner::UART_SERVICE);

    m_baud = baud;
    m_config = config;
    m_rxPin = rxPin;
    m_txPin = txPin;

    m_serial->begin(m_baud, m_config, m_rxPin, m_txPin);
    m_initialized = true;
    return true;
}

void UartService::deinit() {
    if (m_initialized) {
        m_serial->end();
        auto& rm = Esp32Core::ResourceManager::instance();
        rm.release(Esp32Core::Resource::UART_USER, Esp32Core::Owner::UART_SERVICE);
        if (m_rxPin >= 0) rm.releasePin(m_rxPin, Esp32Core::Owner::UART_SERVICE);
        if (m_txPin >= 0) rm.releasePin(m_txPin, Esp32Core::Owner::UART_SERVICE);
        m_initialized = false;
        m_monitoring = false;
    }
}

void UartService::printStatus(Print& out) {
    out.println("\r\n=== ESP32 USER UART STATUS ===");
    out.printf("  Hardware Port : Serial1 (IO%d RX, IO%d TX)\r\n", m_rxPin, m_txPin);
    out.printf("  Initialized   : %s\r\n", m_initialized ? "ACTIVE" : "IDLE");
    out.printf("  Baud Rate     : %lu bps\r\n", (unsigned long)m_baud);
    out.printf("  Framing       : %s\r\n", m_configStr);
    out.printf("  Monitoring    : %s\r\n", m_monitoring ? "ACTIVE" : "STOPPED");
    out.printf("  Buffered RX   : %d bytes\r\n", m_initialized ? m_serial->available() : 0);
    out.println("==============================");
}

bool UartService::setBaud(uint32_t baud, Print& out) {
    if (baud < 300 || baud > 2000000) {
        out.println("Error: Baud rate out of range (300 to 2000000).");
        return false;
    }
    m_baud = baud;
    if (!m_initialized) init(m_baud, m_config, m_rxPin, m_txPin);
    else m_serial->updateBaudRate(m_baud);
    out.printf("UART baud set to %lu bps.\r\n", (unsigned long)m_baud);
    return true;
}

bool UartService::setConfig(const char* cfgStr, Print& out) {
    if (strcasecmp(cfgStr, "8N1") == 0) {
        m_config = SERIAL_8N1;
        strncpy(m_configStr, "8N1", sizeof(m_configStr));
    } else if (strcasecmp(cfgStr, "8E1") == 0) {
        m_config = SERIAL_8E1;
        strncpy(m_configStr, "8E1", sizeof(m_configStr));
    } else if (strcasecmp(cfgStr, "8O1") == 0) {
        m_config = SERIAL_8O1;
        strncpy(m_configStr, "8O1", sizeof(m_configStr));
    } else if (strcasecmp(cfgStr, "7E1") == 0) {
        m_config = SERIAL_7E1;
        strncpy(m_configStr, "7E1", sizeof(m_configStr));
    } else if (strcasecmp(cfgStr, "7O1") == 0) {
        m_config = SERIAL_7O1;
        strncpy(m_configStr, "7O1", sizeof(m_configStr));
    } else {
        out.println("Error: Unsupported config. Supported: 8N1, 8E1, 8O1, 7E1, 7O1");
        return false;
    }

    if (m_initialized) {
        m_serial->end();
        m_serial->begin(m_baud, m_config, m_rxPin, m_txPin);
    } else {
        init(m_baud, m_config, m_rxPin, m_txPin);
    }
    out.printf("UART config set to %s.\r\n", m_configStr);
    return true;
}

void UartService::readData(Print& out) {
    if (!m_initialized) init();
    int avail = m_serial->available();
    if (avail == 0) {
        out.println("No buffered bytes available.");
        return;
    }
    out.printf("Read %d bytes:\r\n  HEX  : ", avail);
    String ascii;
    while (m_serial->available()) {
        uint8_t b = (uint8_t)m_serial->read();
        out.printf("%02X ", b);
        ascii += (b >= 32 && b <= 126) ? (char)b : '.';
    }
    out.println();
    out.printf("  ASCII: %s\r\n", ascii.c_str());
}

bool UartService::writeData(const char* dataStr, bool isHex, Print& out) {
    if (!m_initialized) init();
    if (isHex) {
        size_t len = strlen(dataStr);
        size_t count = 0;
        out.print("TX HEX: ");
        for (size_t i = 0; i < len; ) {
            while (i < len && (isspace(dataStr[i]) || dataStr[i] == ',')) i++;
            if (i >= len) break;
            char hexByte[3] = {0};
            hexByte[0] = dataStr[i++];
            if (i < len && !isspace(dataStr[i]) && dataStr[i] != ',') {
                hexByte[1] = dataStr[i++];
            }
            uint8_t val = (uint8_t)strtoul(hexByte, nullptr, 16);
            m_serial->write(val);
            out.printf("%02X ", val);
            count++;
        }
        out.printf("(%u bytes)\r\n", count);
    } else {
        size_t count = m_serial->print(dataStr);
        out.printf("TX ASCII: %s (%u bytes)\r\n", dataStr, count);
    }
    return true;
}

void UartService::startMonitor(Print& out) {
    if (!m_initialized) init();
    m_monitoring = true;
    out.println("UART continuous monitoring started. Type 'uart stop' to exit.");
}

void UartService::stopMonitor(Print& out) {
    m_monitoring = false;
    out.println("UART monitoring stopped.");
}

void UartService::pollMonitor(Print& out) {
    if (!m_monitoring || !m_initialized) return;
    if (m_serial->available()) {
        uint32_t ms = millis();
        out.printf("[%08lu] RX: ", ms);
        String ascii;
        while (m_serial->available()) {
            uint8_t b = (uint8_t)m_serial->read();
            out.printf("%02X ", b);
            ascii += (b >= 32 && b <= 126) ? (char)b : '.';
        }
        out.printf(" | %s\r\n", ascii.c_str());
    }
}

void UartService::executeCommand(const char* args, Print& out) {
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

    if (strcasecmp(cmd, "baud") == 0) {
        uint32_t b = strtoul(args, nullptr, 10);
        if (b > 0) setBaud(b, out);
        else out.println("Usage: uart baud <rate>");
    } else if (strcasecmp(cmd, "config") == 0) {
        setConfig(args, out);
    } else if (strcasecmp(cmd, "read") == 0) {
        readData(out);
    } else if (strcasecmp(cmd, "write") == 0) {
        writeData(args, true, out);
    } else if (strcasecmp(cmd, "text") == 0) {
        writeData(args, false, out);
    } else if (strcasecmp(cmd, "monitor") == 0) {
        startMonitor(out);
    } else if (strcasecmp(cmd, "stop") == 0) {
        stopMonitor(out);
    } else {
        out.println("Available commands: uart status | uart baud <n> | uart config <8N1> | uart read | uart write <hex> | uart text <str> | uart monitor | uart stop");
    }
}

} // namespace Esp32Bus
