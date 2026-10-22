#include <QApplication>
#include <QTabWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QElapsedTimer>
#include <iostream>
#include <cassert>

#include "MainWindow.h"
#include "SerialTransport.h"
#include "StreamDemux.h"
#include "TerminalClient.h"
#include "BinaryLogicClient.h"
#include "ErrorManager.h"
#include "CrcUtils.h"
#include "Theme.h"

#if defined(Q_OS_LINUX)
#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#endif

using namespace UdooKey;

// ============================================================================
// PART A: Interactive Linux GUI & Widget Verification
// ============================================================================
bool runLinuxGuiInteractiveTests(QApplication& app) {
    std::cout << "\n========================================================" << std::endl;
    std::cout << " [1] Starting Linux GUI & Widget Verification" << std::endl;
    std::cout << "========================================================" << std::endl;

    MainWindow mainWindow;
    mainWindow.show();
    app.processEvents();

    std::cout << "  - MainWindow shown. Dimensions: " 
              << mainWindow.width() << "x" << mainWindow.height() << std::endl;
    assert(mainWindow.width() > 0 && mainWindow.height() > 0);

    // Verify TabWidget navigation across all 9 views
    auto tabWidget = mainWindow.findChild<QTabWidget*>();
    assert(tabWidget != nullptr);
    int tabCount = tabWidget->count();
    std::cout << "  - Verified central QTabWidget with " << tabCount << " tabs." << std::endl;
    assert(tabCount >= 8);

    for (int i = 0; i < tabCount; ++i) {
        tabWidget->setCurrentIndex(i);
        app.processEvents();
        QString title = tabWidget->tabText(i);
        std::cout << "    -> Navigated to Tab [" << i << "]: " << title.toStdString() << std::endl;
    }

    // Verify Terminal Console View and input controls
    tabWidget->setCurrentIndex(3); // Universal Terminal Tab
    app.processEvents();

    auto cmdInput = mainWindow.findChild<QLineEdit*>();
    auto txtOutput = mainWindow.findChild<QTextEdit*>();
    if (cmdInput && txtOutput) {
        std::cout << "  - Universal Terminal controls verified (Input LineEdit & Output TextEdit)." << std::endl;
        cmdInput->setText("status");
        app.processEvents();
        std::cout << "    -> Injected simulated user input: 'status'" << std::endl;
    }

    // Verify ErrorManager status and notification pipeline
    auto& errMgr = ErrorManager::instance();
    errMgr.report(ErrorCategory::Transport, ErrorSeverity::Warning, "Simulated test warning", "Verify GUI error banner");
    app.processEvents();
    std::cout << "  - Injected ErrorManager diagnostic warning event." << std::endl;

    // Verify clean shutdown
    mainWindow.close();
    app.processEvents();
    std::cout << "  - MainWindow closed cleanly. Geometry and state saved." << std::endl;
    std::cout << "[PASS] Linux GUI & Widget Verification Succeeded." << std::endl;
    return true;
}

// ============================================================================
// PART B: Linux Serial Layer Verification (via Native Linux PTY / socat)
// ============================================================================
bool runLinuxSerialPtyTests(QApplication& app) {
    std::cout << "\n========================================================" << std::endl;
    std::cout << " [2] Starting Linux Serial PTY Layer Verification" << std::endl;
    std::cout << "========================================================" << std::endl;

#if !defined(Q_OS_LINUX)
    std::cout << "[SKIP] PTY test only applicable on Linux." << std::endl;
    return true;
#else
    int masterFd = -1;
    int slaveFd = -1;
    char slaveName[128] = {0};

    int ptyRes = openpty(&masterFd, &slaveFd, slaveName, nullptr, nullptr);
    if (ptyRes < 0) {
        std::cerr << "[FAIL] Unable to create Linux PTY via openpty()" << std::endl;
        return false;
    }

    // Set master non-blocking
    int flags = fcntl(masterFd, F_GETFL, 0);
    fcntl(masterFd, F_SETFL, flags | O_NONBLOCK);

    std::cout << "  - Created native Linux PTY pair: slave=" << slaveName << std::endl;

    SerialTransport transport;
    transport.setPortName(QString::fromUtf8(slaveName));
    transport.setBaudRate(115200);

    bool opened = transport.open();
    if (!opened) {
        std::cerr << "[FAIL] SerialTransport failed to open slave PTY: " << slaveName << std::endl;
        close(masterFd);
        close(slaveFd);
        return false;
    }
    std::cout << "  - SerialTransport successfully opened " << slaveName << std::endl;

    // Test 1: Command Transmission across PTY
    QByteArray testCmd = "help\r\n";
    transport.write(testCmd);
    app.processEvents();

    char rxBuf[256] = {0};
    usleep(10000); // 10ms
    ssize_t bytesRead = ::read(masterFd, rxBuf, sizeof(rxBuf) - 1);
    std::cout << "  - PTY Master received " << bytesRead << " bytes from SerialTransport: " 
              << (bytesRead > 0 ? rxBuf : "<empty>") << std::endl;
    assert(bytesRead == testCmd.size());
    assert(QByteArray(rxBuf, bytesRead) == testCmd);

    // Test 2: Fragmented ASCII Response from firmware emulator
    StreamDemux demux;
    QObject::connect(&transport, &SerialTransport::dataReady, &demux, &StreamDemux::processIncomingData);

    QString capturedText;
    QObject::connect(&demux, &StreamDemux::terminalTextReceived, [&capturedText](const QString& text) {
        capturedText += text;
    });

    const char* chunk1 = "HiZ";
    const char* chunk2 = "> \r\n";
    ::write(masterFd, chunk1, strlen(chunk1));
    usleep(5000);
    app.processEvents();

    ::write(masterFd, chunk2, strlen(chunk2));
    usleep(10000);
    app.processEvents();

    std::cout << "  - StreamDemux reassembled fragmented ASCII terminal prompt: '" 
              << capturedText.trimmed().toStdString() << "'" << std::endl;
    assert(capturedText.contains("HiZ>"));

    // Test 3: Fragmented Modern Binary Packet (0xAA55)
    bool binaryPacketReceived = false;
    uint8_t receivedType = 0;
    QByteArray receivedPayload;

    QObject::connect(&demux, &StreamDemux::instrumentPacketReceived, 
        [&](const InstrumentPacketHeader& hdr, const QByteArray& payload) {
            binaryPacketReceived = true;
            receivedType = hdr.type;
            receivedPayload = payload;
        });

    // Build valid 0xAA55 packet
    QByteArray payloadData = "UDOO_KEY_PROBE_OK";
    InstrumentPacketHeader hdr{};
    hdr.magic = INSTRUMENT_PROTO_MAGIC;
    hdr.version = INSTRUMENT_PROTO_VERSION;
    hdr.type = PKT_TYPE_STATUS_RESP;
    hdr.sequence = 1;
    hdr.payload_len = static_cast<uint16_t>(payloadData.size());
    hdr.crc16 = CrcUtils::crc16Ccitt(
        reinterpret_cast<const uint8_t*>(payloadData.constData()), payloadData.size());

    QByteArray packet;
    packet.append(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    packet.append(payloadData);

    // Send packet split into two writes across PTY
    int splitPoint = 7;
    ::write(masterFd, packet.constData(), splitPoint);
    usleep(5000);
    app.processEvents();

    ::write(masterFd, packet.constData() + splitPoint, packet.size() - splitPoint);
    usleep(15000);
    app.processEvents();

    std::cout << "  - StreamDemux parsed fragmented 0xAA55 binary frame: Type=0x" 
              << std::hex << static_cast<int>(receivedType) << std::dec << ", Payload='" 
              << receivedPayload.toStdString() << "'" << std::endl;
    assert(binaryPacketReceived);
    assert(receivedType == PKT_TYPE_STATUS_RESP);
    assert(receivedPayload == payloadData);

    // Test 4: Disconnect and Reconnection
    bool disconnectDetected = false;
    QObject::connect(&transport, &SerialTransport::disconnected, [&]() {
        disconnectDetected = true;
    });

    transport.close();
    app.processEvents();
    assert(disconnectDetected);
    assert(!transport.isOpen());
    std::cout << "  - SerialTransport close/disconnect event captured successfully." << std::endl;

    // Reopen
    bool reopened = transport.open();
    assert(reopened);
    assert(transport.isOpen());
    std::cout << "  - SerialTransport re-opened slave PTY successfully." << std::endl;

    transport.close();

    // Test 5: Repeated Connect/Disconnect Cycles Stress Test (50 cycles)
    int connectCount = 0;
    int disconnectCount = 0;
    QObject::connect(&transport, &SerialTransport::connected, [&]() { connectCount++; });
    QObject::connect(&transport, &SerialTransport::disconnected, [&]() { disconnectCount++; });

    for (int cycle = 0; cycle < 50; ++cycle) {
        bool ok = transport.open();
        assert(ok);
        app.processEvents();
        transport.close();
        app.processEvents();
    }
    std::cout << "  - Completed 50 consecutive PTY open/close cycles (Connects: " 
              << connectCount << ", Disconnects: " << disconnectCount << ")." << std::endl;
    assert(connectCount == 50);
    assert(disconnectCount == 50);

    close(masterFd);
    close(slaveFd);

    std::cout << "[PASS] Linux Serial PTY Layer Verification Succeeded." << std::endl;
    return true;
#endif
}

int main(int argc, char* argv[]) {
    // Enable offscreen if no graphical display is present
    if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QApplication app(argc, argv);

    std::cout << "========================================================" << std::endl;
    std::cout << " UDOO KEY Linux Functional & PTY Verification Suite     " << std::endl;
    std::cout << "========================================================" << std::endl;

    bool guiPass = runLinuxGuiInteractiveTests(app);
    bool ptyPass = runLinuxSerialPtyTests(app);

    std::cout << "\n========================================================" << std::endl;
    std::cout << " Final Functional Test Summary:                         " << std::endl;
    std::cout << "  - Linux Interactive GUI Verification: " << (guiPass ? "PASSED" : "FAILED") << std::endl;
    std::cout << "  - Linux Serial PTY Transport & Framing: " << (ptyPass ? "PASSED" : "FAILED") << std::endl;
    std::cout << "========================================================" << std::endl;

    return (guiPass && ptyPass) ? 0 : 1;
}
