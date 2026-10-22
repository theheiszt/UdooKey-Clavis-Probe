#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <QString>
#include <QMetaType>
#include <QVariant>

namespace UdooKey {

// Device Hardware Capabilities
enum DeviceCapability : uint32_t {
    CAP_NONE            = 0,
    CAP_LOGIC_ANALYZER  = 1 << 0,
    CAP_SUMP_PROTOCOL   = 1 << 1,
    CAP_TERMINAL_SHELL  = 1 << 2,
    CAP_ADC_METER       = 1 << 3,
    CAP_SPI_TOOL        = 1 << 4,
    CAP_I2C_TOOL        = 1 << 5,
    CAP_UART_TOOL       = 1 << 6,
    CAP_CONTINUITY      = 1 << 7,
};

// Logic Analyzer Trigger Modes
enum class TriggerMode : uint8_t {
    Immediate   = 0,
    EdgeRising  = 1,
    EdgeFalling = 2,
    LevelHigh   = 3,
    LevelLow    = 4
};

// Channel Configuration
struct ChannelConfig {
    uint8_t index = 0;
    QString name = "CH0";
    uint32_t color = 0x00FFCC; // 0xRRGGBB
    bool enabled = true;
    TriggerMode triggerMode = TriggerMode::Immediate;
};

// Capture Acquisition Parameters
struct AcquisitionConfig {
    uint32_t sampleRateHz = 10'000'000; // Default 10 MHz
    uint32_t sampleDepth = 65'536;      // Default 64 KiB
    uint8_t triggerChannel = 0;
    TriggerMode triggerMode = TriggerMode::Immediate;
    double preTriggerRatio = 0.10;      // 10% pre-trigger buffer
};

// LOD (Level of Detail) Sample Chunk for Decimated Rendering
struct LodMinMax {
    uint8_t minVals[8];
    uint8_t maxVals[8];
    uint8_t firstVals[8];
    uint8_t lastVals[8];
};

// Protocol Decoder Types
enum class ProtocolType {
    None,
    UART,
    I2C,
    SPI
};

// Decoded Annotation Token
struct DecodedToken {
    uint64_t startSample = 0;
    uint64_t endSample = 0;
    double startTimeSec = 0.0;
    double endTimeSec = 0.0;
    QString text;
    uint32_t color = 0xFFFFFF;
    bool isWarning = false;
};

// Decoded Protocol Packet / Frame
struct DecodedPacket {
    uint64_t startSample = 0;
    uint64_t endSample = 0;
    QString title;
    QVector<DecodedToken> tokens;
};

// Meter / ADC Reading
struct MeterReading {
    double voltageV = 0.0;
    double rawAdc = 0.0;
    bool continuity = false;
    uint64_t timestampMs = 0;
};

// Rich Protocol Event
struct ProtocolEvent {
    uint64_t sampleIndex = 0;
    double timestampSec = 0.0;
    QString protocolId;
    QString eventType; // "START", "ADDR", "DATA", "ACK", "NACK", "STOP"
    uint64_t value = 0;
    QString formattedValue;
    bool isError = false;
    QString annotation;
};

// Decoder Channel Binding descriptor
struct DecoderChannelBinding {
    QString role;
    uint8_t defaultChannel = 0;
    QString description;
};

// Decoder Option Descriptor
struct DecoderOptionDefinition {
    QString key;
    QString label;
    QVariant defaultValue;
    QVariantList allowedValues;
};

} // namespace UdooKey

Q_DECLARE_METATYPE(UdooKey::MeterReading)
Q_DECLARE_METATYPE(UdooKey::AcquisitionConfig)
Q_DECLARE_METATYPE(UdooKey::DecodedPacket)
Q_DECLARE_METATYPE(UdooKey::ProtocolEvent)
