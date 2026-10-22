#include "diagnostics.h"
#include "resource_manager.h"
#include "instrument_protocol.h"
#include <WiFi.h>
#include "../wireless/wifi_service.h"
#include <NimBLEDevice.h>
#include <esp_spi_flash.h>
#include <esp_system.h>

namespace Esp32Core {

const char* diagStatusToString(DiagnosticStatus st) {
    switch (st) {
        case DiagnosticStatus::PASS:        return "PASS";
        case DiagnosticStatus::FAIL:        return "FAIL";
        case DiagnosticStatus::SKIP:        return "SKIP";
        case DiagnosticStatus::NOT_PRESENT: return "NOT_PRESENT";
        case DiagnosticStatus::NOT_TESTED:  return "NOT_TESTED";
        default:                            return "UNKNOWN";
    }
}

void Diagnostics::dumpSystemInfo(Print& out) {
    out.println("\r\n==================================================");
    out.println("  UDOO KEY — ESP32 COMMUNICATIONS COPROCESSOR INFO");
    out.println("==================================================");
    out.printf("  Hardware Model : ESP32-WROVER-E (ESP32-D0WD-V3)\r\n");
    out.printf("  CPU Clock      : %u MHz (Dual Xtensa LX6)\r\n", ESP.getCpuFreqMHz());
    out.printf("  Silicon Rev    : %u\r\n", ESP.getChipRevision());
    out.printf("  Flash Memory   : %u MB (Mode: %u, Speed: %u MHz)\r\n",
               ESP.getFlashChipSize() / (1024 * 1024),
               ESP.getFlashChipMode(),
               ESP.getFlashChipSpeed() / 1000000);
    out.printf("  Internal SRAM  : %u KiB free / %u KiB total\r\n",
               ESP.getFreeHeap() / 1024,
               ESP.getHeapSize() / 1024);
    if (psramFound()) {
        out.printf("  External PSRAM : %u KiB free / %u KiB total (QSPI)\r\n",
                   ESP.getFreePsram() / 1024,
                   ESP.getPsramSize() / 1024);
    } else {
        out.println("  External PSRAM : NOT DETECTED");
    }
    out.printf("  Wi-Fi MAC      : %s\r\n", WiFi.macAddress().c_str());
    out.printf("  SoftAP IP      : %s\r\n", WiFi.softAPIP().toString().c_str());
    out.printf("  Factory UART   : IO22 RX / IO19 TX (1,000,000 baud 8N1)\r\n");
    out.println("==================================================");
}

void Diagnostics::runSelfTest(Print& out, Stream* rp2040Stream) {
    out.println("\r\n==================================================");
    out.println("  ESP32 COPROCESSOR HARDWARE DIAGNOSTIC SELF-TEST ");
    out.println("==================================================");

    int totalPassed = 0;
    int totalTests = 0;

    auto report = [&](const char* name, DiagnosticStatus st, const char* note) {
        totalTests++;
        if (st == DiagnosticStatus::PASS) totalPassed++;
        out.printf("  [%-11s] %-22s : %s\r\n", diagStatusToString(st), name, note);
    };

    // 1. CPU & Monotonic Clock
    uint64_t t1 = esp_timer_get_time();
    delayMicroseconds(100);
    uint64_t t2 = esp_timer_get_time();
    if (t2 > t1 && (t2 - t1) >= 90) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Clock tick %llu us (OK)", t2 - t1);
        report("CPU Monotonic Timer", DiagnosticStatus::PASS, buf);
    } else {
        report("CPU Monotonic Timer", DiagnosticStatus::FAIL, "Timer delta invalid");
    }

    // 2. Flash & Partition Table
    uint32_t flashSize = ESP.getFlashChipSize();
    if (flashSize >= 4 * 1024 * 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u MB Flash OK", flashSize / (1024 * 1024));
        report("SPI Flash Memory", DiagnosticStatus::PASS, buf);
    } else {
        report("SPI Flash Memory", DiagnosticStatus::FAIL, "Flash size smaller than 4MB");
    }

    // 3. PSRAM Verification
    if (psramFound() && ESP.getPsramSize() > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u KB QSPI PSRAM OK", ESP.getPsramSize() / 1024);
        report("External PSRAM", DiagnosticStatus::PASS, buf);
    } else {
        report("External PSRAM", DiagnosticStatus::NOT_PRESENT, "PSRAM disabled or absent");
    }

    // 4. Resource Manager Invariants
    bool rmOk = true;
    for (int p = 6; p <= 11; ++p) {
        if (!ResourceManager::instance().isPinProtected(p)) rmOk = false;
    }
    if (!ResourceManager::instance().isPinProtected(19) || !ResourceManager::instance().isPinProtected(22)) rmOk = false;
    if (rmOk) {
        report("Resource Manager", DiagnosticStatus::PASS, "Protected flash/UART pins locked");
    } else {
        report("Resource Manager", DiagnosticStatus::FAIL, "Lock table violation detected");
    }

    // 5. CRC16 Protocol Algorithm
    const uint8_t testData[] = "123456789";
    uint16_t crc = instrument_crc16(testData, 9);
    if (crc == 0x29B1) {
        report("Protocol CRC-16", DiagnosticStatus::PASS, "CCITT-FALSE vector matched 0x29B1");
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "Expected 0x29B1, got 0x%04X", crc);
        report("Protocol CRC-16", DiagnosticStatus::FAIL, buf);
    }

    // 6. Wi-Fi Stack Initialization
    if (Esp32Wireless::WifiService::instance().apActive()) {
        char buf[48];
        snprintf(buf, sizeof(buf), "SoftAP active (%s)", Esp32Wireless::WifiConfig::AP_SSID);
        report("Wi-Fi Subsystem", DiagnosticStatus::PASS, buf);
    } else {
        report("Wi-Fi Subsystem", DiagnosticStatus::FAIL, "Wi-Fi radio offline or AP failed");
    }

    // 7. BLE Stack Initialization
    if (NimBLEDevice::getInitialized()) {
        report("BLE Controller", DiagnosticStatus::PASS, "NimBLE controller initialized");
    } else {
        report("BLE Controller", DiagnosticStatus::FAIL, "NimBLE controller uninitialized");
    }

    // 8. nRF24L01 Physical SPI Link Check
    // Attempt register read 0x05 (RF_CH) on UEXT
    pinMode(15, OUTPUT); // CSN
    digitalWrite(15, HIGH);
    pinMode(14, OUTPUT); // SCK
    digitalWrite(14, LOW);
    pinMode(12, OUTPUT); // MOSI
    pinMode(35, INPUT);  // MISO

    digitalWrite(15, LOW);
    // Send 0x05 (R_REGISTER | 0x05)
    for (int i = 7; i >= 0; i--) {
        digitalWrite(12, (0x05 >> i) & 1);
        digitalWrite(14, HIGH);
        delayMicroseconds(2);
        digitalWrite(14, LOW);
        delayMicroseconds(2);
    }
    uint8_t val = 0;
    for (int i = 7; i >= 0; i--) {
        digitalWrite(14, HIGH);
        delayMicroseconds(2);
        val = (val << 1) | digitalRead(35);
        digitalWrite(14, LOW);
        delayMicroseconds(2);
    }
    digitalWrite(15, HIGH);

    if (val != 0x00 && val != 0xFF) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Detected radio on UEXT (reg=0x%02X)", val);
        report("nRF24 Transceiver", DiagnosticStatus::PASS, buf);
    } else {
        report("nRF24 Transceiver", DiagnosticStatus::NOT_PRESENT, "Module not fitted to UEXT P3");
    }

    // 9. RP2040 Inter-MCU Link
    if (rp2040Stream) {
        while (rp2040Stream->available()) rp2040Stream->read();
        InstrumentPacketHeader pingHdr;
        pingHdr.magic = INSTRUMENT_PROTO_MAGIC;
        pingHdr.version = INSTRUMENT_PROTO_VERSION;
        pingHdr.type = PKT_TYPE_PING;
        pingHdr.sequence = 999;
        pingHdr.payload_len = 0;
        pingHdr.crc16 = 0;
        rp2040Stream->write((const uint8_t*)&pingHdr, sizeof(pingHdr));
        rp2040Stream->flush();

        uint32_t tStart = millis();
        bool gotPong = false;
        while (millis() - tStart < 150) {
            if (rp2040Stream->available() >= (int)sizeof(InstrumentPacketHeader)) {
                InstrumentPacketHeader rxHdr;
                if (rp2040Stream->readBytes((char*)&rxHdr, sizeof(rxHdr)) == sizeof(rxHdr)) {
                    if (rxHdr.magic == INSTRUMENT_PROTO_MAGIC && rxHdr.type == PKT_TYPE_PONG) {
                        gotPong = true;
                        break;
                    }
                }
            }
            delay(2);
        }

        if (gotPong) {
            report("RP2040 Link", DiagnosticStatus::PASS, "1 Mbps UART Ping/Pong verified");
        } else {
            report("RP2040 Link", DiagnosticStatus::FAIL, "Ping timeout over IO22/IO19");
        }
    } else {
        report("RP2040 Link", DiagnosticStatus::NOT_TESTED, "No stream reference provided");
    }

    out.println("--------------------------------------------------");
    out.printf("  Summary: %d / %d operational checks passed\r\n", totalPassed, totalTests);
    out.println("==================================================");
}

} // namespace Esp32Core
