#include "BusToolsView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFont>
#include <QMessageBox>

namespace UdooKey {

BusToolsView::BusToolsView(BusToolsClient* busTools,
                         ModeClient* mode,
                         GpioClient* gpio,
                         SyntaxClient* syntax,
                         WirelessClient* wireless,
                         TerminalClient* terminal,
                         QWidget* parent)
    : QWidget(parent) {
    setupUi();
    setClients(busTools, mode, gpio, syntax, wireless, terminal);
}

void BusToolsView::setClients(BusToolsClient* busTools,
                              ModeClient* mode,
                              GpioClient* gpio,
                              SyntaxClient* syntax,
                              WirelessClient* wireless,
                              TerminalClient* terminal) {
    m_busToolsClient = busTools;
    m_modeClient     = mode;
    m_gpioClient     = gpio;
    m_syntaxClient   = syntax;
    m_wirelessClient = wireless;
    m_terminalClient = terminal;

    if (m_modeClient) {
        connect(m_modeClient, &ModeClient::modeStatusReceived, this, &BusToolsView::onModeStatusReceived);
    }

    if (m_busToolsClient) {
        connect(m_busToolsClient, &BusToolsClient::i2cScanCompleted, this, &BusToolsView::onI2cScanCompleted);
        connect(m_busToolsClient, &BusToolsClient::i2cTransferCompleted, this, &BusToolsView::onI2cTransferCompleted);
        connect(m_busToolsClient, &BusToolsClient::spiTransferCompleted, this, &BusToolsView::onSpiTransferCompleted);
        connect(m_busToolsClient, &BusToolsClient::spiFlashInfoReceived, this, &BusToolsView::onSpiFlashInfoReceived);
        connect(m_busToolsClient, &BusToolsClient::oneWireSearchCompleted, this, &BusToolsView::onOneWireSearchCompleted);
        connect(m_busToolsClient, &BusToolsClient::oneWireTransferCompleted, this, &BusToolsView::onOneWireTransferCompleted);
        connect(m_busToolsClient, &BusToolsClient::uartDataReceived, this, &BusToolsView::onUartDataReceived);
    }

    if (m_gpioClient) {
        connect(m_gpioClient, &GpioClient::pinStatusReceived, this, &BusToolsView::onGpioStatusReceived);
        connect(m_gpioClient, &GpioClient::allPinsReceived, [this](const InstrumentGpioReadAllPayload& st) {
            if (m_lblGpioLevels) {
                uint8_t levels = st.values_mask;
                QString binStr = QString("%1").arg(levels, 8, 2, QChar('0'));
                m_lblGpioLevels->setText(QString("Probe Bus Levels (GP9..GP2): 0b%1 (0x%2)")
                                        .arg(binStr)
                                        .arg(levels, 2, 16, QChar('0')).toUpper());
            }
        });
    }

    if (m_syntaxClient) {
        connect(m_syntaxClient, &SyntaxClient::syntaxExecuted, this, &BusToolsView::onSyntaxResultReceived);
    }

    if (m_wirelessClient) {
        connect(m_wirelessClient, &WirelessClient::irRxEventReceived, this, &BusToolsView::onIrRxEventReceived);
        connect(m_wirelessClient, &WirelessClient::nrf24StatusReceived, this, &BusToolsView::onNrf24StatusReceived);
        connect(m_wirelessClient, &WirelessClient::nrf24RxEventReceived, this, &BusToolsView::onNrf24RxEventReceived);
        connect(m_wirelessClient, &WirelessClient::nrf24TxStatusReceived, [this](const InstrumentNrf24TxStatusPayload& st) {
            if (m_txtNrf24Log) {
                QString color = (st.status == 0) ? "#00D2A0" : "#FF5555";
                m_txtNrf24Log->append(QString("<b style='color:%1;'>[TX STATUS]</b> Status: %2 (Retries: %3)")
                                      .arg(color).arg(st.status == 0 ? "Delivered" : "Failed")
                                      .arg(st.retries_count));
            }
        });
    }
}

void BusToolsView::setupUi() {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Top: Mode Multiplexer & Invariants Status Bar
    layout->addWidget(createModeBar());

    // Center: Multi-Subsystem Tab Widget
    auto tabWidget = new QTabWidget(this);
    tabWidget->addTab(createI2cTab(), "I2C Bus Explorer");
    tabWidget->addTab(createSpiTab(), "SPI Master & Flash");
    tabWidget->addTab(createOneWireTab(), "1-Wire Master");
    tabWidget->addTab(createUartTab(), "UART Bridge");
    tabWidget->addTab(createGpioTab(), "DIO Probe Bus");
    tabWidget->addTab(createSyntaxTab(), "Interactive Syntax");
    tabWidget->addTab(createWirelessTab(), "Wireless (IR / nRF24)");

    layout->addWidget(tabWidget);
}

QWidget* BusToolsView::createModeBar() {
    auto bar = new QGroupBox("Hardware Mode Multiplexer & Mutual Exclusion Invariants (GP2..GP9)", this);
    auto hLayout = new QHBoxLayout(bar);
    hLayout->setContentsMargins(8, 6, 8, 6);

    hLayout->addWidget(new QLabel("Instrument Mode:", bar));
    m_comboMode = new QComboBox(bar);
    m_comboMode->addItem("Hi-Z (Idle / Floating)", INST_MODE_IDLE);
    m_comboMode->addItem("I2C Bus Master", INST_MODE_I2C);
    m_comboMode->addItem("SPI Bus Master", INST_MODE_SPI);
    m_comboMode->addItem("UART Bridge", INST_MODE_UART);
    m_comboMode->addItem("1-Wire Master", INST_MODE_ONE_WIRE);
    m_comboMode->addItem("Generic DIO", INST_MODE_DIO);
    m_comboMode->addItem("Logic Analyzer", INST_MODE_LOGIC_ANALYZER);
    connect(m_comboMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BusToolsView::onModeChangeRequested);
    hLayout->addWidget(m_comboMode);

    m_btnReleaseHiZ = new QPushButton("🛡 Safe Hi-Z Release", bar);
    m_btnReleaseHiZ->setStyleSheet("font-weight: bold; background-color: #6C5CE7; color: white;");
    connect(m_btnReleaseHiZ, &QPushButton::clicked, this, &BusToolsView::onReleaseHiZClicked);
    hLayout->addWidget(m_btnReleaseHiZ);

    m_lblActiveMode = new QLabel("Active: [IDLE / Hi-Z]", bar);
    m_lblActiveMode->setStyleSheet("color: #00D2A0; font-weight: bold; padding: 2px 6px; background-color: #121820; border-radius: 4px;");
    hLayout->addWidget(m_lblActiveMode);

    m_lblPinStatus = new QLabel("Pins (GP2..GP9): All Hi-Z Inputs", bar);
    m_lblPinStatus->setStyleSheet("color: #8E8E9F; font-size: 11px;");
    hLayout->addWidget(m_lblPinStatus);

    hLayout->addStretch();
    return bar;
}

QWidget* BusToolsView::createI2cTab() {
    auto tab = new QWidget(this);
    auto layout = new QVBoxLayout(tab);

    auto topBar = new QHBoxLayout();
    m_btnScanI2c = new QPushButton("🔍 Scan 7-Bit Address Space", tab);
    m_btnScanI2c->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white; padding: 6px 12px;");
    connect(m_btnScanI2c, &QPushButton::clicked, this, &BusToolsView::onScanI2cClicked);
    topBar->addWidget(m_btnScanI2c);

    m_lblI2cFound = new QLabel("Click 'Scan' to discover I2C peripherals...", tab);
    m_lblI2cFound->setStyleSheet("color: #00D2A0; font-weight: bold;");
    topBar->addWidget(m_lblI2cFound);
    topBar->addStretch();
    layout->addLayout(topBar);

    // 16x8 Address Matrix Table
    m_i2cGridTable = new QTableWidget(8, 16, tab);
    m_i2cGridTable->verticalHeader()->setVisible(true);
    QStringList vHeaders;
    for (int r = 0; r < 8; ++r) vHeaders << QString("0x%1_").arg(r);
    m_i2cGridTable->setVerticalHeaderLabels(vHeaders);

    QStringList hHeaders;
    for (int c = 0; c < 16; ++c) hHeaders << QString("%1").arg(c, 1, 16).toUpper();
    m_i2cGridTable->setHorizontalHeaderLabels(hHeaders);
    m_i2cGridTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 16; ++c) {
            auto item = new QTableWidgetItem("--");
            item->setTextAlignment(Qt::AlignCenter);
            item->setForeground(QColor(90, 90, 105));
            m_i2cGridTable->setItem(r, c, item);
        }
    }
    layout->addWidget(m_i2cGridTable, 1);

    // Transaction Builder
    auto xferBox = new QGroupBox("I2C Write-then-Read Transaction", tab);
    auto xferLayout = new QHBoxLayout(xferBox);
    xferLayout->addWidget(new QLabel("7-bit Addr (Hex):", tab));
    m_editI2cAddr = new QLineEdit("50", tab);
    m_editI2cAddr->setMaximumWidth(60);
    xferLayout->addWidget(m_editI2cAddr);

    xferLayout->addWidget(new QLabel("Write Hex Bytes:", tab));
    m_editI2cWriteData = new QLineEdit("00 00", tab);
    xferLayout->addWidget(m_editI2cWriteData);

    xferLayout->addWidget(new QLabel("Read Bytes:", tab));
    m_editI2cReadLen = new QLineEdit("4", tab);
    m_editI2cReadLen->setMaximumWidth(50);
    xferLayout->addWidget(m_editI2cReadLen);

    m_btnI2cXfer = new QPushButton("Execute Transfer", tab);
    m_btnI2cXfer->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    connect(m_btnI2cXfer, &QPushButton::clicked, this, &BusToolsView::onSendI2cTransferClicked);
    xferLayout->addWidget(m_btnI2cXfer);
    layout->addWidget(xferBox);

    m_txtI2cLog = new QTextEdit(tab);
    m_txtI2cLog->setReadOnly(true);
    m_txtI2cLog->setMaximumHeight(100);
    m_txtI2cLog->setFont(QFont("Consolas", 9));
    layout->addWidget(m_txtI2cLog);

    return tab;
}

QWidget* BusToolsView::createSpiTab() {
    auto tab = new QWidget(this);
    auto layout = new QVBoxLayout(tab);

    auto cfgBox = new QGroupBox("SPI Bus Parameters & Flash Interrogation", tab);
    auto cfgLayout = new QGridLayout(cfgBox);

    cfgLayout->addWidget(new QLabel("Mode:"), 0, 0);
    m_comboSpiMode = new QComboBox(tab);
    m_comboSpiMode->addItem("Mode 0 (CPOL=0, CPHA=0)", 0);
    m_comboSpiMode->addItem("Mode 1 (CPOL=0, CPHA=1)", 1);
    m_comboSpiMode->addItem("Mode 2 (CPOL=1, CPHA=0)", 2);
    m_comboSpiMode->addItem("Mode 3 (CPOL=1, CPHA=1)", 3);
    cfgLayout->addWidget(m_comboSpiMode, 0, 1);

    cfgLayout->addWidget(new QLabel("Speed (Hz):"), 0, 2);
    m_editSpiSpeed = new QLineEdit("10000000", tab);
    cfgLayout->addWidget(m_editSpiSpeed, 0, 3);

    m_btnProbeFlash = new QPushButton("🔍 JEDEC SFDP Flash Probe", tab);
    m_btnProbeFlash->setStyleSheet("font-weight: bold; background-color: #0984E3; color: white;");
    connect(m_btnProbeFlash, &QPushButton::clicked, this, &BusToolsView::onProbeFlashClicked);
    cfgLayout->addWidget(m_btnProbeFlash, 0, 4);

    cfgLayout->addWidget(new QLabel("MOSI Payload Hex:"), 1, 0);
    m_editSpiHex = new QLineEdit("9F 00 00 00", tab);
    cfgLayout->addWidget(m_editSpiHex, 1, 1, 1, 3);

    m_btnSendSpi = new QPushButton("🚀 Full-Duplex Transfer", tab);
    m_btnSendSpi->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    connect(m_btnSendSpi, &QPushButton::clicked, this, &BusToolsView::onSendSpiClicked);
    cfgLayout->addWidget(m_btnSendSpi, 1, 4);

    layout->addWidget(cfgBox);

    auto logBox = new QGroupBox("SPI Transaction & Flash Discovery Log", tab);
    auto logLayout = new QVBoxLayout(logBox);
    m_txtSpiLog = new QTextEdit(tab);
    m_txtSpiLog->setReadOnly(true);
    m_txtSpiLog->setFont(QFont("Consolas", 10));
    logLayout->addWidget(m_txtSpiLog);
    layout->addWidget(logBox);

    return tab;
}

QWidget* BusToolsView::createOneWireTab() {
    auto tab = new QWidget(this);
    auto layout = new QVBoxLayout(tab);

    auto topBar = new QHBoxLayout();
    m_btnSearch1Wire = new QPushButton("🔍 Search 1-Wire ROM Tree", tab);
    m_btnSearch1Wire->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    connect(m_btnSearch1Wire, &QPushButton::clicked, this, &BusToolsView::onSearch1WireClicked);
    topBar->addWidget(m_btnSearch1Wire);
    topBar->addStretch();
    layout->addLayout(topBar);

    layout->addWidget(new QLabel("Discovered 64-Bit Device ROM IDs:", tab));
    m_list1WireRoms = new QListWidget(tab);
    m_list1WireRoms->setMaximumHeight(120);
    layout->addWidget(m_list1WireRoms);

    auto xferBox = new QGroupBox("1-Wire Command Transfer", tab);
    auto xferLayout = new QHBoxLayout(xferBox);
    xferLayout->addWidget(new QLabel("Write Hex (e.g. CC 44 for Convert T):", tab));
    m_edit1WireWrite = new QLineEdit("CC BE", tab);
    xferLayout->addWidget(m_edit1WireWrite);

    xferLayout->addWidget(new QLabel("Read Bytes:", tab));
    m_edit1WireReadLen = new QLineEdit("9", tab);
    m_edit1WireReadLen->setMaximumWidth(50);
    xferLayout->addWidget(m_edit1WireReadLen);

    m_btn1WireXfer = new QPushButton("Transfer", tab);
    m_btn1WireXfer->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    connect(m_btn1WireXfer, &QPushButton::clicked, this, &BusToolsView::onOneWireTransferClicked);
    xferLayout->addWidget(m_btn1WireXfer);
    layout->addWidget(xferBox);

    m_txt1WireLog = new QTextEdit(tab);
    m_txt1WireLog->setReadOnly(true);
    m_txt1WireLog->setFont(QFont("Consolas", 10));
    layout->addWidget(m_txt1WireLog);

    return tab;
}

QWidget* BusToolsView::createUartTab() {
    auto tab = new QWidget(this);
    auto layout = new QVBoxLayout(tab);

    auto topBar = new QHBoxLayout();
    topBar->addWidget(new QLabel("Baud Rate:", tab));
    m_comboUartBaud = new QComboBox(tab);
    m_comboUartBaud->addItems({"9600", "19200", "38400", "57600", "115200", "230400", "921600"});
    m_comboUartBaud->setCurrentText("115200");
    topBar->addWidget(m_comboUartBaud);
    topBar->addStretch();
    layout->addLayout(topBar);

    auto sendLayout = new QHBoxLayout();
    m_editUartSend = new QLineEdit(tab);
    m_editUartSend->setPlaceholderText("Enter ASCII text or commands to send...");
    m_btnSendUart = new QPushButton("Send", tab);
    connect(m_btnSendUart, &QPushButton::clicked, this, &BusToolsView::onSendUartClicked);
    connect(m_editUartSend, &QLineEdit::returnPressed, this, &BusToolsView::onSendUartClicked);

    sendLayout->addWidget(m_editUartSend);
    sendLayout->addWidget(m_btnSendUart);
    layout->addLayout(sendLayout);

    m_txtUartLog = new QTextEdit(tab);
    m_txtUartLog->setReadOnly(true);
    m_txtUartLog->setFont(QFont("Consolas", 10));
    layout->addWidget(m_txtUartLog);

    return tab;
}

QWidget* BusToolsView::createGpioTab() {
    auto tab = new QWidget(this);
    auto layout = new QVBoxLayout(tab);

    auto gridBox = new QGroupBox("Digital I/O Probe Bus (GP2..GP9)", tab);
    auto gridLayout = new QGridLayout(gridBox);

    for (int i = 0; i < 8; ++i) {
        int pinNum = 2 + i;
        gridLayout->addWidget(new QLabel(QString("GP%1:").arg(pinNum), tab), i, 0);

        m_chkGpioDirs[i] = new QCheckBox("Output Mode", tab);
        gridLayout->addWidget(m_chkGpioDirs[i], i, 1);

        m_btnGpioPins[i] = new QPushButton("LOW (0)", tab);
        m_btnGpioPins[i]->setStyleSheet("background-color: #2D3436; color: white; font-weight: bold;");
        connect(m_btnGpioPins[i], &QPushButton::clicked, [this, i]() { onGpioPinToggled(i); });
        gridLayout->addWidget(m_btnGpioPins[i], i, 2);
    }
    layout->addWidget(gridBox);

    auto maskBox = new QGroupBox("Fast 8-Bit Mask Operations", tab);
    auto maskLayout = new QHBoxLayout(maskBox);
    m_editGpioMask = new QLineEdit("0xAA", tab);
    m_editGpioMask->setMaximumWidth(80);
    maskLayout->addWidget(new QLabel("Output Mask (Hex):", tab));
    maskLayout->addWidget(m_editGpioMask);

    m_btnGpioWriteMask = new QPushButton("Write Mask", tab);
    connect(m_btnGpioWriteMask, &QPushButton::clicked, this, &BusToolsView::onGpioWriteMaskClicked);
    maskLayout->addWidget(m_btnGpioWriteMask);

    m_btnGpioReadAll = new QPushButton("Read All Pins", tab);
    connect(m_btnGpioReadAll, &QPushButton::clicked, this, &BusToolsView::onGpioReadAllClicked);
    maskLayout->addWidget(m_btnGpioReadAll);

    m_lblGpioLevels = new QLabel("Probe Bus Levels: --", tab);
    m_lblGpioLevels->setStyleSheet("color: #00D2A0; font-weight: bold;");
    maskLayout->addWidget(m_lblGpioLevels);
    maskLayout->addStretch();
    layout->addWidget(maskBox);

    layout->addStretch();
    return tab;
}

QWidget* BusToolsView::createSyntaxTab() {
    auto tab = new QWidget(this);
    auto layout = new QVBoxLayout(tab);

    auto macroBar = new QHBoxLayout();
    macroBar->addWidget(new QLabel("Quick Macros:", tab));
    auto btnM1 = new QPushButton("Macro 1: I2C Scan", tab);
    connect(btnM1, &QPushButton::clicked, [this]() { onMacroClicked(1); });
    macroBar->addWidget(btnM1);

    auto btnM2 = new QPushButton("Macro 2: SPI Flash Probe", tab);
    connect(btnM2, &QPushButton::clicked, [this]() { onMacroClicked(2); });
    macroBar->addWidget(btnM2);

    auto btnM3 = new QPushButton("Macro 3: SWD Discovery", tab);
    connect(btnM3, &QPushButton::clicked, [this]() { onMacroClicked(3); });
    macroBar->addWidget(btnM3);
    macroBar->addStretch();
    layout->addLayout(macroBar);

    auto syntaxLayout = new QHBoxLayout();
    m_editSyntaxInput = new QLineEdit(tab);
    m_editSyntaxInput->setPlaceholderText("Enter syntax string, e.g. [ 0x50 0x00 ] [ 0x51 r:4 ] or 'v' for ADC");
    m_btnExecSyntax = new QPushButton("Execute Syntax", tab);
    m_btnExecSyntax->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    connect(m_btnExecSyntax, &QPushButton::clicked, this, &BusToolsView::onExecSyntaxClicked);
    connect(m_editSyntaxInput, &QLineEdit::returnPressed, this, &BusToolsView::onExecSyntaxClicked);

    syntaxLayout->addWidget(m_editSyntaxInput);
    syntaxLayout->addWidget(m_btnExecSyntax);
    layout->addLayout(syntaxLayout);

    m_txtSyntaxLog = new QTextEdit(tab);
    m_txtSyntaxLog->setReadOnly(true);
    m_txtSyntaxLog->setFont(QFont("Consolas", 10));
    layout->addWidget(m_txtSyntaxLog);

    return tab;
}

QWidget* BusToolsView::createWirelessTab() {
    auto tab = new QWidget(this);
    auto layout = new QVBoxLayout(tab);

    // IR Subsystem Box
    auto irBox = new QGroupBox("Infrared (IR) Engine (ESP32 IO25 TX / IO26 RX)", tab);
    auto irLayout = new QGridLayout(irBox);

    irLayout->addWidget(new QLabel("Address (Hex):"), 0, 0);
    m_editIrAddr = new QLineEdit("00FF", tab);
    irLayout->addWidget(m_editIrAddr, 0, 1);

    irLayout->addWidget(new QLabel("Command (Hex):"), 0, 2);
    m_editIrCmd = new QLineEdit("12", tab);
    irLayout->addWidget(m_editIrCmd, 0, 3);

    m_btnSendIrNec = new QPushButton("🚀 Transmit NEC Code", tab);
    m_btnSendIrNec->setStyleSheet("font-weight: bold; background-color: #E17055; color: white;");
    connect(m_btnSendIrNec, &QPushButton::clicked, this, &BusToolsView::onSendIrNecClicked);
    irLayout->addWidget(m_btnSendIrNec, 0, 4);

    m_chkIrRx = new QCheckBox("Enable Background Receiver (IO26)", tab);
    connect(m_chkIrRx, &QCheckBox::toggled, this, &BusToolsView::onToggleIrRx);
    irLayout->addWidget(m_chkIrRx, 1, 0, 1, 2);

    m_txtIrLog = new QTextEdit(tab);
    m_txtIrLog->setReadOnly(true);
    m_txtIrLog->setMaximumHeight(80);
    m_txtIrLog->setFont(QFont("Consolas", 9));
    irLayout->addWidget(m_txtIrLog, 2, 0, 1, 5);

    layout->addWidget(irBox);

    // nRF24L01 Subsystem Box
    auto nrfBox = new QGroupBox("nRF24L01 2.4 GHz RF Module (UEXT Header P3)", tab);
    auto nrfLayout = new QGridLayout(nrfBox);

    nrfLayout->addWidget(new QLabel("RF Channel (0..125):"), 0, 0);
    m_editNrf24Channel = new QLineEdit("76", tab);
    nrfLayout->addWidget(m_editNrf24Channel, 0, 1);

    nrfLayout->addWidget(new QLabel("Target Addr (Hex):"), 0, 2);
    m_editNrf24Addr = new QLineEdit("E7E7E7E701", tab);
    nrfLayout->addWidget(m_editNrf24Addr, 0, 3);

    m_btnNrf24Status = new QPushButton("Query Radio Status", tab);
    connect(m_btnNrf24Status, &QPushButton::clicked, this, &BusToolsView::onQueryNrf24StatusClicked);
    nrfLayout->addWidget(m_btnNrf24Status, 0, 4);

    nrfLayout->addWidget(new QLabel("Payload Text:"), 1, 0);
    m_editNrf24Data = new QLineEdit("HELLO_UDOO_KEY", tab);
    nrfLayout->addWidget(m_editNrf24Data, 1, 1, 1, 3);

    m_btnSendNrf24 = new QPushButton("🚀 Send RF Packet", tab);
    m_btnSendNrf24->setStyleSheet("font-weight: bold; background-color: #0984E3; color: white;");
    connect(m_btnSendNrf24, &QPushButton::clicked, this, &BusToolsView::onSendNrf24Clicked);
    nrfLayout->addWidget(m_btnSendNrf24, 1, 4);

    m_txtNrf24Log = new QTextEdit(tab);
    m_txtNrf24Log->setReadOnly(true);
    m_txtNrf24Log->setFont(QFont("Consolas", 9));
    nrfLayout->addWidget(m_txtNrf24Log, 2, 0, 1, 5);

    layout->addWidget(nrfBox);

    return tab;
}

// ─── Mode Multiplexer Slots ──────────────────────────────────────────────────
void BusToolsView::onModeChangeRequested(int index) {
    if (!m_modeClient) return;
    auto mode = static_cast<InstrumentModeId>(m_comboMode->itemData(index).toInt());
    m_modeClient->setMode(mode);
}

void BusToolsView::onReleaseHiZClicked() {
    if (m_modeClient) {
        m_modeClient->releaseToHiZ();
    }
}

void BusToolsView::onModeStatusReceived(const InstrumentModeStatusPayload& status) {
    QString name = QString::fromUtf8(status.mode_name);
    m_lblActiveMode->setText(QString("Active: [%1]").arg(name));

    QStringList pinDescs;
    for (int i = 0; i < 8; ++i) {
        pinDescs << QString("GP%1=%2").arg(2 + i).arg(m_modeClient->pinFunctionDescription(2 + i));
    }
    m_lblPinStatus->setText(pinDescs.join(" | "));
}

// ─── I2C Slots ───────────────────────────────────────────────────────────────
void BusToolsView::onScanI2cClicked() {
    if (!m_busToolsClient) return;
    m_lblI2cFound->setText("Scanning 7-bit I2C address space...");
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 16; ++c) {
            m_i2cGridTable->item(r, c)->setText("--");
            m_i2cGridTable->item(r, c)->setForeground(QColor(90, 90, 105));
            m_i2cGridTable->item(r, c)->setBackground(QColor(30, 30, 38));
        }
    }
    m_busToolsClient->scanI2c();
}

void BusToolsView::onI2cScanCompleted(const QVector<uint8_t>& addrs) {
    if (addrs.isEmpty()) {
        m_lblI2cFound->setText("No I2C devices detected on bus (check wiring / pull-ups)");
        return;
    }

    QString foundStr = "Discovered I2C Peripherals: ";
    for (uint8_t a : addrs) {
        foundStr += QString("0x%1 ").arg(a, 2, 16, QChar('0')).toUpper();
        int row = a / 16;
        int col = a % 16;
        if (row < 8) {
            auto item = m_i2cGridTable->item(row, col);
            item->setText(QString("%1").arg(a, 2, 16, QChar('0')).toUpper());
            item->setForeground(Qt::black);
            item->setBackground(QColor(0, 220, 150));
        }
    }
    m_lblI2cFound->setText(foundStr);
}

void BusToolsView::onSendI2cTransferClicked() {
    if (!m_busToolsClient) return;
    bool ok = false;
    uint8_t addr = static_cast<uint8_t>(m_editI2cAddr->text().toInt(&ok, 16));
    if (!ok) return;

    QByteArray writeData = QByteArray::fromHex(m_editI2cWriteData->text().toUtf8());
    uint16_t readLen = static_cast<uint16_t>(m_editI2cReadLen->text().toUShort());

    m_txtI2cLog->append(QString("<b style='color:#00D2A0;'>I2C TX >></b> Addr 0x%1 | Write: [%2] | Read %3 bytes")
                        .arg(addr, 2, 16, QChar('0')).toUpper()
                        .arg(QString(writeData.toHex(' ').toUpper()))
                        .arg(readLen));
    m_busToolsClient->transferI2c(addr, writeData, readLen, true);
}

void BusToolsView::onI2cTransferCompleted(uint8_t status, const QByteArray& readData) {
    QString statusStr = (status == 0) ? "<span style='color:#00D2A0;'>ACK OK</span>" : "<span style='color:#FF5555;'>NACK / ERROR</span>";
    m_txtI2cLog->append(QString("<b>I2C RX <<</b> Status: %1 | Read: [%2]")
                        .arg(statusStr)
                        .arg(QString(readData.toHex(' ').toUpper())));
}

// ─── SPI Slots ───────────────────────────────────────────────────────────────
void BusToolsView::onSendSpiClicked() {
    if (!m_busToolsClient) return;
    QByteArray tx = QByteArray::fromHex(m_editSpiHex->text().toUtf8());
    if (tx.isEmpty()) return;

    m_txtSpiLog->append(QString("<b style='color:#00D2A0;'>SPI TX >></b> %1").arg(QString(tx.toHex(' ').toUpper())));
    m_busToolsClient->transferSpi(tx, true);
}

void BusToolsView::onSpiTransferCompleted(const QByteArray& rxData) {
    m_txtSpiLog->append(QString("<b style='color:#AA66FF;'>SPI RX <<</b> %1").arg(QString(rxData.toHex(' ').toUpper())));
}

void BusToolsView::onProbeFlashClicked() {
    if (!m_busToolsClient) return;
    m_txtSpiLog->append("<b>[SPI FLASH]</b> Querying JEDEC ID & SFDP parameter table...");
    m_busToolsClient->probeSpiFlash(5);
}

void BusToolsView::onSpiFlashInfoReceived(const InstrumentFlashInfoPayload& flashInfo) {
    QString capStr = QString("%1 MB (%2 MBit)")
                     .arg(flashInfo.capacity_bytes / (1024 * 1024))
                     .arg((flashInfo.capacity_bytes * 8) / (1024 * 1024));
    m_txtSpiLog->append(QString("<b style='color:#00FFCC;'>[FLASH DETECTED]</b> Part: <b>%1</b> | Mfg ID: 0x%2 | Capacity: %3")
                        .arg(QString::fromUtf8(flashInfo.part_name))
                        .arg(flashInfo.mfg_id, 2, 16, QChar('0')).toUpper()
                        .arg(capStr));
}

// ─── 1-Wire Slots ────────────────────────────────────────────────────────────
void BusToolsView::onSearch1WireClicked() {
    if (!m_busToolsClient) return;
    m_list1WireRoms->clear();
    m_txt1WireLog->append("<b>[1-WIRE]</b> Executing binary tree ROM search...");
    m_busToolsClient->search1Wire();
}

void BusToolsView::onOneWireSearchCompleted(const QVector<QByteArray>& romIds) {
    m_list1WireRoms->clear();
    for (const auto& rom : romIds) {
        QString hexRom = QString(rom.toHex(' ').toUpper());
        uint8_t family = static_cast<uint8_t>(rom.at(0));
        QString deviceType = (family == 0x28) ? "DS18B20 Temp Sensor" : "Dallas 1-Wire Device";
        m_list1WireRoms->addItem(QString("%1 [ROM: %2]").arg(deviceType).arg(hexRom));
    }
    m_txt1WireLog->append(QString("<b>[1-WIRE]</b> Found %1 devices on bus.").arg(romIds.size()));
}

void BusToolsView::onOneWireTransferClicked() {
    if (!m_busToolsClient) return;
    QByteArray writeData = QByteArray::fromHex(m_edit1WireWrite->text().toUtf8());
    uint16_t readLen = static_cast<uint16_t>(m_edit1WireReadLen->text().toUShort());

    m_txt1WireLog->append(QString("<b style='color:#00D2A0;'>1-Wire TX >></b> Reset + Write [%1] | Read %2 bytes")
                          .arg(QString(writeData.toHex(' ').toUpper())).arg(readLen));
    m_busToolsClient->transfer1Wire(true, writeData, readLen);
}

void BusToolsView::onOneWireTransferCompleted(bool presenceOk, const QByteArray& readData) {
    QString pres = presenceOk ? "<span style='color:#00D2A0;'>Presence Pulse OK</span>" : "<span style='color:#FF5555;'>No Presence Pulse</span>";
    m_txt1WireLog->append(QString("<b>1-Wire RX <<</b> %1 | Data: [%2]")
                          .arg(pres).arg(QString(readData.toHex(' ').toUpper())));
}

// ─── UART Slots ──────────────────────────────────────────────────────────────
void BusToolsView::onSendUartClicked() {
    if (!m_busToolsClient) return;
    QString txt = m_editUartSend->text();
    if (txt.isEmpty()) return;

    m_txtUartLog->append(QString("<b style='color:#00D2A0;'>TX:</b> %1").arg(txt));
    m_busToolsClient->writeUart(txt.toUtf8());
    m_editUartSend->clear();
}

void BusToolsView::onUartDataReceived(const QByteArray& rxData) {
    m_txtUartLog->append(QString("<b style='color:#AA66FF;'>RX:</b> %1").arg(QString::fromUtf8(rxData)));
}

// ─── GPIO Slots ──────────────────────────────────────────────────────────────
void BusToolsView::onGpioPinToggled(int pinIndex) {
    if (!m_gpioClient) return;
    bool isOutput = m_chkGpioDirs[pinIndex]->isChecked();
    if (!isOutput) {
        m_gpioClient->configurePin(static_cast<uint8_t>(2 + pinIndex), true, false, GPIO_PULL_NONE);
        m_chkGpioDirs[pinIndex]->setChecked(true);
    }
    bool currentHigh = m_btnGpioPins[pinIndex]->text().contains("HIGH");
    bool newLevel = !currentHigh;
    m_gpioClient->writePin(static_cast<uint8_t>(2 + pinIndex), newLevel);
}

void BusToolsView::onGpioReadAllClicked() {
    if (m_gpioClient) {
        m_gpioClient->readAll();
    }
}

void BusToolsView::onGpioWriteMaskClicked() {
    if (!m_gpioClient) return;
    bool ok = false;
    uint8_t mask = static_cast<uint8_t>(m_editGpioMask->text().toInt(&ok, 16));
    if (ok) {
        m_gpioClient->writeMask(0xFF, mask);
    }
}

void BusToolsView::onGpioStatusReceived(const InstrumentGpioStatusPayload& status) {
    int idx = status.pin_or_idx - 2;
    if (idx >= 0 && idx < 8) {
        m_chkGpioDirs[idx]->setChecked(status.is_output != 0);
        if (status.logic_level) {
            m_btnGpioPins[idx]->setText("HIGH (1)");
            m_btnGpioPins[idx]->setStyleSheet("background-color: #00B894; color: black; font-weight: bold;");
        } else {
            m_btnGpioPins[idx]->setText("LOW (0)");
            m_btnGpioPins[idx]->setStyleSheet("background-color: #2D3436; color: white; font-weight: bold;");
        }
    }
}

// ─── Syntax Slots ────────────────────────────────────────────────────────────
void BusToolsView::onExecSyntaxClicked() {
    if (!m_syntaxClient) return;
    QString syntax = m_editSyntaxInput->text().trimmed();
    if (syntax.isEmpty()) return;

    m_txtSyntaxLog->append(QString("<b style='color:#00D2A0;'>SYNTAX >></b> %1").arg(syntax));
    m_syntaxClient->executeSyntax(syntax);
    m_editSyntaxInput->clear();
}

void BusToolsView::onMacroClicked(int macroId) {
    if (m_syntaxClient) {
        m_txtSyntaxLog->append(QString("<b>[MACRO %1]</b> Executing...").arg(macroId));
        m_syntaxClient->executeMacro(macroId);
    }
}

void BusToolsView::onSyntaxResultReceived(uint8_t status, const QString& result) {
    QString statusTag = (status == 0) ? "<span style='color:#00D2A0;'>[OK]</span>" : "<span style='color:#FF5555;'>[ERR]</span>";
    m_txtSyntaxLog->append(QString("%1 %2").arg(statusTag).arg(result));
}

// ─── Wireless Slots ──────────────────────────────────────────────────────────
void BusToolsView::onSendIrNecClicked() {
    if (!m_wirelessClient) return;
    bool okAddr = false, okCmd = false;
    uint16_t addr = static_cast<uint16_t>(m_editIrAddr->text().toInt(&okAddr, 16));
    uint32_t cmd  = static_cast<uint32_t>(m_editIrCmd->text().toInt(&okCmd, 16));
    if (okAddr && okCmd) {
        m_txtIrLog->append(QString("<b style='color:#E17055;'>[IR TX]</b> NEC Addr=0x%1 Cmd=0x%2")
                           .arg(addr, 4, 16, QChar('0')).toUpper()
                           .arg(cmd, 2, 16, QChar('0')).toUpper());
        m_wirelessClient->transmitIrNec(addr, cmd);
    }
}

void BusToolsView::onToggleIrRx(bool checked) {
    if (m_wirelessClient) {
        m_wirelessClient->configureIrRx(checked);
        m_txtIrLog->append(QString("<b>[IR RX]</b> %1").arg(checked ? "Listening on IO26..." : "Stopped."));
    }
}

void BusToolsView::onIrRxEventReceived(const InstrumentIrRxEventPayload& ev) {
    m_txtIrLog->append(QString("<b style='color:#00D2A0;'>[IR RX EVENT]</b> Protocol: %1 | Addr: 0x%2 | Cmd: 0x%3 | TS: %4 us")
                       .arg(ev.protocol == 1 ? "NEC" : "RAW")
                       .arg(ev.address, 4, 16, QChar('0')).toUpper()
                       .arg(ev.command, 2, 16, QChar('0')).toUpper()
                       .arg(ev.timestamp_us));
}

void BusToolsView::onSendNrf24Clicked() {
    if (!m_wirelessClient) return;
    QByteArray addr = QByteArray::fromHex(m_editNrf24Addr->text().toUtf8());
    QByteArray payloadBytes = m_editNrf24Data->text().toUtf8();
    if (!addr.isEmpty() && !payloadBytes.isEmpty()) {
        m_txtNrf24Log->append(QString("<b style='color:#0984E3;'>[nRF24 TX]</b> Dest: %1 | Payload: %2")
                              .arg(QString(addr.toHex().toUpper()))
                              .arg(QString::fromUtf8(payloadBytes)));
        m_wirelessClient->transmitNrf24(addr, payloadBytes);
    }
}

void BusToolsView::onQueryNrf24StatusClicked() {
    if (m_wirelessClient) {
        m_wirelessClient->queryNrf24Status();
    }
}

void BusToolsView::onNrf24StatusReceived(const InstrumentNrf24StatusPayload& st) {
    m_txtNrf24Log->append(QString("<b style='color:#00D2A0;'>[nRF24 STATUS]</b> Connected: %1 | Ch: %2 (2.%3 GHz) | Carrier: %4")
                          .arg(st.is_connected ? "YES" : "NO")
                          .arg(st.active_channel)
                          .arg(400 + st.active_channel)
                          .arg(st.carrier_detected ? "DETECTED" : "None"));
}

void BusToolsView::onNrf24RxEventReceived(const InstrumentNrf24RxEventPayload& ev) {
    QByteArray payload(reinterpret_cast<const char*>(ev.data), ev.length);
    m_txtNrf24Log->append(QString("<b style='color:#AA66FF;'>[nRF24 RX EVENT]</b> Pipe: %1 | Len: %2 | Data: [%3] | TS: %4 us")
                          .arg(ev.pipe_index)
                          .arg(ev.length)
                          .arg(QString::fromUtf8(payload))
                          .arg(ev.timestamp_us));
}

} // namespace UdooKey
