#pragma once

#include <cstdint>

namespace Esp32Wireless {
namespace WifiConfig {

// Standalone Access Point Configuration
constexpr const char* AP_SSID = "UDOO-BIT-PIRATE-SCOPE";
constexpr const char* AP_PASSWORD = "udoopass123";
constexpr uint8_t     AP_CHANNEL = 1;
constexpr bool        AP_HIDDEN = false;
constexpr uint8_t     AP_MAX_CLIENTS = 4;

// Default Static AP IP Configuration: 192.168.4.1 / 255.255.255.0
constexpr uint8_t AP_IP[4]      = {192, 168, 4, 1};
constexpr uint8_t AP_GW[4]      = {192, 168, 4, 1};
constexpr uint8_t AP_NETMASK[4] = {255, 255, 255, 0};

// HTTP & WebSocket Server Ports
constexpr uint16_t HTTP_PORT = 80;
constexpr const char* WS_ENDPOINT = "/ws";

// Health Monitor Parameters
constexpr uint32_t HEALTH_CHECK_INTERVAL_MS = 5000;
constexpr uint8_t  MAX_RECOVERY_ATTEMPTS = 3;

} // namespace WifiConfig
} // namespace Esp32Wireless
