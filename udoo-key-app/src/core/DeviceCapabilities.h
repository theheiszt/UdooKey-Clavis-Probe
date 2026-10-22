#pragma once

#include <cstdint>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonDocument>
#include "Types.h"

namespace UdooKey {

// Extended capability bits
enum ExtendedDeviceCapability : uint32_t {
    CAP_EXT_JTAG        = 1 << 8,
    CAP_EXT_SWD         = 1 << 9,
    CAP_EXT_USB_TEST    = 1 << 10,
    CAP_EXT_USB_EMU     = 1 << 11,
};

struct DeviceCapabilities {
    // Hardware Identity
    QString hardwareModel = "UDOO KEY";
    QString mcuPrimary = "RP2040 (Dual ARM Cortex-M0+ @ 133MHz)";
    QString mcuCoprocessor = "ESP32-WROOM-32E (Dual Xtensa LX6 @ 240MHz)";
    QString firmwareVersion = "1.2.0";
    QString protocolVersion = "2.0";

    // Logic Analyzer Limits
    uint8_t logicChannels = 8;
    uint32_t maxSampleRateHz = 100'000'000; // 100 MSPS
    uint32_t minSampleRateHz = 1'000;       // 1 kHz
    uint32_t sampleMemoryBytes = 131'072;   // 128 KiB
    bool supportsRleCompression = true;
    bool supportsComplexTriggers = true;

    // Multimeter / ADC Limits
    uint8_t adcChannels = 4;
    double adcVoltageRange = 3.3; // Volts
    double adcResolutionBits = 12;

    // Protocol & Instrument Flags
    bool hasLogicAnalyzer = true;
    bool hasSumpProtocol = true;
    bool hasTerminalShell = true;
    bool hasAdcMeter = true;
    bool hasContinuity = true;
    bool hasI2cTool = true;
    bool hasSpiTool = true;
    bool hasUartTool = true;
    bool hasJtag = true;
    bool hasSwd = true;
    bool hasUsbTest = true;
    bool hasUsbEmulation = true;

    // Bus Tool Frequency Limits
    uint32_t maxI2cClockHz = 1'000'000;   // 1 MHz Fast-Mode Plus
    uint32_t maxSpiClockHz = 25'000'000;  // 25 MHz
    uint32_t maxJtagClockHz = 10'000'000; // 10 MHz
    uint32_t maxSwdClockHz = 10'000'000;  // 10 MHz

    // Legacy bitmask conversion
    uint32_t toLegacyMask() const;
    void fromLegacyMask(uint32_t mask);

    // Serialization & Inspection
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
    QString summary() const;
    QStringList capabilityNames() const;

    // Preset factory methods
    static DeviceCapabilities defaultUdooKey();
    static DeviceCapabilities mockDevice();
};

} // namespace UdooKey
