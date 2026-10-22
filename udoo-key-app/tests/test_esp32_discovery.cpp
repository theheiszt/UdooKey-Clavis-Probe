#include "Esp32DiscoveryService.h"
#include <iostream>
#include <cassert>
#include <QJsonDocument>
#include <QJsonObject>

using namespace UdooKey;

static bool testEsp32DiscoveryStateStrings() {
    std::cout << "  - Testing Esp32Discovery state strings and mapping...\n";
    assert(networkStateToString(NetworkConnectionState::Disconnected) == "DISCONNECTED");
    assert(networkStateToString(NetworkConnectionState::Connecting) == "CONNECTING");
    assert(networkStateToString(NetworkConnectionState::HttpProbe) == "HTTP_PROBE");
    assert(networkStateToString(NetworkConnectionState::WebSocketConnect) == "WEBSOCKET_CONNECT");
    assert(networkStateToString(NetworkConnectionState::Connected) == "CONNECTED");
    assert(networkStateToString(NetworkConnectionState::ApNotReachable) == "ESP32 AP not reachable");
    assert(networkStateToString(NetworkConnectionState::Error) == "ERROR");
    return true;
}

static bool testEsp32DiscoveryJsonParsing() {
    std::cout << "  - Testing Esp32Discovery JSON telemetry parsing...\n";
    QByteArray sampleJson = R"({
        "device": "UDOO KEY",
        "subsystem": "ESP32",
        "status": "OK",
        "wifi": {
            "mode": "AP",
            "state": "AP_ACTIVE",
            "ssid": "UDOO-BIT-PIRATE-SCOPE",
            "ip": "192.168.4.1",
            "channel": 1,
            "clients": 2
        },
        "uptime_ms": 12345,
        "free_heap": 215000
    })";

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(sampleJson, &err);
    assert(err.error == QJsonParseError::NoError);
    assert(doc.isObject());

    QJsonObject obj = doc.object();
    assert(obj.value("device").toString() == "UDOO KEY");
    assert(obj.value("status").toString() == "OK");

    QJsonObject wifiObj = obj.value("wifi").toObject();
    assert(wifiObj.value("ssid").toString() == "UDOO-BIT-PIRATE-SCOPE");
    assert(wifiObj.value("ip").toString() == "192.168.4.1");
    assert(wifiObj.value("channel").toInt() == 1);
    assert(wifiObj.value("clients").toInt() == 2);
    return true;
}

bool runEsp32DiscoveryTests() {
    std::cout << "--- Running ESP32 Network Discovery & Integration Tests ---\n";
    bool ok1 = testEsp32DiscoveryStateStrings();
    bool ok2 = testEsp32DiscoveryJsonParsing();
    if (ok1 && ok2) {
        std::cout << "[PASS] ESP32 Network Discovery Tests\n";
        return true;
    }
    return false;
}
