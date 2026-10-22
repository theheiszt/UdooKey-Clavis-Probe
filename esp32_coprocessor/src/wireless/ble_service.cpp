#include "ble_service.h"
#include "../core/resource_manager.h"
#include "../packet/pcap_service.h"
#include <cstring>
#include <strings.h>

namespace Esp32Wireless {

class BleScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        // Raw advertisement received
        const uint8_t* payload = advertisedDevice->getPayload();
        size_t payloadLen = advertisedDevice->getPayloadLength();

        if (payload && payloadLen > 0) {
            Esp32Packet::PacketRecord rec;
            rec.timestamp_us = esp_timer_get_time();
            rec.source = static_cast<uint8_t>(Esp32Packet::CaptureSource::BLE_ADV);
            rec.channel = 37; // Standard primary advertisement channel
            rec.rssi = advertisedDevice->getRSSI();
            rec.captured_length = (payloadLen > sizeof(rec.data)) ? sizeof(rec.data) : payloadLen;
            rec.original_length = payloadLen;
            rec.link_type = Esp32Packet::LINKTYPE_BLUETOOTH_LE_LL;
            memcpy(rec.data, payload, rec.captured_length);

            Esp32Packet::PcapService::instance().pushPacket(rec);
        }
    }
};

static BleScanCallbacks s_scanCallbacks;

BleService& BleService::instance() {
    static BleService s_instance;
    return s_instance;
}

BleService::BleService() = default;

bool BleService::init() {
    if (m_initialized) return true;
    auto& rm = Esp32Core::ResourceManager::instance();
    rm.acquire(Esp32Core::Resource::BLE_RADIO, Esp32Core::Owner::BLE_SERVICE);

    NimBLEDevice::init("UDOO-KEY-ANALYZER");
    m_scan = NimBLEDevice::getScan();
    m_scan->setAdvertisedDeviceCallbacks(&s_scanCallbacks);
    m_scan->setActiveScan(true);
    m_scan->setInterval(100);
    m_scan->setWindow(99);
    m_initialized = true;
    return true;
}

void BleService::printStatus(Print& out) {
    out.println("\r\n=== BLUETOOTH LOW ENERGY (BLE) STATUS ===");
    out.printf("  Stack Driver  : NimBLE (ESP32-D0WD-V3)\r\n");
    out.printf("  Initialized   : %s\r\n", m_initialized ? "ACTIVE" : "IDLE");
    out.printf("  Scan State    : %s\r\n", isScanning() ? "SCANNING" : "IDLE");
    out.printf("  Cached Devices: %u\r\n", (unsigned int)m_devices.size());
    out.println("=========================================");
}

bool BleService::isScanning() const {
    return m_scan && m_scan->isScanning();
}

void BleService::startScan(uint32_t durationSec, bool continuous, Print* out) {
    if (!m_initialized) init();
    m_devices.clear();
    m_scan->start(continuous ? 0 : durationSec, false);
    if (out) {
        out->printf("Bluetooth LE scan started (%lu seconds)...\r\n", (unsigned long)durationSec);
    }
}

void BleService::stopScan(Print* out) {
    if (m_scan && m_scan->isScanning()) {
        m_scan->stop();
        if (out) out->println("Bluetooth LE scan stopped.");
    }
}

void BleService::listDevices(Print& out) {
    if (!m_scan) {
        out.println("BLE scan not initialized. Run 'ble scan' first.");
        return;
    }

    NimBLEScanResults results = m_scan->getResults();
    out.printf("\r\nDiscovered %d Bluetooth LE device(s):\r\n", results.getCount());
    out.println(" Address            Type    RSSI  CompID  Name");
    out.println(" ─────────────────  ──────  ────  ──────  ────────────────────────");

    m_devices.clear();
    for (int i = 0; i < results.getCount(); ++i) {
        NimBLEAdvertisedDevice dev = results.getDevice(i);

        BleDeviceRecord rec;
        memset(&rec, 0, sizeof(rec));
        strncpy(rec.mac, dev.getAddress().toString().c_str(), sizeof(rec.mac) - 1);
        strncpy(rec.name, dev.getName().c_str(), sizeof(rec.name) - 1);
        rec.rssi = dev.getRSSI();
        rec.addrType = dev.getAddressType();

        const uint8_t* p = dev.getPayload();
        size_t plen = dev.getPayloadLength();
        rec.rawPayloadLen = (plen > sizeof(rec.rawPayload)) ? sizeof(rec.rawPayload) : plen;
        if (p && rec.rawPayloadLen > 0) {
            memcpy(rec.rawPayload, p, rec.rawPayloadLen);
        }

        // Find company ID if present
        if (dev.haveManufacturerData()) {
            std::string mdata = dev.getManufacturerData();
            if (mdata.length() >= 2) {
                rec.companyId = (uint8_t)mdata[0] | ((uint8_t)mdata[1] << 8);
            }
        }

        m_devices.push_back(rec);

        out.printf(" %-17s  %-6s  %-4d  0x%04X  %s\r\n",
                   rec.mac,
                   (rec.addrType == BLE_ADDR_PUBLIC) ? "PUBLIC" : "RANDOM",
                   rec.rssi,
                   rec.companyId,
                   rec.name[0] ? rec.name : "<Unknown>");
    }
    out.println("==================================================================");
}

void BleService::showDeviceDetails(const char* macStr, Print& out) {
    if (m_devices.empty() && m_scan) {
        listDevices(out);
    }

    const BleDeviceRecord* target = nullptr;
    for (const auto& dev : m_devices) {
        if (strcasecmp(dev.mac, macStr) == 0) {
            target = &dev;
            break;
        }
    }

    if (!target) {
        out.printf("Device with MAC '%s' not found in scan cache.\r\n", macStr);
        return;
    }

    out.printf("\r\n=== BLE ADVERTISEMENT DETAILS: %s ===\r\n", target->mac);
    out.printf("  Name       : %s\r\n", target->name[0] ? target->name : "<None>");
    out.printf("  Address Type: %s\r\n", (target->addrType == BLE_ADDR_PUBLIC) ? "PUBLIC" : "RANDOM");
    out.printf("  RSSI Level : %d dBm\r\n", target->rssi);
    out.println("\r\nDecoded Advertising Data (AD) Structures:");

    // Decode TLV (Length - Type - Value) structures
    const uint8_t* p = target->rawPayload;
    size_t len = target->rawPayloadLen;
    size_t offset = 0;

    while (offset < len) {
        uint8_t adLen = p[offset++];
        if (adLen == 0 || offset + adLen > len) break;

        uint8_t adType = p[offset++];
        uint8_t valLen = adLen - 1;
        const uint8_t* val = &p[offset];
        offset += valLen;

        switch (adType) {
            case 0x01: // Flags
                out.printf("  [0x01] Flags: 0x%02X\r\n", val[0]);
                break;
            case 0x02: // Incomplete 16-bit UUID
            case 0x03: // Complete 16-bit UUID
                out.print("  [0x03] 16-bit Service UUIDs: ");
                for (size_t u = 0; u + 1 < valLen; u += 2) {
                    out.printf("0x%04X ", val[u] | (val[u+1] << 8));
                }
                out.println();
                break;
            case 0x08: // Shortened Name
            case 0x09: // Complete Name
                out.printf("  [0x09] Local Name: \"%.*s\"\r\n", valLen, (const char*)val);
                break;
            case 0x0A: // TX Power
                out.printf("  [0x0A] TX Power Level: %d dBm\r\n", (int8_t)val[0]);
                break;
            case 0x19: // Appearance
                if (valLen >= 2) {
                    out.printf("  [0x19] Appearance: 0x%04X\r\n", val[0] | (val[1] << 8));
                }
                break;
            case 0xFF: // Manufacturer Specific Data
                if (valLen >= 2) {
                    uint16_t cid = val[0] | (val[1] << 8);
                    out.printf("  [0xFF] Manufacturer Data (Company ID: 0x%04X):\r\n    HEX: ", cid);
                    for (size_t m = 2; m < valLen; ++m) {
                        out.printf("%02X ", val[m]);
                    }
                    out.println();
                }
                break;
            default:
                out.printf("  [0x%02X] Type (len=%u): ", adType, valLen);
                for (size_t d = 0; d < valLen; ++d) out.printf("%02X ", val[d]);
                out.println();
                break;
        }
    }
    out.println("=================================================");
}

void BleService::printBtClassicStatus(Print& out) {
    out.println("\r\n=== BLUETOOTH CLASSIC (BR/EDR) STATUS ===");
    out.println("  Status      : UNAVAILABLE");
    out.println("  Hardware    : Supported by ESP32 Silicon");
    out.println("  Stack Config: NimBLE Single-Mode (BLE Only)");
    out.println("  Explanation : Firmware configured with NimBLE stack to optimize internal");
    out.println("                SRAM and preserve maximum resources for 100 MSPS capture buffers.");
    out.println("                Dual-mode Bluedroid is disabled in this profile.");
    out.println("=========================================");
}

void BleService::executeCommand(const char* args, Print& out) {
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
        if (strcasecmp(args, "start") == 0) {
            startScan(0, true, &out);
        } else if (strcasecmp(args, "stop") == 0) {
            stopScan(&out);
        } else {
            uint32_t sec = atoi(args);
            startScan(sec ? sec : 10, false, &out);
        }
    } else if (strcasecmp(cmd, "devices") == 0) {
        listDevices(out);
    } else if (strcasecmp(cmd, "show") == 0) {
        showDeviceDetails(args, out);
    } else {
        out.println("Available commands: ble status | ble scan [sec] | ble scan start/stop | ble devices | ble show <mac>");
    }
}

void BleService::executeBtClassicCommand(const char* /*args*/, Print& out) {
    printBtClassicStatus(out);
}

} // namespace Esp32Wireless
