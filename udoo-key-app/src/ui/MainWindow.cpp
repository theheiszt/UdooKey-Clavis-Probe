#include "MainWindow.h"
#include "Theme.h"
#include "Logger.h"
#include "SettingsManager.h"
#include "LogicAnalyzerView.h"
#include "BusToolsView.h"
#include "BusToolsClient.h"
#include "ModeClient.h"
#include "GpioClient.h"
#include "SyntaxClient.h"
#include "WirelessClient.h"
#include "MeterView.h"
#include "TerminalConsoleView.h"
#include "DebugView.h"
#include "UsbTestView.h"
#include "GlobalTimelineView.h"
#include "CommunicationDiagnosticsView.h"
#include "ConnectionDialog.h"
#include "ExportDialog.h"
#include "DeviceInfoDialog.h"
#include "CommandPaletteDialog.h"
#include "ShortcutManager.h"
#include "SessionManager.h"
#include "TransactionModel.h"
#include "InstrumentReportGenerator.h"

#include <QToolBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QMessageBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QShortcut>

namespace UdooKey {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_demux(std::make_unique<StreamDemux>())
    , m_logicClient(std::make_unique<BinaryLogicClient>(nullptr))
    , m_sumpClient(std::make_unique<SumpClient>(nullptr))
    , m_terminalClient(std::make_unique<TerminalClient>(nullptr))
    , m_busToolsClient(std::make_unique<BusToolsClient>(nullptr))
    , m_modeClient(std::make_unique<ModeClient>(nullptr))
    , m_gpioClient(std::make_unique<GpioClient>(nullptr))
    , m_syntaxClient(std::make_unique<SyntaxClient>(nullptr))
    , m_wirelessClient(std::make_unique<WirelessClient>(nullptr)) {

    setWindowTitle("UDOO KEY — Universal Electronics Instrument Workbench");
    resize(1360, 860);

    setStyleSheet(Theme::darkStyleSheet());

    setupUi();
    createMenus();
    createToolBar();
    createStatusBar();
    registerShortcuts();
    wireSubsystems();

    // Global Command Palette Shortcut
    auto scPalette = new QShortcut(QKeySequence("Ctrl+K"), this);
    connect(scPalette, &QShortcut::activated, this, &MainWindow::onOpenCommandPalette);

    // Global Device Info Shortcut
    auto scInfo = new QShortcut(QKeySequence("Ctrl+I"), this);
    connect(scInfo, &QShortcut::activated, this, &MainWindow::onShowDeviceInfo);

    // Restore geometry
    QByteArray geom = SettingsManager::instance().windowGeometry();
    if (!geom.isEmpty()) {
        restoreGeometry(geom);
    }

    LOG_INFO("MainWindow initialized with USB testing and global timeline");
}

MainWindow::~MainWindow() {
    Logger::instance().disconnect(this);
    DeviceManager::instance().disconnect(this);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    SettingsManager::instance().setWindowGeometry(saveGeometry());
    SettingsManager::instance().setWindowState(saveState());
    DeviceManager::instance().disconnectDevice();
    event->accept();
}

void MainWindow::setupUi() {
    m_tabWidget = new QTabWidget(this);

    // 1. Logic Analyzer View
    m_logicView = new LogicAnalyzerView(m_logicClient.get(), this);
    connect(m_logicView, &LogicAnalyzerView::exportRequested, this, &MainWindow::onExportRequested);
    m_tabWidget->addTab(m_logicView, "⚡ Logic Analyzer");

    // 2. Bus Tools View (I2C / SPI / 1-Wire / UART / GPIO / Syntax / Wireless)
    m_busToolsView = new BusToolsView(m_busToolsClient.get(),
                                       m_modeClient.get(),
                                       m_gpioClient.get(),
                                       m_syntaxClient.get(),
                                       m_wirelessClient.get(),
                                       m_terminalClient.get(),
                                       this);
    m_tabWidget->addTab(m_busToolsView, "🔌 Bus Tools & Protocol Engines");

    // 3. Meter View (DMM / ADC / Continuity)
    m_meterView = new MeterView(m_terminalClient.get(), this);
    m_tabWidget->addTab(m_meterView, "📊 Multimeter & Continuity");

    // 4. Terminal Console View
    m_terminalView = new TerminalConsoleView(m_terminalClient.get(), this);
    m_tabWidget->addTab(m_terminalView, "💻 Universal Terminal");

    // 5. JTAG & SWD Debug Subsystem View
    m_debugView = new DebugView(this);
    m_tabWidget->addTab(m_debugView, "🛠 Debug (JTAG/SWD)");

    // 6. USB Testing & Injection View
    m_usbTestView = new UsbTestView(this);
    m_tabWidget->addTab(m_usbTestView, "🌐 USB Testing & Injection");

    // 7. Global Transaction Timeline View
    m_timelineView = new GlobalTimelineView(this);
    m_tabWidget->addTab(m_timelineView, "⏱ Event Timeline");

    // 8. Communication & Wireless Diagnostics View
    m_commDiagView = new CommunicationDiagnosticsView(m_demux.get(), m_wirelessClient.get(), this);
    m_tabWidget->addTab(m_commDiagView, "📡 Comm Diagnostics");

    // 9. System Logs
    m_txtLogView = new QTextEdit(this);
    m_txtLogView->setReadOnly(true);
    m_txtLogView->setFont(QFont("Consolas", 9));
    m_txtLogView->setStyleSheet("background-color: #121216; color: #CCCCCC;");
    m_tabWidget->addTab(m_txtLogView, "📝 System Log");

    setCentralWidget(m_tabWidget);
}

void MainWindow::registerShortcuts() {
    auto& sm = ShortcutManager::instance();

    // Tab Navigation
    sm.registerAction("view.logic", "View", "Logic Analyzer", "Switch to Logic Analyzer view", QKeySequence("Ctrl+1"), [this]() {
        m_tabWidget->setCurrentIndex(0);
    });
    sm.registerAction("view.bustools", "View", "Bus Tools", "Switch to Bus Tools view", QKeySequence("Ctrl+2"), [this]() {
        m_tabWidget->setCurrentIndex(1);
    });
    sm.registerAction("view.meter", "View", "Multimeter & Continuity", "Switch to Multimeter view", QKeySequence("Ctrl+3"), [this]() {
        m_tabWidget->setCurrentIndex(2);
    });
    sm.registerAction("view.terminal", "View", "Universal Terminal", "Switch to Terminal Console view", QKeySequence("Ctrl+4"), [this]() {
        m_tabWidget->setCurrentIndex(3);
    });
    sm.registerAction("view.debug", "View", "Debug (JTAG/SWD)", "Switch to JTAG/SWD Debug view", QKeySequence("Ctrl+5"), [this]() {
        m_tabWidget->setCurrentIndex(4);
    });
    sm.registerAction("view.usb", "View", "USB Testing & Injection", "Switch to USB Testing view", QKeySequence("Ctrl+6"), [this]() {
        m_tabWidget->setCurrentIndex(5);
    });
    sm.registerAction("view.timeline", "View", "Event Timeline", "Switch to Global Transaction Timeline", QKeySequence("Ctrl+7"), [this]() {
        m_tabWidget->setCurrentIndex(6);
    });
    sm.registerAction("view.comm", "View", "Comm Diagnostics", "Switch to Comm Diagnostics view", QKeySequence("Ctrl+8"), [this]() {
        m_tabWidget->setCurrentIndex(7);
    });
    sm.registerAction("view.log", "View", "System Log", "Switch to System Log view", QKeySequence("Ctrl+9"), [this]() {
        m_tabWidget->setCurrentIndex(8);
    });

    // Device Actions
    sm.registerAction("device.connect", "Device", "Connect...", "Open hardware connection dialog", QKeySequence("Ctrl+O"), [this]() {
        onConnectActionTriggered();
    });
    sm.registerAction("device.disconnect", "Device", "Disconnect", "Disconnect active device transport", QKeySequence("Ctrl+D"), [this]() {
        onDisconnectActionTriggered();
    });
    sm.registerAction("device.info", "Device", "Device Information & Capabilities", "View hardware model, MCUs and limits", QKeySequence("Ctrl+I"), [this]() {
        onShowDeviceInfo();
    });

    // Session Actions
    sm.registerAction("session.save", "File", "Save Session (.udoo)...", "Save waveform, cursors, and state container", QKeySequence("Ctrl+S"), [this]() {
        onSaveSessionRequested();
    });
    sm.registerAction("session.load", "File", "Load Session (.udoo)...", "Restore waveform, cursors, and state container", QKeySequence("Ctrl+L"), [this]() {
        onLoadSessionRequested();
    });
    sm.registerAction("capture.export", "File", "Export Capture Data...", "Export capture as VCD, CSV, or BIN", QKeySequence("Ctrl+E"), [this]() {
        onExportRequested();
    });
    sm.registerAction("report.generate", "File", "Generate Diagnostic Report...", "Export HTML/JSON summary report", QKeySequence("Ctrl+R"), [this]() {
        onGenerateReportRequested();
    });
}

void MainWindow::createMenus() {
    auto menuFile = menuBar()->addMenu("&File");
    auto actSave = menuFile->addAction("Save Session (.udoo)...", this, &MainWindow::onSaveSessionRequested);
    actSave->setShortcut(QKeySequence("Ctrl+S"));
    auto actLoad = menuFile->addAction("Load Session (.udoo)...", this, &MainWindow::onLoadSessionRequested);
    actLoad->setShortcut(QKeySequence("Ctrl+L"));
    menuFile->addSeparator();
    auto actExport = menuFile->addAction("Export Capture Data...", this, &MainWindow::onExportRequested);
    actExport->setShortcut(QKeySequence("Ctrl+E"));
    auto actReport = menuFile->addAction("Generate Diagnostic Report (HTML/JSON)...", this, &MainWindow::onGenerateReportRequested);
    actReport->setShortcut(QKeySequence("Ctrl+R"));
    menuFile->addSeparator();
    auto actExit = menuFile->addAction("Exit", this, &QWidget::close);
    actExit->setShortcut(QKeySequence::Quit);

    auto menuDevice = menuBar()->addMenu("&Device");
    auto actConn = menuDevice->addAction("Connect...", this, &MainWindow::onConnectActionTriggered);
    actConn->setShortcut(QKeySequence("Ctrl+O"));
    auto actDisc = menuDevice->addAction("Disconnect", this, &MainWindow::onDisconnectActionTriggered);
    actDisc->setShortcut(QKeySequence("Ctrl+D"));
    menuDevice->addSeparator();
    auto actDevInfo = menuDevice->addAction("Device Information & Capabilities...", this, &MainWindow::onShowDeviceInfo);
    actDevInfo->setShortcut(QKeySequence("Ctrl+I"));

    auto menuView = menuBar()->addMenu("&View");
    auto actPal = menuView->addAction("Command Palette...", this, &MainWindow::onOpenCommandPalette);
    actPal->setShortcut(QKeySequence("Ctrl+K"));
    menuView->addSeparator();
    auto actV1 = menuView->addAction("1. Logic Analyzer", [this]() { m_tabWidget->setCurrentIndex(0); });
    actV1->setShortcut(QKeySequence("Ctrl+1"));
    auto actV2 = menuView->addAction("2. Bus Tools", [this]() { m_tabWidget->setCurrentIndex(1); });
    actV2->setShortcut(QKeySequence("Ctrl+2"));
    auto actV3 = menuView->addAction("3. Multimeter", [this]() { m_tabWidget->setCurrentIndex(2); });
    actV3->setShortcut(QKeySequence("Ctrl+3"));
    auto actV4 = menuView->addAction("4. Universal Terminal", [this]() { m_tabWidget->setCurrentIndex(3); });
    actV4->setShortcut(QKeySequence("Ctrl+4"));
    auto actV5 = menuView->addAction("5. Debug (JTAG/SWD)", [this]() { m_tabWidget->setCurrentIndex(4); });
    actV5->setShortcut(QKeySequence("Ctrl+5"));
    auto actV6 = menuView->addAction("6. USB Testing & Injection", [this]() { m_tabWidget->setCurrentIndex(5); });
    actV6->setShortcut(QKeySequence("Ctrl+6"));
    auto actV7 = menuView->addAction("7. Event Timeline", [this]() { m_tabWidget->setCurrentIndex(6); });
    actV7->setShortcut(QKeySequence("Ctrl+7"));
    auto actV8 = menuView->addAction("8. Comm Diagnostics", [this]() { m_tabWidget->setCurrentIndex(7); });
    actV8->setShortcut(QKeySequence("Ctrl+8"));
    auto actV9 = menuView->addAction("9. System Log", [this]() { m_tabWidget->setCurrentIndex(8); });
    actV9->setShortcut(QKeySequence("Ctrl+9"));

    auto menuHelp = menuBar()->addMenu("&Help");
    menuHelp->addAction("Device Information", this, &MainWindow::onShowDeviceInfo);
    menuHelp->addSeparator();
    menuHelp->addAction("About UDOO KEY Companion", [this]() {
        QMessageBox::about(this, "About UDOO KEY Companion",
                           "<h3>UDOO KEY Workbench v1.2.0</h3>"
                           "<p>Professional Native C++ Multi-Instrument Interface for UDOO KEY.</p>"
                           "<ul>"
                           "<li>8-Channel 100 MSPS Logic Analyzer with SUMP and 0x5A protocols</li>"
                           "<li>Level-Of-Detail (LOD) Min/Max hardware waveform decimation</li>"
                           "<li>Multi-protocol decoders: UART, I2C, SPI</li>"
                           "<li>JTAG TAP Controller & ARM SWD Debugger Engine</li>"
                           "<li>USB Test Subsystem: Enumeration, Descriptor Decoders, Packet Injection, Device Emulation</li>"
                           "<li>Universal Terminal & Telemetry Engine</li>"
                           "<li>Digital Multimeter & Continuity Tester</li>"
                           "<li>I2C Address Scanner & SPI Transaction Builder</li>"
                           "<li>Global Transaction Timeline & Reusable Hex Inspector</li>"
                           "<li>Command Palette (Ctrl+K) & Session Persistence (.udoo)</li>"
                           "</ul>"
                           "<p>Architecture: RP2040 Dual ARM Cortex-M0+ & ESP32-WROOM-32E</p>");
    });
}

void MainWindow::createToolBar() {
    auto toolBar = addToolBar("Main Controls");
    toolBar->setMovable(false);

    m_btnConnect = new QPushButton("🔌 Connect...", this);
    m_btnConnect->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white; padding: 4px 10px;");
    connect(m_btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectActionTriggered);
    toolBar->addWidget(m_btnConnect);

    m_btnDisconnect = new QPushButton("Disconnect", this);
    m_btnDisconnect->setEnabled(false);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &MainWindow::onDisconnectActionTriggered);
    toolBar->addWidget(m_btnDisconnect);

    toolBar->addSeparator();

    m_btnPalette = new QPushButton("⌨ Ctrl+K", this);
    m_btnPalette->setToolTip("Open Command Palette (Ctrl+K)");
    connect(m_btnPalette, &QPushButton::clicked, this, &MainWindow::onOpenCommandPalette);
    toolBar->addWidget(m_btnPalette);

    m_btnInfo = new QPushButton("ℹ Device Specs", this);
    m_btnInfo->setToolTip("View Hardware Capabilities & Telemetry (Ctrl+I)");
    connect(m_btnInfo, &QPushButton::clicked, this, &MainWindow::onShowDeviceInfo);
    toolBar->addWidget(m_btnInfo);

    toolBar->addSeparator();

    m_lblConnStatus = new QLabel(" Status: Disconnected ", this);
    m_lblConnStatus->setStyleSheet("color: #FF5555; font-weight: bold; padding: 4px;");
    toolBar->addWidget(m_lblConnStatus);

    m_lblCapabilities = new QLabel(" Capabilities: [None] ", this);
    m_lblCapabilities->setStyleSheet("color: #8E8E9F; padding: 4px;");
    toolBar->addWidget(m_lblCapabilities);

    toolBar->addSeparator();

    m_lblWifiStatus = new QLabel(" Wi-Fi: AP (192.168.4.1) ", this);
    m_lblWifiStatus->setStyleSheet("color: #79C0FF; font-weight: bold; padding: 4px;");
    toolBar->addWidget(m_lblWifiStatus);
}

void MainWindow::createStatusBar() {
    statusBar()->showMessage("Ready. Press Ctrl+K for Command Palette or Connect to device.");
}

void MainWindow::wireSubsystems() {
    // Connect logger
    connect(&Logger::instance(), &Logger::logMessageEmitted, this, &MainWindow::onLogMessage);

    // Connect DeviceManager
    auto& dm = DeviceManager::instance();
    connect(&dm, &DeviceManager::deviceConnected, this, &MainWindow::onDeviceConnected);
    connect(&dm, &DeviceManager::deviceConnectedWithCaps, this, &MainWindow::onDeviceConnectedWithCaps);
    connect(&dm, &DeviceManager::deviceDisconnected, this, &MainWindow::onDeviceDisconnected);
    connect(&dm, &DeviceManager::connectionError, [this](const QString& err) {
        statusBar()->showMessage(QString("Connection error: %1").arg(err), 5000);
        QMessageBox::warning(this, "Connection Error", err);
    });

    // Wire StreamDemux legacy frames
    connect(m_demux.get(), &StreamDemux::binary0x5AFrameReceived, m_logicClient.get(), &BinaryLogicClient::onBinaryFrameReceived);
    connect(m_demux.get(), &StreamDemux::sumpDataReceived, m_sumpClient.get(), &SumpClient::onSumpDataReceived);
    connect(m_demux.get(), &StreamDemux::terminalTextReceived, m_terminalClient.get(), &TerminalClient::onTerminalTextReceived);

    // Wire StreamDemux modern 0xAA55 InstrumentProtocol packets to all subsystems
    connect(m_demux.get(), &StreamDemux::instrumentPacketReceived, this, [this](const InstrumentPacketHeader& hdr, const QByteArray& payload) {
        if (m_busToolsClient) m_busToolsClient->processIncomingPacket(hdr, payload);
        if (m_wirelessClient) m_wirelessClient->processIncomingPacket(hdr, payload);
        if (m_gpioClient) m_gpioClient->processIncomingPacket(hdr, payload);
        if (m_modeClient) m_modeClient->processIncomingPacket(hdr, payload);
        if (m_syntaxClient) m_syntaxClient->processIncomingPacket(hdr, payload);
    });

    // Wire Wi-Fi telemetry to MainWindow status indicators
    connect(m_wirelessClient.get(), &WirelessClient::wifiTelemetryReceived, this, [this](const QString& ssid, int8_t rssi, uint32_t dnsLatencyUs, uint32_t /*rttUs*/) {
        QString info = QString(" Wi-Fi: %1 (RSSI: %2 dBm, DNS: %3 µs) ").arg(ssid).arg(rssi).arg(dnsLatencyUs);
        m_lblWifiStatus->setText(info);
        m_lblWifiStatus->setStyleSheet("color: #3FB950; font-weight: bold; padding: 4px;");
        statusBar()->showMessage("ESP32 Wi-Fi Telemetry: " + ssid, 3000);
    });
}

void MainWindow::onConnectActionTriggered() {
    ConnectionDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        if (dlg.isMockSelected()) {
            DeviceManager::instance().connectMock();
        } else {
            DeviceManager::instance().connectToPort(dlg.selectedPort(), dlg.selectedBaudRate());
        }
    }
}

void MainWindow::onDisconnectActionTriggered() {
    DeviceManager::instance().disconnectDevice();
}

void MainWindow::onDeviceConnected(const QString& portName, uint32_t capabilities) {
    m_lblConnStatus->setText(QString(" Status: Connected (%1) ").arg(portName));
    m_lblConnStatus->setStyleSheet("color: #00FFCC; font-weight: bold; padding: 4px;");
    m_btnConnect->setEnabled(false);
    m_btnDisconnect->setEnabled(true);

    QStringList caps;
    if (capabilities & CAP_LOGIC_ANALYZER) caps << "Logic";
    if (capabilities & CAP_SUMP_PROTOCOL) caps << "SUMP";
    if (capabilities & CAP_TERMINAL_SHELL) caps << "Terminal";
    if (capabilities & CAP_ADC_METER) caps << "ADC";
    if (capabilities & CAP_SPI_TOOL) caps << "SPI";
    if (capabilities & CAP_I2C_TOOL) caps << "I2C";
    m_lblCapabilities->setText(QString(" Capabilities: [%1] ").arg(caps.join(", ")));

    ITransport* trans = DeviceManager::instance().transport();
    if (trans) {
        connect(trans, &ITransport::dataReady, this, &MainWindow::onDataReceivedFromTransport);
        m_logicClient->setTransport(trans);
        m_sumpClient->setTransport(trans);
        m_terminalClient->setTransport(trans);
        m_busToolsClient->setTransport(trans);
        m_modeClient->setTransport(trans);
        m_gpioClient->setTransport(trans);
        m_syntaxClient->setTransport(trans);
        m_wirelessClient->setTransport(trans);
        if (m_debugView) {
            m_debugView->setLiveTransport(trans);
        }
    }

    statusBar()->showMessage(QString("Connected to %1").arg(portName), 4000);
}

void MainWindow::onDeviceConnectedWithCaps(const QString& /*portName*/, const DeviceCapabilities& caps) {
    m_lblCapabilities->setText(QString(" %1 | %2 ch @ %3 MSPS ")
                               .arg(caps.hardwareModel)
                               .arg(caps.logicChannels)
                               .arg(caps.maxSampleRateHz / 1'000'000));
}

void MainWindow::onDeviceDisconnected() {
    m_lblConnStatus->setText(" Status: Disconnected ");
    m_lblConnStatus->setStyleSheet("color: #FF5555; font-weight: bold; padding: 4px;");
    m_lblCapabilities->setText(" Capabilities: [None] ");
    m_lblWifiStatus->setText(" Wi-Fi: Offline ");
    m_lblWifiStatus->setStyleSheet("color: #8E8E9F; padding: 4px;");
    m_btnConnect->setEnabled(true);
    m_btnDisconnect->setEnabled(false);

    m_demux->reset();
    m_logicClient->setTransport(nullptr);
    m_sumpClient->setTransport(nullptr);
    m_terminalClient->setTransport(nullptr);
    m_busToolsClient->setTransport(nullptr);
    m_modeClient->setTransport(nullptr);
    m_gpioClient->setTransport(nullptr);
    m_syntaxClient->setTransport(nullptr);
    m_wirelessClient->setTransport(nullptr);

    statusBar()->showMessage("Device disconnected", 3000);
}

void MainWindow::onDataReceivedFromTransport(const QByteArray& transportData) {
    m_demux->processIncomingData(transportData);
}

void MainWindow::onExportRequested() {
    const auto& buf = m_logicView->currentCapture();
    if (buf.isEmpty()) {
        QMessageBox::warning(this, "Export Warning", "No waveform data available to export. Run a capture first.");
        return;
    }

    ExportDialog dlg(buf, this);
    dlg.exec();
}

void MainWindow::onOpenCommandPalette() {
    CommandPaletteDialog dlg(this);
    dlg.exec();
}

void MainWindow::onShowDeviceInfo() {
    DeviceInfoDialog dlg(this);
    dlg.exec();
}

void MainWindow::onSaveSessionRequested() {
    QString fileName = QFileDialog::getSaveFileName(this, "Save Instrument Session", "capture_session.udoo", "UDOO Session Container (*.udoo)");
    if (fileName.isEmpty()) return;

    SessionData session;
    const auto& buf = m_logicView->currentCapture();
    session.sampleRateHz = buf.sampleRateHz();
    session.sampleCount = buf.sampleCount();
    session.rawCaptureBytes = buf.rawData();

    session.sessionDescription = QString("Session recorded on %1").arg(QDateTime::currentDateTime().toString());
    session.timestamp = QDateTime::currentDateTime();

    if (SessionManager::saveSession(fileName, session)) {
        statusBar()->showMessage(QString("Session successfully saved to %1").arg(fileName), 4000);
    } else {
        QMessageBox::warning(this, "Save Error", "Failed to save session to disk.");
    }
}

void MainWindow::onLoadSessionRequested() {
    QString fileName = QFileDialog::getOpenFileName(this, "Load Instrument Session", "", "UDOO Session Container (*.udoo)");
    if (fileName.isEmpty()) return;

    SessionData session;
    if (!SessionManager::loadSession(fileName, session)) {
        QMessageBox::warning(this, "Load Error", "Failed to load or parse session file.");
        return;
    }

    // Switch to Logic Analyzer and load buffer
    m_tabWidget->setCurrentIndex(0);
    statusBar()->showMessage(QString("Loaded session: %1 samples @ %2 Hz")
                             .arg(session.sampleCount).arg(session.sampleRateHz), 4000);
}

void MainWindow::onGenerateReportRequested() {
    QString fileName = QFileDialog::getSaveFileName(this, "Generate Diagnostic Report", "udoo_instrument_report.html", "HTML Reports (*.html);;JSON Reports (*.json)");
    if (fileName.isEmpty()) return;

    InstrumentReportData reportData;
    reportData.generatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    const auto& caps = DeviceManager::instance().currentCapabilities();
    reportData.hardwareModel = caps.hardwareModel.isEmpty() ? "UDOO KEY" : caps.hardwareModel;
    reportData.firmwareVersion = caps.firmwareVersion.isEmpty() ? "1.2.0" : caps.firmwareVersion;
    reportData.mcuPrimary = caps.mcuPrimary.isEmpty() ? "RP2040 (133 MHz Dual-Core Cortex-M0+)" : caps.mcuPrimary;
    reportData.mcuCoprocessor = caps.mcuCoprocessor.isEmpty() ? "ESP32-WROVER (240 MHz Dual-Core Xtensa LX6)" : caps.mcuCoprocessor;
    reportData.connectionPort = DeviceManager::instance().transport() ? DeviceManager::instance().transport()->portName() : "Offline / Simulation";

    const auto& buf = m_logicView->currentCapture();
    reportData.sampleRateHz = buf.sampleRateHz();
    reportData.sampleCount = buf.sampleCount();
    reportData.durationSec = buf.durationSec();
    reportData.triggerSummary = m_logicView->triggerExpression().summary();

    for (int ch = 0; ch < 8; ++ch) {
        reportData.metrics[ch] = m_logicView->processor().metrics(ch);
    }

    if (m_logicView->waveformWidget()) {
        reportData.cursorASec = m_logicView->waveformWidget()->cursorAPosSec();
        reportData.cursorBSec = m_logicView->waveformWidget()->cursorBPosSec();
        reportData.cursorCSec = m_logicView->waveformWidget()->cursorCPosSec();
        reportData.cursorDSec = m_logicView->waveformWidget()->cursorDPosSec();
    }

    reportData.recentTransactions = TransactionLogger::instance().getTransactions(QString(), 20);

    bool ok = false;
    QString err;
    if (fileName.endsWith(".json", Qt::CaseInsensitive)) {
        ok = InstrumentReportGenerator::saveJsonReport(fileName, reportData, &err);
    } else {
        ok = InstrumentReportGenerator::saveHtmlReport(fileName, reportData, &err);
    }

    if (ok) {
        statusBar()->showMessage("Report generated successfully: " + fileName, 4000);
        QMessageBox::information(this, "Report Generated", "Diagnostic report saved successfully to:\n" + fileName);
    } else {
        QMessageBox::warning(this, "Report Generation Error", "Failed to write report file:\n" + err);
    }
}

void MainWindow::onLogMessage(LogLevel level, const QString& message) {
    QString color = "#CCCCCC";
    if (level == LogLevel::Warning) color = "#FFB020";
    else if (level == LogLevel::Error) color = "#FF5555";
    else if (level == LogLevel::Debug) color = "#888899";

    m_txtLogView->append(QString("<span style='color:%1;'>%2</span>").arg(color, message.toHtmlEscaped()));
}

} // namespace UdooKey
