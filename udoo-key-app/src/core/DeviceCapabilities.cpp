#include "DeviceCapabilities.h"
#include <QJsonArray>

namespace UdooKey {

uint32_t DeviceCapabilities::toLegacyMask() const {
    uint32_t mask = CAP_NONE;
    if (hasLogicAnalyzer) mask |= CAP_LOGIC_ANALYZER;
    if (hasSumpProtocol)  mask |= CAP_SUMP_PROTOCOL;
    if (hasTerminalShell) mask |= CAP_TERMINAL_SHELL;
    if (hasAdcMeter)       mask |= CAP_ADC_METER;
    if (hasSpiTool)        mask |= CAP_SPI_TOOL;
    if (hasI2cTool)        mask |= CAP_I2C_TOOL;
    if (hasUartTool)       mask |= CAP_UART_TOOL;
    if (hasContinuity)     mask |= CAP_CONTINUITY;
    if (hasJtag)           mask |= CAP_EXT_JTAG;
    if (hasSwd)            mask |= CAP_EXT_SWD;
    if (hasUsbTest)        mask |= CAP_EXT_USB_TEST;
    if (hasUsbEmulation)   mask |= CAP_EXT_USB_EMU;
    return mask;
}

void DeviceCapabilities::fromLegacyMask(uint32_t mask) {
    hasLogicAnalyzer = (mask & CAP_LOGIC_ANALYZER) != 0;
    hasSumpProtocol  = (mask & CAP_SUMP_PROTOCOL) != 0;
    hasTerminalShell = (mask & CAP_TERMINAL_SHELL) != 0;
    hasAdcMeter      = (mask & CAP_ADC_METER) != 0;
    hasSpiTool       = (mask & CAP_SPI_TOOL) != 0;
    hasI2cTool       = (mask & CAP_I2C_TOOL) != 0;
    hasUartTool      = (mask & CAP_UART_TOOL) != 0;
    hasContinuity    = (mask & CAP_CONTINUITY) != 0;
    hasJtag          = (mask & CAP_EXT_JTAG) != 0;
    hasSwd           = (mask & CAP_EXT_SWD) != 0;
    hasUsbTest       = (mask & CAP_EXT_USB_TEST) != 0;
    hasUsbEmulation  = (mask & CAP_EXT_USB_EMU) != 0;
}

QJsonObject DeviceCapabilities::toJson() const {
    QJsonObject obj;
    obj["hardwareModel"] = hardwareModel;
    obj["mcuPrimary"] = mcuPrimary;
    obj["mcuCoprocessor"] = mcuCoprocessor;
    obj["firmwareVersion"] = firmwareVersion;
    obj["protocolVersion"] = protocolVersion;
    obj["logicChannels"] = static_cast<int>(logicChannels);
    obj["maxSampleRateHz"] = static_cast<double>(maxSampleRateHz);
    obj["minSampleRateHz"] = static_cast<double>(minSampleRateHz);
    obj["sampleMemoryBytes"] = static_cast<int>(sampleMemoryBytes);
    obj["supportsRleCompression"] = supportsRleCompression;
    obj["supportsComplexTriggers"] = supportsComplexTriggers;
    obj["adcChannels"] = static_cast<int>(adcChannels);
    obj["adcVoltageRange"] = adcVoltageRange;
    obj["adcResolutionBits"] = adcResolutionBits;
    obj["hasLogicAnalyzer"] = hasLogicAnalyzer;
    obj["hasSumpProtocol"] = hasSumpProtocol;
    obj["hasTerminalShell"] = hasTerminalShell;
    obj["hasAdcMeter"] = hasAdcMeter;
    obj["hasContinuity"] = hasContinuity;
    obj["hasI2cTool"] = hasI2cTool;
    obj["hasSpiTool"] = hasSpiTool;
    obj["hasUartTool"] = hasUartTool;
    obj["hasJtag"] = hasJtag;
    obj["hasSwd"] = hasSwd;
    obj["hasUsbTest"] = hasUsbTest;
    obj["hasUsbEmulation"] = hasUsbEmulation;
    obj["maxI2cClockHz"] = static_cast<double>(maxI2cClockHz);
    obj["maxSpiClockHz"] = static_cast<double>(maxSpiClockHz);
    obj["maxJtagClockHz"] = static_cast<double>(maxJtagClockHz);
    obj["maxSwdClockHz"] = static_cast<double>(maxSwdClockHz);
    return obj;
}

void DeviceCapabilities::fromJson(const QJsonObject& obj) {
    if (obj.contains("hardwareModel")) hardwareModel = obj["hardwareModel"].toString();
    if (obj.contains("mcuPrimary")) mcuPrimary = obj["mcuPrimary"].toString();
    if (obj.contains("mcuCoprocessor")) mcuCoprocessor = obj["mcuCoprocessor"].toString();
    if (obj.contains("firmwareVersion")) firmwareVersion = obj["firmwareVersion"].toString();
    if (obj.contains("protocolVersion")) protocolVersion = obj["protocolVersion"].toString();
    if (obj.contains("logicChannels")) logicChannels = static_cast<uint8_t>(obj["logicChannels"].toInt());
    if (obj.contains("maxSampleRateHz")) maxSampleRateHz = static_cast<uint32_t>(obj["maxSampleRateHz"].toDouble());
    if (obj.contains("minSampleRateHz")) minSampleRateHz = static_cast<uint32_t>(obj["minSampleRateHz"].toDouble());
    if (obj.contains("sampleMemoryBytes")) sampleMemoryBytes = static_cast<uint32_t>(obj["sampleMemoryBytes"].toInt());
    if (obj.contains("supportsRleCompression")) supportsRleCompression = obj["supportsRleCompression"].toBool();
    if (obj.contains("supportsComplexTriggers")) supportsComplexTriggers = obj["supportsComplexTriggers"].toBool();
    if (obj.contains("adcChannels")) adcChannels = static_cast<uint8_t>(obj["adcChannels"].toInt());
    if (obj.contains("adcVoltageRange")) adcVoltageRange = obj["adcVoltageRange"].toDouble();
    if (obj.contains("adcResolutionBits")) adcResolutionBits = obj["adcResolutionBits"].toDouble();
    if (obj.contains("hasLogicAnalyzer")) hasLogicAnalyzer = obj["hasLogicAnalyzer"].toBool();
    if (obj.contains("hasSumpProtocol")) hasSumpProtocol = obj["hasSumpProtocol"].toBool();
    if (obj.contains("hasTerminalShell")) hasTerminalShell = obj["hasTerminalShell"].toBool();
    if (obj.contains("hasAdcMeter")) hasAdcMeter = obj["hasAdcMeter"].toBool();
    if (obj.contains("hasContinuity")) hasContinuity = obj["hasContinuity"].toBool();
    if (obj.contains("hasI2cTool")) hasI2cTool = obj["hasI2cTool"].toBool();
    if (obj.contains("hasSpiTool")) hasSpiTool = obj["hasSpiTool"].toBool();
    if (obj.contains("hasUartTool")) hasUartTool = obj["hasUartTool"].toBool();
    if (obj.contains("hasJtag")) hasJtag = obj["hasJtag"].toBool();
    if (obj.contains("hasSwd")) hasSwd = obj["hasSwd"].toBool();
    if (obj.contains("hasUsbTest")) hasUsbTest = obj["hasUsbTest"].toBool();
    if (obj.contains("hasUsbEmulation")) hasUsbEmulation = obj["hasUsbEmulation"].toBool();
    if (obj.contains("maxI2cClockHz")) maxI2cClockHz = static_cast<uint32_t>(obj["maxI2cClockHz"].toDouble());
    if (obj.contains("maxSpiClockHz")) maxSpiClockHz = static_cast<uint32_t>(obj["maxSpiClockHz"].toDouble());
    if (obj.contains("maxJtagClockHz")) maxJtagClockHz = static_cast<uint32_t>(obj["maxJtagClockHz"].toDouble());
    if (obj.contains("maxSwdClockHz")) maxSwdClockHz = static_cast<uint32_t>(obj["maxSwdClockHz"].toDouble());
}

QStringList DeviceCapabilities::capabilityNames() const {
    QStringList list;
    if (hasLogicAnalyzer) list << "Logic Analyzer (8ch, 100MSPS)";
    if (hasSumpProtocol)  list << "SUMP Protocol";
    if (hasTerminalShell) list << "Interactive Shell";
    if (hasAdcMeter)       list << "Digital Multimeter / ADC";
    if (hasContinuity)     list << "Continuity Tester";
    if (hasI2cTool)        list << "I2C Master/Sniffer";
    if (hasSpiTool)        list << "SPI Master/Sniffer";
    if (hasUartTool)       list << "UART Bridge";
    if (hasJtag)           list << "JTAG TAP Controller";
    if (hasSwd)            list << "ARM SWD Debugger";
    if (hasUsbTest)        list << "USB Test & Injection";
    if (hasUsbEmulation)   list << "USB Device Emulation";
    return list;
}

QString DeviceCapabilities::summary() const {
    return QString("%1 (FW %2) — %3 channels, up to %4 MHz")
        .arg(hardwareModel)
        .arg(firmwareVersion)
        .arg(logicChannels)
        .arg(maxSampleRateHz / 1'000'000.0, 0, 'f', 1);
}

DeviceCapabilities DeviceCapabilities::defaultUdooKey() {
    DeviceCapabilities caps;
    caps.hardwareModel = "UDOO KEY";
    caps.firmwareVersion = "1.2.0";
    caps.protocolVersion = "2.0";
    caps.logicChannels = 8;
    caps.maxSampleRateHz = 100'000'000;
    caps.sampleMemoryBytes = 131'072;
    return caps;
}

DeviceCapabilities DeviceCapabilities::mockDevice() {
    DeviceCapabilities caps;
    caps.hardwareModel = "UDOO KEY (Mock Simulator)";
    caps.firmwareVersion = "1.2.0-sim";
    caps.protocolVersion = "2.0";
    caps.logicChannels = 8;
    caps.maxSampleRateHz = 100'000'000;
    caps.sampleMemoryBytes = 131'072;
    return caps;
}

} // namespace UdooKey
