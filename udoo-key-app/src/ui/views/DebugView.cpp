#include "DebugView.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>

namespace UdooKey {

DebugView::DebugView(QWidget* parent)
    : QWidget(parent) {
    // Initialize mock transport as default for instant workbench interaction
    m_mockTransport = std::make_unique<udoo::debug::MockDebugTransport>(this);
    m_mockTransport->open();
    m_activeTransport = m_mockTransport.get();

    m_jtagClient = std::make_unique<udoo::debug::JtagClient>(m_activeTransport, this);
    m_swdClient = std::make_unique<udoo::debug::SwdClient>(m_activeTransport, this);

    setupUi();

    // Connect JTAG client signals
    connect(m_jtagClient.get(), &udoo::debug::JtagClient::tapStateChanged, this, &DebugView::onJtagTapStateChanged);
    connect(m_jtagClient.get(), &udoo::debug::JtagClient::chainScanned, this, &DebugView::onJtagChainScanned);
    connect(m_jtagClient.get(), &udoo::debug::JtagClient::transactionCompleted, this, &DebugView::onJtagTransactionCompleted);
    connect(m_jtagClient.get(), &udoo::debug::JtagClient::batchCompleted, this, &DebugView::onJtagBatchCompleted);
    connect(m_jtagClient.get(), &udoo::debug::JtagClient::logMessage, this, &DebugView::onLogMessage);
    connect(m_jtagClient.get(), &udoo::debug::JtagClient::errorOccurred, this, &DebugView::onLogMessage);

    // Connect SWD client signals
    connect(m_swdClient.get(), &udoo::debug::SwdClient::targetConnected, this, &DebugView::onSwdTargetConnected);
    connect(m_swdClient.get(), &udoo::debug::SwdClient::targetDisconnected, this, &DebugView::onSwdTargetDisconnected);
    connect(m_swdClient.get(), &udoo::debug::SwdClient::logMessage, this, &DebugView::onLogMessage);
    connect(m_swdClient.get(), &udoo::debug::SwdClient::errorOccurred, this, &DebugView::onLogMessage);

    updateTapStateBadge(udoo::debug::JtagTapState::TestLogicReset);
    onLogMessage("Debug View initialized. Simulation Transport active (IEEE 1149.1 / ARM CoreSight).");
}

void DebugView::setLiveTransport(ITransport* transport) {
    if (!m_serialTransport) {
        m_serialTransport = std::make_unique<udoo::debug::SerialDebugTransport>(transport, this);
    } else {
        m_serialTransport->setTransport(transport);
    }

    if (m_comboTransportMode->currentIndex() == 1) { // Live Hardware
        m_activeTransport = m_serialTransport.get();
        m_lblTransportStatus->setText(" Live Hardware (USB CDC) ");
        m_lblTransportStatus->setStyleSheet("color: #00E676; font-weight: bold;");
    }
}

void DebugView::onTransportModeChanged(int index) {
    if (index == 0) { // Simulation
        m_activeTransport = m_mockTransport.get();
        m_lblTransportStatus->setText(" Simulation Engine (Zero Hardware) ");
        m_lblTransportStatus->setStyleSheet("color: #00B0FF; font-weight: bold;");
        onLogMessage("Switched to Simulation Engine.");
    } else { // Live
        if (m_serialTransport && m_serialTransport->isConnected()) {
            m_activeTransport = m_serialTransport.get();
            m_lblTransportStatus->setText(" Live Hardware (USB CDC) ");
            m_lblTransportStatus->setStyleSheet("color: #00E676; font-weight: bold;");
            onLogMessage("Switched to Live Hardware Transport.");
        } else {
            m_lblTransportStatus->setText(" Live Hardware Disconnected ");
            m_lblTransportStatus->setStyleSheet("color: #FF5252; font-weight: bold;");
            onLogMessage("Live hardware is currently not connected. Connect via Device menu.");
        }
    }
}

void DebugView::onLogMessage(const QString& msg) {
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_txtDebugLog->append(QString("[%1] %2").arg(timeStr).arg(msg));
}

void DebugView::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // Top Bar: Transport Selector & General Status
    auto topBar = new QHBoxLayout();
    topBar->addWidget(new QLabel("Transport Backend:"));
    m_comboTransportMode = new QComboBox(this);
    m_comboTransportMode->addItem("Simulation Engine (ARM Cortex-M0+ & FPGA)", 0);
    m_comboTransportMode->addItem("UDOO KEY Live Hardware (Probe Bus GP4..GP9)", 1);
    connect(m_comboTransportMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DebugView::onTransportModeChanged);
    topBar->addWidget(m_comboTransportMode);

    m_lblTransportStatus = new QLabel(" Simulation Engine (Zero Hardware) ", this);
    m_lblTransportStatus->setStyleSheet("color: #00B0FF; font-weight: bold;");
    topBar->addWidget(m_lblTransportStatus);
    topBar->addStretch();
    mainLayout->addLayout(topBar);

    // Center: Split Tab Widget for JTAG and SWD
    auto tabWidget = new QTabWidget(this);
    tabWidget->addTab(createJtagTab(), "⛓ IEEE 1149.1 JTAG Subsystem");
    tabWidget->addTab(createSwdTab(), "⚡ ARM CoreSight SWD Subsystem");
    mainLayout->addWidget(tabWidget, 3);

    // Bottom: Activity / Log Box
    auto grpLog = new QGroupBox("Debug Transaction & Hardware Telemetry Log", this);
    auto logLayout = new QVBoxLayout(grpLog);
    logLayout->setContentsMargins(4, 4, 4, 4);

    m_txtDebugLog = new QTextEdit(grpLog);
    m_txtDebugLog->setReadOnly(true);
    m_txtDebugLog->setFont(QFont("Consolas", 9));
    m_txtDebugLog->setStyleSheet("background-color: #121216; color: #DCDCDC;");
    logLayout->addWidget(m_txtDebugLog);

    auto btnClearLog = new QPushButton("Clear Log", grpLog);
    connect(btnClearLog, &QPushButton::clicked, m_txtDebugLog, &QTextEdit::clear);
    logLayout->addWidget(btnClearLog, 0, Qt::AlignRight);

    mainLayout->addWidget(grpLog, 1);
}

QWidget* DebugView::createJtagTab() {
    auto widget = new QWidget(this);
    auto layout = new QVBoxLayout(widget);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // Group 1: JTAG Controls & State Machine Monitor
    auto grpControls = new QGroupBox("JTAG TAP Controller & Clock Configuration", widget);
    auto gridControls = new QGridLayout(grpControls);

    gridControls->addWidget(new QLabel("TAP State:"), 0, 0);
    m_lblTapState = new QLabel("Test-Logic-Reset", grpControls);
    m_lblTapState->setAlignment(Qt::AlignCenter);
    m_lblTapState->setFixedHeight(28);
    gridControls->addWidget(m_lblTapState, 0, 1);

    m_btnJtagReset = new QPushButton("🔄 Reset TAP (5x TMS=1)", grpControls);
    m_btnJtagReset->setStyleSheet("font-weight: bold; background-color: #C62828; color: white;");
    connect(m_btnJtagReset, &QPushButton::clicked, this, &DebugView::onJtagResetClicked);
    gridControls->addWidget(m_btnJtagReset, 0, 2);

    gridControls->addWidget(new QLabel("TCK Clock:"), 0, 3);
    m_comboJtagClock = new QComboBox(grpControls);
    m_comboJtagClock->addItems({"100 kHz", "500 kHz", "1.0 MHz", "2.0 MHz", "5.0 MHz", "10.0 MHz"});
    m_comboJtagClock->setCurrentIndex(2); // 1.0 MHz
    gridControls->addWidget(m_comboJtagClock, 0, 4);

    auto btnSetClock = new QPushButton("Apply Clock", grpControls);
    connect(btnSetClock, &QPushButton::clicked, this, &DebugView::onJtagSetClockClicked);
    gridControls->addWidget(btnSetClock, 0, 5);

    // TAP Navigation
    gridControls->addWidget(new QLabel("Navigate TAP To:"), 1, 0);
    m_comboTargetTapState = new QComboBox(grpControls);
    m_comboTargetTapState->addItems({
        "Test-Logic-Reset", "Run-Test/Idle", "Select-DR-Scan", "Capture-DR", "Shift-DR",
        "Exit1-DR", "Pause-DR", "Exit2-DR", "Update-DR", "Select-IR-Scan",
        "Capture-IR", "Shift-IR", "Exit1-IR", "Pause-IR", "Exit2-IR", "Update-IR"
    });
    m_comboTargetTapState->setCurrentIndex(1); // Run-Test/Idle
    gridControls->addWidget(m_comboTargetTapState, 1, 1);

    m_btnJtagNavigate = new QPushButton("Navigate (Shortest TMS)", grpControls);
    connect(m_btnJtagNavigate, &QPushButton::clicked, this, &DebugView::onJtagNavigateStateClicked);
    gridControls->addWidget(m_btnJtagNavigate, 1, 2);

    m_btnJtagScanChain = new QPushButton("🔎 Enumerate JTAG Chain", grpControls);
    m_btnJtagScanChain->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    connect(m_btnJtagScanChain, &QPushButton::clicked, this, &DebugView::onJtagScanChainClicked);
    gridControls->addWidget(m_btnJtagScanChain, 1, 3, 1, 3);

    layout->addWidget(grpControls);

    // Group 2: Chain Enumeration Results
    auto grpChain = new QGroupBox("Detected Devices in Chain", widget);
    auto chainLayout = new QVBoxLayout(grpChain);

    m_treeJtagChain = new QTreeWidget(grpChain);
    m_treeJtagChain->setHeaderLabels({"#", "Device Name", "IDCODE (32-bit)", "IR Length", "Manufacturer", "Part No", "Rev"});
    m_treeJtagChain->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_treeJtagChain->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    chainLayout->addWidget(m_treeJtagChain);

    m_lblChainTopology = new QLabel("Chain Topology: [Not enumerated]", grpChain);
    m_lblChainTopology->setStyleSheet("font-family: Consolas; color: #4FC3F7; font-weight: bold; padding: 2px;");
    chainLayout->addWidget(m_lblChainTopology);

    layout->addWidget(grpChain, 2);

    // Group 3: Bit-Precise & Targeted Device Operations
    auto grpShift = new QGroupBox("Bit-Precise Scan & Multi-Device Target Operations", widget);
    auto gridShift = new QGridLayout(grpShift);

    // Row 0: Target Device & Preset Instruction
    gridShift->addWidget(new QLabel("Target Device:"), 0, 0);
    m_comboTargetDevice = new QComboBox(grpShift);
    m_comboTargetDevice->addItem("Entire Chain (Direct Shift)");
    connect(m_comboTargetDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DebugView::onJtagTargetDeviceChanged);
    gridShift->addWidget(m_comboTargetDevice, 0, 1, 1, 2);

    gridShift->addWidget(new QLabel("Preset Instruction:"), 0, 3);
    m_comboStdInstruction = new QComboBox(grpShift);
    m_comboStdInstruction->addItems({"Custom (Manual Hex)", "IDCODE", "BYPASS", "EXTEST", "SAMPLE/PRELOAD", "CLAMP", "HIGHZ"});
    connect(m_comboStdInstruction, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DebugView::onJtagStandardInstructionSelected);
    gridShift->addWidget(m_comboStdInstruction, 0, 4, 1, 2);

    // Row 1: Scan Type, Bit Length, Exit State
    gridShift->addWidget(new QLabel("Scan Type:"), 1, 0);
    m_comboShiftType = new QComboBox(grpShift);
    m_comboShiftType->addItems({"Shift-DR (Data Register)", "Shift-IR (Instruction Register)"});
    gridShift->addWidget(m_comboShiftType, 1, 1);

    gridShift->addWidget(new QLabel("Bit Length:"), 1, 2);
    m_spinShiftBits = new QSpinBox(grpShift);
    m_spinShiftBits->setRange(1, 4096);
    m_spinShiftBits->setValue(32);
    gridShift->addWidget(m_spinShiftBits, 1, 3);

    gridShift->addWidget(new QLabel("Exit State:"), 1, 4);
    m_comboShiftExitState = new QComboBox(grpShift);
    m_comboShiftExitState->addItems({"Run-Test/Idle", "Shift-DR", "Shift-IR", "Pause-DR", "Pause-IR", "Test-Logic-Reset"});
    gridShift->addWidget(m_comboShiftExitState, 1, 5);

    // Row 2: TDI & Action Buttons
    gridShift->addWidget(new QLabel("TDI In (Hex):"), 2, 0);
    m_editShiftTdi = new QLineEdit("0x00000000", grpShift);
    gridShift->addWidget(m_editShiftTdi, 2, 1, 1, 2);

    m_btnExecuteShift = new QPushButton("🚀 Raw Scan", grpShift);
    m_btnExecuteShift->setToolTip("Clock scan directly over the physical chain");
    connect(m_btnExecuteShift, &QPushButton::clicked, this, &DebugView::onJtagShiftClicked);
    gridShift->addWidget(m_btnExecuteShift, 2, 3);

    m_btnExecuteTargetedShift = new QPushButton("🎯 Targeted Shift", grpShift);
    m_btnExecuteTargetedShift->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    m_btnExecuteTargetedShift->setToolTip("Shifts to selected device with automatic BYPASS padding for others");
    connect(m_btnExecuteTargetedShift, &QPushButton::clicked, this, &DebugView::onJtagExecuteTargetedShiftClicked);
    gridShift->addWidget(m_btnExecuteTargetedShift, 2, 4);

    m_btnExecuteBatch = new QPushButton("⚡ Run Batch", grpShift);
    m_btnExecuteBatch->setStyleSheet("font-weight: bold; background-color: #2979FF; color: white;");
    m_btnExecuteBatch->setToolTip("Executes deterministic multi-op verification sequence in 1 round-trip");
    connect(m_btnExecuteBatch, &QPushButton::clicked, this, &DebugView::onJtagExecuteBatchClicked);
    gridShift->addWidget(m_btnExecuteBatch, 2, 5);

    // Row 3: TDO Output
    gridShift->addWidget(new QLabel("TDO Out (Hex):"), 3, 0);
    m_editShiftTdoHex = new QLineEdit(grpShift);
    m_editShiftTdoHex->setReadOnly(true);
    gridShift->addWidget(m_editShiftTdoHex, 3, 1, 1, 2);

    gridShift->addWidget(new QLabel("TDO Out (Bin):"), 3, 3);
    m_editShiftTdoBin = new QLineEdit(grpShift);
    m_editShiftTdoBin->setReadOnly(true);
    gridShift->addWidget(m_editShiftTdoBin, 3, 4, 1, 2);

    layout->addWidget(grpShift);
    return widget;
}

QWidget* DebugView::createSwdTab() {
    auto widget = new QWidget(this);
    auto layout = new QVBoxLayout(widget);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // Group 1: SWD Connection & Target Badge
    auto grpTarget = new QGroupBox("ARM CoreSight Target Control", widget);
    auto gridTarget = new QGridLayout(grpTarget);

    m_btnSwdReset = new QPushButton("⚡ Line Reset (50+ clocks)", grpTarget);
    connect(m_btnSwdReset, &QPushButton::clicked, this, &DebugView::onSwdResetClicked);
    gridTarget->addWidget(m_btnSwdReset, 0, 0);

    m_btnSwdConnect = new QPushButton("🔗 Connect Target (0xE79E)", grpTarget);
    m_btnSwdConnect->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    connect(m_btnSwdConnect, &QPushButton::clicked, this, &DebugView::onSwdConnectClicked);
    gridTarget->addWidget(m_btnSwdConnect, 0, 1);

    gridTarget->addWidget(new QLabel("SWD Clock:"), 0, 2);
    m_comboSwdClock = new QComboBox(grpTarget);
    m_comboSwdClock->addItems({"100 kHz", "500 kHz", "1.0 MHz", "2.0 MHz", "4.0 MHz", "8.0 MHz"});
    m_comboSwdClock->setCurrentIndex(2); // 1.0 MHz
    gridTarget->addWidget(m_comboSwdClock, 0, 3);

    auto btnSetSwdClock = new QPushButton("Apply Clock", grpTarget);
    connect(btnSetSwdClock, &QPushButton::clicked, this, &DebugView::onSwdSetClockClicked);
    gridTarget->addWidget(btnSetSwdClock, 0, 4);

    gridTarget->addWidget(new QLabel("Target Status:"), 1, 0);
    m_lblSwdTargetStatus = new QLabel("Offline / Not Connected", grpTarget);
    m_lblSwdTargetStatus->setStyleSheet("color: #FF5252; font-weight: bold;");
    gridTarget->addWidget(m_lblSwdTargetStatus, 1, 1);

    gridTarget->addWidget(new QLabel("DPIDR:"), 1, 2);
    m_lblSwdDpidr = new QLabel("0x--------", grpTarget);
    m_lblSwdDpidr->setStyleSheet("font-family: Consolas; font-weight: bold;");
    gridTarget->addWidget(m_lblSwdDpidr, 1, 3);

    m_lblSwdTargetInfo = new QLabel("Connect to probe ARM CoreSight Debug Port (RP2040 Cortex-M0+ / External)", grpTarget);
    m_lblSwdTargetInfo->setStyleSheet("color: #8E8E9F;");
    gridTarget->addWidget(m_lblSwdTargetInfo, 2, 0, 1, 5);

    layout->addWidget(grpTarget);

    // Group 2: Core DP / AP Registers
    auto grpRegs = new QGroupBox("Debug Port (DP) & Access Port (AP) Register Access", widget);
    auto gridRegs = new QGridLayout(grpRegs);

    // DP Registers
    gridRegs->addWidget(new QLabel("DP Register:"), 0, 0);
    m_comboDpReg = new QComboBox(grpRegs);
    m_comboDpReg->addItem("DP_IDCODE (0x00)", udoo::debug::DpRegister::IDCODE);
    m_comboDpReg->addItem("DP_CTRL_STAT (0x04)", udoo::debug::DpRegister::CTRL_STAT);
    m_comboDpReg->addItem("DP_SELECT (0x08)", udoo::debug::DpRegister::SELECT);
    m_comboDpReg->addItem("DP_RDBUFF (0x0C)", udoo::debug::DpRegister::RDBUFF);
    gridRegs->addWidget(m_comboDpReg, 0, 1);

    m_editDpVal = new QLineEdit("0x00000000", grpRegs);
    gridRegs->addWidget(m_editDpVal, 0, 2);

    m_btnReadDp = new QPushButton("Read DP", grpRegs);
    connect(m_btnReadDp, &QPushButton::clicked, this, &DebugView::onSwdReadDpClicked);
    gridRegs->addWidget(m_btnReadDp, 0, 3);

    m_btnWriteDp = new QPushButton("Write DP", grpRegs);
    connect(m_btnWriteDp, &QPushButton::clicked, this, &DebugView::onSwdWriteDpClicked);
    gridRegs->addWidget(m_btnWriteDp, 0, 4);

    // AP Registers
    gridRegs->addWidget(new QLabel("MEM-AP Register:"), 1, 0);
    m_comboApReg = new QComboBox(grpRegs);
    m_comboApReg->addItem("AP_CSW (0x00)", udoo::debug::ApRegister::CSW);
    m_comboApReg->addItem("AP_TAR (0x04)", udoo::debug::ApRegister::TAR);
    m_comboApReg->addItem("AP_DRW (0x0C)", udoo::debug::ApRegister::DRW);
    m_comboApReg->addItem("AP_IDR (0xFC)", udoo::debug::ApRegister::IDR);
    gridRegs->addWidget(m_comboApReg, 1, 1);

    m_editApVal = new QLineEdit("0x00000000", grpRegs);
    gridRegs->addWidget(m_editApVal, 1, 2);

    m_btnReadAp = new QPushButton("Read AP", grpRegs);
    connect(m_btnReadAp, &QPushButton::clicked, this, &DebugView::onSwdReadApClicked);
    gridRegs->addWidget(m_btnReadAp, 1, 3);

    m_btnWriteAp = new QPushButton("Write AP", grpRegs);
    connect(m_btnWriteAp, &QPushButton::clicked, this, &DebugView::onSwdWriteApClicked);
    gridRegs->addWidget(m_btnWriteAp, 1, 4);

    layout->addWidget(grpRegs);

    // Group 3: Memory Access Primitives
    auto grpMem = new QGroupBox("Direct Memory Access Primitives (32-bit Word)", widget);
    auto gridMem = new QGridLayout(grpMem);

    gridMem->addWidget(new QLabel("Memory Address:"), 0, 0);
    m_editMemAddr = new QLineEdit("0x20000000", grpMem); // SRAM default
    gridMem->addWidget(m_editMemAddr, 0, 1);

    gridMem->addWidget(new QLabel("Word Value (32-bit):"), 0, 2);
    m_editMemVal = new QLineEdit("0x00000000", grpMem);
    gridMem->addWidget(m_editMemVal, 0, 3);

    m_btnReadMem = new QPushButton("Read Word", grpMem);
    connect(m_btnReadMem, &QPushButton::clicked, this, &DebugView::onSwdReadMemClicked);
    gridMem->addWidget(m_btnReadMem, 0, 4);

    m_btnWriteMem = new QPushButton("Write Word", grpMem);
    connect(m_btnWriteMem, &QPushButton::clicked, this, &DebugView::onSwdWriteMemClicked);
    gridMem->addWidget(m_btnWriteMem, 0, 5);

    layout->addWidget(grpMem);
    layout->addStretch();
    return widget;
}

void DebugView::updateTapStateBadge(udoo::debug::JtagTapState state) {
    m_lblTapState->setText(udoo::debug::tapStateToString(state));
    switch (state) {
        case udoo::debug::JtagTapState::TestLogicReset:
            m_lblTapState->setStyleSheet("background-color: #B71C1C; color: white; font-weight: bold; border-radius: 4px;");
            break;
        case udoo::debug::JtagTapState::RunTestIdle:
            m_lblTapState->setStyleSheet("background-color: #1B5E20; color: white; font-weight: bold; border-radius: 4px;");
            break;
        case udoo::debug::JtagTapState::ShiftDr:
        case udoo::debug::JtagTapState::ShiftIr:
            m_lblTapState->setStyleSheet("background-color: #0091EA; color: white; font-weight: bold; border-radius: 4px;");
            break;
        default:
            m_lblTapState->setStyleSheet("background-color: #E65100; color: white; font-weight: bold; border-radius: 4px;");
            break;
    }
}

// ---------------- JTAG Slots ----------------

void DebugView::onJtagResetClicked() {
    m_jtagClient->resetTap();
}

void DebugView::onJtagScanChainClicked() {
    m_jtagClient->scanChain();
}

void DebugView::onJtagNavigateStateClicked() {
    QString stateName = m_comboTargetTapState->currentText();
    udoo::debug::JtagTapState target = udoo::debug::tapStateFromString(stateName);
    m_jtagClient->navigateTap(target);
}

void DebugView::onJtagSetClockClicked() {
    static const uint32_t clocks[] = {100000, 500000, 1000000, 2000000, 5000000, 10000000};
    int idx = m_comboJtagClock->currentIndex();
    if (idx >= 0 && idx < 6) {
        m_jtagClient->setClock(clocks[idx]);
    }
}

void DebugView::onJtagShiftClicked() {
    size_t bits = static_cast<size_t>(m_spinShiftBits->value());
    udoo::debug::JtagScanData tdi = udoo::debug::JtagScanData::fromHex(m_editShiftTdi->text(), bits);
    udoo::debug::JtagTapState exitState = udoo::debug::tapStateFromString(m_comboShiftExitState->currentText());

    if (m_comboShiftType->currentIndex() == 0) { // Shift-DR
        m_jtagClient->shiftDr(tdi, exitState);
    } else { // Shift-IR
        m_jtagClient->shiftIr(tdi, exitState);
    }
}

void DebugView::onJtagTapStateChanged(udoo::debug::JtagTapState state) {
    updateTapStateBadge(state);
}

void DebugView::onJtagChainScanned(const std::vector<udoo::debug::JtagDevice>& devices) {
    m_treeJtagChain->clear();
    m_comboTargetDevice->clear();
    m_comboTargetDevice->addItem("Entire Chain (Direct Shift)");

    QString topo = "Chain: [TDI] -> ";
    for (const auto& dev : devices) {
        auto item = new QTreeWidgetItem(m_treeJtagChain);
        item->setText(0, QString::number(dev.chainIndex));
        item->setText(1, dev.name);
        item->setText(2, dev.idcode.formatted());
        item->setText(3, QString("%1 bits").arg(dev.irLength));
        item->setText(4, dev.manufacturer);
        item->setText(5, QString("0x%1").arg(dev.idcode.partNumber(), 4, 16, QChar('0')).toUpper());
        item->setText(6, QString::number(dev.idcode.version()));

        m_comboTargetDevice->addItem(QString("Dev #%1: %2 (IR: %3b)").arg(dev.chainIndex).arg(dev.name).arg(dev.irLength));
        topo += QString("[%1 (IR:%2b)] -> ").arg(dev.name).arg(dev.irLength);
    }
    topo += "[TDO]";
    m_lblChainTopology->setText(topo);
}

void DebugView::onJtagTargetDeviceChanged(int index) {
    if (index <= 0) return;
    size_t devIdx = static_cast<size_t>(index - 1);
    if (devIdx < m_jtagClient->devices().size()) {
        const auto& dev = m_jtagClient->devices()[devIdx];
        if (m_comboShiftType->currentIndex() == 1) { // IR
            m_spinShiftBits->setValue(dev.irLength > 0 ? static_cast<int>(dev.irLength) : 4);
        } else {
            m_spinShiftBits->setValue(32);
        }
    }
}

void DebugView::onJtagStandardInstructionSelected(int index) {
    if (index <= 0) return;
    int devIdx = m_comboTargetDevice->currentIndex() - 1;
    if (devIdx < 0) {
        onLogMessage("Please select a specific target device to apply preset instruction.");
        return;
    }
    udoo::debug::JtagStandardInstruction inst;
    switch (index) {
        case 1: inst = udoo::debug::JtagStandardInstruction::IdCode; break;
        case 2: inst = udoo::debug::JtagStandardInstruction::Bypass; break;
        case 3: inst = udoo::debug::JtagStandardInstruction::Extest; break;
        case 4: inst = udoo::debug::JtagStandardInstruction::SamplePreload; break;
        case 5: inst = udoo::debug::JtagStandardInstruction::Clamp; break;
        case 6: inst = udoo::debug::JtagStandardInstruction::HighZ; break;
        default: return;
    }

    auto opcode = m_jtagClient->chain().standardInstruction(devIdx, inst);
    m_editShiftTdi->setText(opcode.toHexString());
    m_spinShiftBits->setValue(static_cast<int>(opcode.bitCount()));
    m_comboShiftType->setCurrentIndex(1); // Set to Shift-IR
    onLogMessage(QString("Device #%1: Preset instruction %2 loaded (Opcode: %3)")
        .arg(devIdx).arg(udoo::debug::standardInstructionToString(inst)).arg(opcode.toHexString()));
}

void DebugView::onJtagExecuteTargetedShiftClicked() {
    int devIdx = m_comboTargetDevice->currentIndex() - 1;
    if (devIdx < 0) {
        onJtagShiftClicked();
        return;
    }

    size_t bits = static_cast<size_t>(m_spinShiftBits->value());
    udoo::debug::JtagScanData tdi = udoo::debug::JtagScanData::fromHex(m_editShiftTdi->text(), bits);
    udoo::debug::JtagTapState exitState = udoo::debug::tapStateFromString(m_comboShiftExitState->currentText());

    if (m_comboShiftType->currentIndex() == 1) { // Shift-IR
        m_jtagClient->shiftDeviceIr(devIdx, tdi, exitState);
    } else { // Shift-DR
        m_jtagClient->shiftDeviceDr(devIdx, tdi, bits, exitState);
    }
}

void DebugView::onJtagExecuteBatchClicked() {
    onLogMessage("Executing deterministic JTAG verification batch sequence...");
    std::vector<udoo::debug::JtagBatchOperation> batch;

    // 1. Reset TAP
    udoo::debug::JtagBatchOperation opReset;
    opReset.type = udoo::debug::JtagOpType::TapReset;
    batch.push_back(opReset);

    // 2. Navigate to Shift-DR
    udoo::debug::JtagBatchOperation opNav;
    opNav.type = udoo::debug::JtagOpType::GotoState;
    opNav.targetState = udoo::debug::JtagTapState::ShiftDr;
    batch.push_back(opNav);

    // 3. Shift 32 bits DR (reads IDCODE of first device)
    udoo::debug::JtagBatchOperation opShift;
    opShift.type = udoo::debug::JtagOpType::ShiftDr;
    opShift.data = udoo::debug::JtagScanData(32);
    opShift.targetState = udoo::debug::JtagTapState::RunTestIdle;
    batch.push_back(opShift);

    // 4. Clock 16 idle cycles
    udoo::debug::JtagBatchOperation opIdle;
    opIdle.type = udoo::debug::JtagOpType::ClockIdle;
    opIdle.clockCycles = 16;
    batch.push_back(opIdle);

    m_jtagClient->executeBatch(batch);
}

void DebugView::onJtagBatchCompleted(const udoo::debug::JtagBatchResult& result) {
    if (result.success) {
        onLogMessage(QString("Batch completed successfully! (%1 shift outputs returned, Final TAP state: %2)")
            .arg(result.shiftResults.size())
            .arg(udoo::debug::tapStateToString(result.finalState)));
        if (!result.shiftResults.empty()) {
            m_editShiftTdoHex->setText(result.shiftResults[0].toHexString());
            m_editShiftTdoBin->setText(result.shiftResults[0].toBinString());
        }
    } else {
        onLogMessage(QString("Batch execution failed: %1").arg(result.errorMessage));
    }
}

void DebugView::onJtagTransactionCompleted(const udoo::debug::JtagTransactionResult& result) {
    if (result.success) {
        m_editShiftTdoHex->setText(result.tdoData.toHexString());
        m_editShiftTdoBin->setText(result.tdoData.toBinString());
        onLogMessage(QString("Shift Completed: TDO = %1 (%2 bits)")
            .arg(result.tdoData.toHexString())
            .arg(result.tdoData.bitCount()));
    }
}

// ---------------- SWD Slots ----------------

void DebugView::onSwdResetClicked() {
    m_swdClient->resetLine();
}

void DebugView::onSwdConnectClicked() {
    udoo::debug::SwdDpidr dpidr;
    m_swdClient->connectTarget(dpidr);
}

void DebugView::onSwdSetClockClicked() {
    static const uint32_t clocks[] = {100000, 500000, 1000000, 2000000, 4000000, 8000000};
    int idx = m_comboSwdClock->currentIndex();
    if (idx >= 0 && idx < 6) {
        m_swdClient->setClock(clocks[idx]);
    }
}

void DebugView::onSwdTargetConnected(const udoo::debug::SwdDpidr& dpidr) {
    m_lblSwdTargetStatus->setText("CONNECTED (ARM CoreSight)");
    m_lblSwdTargetStatus->setStyleSheet("color: #00E676; font-weight: bold;");
    m_lblSwdDpidr->setText(dpidr.formatted());
    m_lblSwdTargetInfo->setText(dpidr.description());
}

void DebugView::onSwdTargetDisconnected() {
    m_lblSwdTargetStatus->setText("Offline / Not Connected");
    m_lblSwdTargetStatus->setStyleSheet("color: #FF5252; font-weight: bold;");
    m_lblSwdDpidr->setText("0x--------");
    m_lblSwdTargetInfo->setText("Target disconnected.");
}

void DebugView::onSwdReadDpClicked() {
    uint8_t addr = static_cast<uint8_t>(m_comboDpReg->currentData().toUInt());
    auto res = m_swdClient->readDp(addr);
    if (res.isOk()) {
        m_editDpVal->setText(QString("0x%1").arg(res.data, 8, 16, QChar('0')).toUpper());
        onLogMessage(QString("Read DP [0x%1] = 0x%2 (%3)")
            .arg(addr, 2, 16, QChar('0')).toUpper()
            .arg(res.data, 8, 16, QChar('0')).toUpper()
            .arg(udoo::debug::swdAckToString(res.ack)));
    }
}

void DebugView::onSwdWriteDpClicked() {
    uint8_t addr = static_cast<uint8_t>(m_comboDpReg->currentData().toUInt());
    bool ok = false;
    uint32_t val = m_editDpVal->text().toUInt(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this, "Invalid Hex", "Please enter a valid 32-bit hexadecimal value.");
        return;
    }
    auto res = m_swdClient->writeDp(addr, val);
    if (res.isOk()) {
        onLogMessage(QString("Write DP [0x%1] = 0x%2 OK")
            .arg(addr, 2, 16, QChar('0')).toUpper()
            .arg(val, 8, 16, QChar('0')).toUpper());
    }
}

void DebugView::onSwdReadApClicked() {
    uint8_t addr = static_cast<uint8_t>(m_comboApReg->currentData().toUInt());
    auto res = m_swdClient->readAp(addr);
    if (res.isOk()) {
        m_editApVal->setText(QString("0x%1").arg(res.data, 8, 16, QChar('0')).toUpper());
        onLogMessage(QString("Read AP [0x%1] = 0x%2 (%3)")
            .arg(addr, 2, 16, QChar('0')).toUpper()
            .arg(res.data, 8, 16, QChar('0')).toUpper()
            .arg(udoo::debug::swdAckToString(res.ack)));
    }
}

void DebugView::onSwdWriteApClicked() {
    uint8_t addr = static_cast<uint8_t>(m_comboApReg->currentData().toUInt());
    bool ok = false;
    uint32_t val = m_editApVal->text().toUInt(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this, "Invalid Hex", "Please enter a valid 32-bit hexadecimal value.");
        return;
    }
    auto res = m_swdClient->writeAp(addr, val);
    if (res.isOk()) {
        onLogMessage(QString("Write AP [0x%1] = 0x%2 OK")
            .arg(addr, 2, 16, QChar('0')).toUpper()
            .arg(val, 8, 16, QChar('0')).toUpper());
    }
}

void DebugView::onSwdReadMemClicked() {
    bool ok = false;
    uint32_t addr = m_editMemAddr->text().toUInt(&ok, 16);
    if (!ok) {
        QMessageBox::warning(this, "Invalid Address", "Please enter a valid hexadecimal 32-bit address.");
        return;
    }
    auto res = m_swdClient->readMem32(addr);
    if (res.isOk()) {
        m_editMemVal->setText(QString("0x%1").arg(res.data, 8, 16, QChar('0')).toUpper());
        onLogMessage(QString("Read Mem [0x%1] = 0x%2")
            .arg(addr, 8, 16, QChar('0')).toUpper()
            .arg(res.data, 8, 16, QChar('0')).toUpper());
    }
}

void DebugView::onSwdWriteMemClicked() {
    bool okAddr = false, okVal = false;
    uint32_t addr = m_editMemAddr->text().toUInt(&okAddr, 16);
    uint32_t val = m_editMemVal->text().toUInt(&okVal, 16);
    if (!okAddr || !okVal) {
        QMessageBox::warning(this, "Invalid Input", "Please enter valid 32-bit hexadecimal address and value.");
        return;
    }
    auto res = m_swdClient->writeMem32(addr, val);
    if (res.isOk()) {
        onLogMessage(QString("Write Mem [0x%1] = 0x%2 OK")
            .arg(addr, 8, 16, QChar('0')).toUpper()
            .arg(val, 8, 16, QChar('0')).toUpper());
    }
}

} // namespace UdooKey
