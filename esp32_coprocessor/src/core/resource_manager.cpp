#include "resource_manager.h"
#include <cstring>

namespace Esp32Core {

const char* resourceToString(Resource res) {
    switch (res) {
        case Resource::UART_USER:        return "UART_USER";
        case Resource::SPI_BUS:          return "SPI_BUS";
        case Resource::I2C_BUS:          return "I2C_BUS";
        case Resource::WIFI_RADIO:       return "WIFI_RADIO";
        case Resource::BLE_RADIO:        return "BLE_RADIO";
        case Resource::BT_CLASSIC:       return "BT_CLASSIC";
        case Resource::IR_PERIPHERAL:    return "IR_PERIPHERAL";
        case Resource::NRF24_RADIO:      return "NRF24_RADIO";
        case Resource::RP2040_TRANSPORT: return "RP2040_TRANSPORT";
        default:                         return "NONE";
    }
}

const char* ownerToString(Owner owner) {
    switch (owner) {
        case Owner::UNASSIGNED:    return "UNASSIGNED";
        case Owner::SYSTEM:        return "SYSTEM";
        case Owner::USER:          return "USER";
        case Owner::RP2040_LINK:   return "RP2040_LINK";
        case Owner::I2C_SERVICE:   return "I2C_SERVICE";
        case Owner::SPI_SERVICE:   return "SPI_SERVICE";
        case Owner::UART_SERVICE:  return "UART_SERVICE";
        case Owner::GPIO_SERVICE:  return "GPIO_SERVICE";
        case Owner::WIFI_SERVICE:  return "WIFI_SERVICE";
        case Owner::BLE_SERVICE:   return "BLE_SERVICE";
        case Owner::IR_SERVICE:    return "IR_SERVICE";
        case Owner::NRF24_SERVICE: return "NRF24_SERVICE";
        default:                   return "UNKNOWN";
    }
}

const char* resourceErrorToString(ResourceError err) {
    switch (err) {
        case ResourceError::OK:                     return "OK";
        case ResourceError::RESOURCE_BUSY:          return "RESOURCE_BUSY";
        case ResourceError::RESOURCE_NOT_AVAILABLE: return "RESOURCE_NOT_AVAILABLE";
        case ResourceError::INVALID_CONFIGURATION:  return "INVALID_CONFIGURATION";
        case ResourceError::PIN_PROTECTED:          return "PIN_PROTECTED";
        case ResourceError::PIN_INPUT_ONLY:         return "PIN_INPUT_ONLY";
        case ResourceError::NOT_OWNER:              return "NOT_OWNER";
        default:                                    return "UNKNOWN_ERROR";
    }
}

ResourceManager& ResourceManager::instance() {
    static ResourceManager s_instance;
    return s_instance;
}

ResourceManager::ResourceManager() {
    for (size_t i = 0; i < static_cast<size_t>(Resource::COUNT); ++i) {
        m_resourceOwners[i] = Owner::UNASSIGNED;
    }
    for (size_t i = 0; i < 40; ++i) {
        m_pinOwners[i] = Owner::UNASSIGNED;
    }

    // Protect hardware system pins on UDOO KEY
    // SPI Flash / PSRAM on ESP32-WROVER-E (GPIO 6..11)
    for (int p = 6; p <= 11; ++p) {
        m_pinOwners[p] = Owner::SYSTEM;
    }

    // Inter-MCU link UART (IO22 RX, IO19 TX) & Reset (IO23)
    m_pinOwners[19] = Owner::RP2040_LINK;
    m_pinOwners[22] = Owner::RP2040_LINK;
    m_pinOwners[23] = Owner::RP2040_LINK;

    // RP2040 transport is permanently allocated to RP2040_LINK
    m_resourceOwners[static_cast<size_t>(Resource::RP2040_TRANSPORT)] = Owner::RP2040_LINK;
}

ResourceError ResourceManager::acquire(Resource res, Owner owner) {
    if (res == Resource::NONE || res >= Resource::COUNT) {
        return ResourceError::INVALID_CONFIGURATION;
    }
    size_t idx = static_cast<size_t>(res);
    if (m_resourceOwners[idx] != Owner::UNASSIGNED && m_resourceOwners[idx] != owner) {
        return ResourceError::RESOURCE_BUSY;
    }
    m_resourceOwners[idx] = owner;
    return ResourceError::OK;
}

ResourceError ResourceManager::release(Resource res, Owner owner) {
    if (res == Resource::NONE || res >= Resource::COUNT) {
        return ResourceError::INVALID_CONFIGURATION;
    }
    size_t idx = static_cast<size_t>(res);
    if (m_resourceOwners[idx] == Owner::UNASSIGNED) {
        return ResourceError::OK;
    }
    if (m_resourceOwners[idx] != owner && owner != Owner::SYSTEM) {
        return ResourceError::NOT_OWNER;
    }
    m_resourceOwners[idx] = Owner::UNASSIGNED;
    return ResourceError::OK;
}

Owner ResourceManager::getResourceOwner(Resource res) const {
    if (res >= Resource::COUNT) return Owner::UNASSIGNED;
    return m_resourceOwners[static_cast<size_t>(res)];
}

bool ResourceManager::isResourceAvailable(Resource res) const {
    if (res >= Resource::COUNT) return false;
    return m_resourceOwners[static_cast<size_t>(res)] == Owner::UNASSIGNED;
}

bool ResourceManager::isPinProtected(uint8_t pin) const {
    // QSPI Flash / PSRAM pins
    if (pin >= 6 && pin <= 11) return true;
    // Inter-MCU hardwired pins
    if (pin == 19 || pin == 22 || pin == 23) return true;
    // Invalid GPIO numbers on ESP32
    if (pin == 20 || pin == 24 || pin == 28 || pin == 29 || pin == 30 || pin == 31 || pin >= 40) return true;
    return false;
}

bool ResourceManager::isPinInputOnly(uint8_t pin) const {
    return (pin == 34 || pin == 35 || pin == 36 || pin == 39);
}

ResourceError ResourceManager::acquirePin(uint8_t pin, Owner owner) {
    if (isPinProtected(pin)) {
        return ResourceError::PIN_PROTECTED;
    }
    if (m_pinOwners[pin] != Owner::UNASSIGNED && m_pinOwners[pin] != owner) {
        return ResourceError::RESOURCE_BUSY;
    }
    m_pinOwners[pin] = owner;
    return ResourceError::OK;
}

ResourceError ResourceManager::releasePin(uint8_t pin, Owner owner) {
    if (pin >= 40) return ResourceError::INVALID_CONFIGURATION;
    if (isPinProtected(pin) && owner != Owner::SYSTEM) {
        return ResourceError::PIN_PROTECTED;
    }
    if (m_pinOwners[pin] != owner && owner != Owner::SYSTEM) {
        return ResourceError::NOT_OWNER;
    }
    m_pinOwners[pin] = Owner::UNASSIGNED;
    return ResourceError::OK;
}

Owner ResourceManager::getPinOwner(uint8_t pin) const {
    if (pin >= 40) return Owner::SYSTEM;
    return m_pinOwners[pin];
}

bool ResourceManager::isPinAvailable(uint8_t pin) const {
    if (isPinProtected(pin)) return false;
    return m_pinOwners[pin] == Owner::UNASSIGNED;
}

void ResourceManager::resetUserResources() {
    for (size_t i = 0; i < static_cast<size_t>(Resource::COUNT); ++i) {
        if (m_resourceOwners[i] == Owner::USER) {
            m_resourceOwners[i] = Owner::UNASSIGNED;
        }
    }
    for (size_t p = 0; p < 40; ++p) {
        if (!isPinProtected(p) && m_pinOwners[p] == Owner::USER) {
            m_pinOwners[p] = Owner::UNASSIGNED;
            pinMode(p, INPUT); // Return to safe Hi-Z
        }
    }
}

void ResourceManager::dumpStatus(Print& out) const {
    out.println("\r\n=== ESP32 HARDWARE RESOURCE TABLE ===");
    for (size_t i = 1; i < static_cast<size_t>(Resource::COUNT); ++i) {
        Resource r = static_cast<Resource>(i);
        out.printf("  %-18s: %s\r\n", resourceToString(r), ownerToString(m_resourceOwners[i]));
    }
    out.println("\r\n=== PIN ARBITRATION MATRIX ===");
    for (uint8_t p = 0; p < 40; ++p) {
        if (p == 20 || p == 24 || (p >= 28 && p <= 31)) continue;
        const char* note = isPinProtected(p) ? "[PROTECTED]" : (isPinInputOnly(p) ? "[IN-ONLY]" : "");
        out.printf("  GPIO%-2u : %-14s %s\r\n", p, ownerToString(m_pinOwners[p]), note);
    }
    out.println("=====================================");
}

} // namespace Esp32Core
