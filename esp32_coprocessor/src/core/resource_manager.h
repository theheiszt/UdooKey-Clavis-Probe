#pragma once

#include <Arduino.h>
#include <cstdint>

namespace Esp32Core {

enum class Resource : uint8_t {
    NONE = 0,
    UART_USER,
    SPI_BUS,
    I2C_BUS,
    WIFI_RADIO,
    BLE_RADIO,
    BT_CLASSIC,
    IR_PERIPHERAL,
    NRF24_RADIO,
    RP2040_TRANSPORT,
    COUNT
};

enum class Owner : uint8_t {
    UNASSIGNED = 0,
    SYSTEM,
    USER,
    RP2040_LINK,
    I2C_SERVICE,
    SPI_SERVICE,
    UART_SERVICE,
    GPIO_SERVICE,
    WIFI_SERVICE,
    BLE_SERVICE,
    IR_SERVICE,
    NRF24_SERVICE
};

enum class ResourceError : uint8_t {
    OK = 0,
    RESOURCE_BUSY,
    RESOURCE_NOT_AVAILABLE,
    INVALID_CONFIGURATION,
    PIN_PROTECTED,
    PIN_INPUT_ONLY,
    NOT_OWNER
};

const char* resourceToString(Resource res);
const char* ownerToString(Owner owner);
const char* resourceErrorToString(ResourceError err);

class ResourceManager {
public:
    static ResourceManager& instance();

    // Peripheral Resource Management
    ResourceError acquire(Resource res, Owner owner);
    ResourceError release(Resource res, Owner owner);
    Owner getResourceOwner(Resource res) const;
    bool isResourceAvailable(Resource res) const;

    // GPIO Pin-level Arbitration
    ResourceError acquirePin(uint8_t pin, Owner owner);
    ResourceError releasePin(uint8_t pin, Owner owner);
    Owner getPinOwner(uint8_t pin) const;
    bool isPinAvailable(uint8_t pin) const;
    bool isPinProtected(uint8_t pin) const;
    bool isPinInputOnly(uint8_t pin) const;

    void resetUserResources();
    void dumpStatus(Print& out) const;

private:
    ResourceManager();

    Owner m_resourceOwners[static_cast<size_t>(Resource::COUNT)];
    Owner m_pinOwners[40]; // ESP32 has GPIO 0..39
};

} // namespace Esp32Core
