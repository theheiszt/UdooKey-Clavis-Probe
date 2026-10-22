#include "DeviceInfoDialog.h"
#include "DeviceManager.h"
#include "TransactionModel.h"
#include <QHBoxLayout>
#include <QHeaderView>

namespace UdooKey {

DeviceInfoDialog::DeviceInfoDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Device Information & Capabilities");
    resize(520, 460);

    setStyleSheet(
        "QDialog { background-color: #16161D; color: #E0E0E0; }"
        "QTreeWidget { background-color: #101016; color: #D0D0D0; border: 1px solid #2B2B36; }"
        "QHeaderView::section { background-color: #1E1E28; color: #A0A0B0; padding: 4px; border: none; }"
        "QPushButton { background-color: #262633; color: white; border: 1px solid #3E3E4F; padding: 6px 14px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #323244; }"
    );

    setupUi();
    populateInfo();
}

void DeviceInfoDialog::setupUi() {
    auto layout = new QVBoxLayout(this);

    m_lblHeader = new QLabel("<h2>UDOO KEY Hardware & Protocol Status</h2>", this);
    m_lblHeader->setStyleSheet("color: #00FFCC; margin-bottom: 4px;");
    layout->addWidget(m_lblHeader);

    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setColumnCount(2);
    m_treeWidget->setHeaderLabels({"Property", "Value"});
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(m_treeWidget);

    auto btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_btnClose = new QPushButton("Close", this);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_btnClose);
    layout->addLayout(btnLayout);
}

void DeviceInfoDialog::populateInfo() {
    m_treeWidget->clear();
    const auto& caps = DeviceManager::instance().currentCapabilities();
    bool connected = DeviceManager::instance().isConnected();

    // 1. Hardware Identity
    auto grpHw = new QTreeWidgetItem(m_treeWidget, {"Hardware Identity", ""});
    grpHw->setExpanded(true);
    new QTreeWidgetItem(grpHw, {"Model", caps.hardwareModel});
    new QTreeWidgetItem(grpHw, {"Primary MCU", caps.mcuPrimary});
    new QTreeWidgetItem(grpHw, {"Coprocessor MCU", caps.mcuCoprocessor});
    new QTreeWidgetItem(grpHw, {"Firmware Version", caps.firmwareVersion});
    new QTreeWidgetItem(grpHw, {"Protocol Version", caps.protocolVersion});

    // 2. Acquisition Subsystem
    auto grpAcq = new QTreeWidgetItem(m_treeWidget, {"Acquisition Specs", ""});
    grpAcq->setExpanded(true);
    new QTreeWidgetItem(grpAcq, {"Digital Channels", QString::number(caps.logicChannels)});
    new QTreeWidgetItem(grpAcq, {"Max Sample Rate", QString("%1 MSPS (%2 MHz)").arg(caps.maxSampleRateHz / 1'000'000).arg(caps.maxSampleRateHz / 1'000'000)});
    new QTreeWidgetItem(grpAcq, {"Min Sample Rate", QString("%1 Hz").arg(caps.minSampleRateHz)});
    new QTreeWidgetItem(grpAcq, {"Sample Buffer Depth", QString("%1 bytes (%2 KiB)").arg(caps.sampleMemoryBytes).arg(caps.sampleMemoryBytes / 1024)});
    new QTreeWidgetItem(grpAcq, {"ADC Channels", QString::number(caps.adcChannels)});
    new QTreeWidgetItem(grpAcq, {"ADC Voltage Range", QString("%1 V").arg(caps.adcVoltageRange, 0, 'f', 1)});
    new QTreeWidgetItem(grpAcq, {"ADC Resolution", QString("%1-bit").arg(caps.adcResolutionBits, 0, 'f', 0)});

    // 3. Supported Instruments
    auto grpInst = new QTreeWidgetItem(m_treeWidget, {"Instrument Capabilities", ""});
    grpInst->setExpanded(true);
    auto addCapItem = [grpInst](const QString& name, bool supported) {
        new QTreeWidgetItem(grpInst, {name, supported ? "✔ Supported" : "✖ Not Available"});
    };
    addCapItem("Logic Analyzer (100 MSPS)", caps.hasLogicAnalyzer);
    addCapItem("SUMP Protocol Engine", caps.hasSumpProtocol);
    addCapItem("Universal Terminal Shell", caps.hasTerminalShell);
    addCapItem("Digital Multimeter (DMM)", caps.hasAdcMeter);
    addCapItem("Continuity Tester", caps.hasContinuity);
    addCapItem("I2C Master & Sniffer", caps.hasI2cTool);
    addCapItem("SPI Master & Sniffer", caps.hasSpiTool);
    addCapItem("UART Bridge", caps.hasUartTool);
    addCapItem("JTAG TAP Controller", caps.hasJtag);
    addCapItem("ARM SWD Debug Engine", caps.hasSwd);
    addCapItem("USB Test & Injection", caps.hasUsbTest);
    addCapItem("USB Device Emulation", caps.hasUsbEmulation);

    // 4. Live Telemetry
    auto grpLive = new QTreeWidgetItem(m_treeWidget, {"Live Telemetry", ""});
    grpLive->setExpanded(true);
    new QTreeWidgetItem(grpLive, {"Transport Status", connected ? "Connected" : "Disconnected"});
    ITransport* trans = DeviceManager::instance().transport();
    new QTreeWidgetItem(grpLive, {"Port Name", (trans && connected) ? trans->portName() : "N/A"});
    new QTreeWidgetItem(grpLive, {"Total Transactions Logged", QString::number(TransactionLogger::instance().count())});
}

} // namespace UdooKey
