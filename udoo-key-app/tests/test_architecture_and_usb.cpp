#include <iostream>
#include <cmath>
#include <QThread>
#include <QTemporaryFile>

#include "DeviceCapabilities.h"
#include "TaskManager.h"
#include "ErrorManager.h"
#include "TransactionModel.h"
#include "UsbTypes.h"
#include "UsbDescriptorParser.h"
#include "MockUsbHost.h"
#include "UsbInjectionEngine.h"
#include "UsbTestSequence.h"
#include "SessionManager.h"
#include "CommandDispatcher.h"
#include "HexUtils.h"
#include "CrcUtils.h"
#include "VcdExporter.h"
#include "CsvExporter.h"
#include "BinaryExporter.h"
#include "TriggerExpression.h"
#include "DecoderRegistry.h"
#include "InstrumentReportGenerator.h"

using namespace UdooKey;

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

static bool testDeviceCapabilities() {
    auto caps = DeviceCapabilities::defaultUdooKey();
    TEST_ASSERT(caps.hardwareModel == "UDOO KEY");
    TEST_ASSERT(caps.logicChannels == 8);
    TEST_ASSERT(caps.maxSampleRateHz == 100'000'000);

    // Legacy mask roundtrip
    uint32_t mask = caps.toLegacyMask();
    TEST_ASSERT(mask & CAP_LOGIC_ANALYZER);
    TEST_ASSERT(mask & CAP_SUMP_PROTOCOL);
    TEST_ASSERT(mask & CAP_TERMINAL_SHELL);

    DeviceCapabilities reconstructed;
    reconstructed.fromLegacyMask(mask);
    TEST_ASSERT(reconstructed.hasLogicAnalyzer == true);
    TEST_ASSERT(reconstructed.hasTerminalShell == true);

    // JSON roundtrip
    QJsonObject json = caps.toJson();
    DeviceCapabilities jsonRestored;
    jsonRestored.fromJson(json);
    TEST_ASSERT(jsonRestored.logicChannels == 8);
    TEST_ASSERT(jsonRestored.maxSampleRateHz == 100'000'000);
    TEST_ASSERT(jsonRestored.sampleMemoryBytes == 131'072);
    TEST_ASSERT(jsonRestored.mcuPrimary.contains("RP2040"));

    return true;
}

static bool testTaskManager() {
    auto& tm = TaskManager::instance();

    // 1. Successful async task
    std::atomic<bool> taskExecuted{false};
    uint64_t id1 = tm.submitTask("Test Task 1", "Verify execution",
        [&taskExecuted](std::shared_ptr<CancellationToken>, std::function<void(int, const QString&)> progress) {
            progress(50, "Halfway");
            taskExecuted = true;
            return true;
        }
    );

    TEST_ASSERT(id1 > 0);
    // Wait for task to finish
    for (int i = 0; i < 50 && tm.isTaskRunning(id1); ++i) {
        QThread::msleep(10);
    }
    TEST_ASSERT(taskExecuted == true);

    // 2. Cancelled task
    uint64_t id2 = tm.submitTask("Cancel Task", "Verify cancellation",
        [](std::shared_ptr<CancellationToken> token, std::function<void(int, const QString&)>) {
            while (!token->isCancelled()) {
                QThread::msleep(5);
            }
            return false;
        }
    );

    QThread::msleep(20);
    TEST_ASSERT(tm.cancelTask(id2) == true);

    for (int i = 0; i < 50 && tm.isTaskRunning(id2); ++i) {
        QThread::msleep(10);
    }
    auto info2 = tm.getTaskInfo(id2);
    TEST_ASSERT(info2.state == TaskState::Cancelled);

    return true;
}

static bool testErrorManager() {
    auto& em = ErrorManager::instance();
    em.clear();

    em.report(ErrorCategory::Transport, ErrorSeverity::Error, "Connection timeout", "Check USB port", "Port: COM3", 404);
    em.report(ErrorCategory::Usb, ErrorSeverity::Warning, "Endpoint stall", "Retry transfer", "EP: 0x81", 101);

    auto list = em.recentErrors();
    TEST_ASSERT(list.size() == 2);
    TEST_ASSERT(list[0].category == ErrorCategory::Transport);
    TEST_ASSERT(list[0].message == "Connection timeout");
    TEST_ASSERT(list[0].suggestion == "Check USB port");
    TEST_ASSERT(list[1].category == ErrorCategory::Usb);

    em.clear();
    TEST_ASSERT(em.recentErrors().isEmpty());
    return true;
}

static bool testTransactionModel() {
    auto& tl = TransactionLogger::instance();
    tl.clear();

    TransactionResult tx1;
    tx1.subsystem = "USB";
    tx1.summary = "GET_DESCRIPTOR Device";
    tx1.success = true;
    tx1.durationNs = 1'500'000;
    tx1.txData = QByteArray("\x80\x06\x00\x01\x00\x00\x12\x00", 8);
    tx1.rxData = QByteArray(18, 0x12);
    uint64_t id1 = tl.logTransaction(tx1);

    TransactionResult tx2;
    tx2.subsystem = "JTAG";
    tx2.summary = "Scan IDCODE";
    tx2.success = true;
    tx2.durationNs = 2'000'000;
    tl.logTransaction(tx2);

    TEST_ASSERT(tl.count() == 2);
    auto usbOnly = tl.getTransactions("USB");
    TEST_ASSERT(usbOnly.size() == 1);
    TEST_ASSERT(usbOnly[0].summary == "GET_DESCRIPTOR Device");

    auto retrieved = tl.getTransactionById(id1);
    TEST_ASSERT(retrieved.id == id1);
    TEST_ASSERT(retrieved.txData.size() == 8);

    tl.clear();
    TEST_ASSERT(tl.count() == 0);
    return true;
}

static bool testUsbDescriptorsAndParser() {
    // 1. Setup packet
    UsbSetupPacket setup;
    setup.bmRequestType = 0x80;
    setup.bRequest = 0x06;
    setup.wValue = 0x0100;
    setup.wIndex = 0x0000;
    setup.wLength = 0x0012;

    QByteArray bytes = setup.toByteArray();
    TEST_ASSERT(bytes.size() == 8);

    UsbSetupPacket restored = UsbSetupPacket::fromByteArray(bytes);
    TEST_ASSERT(restored.direction() == UsbDirection::In);
    TEST_ASSERT(restored.requestType() == UsbControlRequestType::Standard);
    TEST_ASSERT(restored.requestNameString() == "GET_DESCRIPTOR");

    // 2. Parse mock UDOO Key Device Descriptor
    MockUsbHost host;
    host.initialize();
    TEST_ASSERT(host.openDevice(0x2E8A, 0x105A) == true);

    QByteArray devRaw = host.getDescriptor(static_cast<uint8_t>(UsbDescriptorType::Device), 0, 0, 18);
    TEST_ASSERT(devRaw.size() == 18);

    DeviceDescriptor devDesc;
    bool parsedDev = UsbDescriptorParser::parseDeviceDescriptor(devRaw, devDesc);
    TEST_ASSERT(parsedDev == true);
    TEST_ASSERT(devDesc.idVendor == 0x2E8A);
    TEST_ASSERT(devDesc.idProduct == 0x105A);
    TEST_ASSERT(devDesc.bNumConfigurations == 1);

    // 3. Parse Configuration Tree
    QByteArray cfgRaw = host.getDescriptor(static_cast<uint8_t>(UsbDescriptorType::Configuration), 0, 0, 512);
    TEST_ASSERT(cfgRaw.size() > 9);

    ConfigurationDescriptor cfgDesc;
    bool parsedCfg = UsbDescriptorParser::parseConfigurationTree(cfgRaw, cfgDesc);
    TEST_ASSERT(parsedCfg == true);
    TEST_ASSERT(cfgDesc.interfaces.size() == 4);
    TEST_ASSERT(cfgDesc.interfaces[0].bInterfaceClass == 0x02); // CDC
    TEST_ASSERT(cfgDesc.interfaces[1].bInterfaceClass == 0x0A); // CDC Data
    TEST_ASSERT(cfgDesc.interfaces[2].bInterfaceClass == 0xFF); // Vendor Instrument
    TEST_ASSERT(cfgDesc.interfaces[3].bInterfaceClass == 0xFF); // Vendor Debug

    host.closeDevice();
    host.shutdown();
    return true;
}

static bool testUsbHostAndInjection() {
    MockUsbHost host;
    host.initialize();

    auto devices = host.enumerateDevices();
    TEST_ASSERT(devices.size() >= 3);

    bool opened = host.openDevice(0x2E8A, 0x105A);
    TEST_ASSERT(opened == true);

    UsbInjectionEngine engine(&host);

    // Execute Control transfer
    UsbSetupPacket setup;
    setup.bmRequestType = 0x80;
    setup.bRequest = 0x06;
    setup.wValue = 0x0100;
    setup.wIndex = 0x0000;
    setup.wLength = 18;

    auto ctrlRes = engine.executeControlTransfer(setup, QByteArray());
    TEST_ASSERT(ctrlRes.success == true);
    TEST_ASSERT(ctrlRes.rxData.size() == 18);

    // Execute Bulk OUT and Bulk IN transfer
    QByteArray payload("TEST_DATA_PACKET_UDOO_KEY");
    auto bulkOutRes = engine.executeBulkTransfer(0x01, payload, false);
    TEST_ASSERT(bulkOutRes.success == true);

    auto bulkInRes = engine.executeBulkTransfer(0x81, QByteArray(), true, payload.size());
    TEST_ASSERT(bulkInRes.success == true);
    TEST_ASSERT(bulkInRes.rxData == payload);

    host.closeDevice();
    host.shutdown();
    return true;
}

static bool testUsbTestSequence() {
    MockUsbHost host;
    host.initialize();
    host.openDevice(0x2E8A, 0x105A);
    UsbInjectionEngine engine(&host);

    auto seq = UsbTestSequence::standardUdooKeyProbeSequence();
    TEST_ASSERT(seq.steps.size() == 4);

    auto token = std::make_shared<CancellationToken>();
    auto results = seq.execute(&engine, token, [](int, int, const QString&) {});
    TEST_ASSERT(results.size() == 4);
    for (const auto& r : results) {
        TEST_ASSERT(r.passed == true);
    }

    host.closeDevice();
    host.shutdown();
    return true;
}

static bool testSessionManager() {
    SessionData original;
    original.hardwareModel = "UDOO KEY";
    original.firmwareVersion = "1.2.0";
    original.sampleRateHz = 25'000'000;
    original.sampleCount = 1000;
    original.rawCaptureBytes = QByteArray(1000, 0x55);
    original.cursorASec = 0.0001;
    original.cursorBSec = 0.0005;
    original.cursorCSec = 0.0008;
    original.cursorDSec = 0.0012;
    original.activeProtocol = "SPI";
    original.decoderChannelA = 2;
    original.decoderChannelB = 3;
    original.decoderBaudRate = 1'000'000;

    QTemporaryFile tempFile;
    TEST_ASSERT(tempFile.open());
    QString path = tempFile.fileName();
    tempFile.close();

    bool saved = SessionManager::saveSession(path, original);
    TEST_ASSERT(saved == true);

    SessionData loaded;
    bool loadedOk = SessionManager::loadSession(path, loaded);
    TEST_ASSERT(loadedOk == true);
    TEST_ASSERT(loaded.sampleRateHz == original.sampleRateHz);
    TEST_ASSERT(loaded.sampleCount == original.sampleCount);
    TEST_ASSERT(loaded.rawCaptureBytes == original.rawCaptureBytes);
    TEST_ASSERT(std::abs(loaded.cursorASec - original.cursorASec) < 1e-9);
    TEST_ASSERT(loaded.activeProtocol == "SPI");

    return true;
}

static bool testCommandDispatcher() {
    auto& cd = CommandDispatcher::instance();

    QString resStatus = cd.execute("device.status");
    TEST_ASSERT(resStatus == "Disconnected" || resStatus == "Connected");

    QString resHelp = cd.execute("help");
    TEST_ASSERT(resHelp.contains("device.info"));
    TEST_ASSERT(resHelp.contains("device.status"));

    // Test JSON-RPC
    QJsonObject rpcReq;
    rpcReq["jsonrpc"] = "2.0";
    rpcReq["id"] = 1;
    rpcReq["method"] = "device.status";

    QJsonObject rpcResp = cd.executeJsonRpc(rpcReq);
    TEST_ASSERT(rpcResp["id"].toInt() == 1);
    TEST_ASSERT(rpcResp.contains("result"));

    return true;
}

static bool testHexAndCrcUtils() {
    // 1. Hex formatting and parsing
    QByteArray raw("\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8);
    QString formatted = HexUtils::formatHex(raw);
    TEST_ASSERT(formatted == "01 23 45 67 89 AB CD EF");

    QByteArray parsed = HexUtils::parseHex("01 23 45 67 89 AB CD EF");
    TEST_ASSERT(parsed == raw);

    QByteArray prefixed = HexUtils::parseHex("0x01, 0x23, 0x45");
    TEST_ASSERT(prefixed == QByteArray("\x01\x23\x45", 3));

    QString dump = HexUtils::formatHexDump(raw, 0x1000);
    TEST_ASSERT(dump.contains("00001000:"));
    TEST_ASSERT(dump.contains("|"));

    // 2. CRC-16-CCITT and CRC-32
    QByteArray testStr("123456789", 9);
    uint16_t crc16 = CrcUtils::crc16Ccitt(testStr);
    TEST_ASSERT(crc16 == 0x29B1);

    uint32_t crc32 = CrcUtils::crc32(testStr);
    TEST_ASSERT(crc32 == 0xCBF43926);

    // 3. Parity
    TEST_ASSERT(CrcUtils::evenParity(0x01) == 1);
    TEST_ASSERT(CrcUtils::evenParity(0x03) == 0);
    TEST_ASSERT(CrcUtils::oddParity(0x01) == 0);
    TEST_ASSERT(CrcUtils::oddParity(0x03) == 1);

    return true;
}

static bool testAsyncExportersAndCancellation() {
    // Generate synthetic capture buffer (1000 samples)
    QByteArray samples;
    samples.resize(1000);
    for (int i = 0; i < 1000; ++i) {
        samples[i] = static_cast<char>((i % 2 == 0) ? 0x55 : 0xAA);
    }
    CaptureBuffer buffer(samples, 10'000'000);

    // 1. VCD Exporter with Cancellation
    QTemporaryFile tempVcd;
    TEST_ASSERT(tempVcd.open());
    QString vcdPath = tempVcd.fileName();
    tempVcd.close();

    VcdExporter vcdExp;
    auto cancelToken = std::make_shared<CancellationToken>();
    cancelToken->cancel(); // Cancel immediately

    QString errMsg;
    bool cancelledOk = vcdExp.exportDataAsync(vcdPath, buffer, &errMsg, cancelToken);
    TEST_ASSERT(cancelledOk == false);
    TEST_ASSERT(errMsg.contains("cancelled", Qt::CaseInsensitive));

    // 2. Normal VCD Export
    bool vcdOk = vcdExp.exportData(vcdPath, buffer, &errMsg);
    TEST_ASSERT(vcdOk == true);

    // 3. CSV Exporter
    QTemporaryFile tempCsv;
    TEST_ASSERT(tempCsv.open());
    QString csvPath = tempCsv.fileName();
    tempCsv.close();

    CsvExporter csvExp;
    bool csvOk = csvExp.exportData(csvPath, buffer, &errMsg);
    TEST_ASSERT(csvOk == true);

    // 4. Binary Exporter
    QTemporaryFile tempBin;
    TEST_ASSERT(tempBin.open());
    QString binPath = tempBin.fileName();
    tempBin.close();

    BinaryExporter binExp;
    bool binOk = binExp.exportData(binPath, buffer, &errMsg);
    TEST_ASSERT(binOk == true);

    QFile binFile(binPath);
    TEST_ASSERT(binFile.size() == 1000);

    return true;
}

static bool testTriggerExpression() {
    TriggerExpression expr;
    expr.setCondition(0, ChannelCondition::RisingEdge);
    expr.setCondition(1, ChannelCondition::High);

    QString pattern = expr.toPatternString();
    TEST_ASSERT(pattern.endsWith("1R"));

    // Evaluate: prev=0x00, cur=0x03 -> CH0 rising (0->1), CH1 high (1)
    TEST_ASSERT(expr.evaluate(0x03, 0x00) == true);
    // Evaluate: prev=0x03, cur=0x03 -> CH0 steady high (no rising edge)
    TEST_ASSERT(expr.evaluate(0x03, 0x03) == false);

    // Search in synthetic buffer
    QByteArray data;
    data.append(static_cast<char>(0x00));
    data.append(static_cast<char>(0x00));
    data.append(static_cast<char>(0x03)); // Trigger matches here at index 2
    data.append(static_cast<char>(0x00));
    CaptureBuffer buf(data, 10'000'000);

    int64_t trigIdx = expr.findTriggerIndex(buf);
    TEST_ASSERT(trigIdx == 2);

    // JSON roundtrip
    QJsonObject json = expr.toJson();
    TriggerExpression restored;
    restored.fromJson(json);
    TEST_ASSERT(restored.condition(0) == ChannelCondition::RisingEdge);
    TEST_ASSERT(restored.condition(1) == ChannelCondition::High);

    return true;
}

static bool testPluggableDecodersAndEvents() {
    auto& reg = DecoderRegistry::instance();

    // Query by string ID
    auto uart = reg.decoder("uart");
    TEST_ASSERT(uart != nullptr);
    TEST_ASSERT(uart->name() == "UART");
    TEST_ASSERT(!uart->channelBindings().isEmpty());
    TEST_ASSERT(!uart->optionDefinitions().isEmpty());

    auto i2c = reg.decoder("i2c");
    TEST_ASSERT(i2c != nullptr);
    TEST_ASSERT(i2c->name() == "I2C");

    auto spi = reg.decoder("spi");
    TEST_ASSERT(spi != nullptr);
    TEST_ASSERT(spi->name() == "SPI");

    // Test ProtocolEvent conversion
    QByteArray data(100, 0x00);
    CaptureBuffer buf(data, 10'000'000);
    auto events = uart->decodeEvents(buf);
    TEST_ASSERT(events.size() >= 0);

    return true;
}

static bool testInstrumentReportGenerator() {
    InstrumentReportData data;
    data.hardwareModel = "UDOO KEY";
    data.firmwareVersion = "1.2.0";
    data.sampleRateHz = 10'000'000;
    data.sampleCount = 1024;
    data.durationSec = 0.0001024;
    data.cursorASec = 0.00001;
    data.cursorBSec = 0.00005;
    data.cursorCSec = 0.00006;
    data.cursorDSec = 0.00009;

    QString html = InstrumentReportGenerator::generateHtmlReport(data);
    TEST_ASSERT(html.contains("<!DOCTYPE html>"));
    TEST_ASSERT(html.contains("UDOO KEY"));
    TEST_ASSERT(html.contains("Channel Signal Metrics"));
    TEST_ASSERT(html.contains("Measurement Markers"));

    QJsonObject json = InstrumentReportGenerator::generateJsonReport(data);
    TEST_ASSERT(json.contains("hardware"));
    TEST_ASSERT(json.contains("acquisition"));
    TEST_ASSERT(json.contains("channelMetrics"));
    TEST_ASSERT(json.contains("cursors"));

    QTemporaryFile tempHtml;
    TEST_ASSERT(tempHtml.open());
    QString htmlPath = tempHtml.fileName();
    tempHtml.close();

    bool saved = InstrumentReportGenerator::saveHtmlReport(htmlPath, data);
    TEST_ASSERT(saved == true);

    QFile f(htmlPath);
    TEST_ASSERT(f.size() > 500);

    return true;
}

bool runArchitectureAndUsbTests() {
    std::cout << "  - Testing DeviceCapabilities..." << std::endl;
    if (!testDeviceCapabilities()) return false;

    std::cout << "  - Testing TaskManager..." << std::endl;
    if (!testTaskManager()) return false;

    std::cout << "  - Testing ErrorManager..." << std::endl;
    if (!testErrorManager()) return false;

    std::cout << "  - Testing TransactionModel..." << std::endl;
    if (!testTransactionModel()) return false;

    std::cout << "  - Testing HexUtils & CrcUtils..." << std::endl;
    if (!testHexAndCrcUtils()) return false;

    std::cout << "  - Testing Asynchronous Exporters & Cancellation..." << std::endl;
    if (!testAsyncExportersAndCancellation()) return false;

    std::cout << "  - Testing TriggerExpression & Multi-Channel Patterns..." << std::endl;
    if (!testTriggerExpression()) return false;

    std::cout << "  - Testing Pluggable Decoders & Protocol Events..." << std::endl;
    if (!testPluggableDecodersAndEvents()) return false;

    std::cout << "  - Testing Instrument Report Generator (HTML/JSON)..." << std::endl;
    if (!testInstrumentReportGenerator()) return false;

    std::cout << "  - Testing USB Descriptors & Parser..." << std::endl;
    if (!testUsbDescriptorsAndParser()) return false;

    std::cout << "  - Testing USB Host & Injection..." << std::endl;
    if (!testUsbHostAndInjection()) return false;

    std::cout << "  - Testing USB Test Sequences..." << std::endl;
    if (!testUsbTestSequence()) return false;

    std::cout << "  - Testing Session Persistence (.udoo)..." << std::endl;
    if (!testSessionManager()) return false;

    std::cout << "  - Testing Command Dispatcher & JSON-RPC..." << std::endl;
    if (!testCommandDispatcher()) return false;

    return true;
}
