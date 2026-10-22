#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <vector>

namespace Esp32Wireless {

struct BleDeviceRecord {
    char mac[18];
    char name[32];
    int8_t rssi;
    uint8_t addrType;
    uint16_t companyId;
    uint8_t rawPayload[62];
    uint8_t rawPayloadLen;
};

class BleService {
public:
    static BleService& instance();

    bool init();

    void printStatus(Print& out);
    void startScan(uint32_t durationSec = 10, bool continuous = false, Print* out = nullptr);
    void stopScan(Print* out = nullptr);
    bool isScanning() const;

    void listDevices(Print& out);
    void showDeviceDetails(const char* macStr, Print& out);

    // Bluetooth Classic Inquiry Check
    void printBtClassicStatus(Print& out);

    void processScanResults();

    void executeCommand(const char* args, Print& out);
    void executeBtClassicCommand(const char* args, Print& out);

private:
    BleService();

    bool m_initialized{false};
    NimBLEScan* m_scan{nullptr};
    std::vector<BleDeviceRecord> m_devices;
};

} // namespace Esp32Wireless
