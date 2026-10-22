#include "UsbTestView.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QHeaderView>
#include <QMessageBox>
#include "Theme.h"
#include "Logger.h"

namespace UdooKey {

UsbTestView::UsbTestView(QWidget* parent)
    : QWidget(parent)
    , m_usbHost(std::make_unique<MockUsbHost>())
    , m_injectionEngine(std::make_unique<UsbInjectionEngine>(m_usbHost.get(), this))
    , m_emulator(std::make_unique<UsbDeviceEmulator>(this)) {

    m_usbHost->initialize();

    setupUi();

    connect(m_injectionEngine.get(), &UsbInjectionEngine::transferExecuted,
            this, &UsbTestView::onTransferExecuted);
    connect(m_emulator.get(), &UsbDeviceEmulator::telemetryReportGenerated,
            this, &UsbTestView::onEmulatorTelemetryGenerated);
    connect(m_emulator.get(), &UsbDeviceEmulator::statisticsUpdated,
            this, &UsbTestView::onEmulatorStatsUpdated);

    refreshDeviceList();
}

UsbTestView::~UsbTestView() {
    m_emulator->stop();
    m_usbHost->shutdown();
}

void UsbTestView::setupUi() {
    auto mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    auto mainSplitter = new QSplitter(Qt::Horizontal, this);

    // ==========================================
    // 1. LEFT PANE: Device Tree & Discovery
    // ==========================================
    auto leftBox = new QGroupBox("USB Devices", this);
    auto leftLayout = new QVBoxLayout(leftBox);

    m_deviceList = new QListWidget(this);
    connect(m_deviceList, &QListWidget::currentRowChanged, this, &UsbTestView::onDeviceSelected);
    leftLayout->addWidget(m_deviceList);

    auto btnRow = new QHBoxLayout();
    m_btnRefresh = new QPushButton("🔄 Refresh", this);
    connect(m_btnRefresh, &QPushButton::clicked, this, &UsbTestView::refreshDeviceList);
    btnRow->addWidget(m_btnRefresh);

    m_btnOpen = new QPushButton("Open Device", this);
    m_btnOpen->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    connect(m_btnOpen, &QPushButton::clicked, this, &UsbTestView::onOpenDeviceClicked);
    btnRow->addWidget(m_btnOpen);

    m_btnClose = new QPushButton("Close", this);
    m_btnClose->setEnabled(false);
    connect(m_btnClose, &QPushButton::clicked, this, &UsbTestView::onCloseDeviceClicked);
    btnRow->addWidget(m_btnClose);

    leftLayout->addLayout(btnRow);

    m_lblDeviceStatus = new QLabel("Status: Closed", this);
    m_lblDeviceStatus->setStyleSheet("color: #8E8E9F;");
    leftLayout->addWidget(m_lblDeviceStatus);

    mainSplitter->addWidget(leftBox);

    // ==========================================
    // 2. CENTER PANE: Decoded Hierarchy & HexView
    // ==========================================
    auto centerSplitter = new QSplitter(Qt::Vertical, this);

    auto descBox = new QGroupBox("Descriptor Hierarchy", this);
    auto descLayout = new QVBoxLayout(descBox);
    m_descriptorTree = new QTreeWidget(this);
    m_descriptorTree->setHeaderLabels({"Descriptor Element", "Value / Offset"});
    m_descriptorTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_descriptorTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    connect(m_descriptorTree, &QTreeWidget::itemClicked, this, &UsbTestView::onDescriptorItemClicked);
    descLayout->addWidget(m_descriptorTree);
    centerSplitter->addWidget(descBox);

    auto hexBox = new QGroupBox("Raw Descriptor / Payload Buffer", this);
    auto hexLayout = new QVBoxLayout(hexBox);
    m_hexView = new HexViewWidget(this);
    hexLayout->addWidget(m_hexView);
    centerSplitter->addWidget(hexBox);

    centerSplitter->setStretchFactor(0, 3);
    centerSplitter->setStretchFactor(1, 2);
    mainSplitter->addWidget(centerSplitter);

    // ==========================================
    // 3. RIGHT PANE: Control / Injection / Emulation
    // ==========================================
    m_consoleTabs = new QTabWidget(this);

    // Tab 3.1: Control Transfer Injection
    auto tabControl = new QWidget();
    auto ctrlLayout = new QVBoxLayout(tabControl);

    auto formGrid = new QGridLayout();
    formGrid->addWidget(new QLabel("bmRequestType (Hex):"), 0, 0);
    m_editReqType = new QLineEdit("0x80", this);
    formGrid->addWidget(m_editReqType, 0, 1);

    formGrid->addWidget(new QLabel("bRequest (Hex):"), 1, 0);
    m_editRequest = new QLineEdit("0x06", this);
    formGrid->addWidget(m_editRequest, 1, 1);

    formGrid->addWidget(new QLabel("wValue (Hex):"), 2, 0);
    m_editValue = new QLineEdit("0x0100", this);
    formGrid->addWidget(m_editValue, 2, 1);

    formGrid->addWidget(new QLabel("wIndex (Hex):"), 3, 0);
    m_editIndex = new QLineEdit("0x0000", this);
    formGrid->addWidget(m_editIndex, 3, 1);

    formGrid->addWidget(new QLabel("wLength (Bytes):"), 4, 0);
    m_spinLength = new QSpinBox(this);
    m_spinLength->setRange(0, 4096);
    m_spinLength->setValue(18);
    formGrid->addWidget(m_spinLength, 4, 1);

    formGrid->addWidget(new QLabel("Data Out (Hex):"), 5, 0);
    m_editDataOut = new QLineEdit("", this);
    formGrid->addWidget(m_editDataOut, 5, 1);

    ctrlLayout->addLayout(formGrid);

    m_btnSendControl = new QPushButton("🚀 Send Control Transfer", this);
    m_btnSendControl->setStyleSheet("background-color: #007A5E; font-weight: bold; color: white; padding: 6px;");
    connect(m_btnSendControl, &QPushButton::clicked, this, &UsbTestView::onSendControlTransfer);
    ctrlLayout->addWidget(m_btnSendControl);

    ctrlLayout->addSpacing(12);

    // Bulk / Interrupt
    auto bulkBox = new QGroupBox("Bulk / Interrupt Transfer", this);
    auto bulkLayout = new QGridLayout(bulkBox);

    bulkLayout->addWidget(new QLabel("Endpoint:"), 0, 0);
    m_spinBulkEp = new QSpinBox(this);
    m_spinBulkEp->setRange(1, 15);
    m_spinBulkEp->setValue(1);
    bulkLayout->addWidget(m_spinBulkEp, 0, 1);

    bulkLayout->addWidget(new QLabel("Direction:"), 1, 0);
    m_comboBulkDir = new QComboBox(this);
    m_comboBulkDir->addItems({"IN (Read from device)", "OUT (Write to device)"});
    bulkLayout->addWidget(m_comboBulkDir, 1, 1);

    bulkLayout->addWidget(new QLabel("Payload (Hex):"), 2, 0);
    m_editBulkData = new QLineEdit("55 AA 01 02", this);
    bulkLayout->addWidget(m_editBulkData, 2, 1);

    m_btnSendBulk = new QPushButton("⚡ Execute Endpoint Transfer", this);
    connect(m_btnSendBulk, &QPushButton::clicked, this, &UsbTestView::onSendBulkTransfer);
    bulkLayout->addWidget(m_btnSendBulk, 3, 0, 1, 2);

    ctrlLayout->addWidget(bulkBox);
    ctrlLayout->addStretch();
    m_consoleTabs->addTab(tabControl, "Packet Injection");

    // Tab 3.2: Automated Sequences
    auto tabSeq = new QWidget();
    auto seqLayout = new QVBoxLayout(tabSeq);

    m_btnRunProbeSeq = new QPushButton("▶ Run Standard UDOO Compliance Probe", this);
    m_btnRunProbeSeq->setStyleSheet("background-color: #1A5E9A; font-weight: bold; color: white; padding: 6px;");
    connect(m_btnRunProbeSeq, &QPushButton::clicked, this, &UsbTestView::onRunProbeSequence);
    seqLayout->addWidget(m_btnRunProbeSeq);

    m_seqProgress = new QProgressBar(this);
    m_seqProgress->setRange(0, 100);
    m_seqProgress->setValue(0);
    seqLayout->addWidget(m_seqProgress);

    m_txtSeqLog = new QTextEdit(this);
    m_txtSeqLog->setReadOnly(true);
    m_txtSeqLog->setFont(QFont("Consolas", 9));
    m_txtSeqLog->setStyleSheet("background-color: #0E0E12; color: #00FFCC;");
    seqLayout->addWidget(m_txtSeqLog);

    m_consoleTabs->addTab(tabSeq, "Test Sequences");

    // Tab 3.3: Device Emulation
    auto tabEmu = new QWidget();
    auto emuLayout = new QVBoxLayout(tabEmu);

    emuLayout->addWidget(new QLabel("Emulation Profile:"));
    m_comboEmuMode = new QComboBox(this);
    m_comboEmuMode->addItem("CDC ACM Virtual COM Loopback", static_cast<int>(UsbEmulatorMode::CdcAcmLoopback));
    m_comboEmuMode->addItem("HID Periodic Telemetry Generator", static_cast<int>(UsbEmulatorMode::HidTelemetryGenerator));
    m_comboEmuMode->addItem("Throughput Burst Stress Test", static_cast<int>(UsbEmulatorMode::ThroughputStressTest));
    emuLayout->addWidget(m_comboEmuMode);

    m_btnStartEmu = new QPushButton("Start Emulation", this);
    m_btnStartEmu->setStyleSheet("background-color: #7A5E00; font-weight: bold; color: white;");
    connect(m_btnStartEmu, &QPushButton::clicked, this, &UsbTestView::onToggleEmulation);
    emuLayout->addWidget(m_btnStartEmu);

    m_lblEmuStats = new QLabel("Status: Emulation Inactive", this);
    m_lblEmuStats->setStyleSheet("padding: 8px; color: #CCCCCC; font-size: 12px;");
    emuLayout->addWidget(m_lblEmuStats);

    emuLayout->addStretch();
    m_consoleTabs->addTab(tabEmu, "Device Emulation");

    mainSplitter->addWidget(m_consoleTabs);

    mainSplitter->setStretchFactor(0, 2);
    mainSplitter->setStretchFactor(1, 4);
    mainSplitter->setStretchFactor(2, 3);
    mainLayout->addWidget(mainSplitter);
}

void UsbTestView::refreshDeviceList() {
    m_deviceList->clear();
    m_cachedDevices = m_usbHost->enumerateDevices();

    for (const auto& dev : m_cachedDevices) {
        QString label = dev.summary();
        if (dev.isUdooKey) {
            label = "★ [UDOO KEY] " + label;
        }
        auto item = new QListWidgetItem(label, m_deviceList);
        if (dev.isUdooKey) {
            item->setForeground(QColor(Theme::COLOR_ACCENT));
        }
    }

    if (!m_cachedDevices.isEmpty()) {
        m_deviceList->setCurrentRow(0);
    }
}

void UsbTestView::onDeviceSelected(int row) {
    if (row < 0 || row >= m_cachedDevices.size()) return;
    const auto& dev = m_cachedDevices[row];
    m_lblDeviceStatus->setText(QString("Selected: 0x%1:0x%2 (%3)")
                               .arg(dev.vendorId, 4, 16, QChar('0'))
                               .arg(dev.productId, 4, 16, QChar('0'))
                               .arg(m_usbHost->isDeviceOpen() ? "Active" : "Closed"));
}

void UsbTestView::onOpenDeviceClicked() {
    int row = m_deviceList->currentRow();
    if (row < 0 || row >= m_cachedDevices.size()) return;

    const auto& dev = m_cachedDevices[row];
    if (m_usbHost->openDevice(dev.vendorId, dev.productId)) {
        m_btnOpen->setEnabled(false);
        m_btnClose->setEnabled(true);
        m_lblDeviceStatus->setText(QString("Connected to 0x%1:0x%2")
                                   .arg(dev.vendorId, 4, 16, QChar('0'))
                                   .arg(dev.productId, 4, 16, QChar('0')));
        m_lblDeviceStatus->setStyleSheet("color: #00FFCC; font-weight: bold;");
        populateDescriptorTree();
    } else {
        QMessageBox::warning(this, "USB Error", "Failed to open USB device handle.");
    }
}

void UsbTestView::onCloseDeviceClicked() {
    m_usbHost->closeDevice();
    m_btnOpen->setEnabled(true);
    m_btnClose->setEnabled(false);
    m_lblDeviceStatus->setText("Status: Closed");
    m_lblDeviceStatus->setStyleSheet("color: #8E8E9F;");
    m_descriptorTree->clear();
    m_hexView->clear();
}

void UsbTestView::populateDescriptorTree() {
    m_descriptorTree->clear();
    if (!m_usbHost->isDeviceOpen()) return;

    // 1. Fetch Device Descriptor
    m_rawDeviceDesc = m_usbHost->getDescriptor(static_cast<uint8_t>(UsbDescriptorType::Device), 0, 0, 18);
    DeviceDescriptor devDesc;
    UsbDescriptorParser::parseDeviceDescriptor(m_rawDeviceDesc, devDesc);

    auto itemDev = new QTreeWidgetItem(m_descriptorTree, {"Device Descriptor (18 bytes)", ""});
    itemDev->setExpanded(true);
    itemDev->setData(0, Qt::UserRole, m_rawDeviceDesc);

    new QTreeWidgetItem(itemDev, {"bLength", QString::number(devDesc.bLength)});
    new QTreeWidgetItem(itemDev, {"bDescriptorType", "0x01 (DEVICE)"});
    new QTreeWidgetItem(itemDev, {"bcdUSB", QString("0x%1").arg(devDesc.bcdUSB, 4, 16, QChar('0'))});
    new QTreeWidgetItem(itemDev, {"bDeviceClass", QString("0x%1 (%2)").arg(devDesc.bDeviceClass, 2, 16, QChar('0')).arg(UsbDescriptorParser::resolveClassName(devDesc.bDeviceClass))});
    new QTreeWidgetItem(itemDev, {"bMaxPacketSize0", QString::number(devDesc.bMaxPacketSize0)});
    new QTreeWidgetItem(itemDev, {"idVendor", QString("0x%1").arg(devDesc.idVendor, 4, 16, QChar('0')).toUpper()});
    new QTreeWidgetItem(itemDev, {"idProduct", QString("0x%1").arg(devDesc.idProduct, 4, 16, QChar('0')).toUpper()});
    new QTreeWidgetItem(itemDev, {"bNumConfigurations", QString::number(devDesc.bNumConfigurations)});

    // 2. Fetch Config Descriptor Tree
    m_rawConfigDesc = m_usbHost->getDescriptor(static_cast<uint8_t>(UsbDescriptorType::Configuration), 0, 0, 512);
    ConfigurationDescriptor cfgDesc;
    UsbDescriptorParser::parseConfigurationTree(m_rawConfigDesc, cfgDesc);

    auto itemCfg = new QTreeWidgetItem(m_descriptorTree, {"Configuration Descriptor", QString("%1 interfaces").arg(cfgDesc.interfaces.size())});
    itemCfg->setExpanded(true);
    itemCfg->setData(0, Qt::UserRole, m_rawConfigDesc);

    new QTreeWidgetItem(itemCfg, {"wTotalLength", QString("%1 bytes").arg(cfgDesc.wTotalLength)});
    new QTreeWidgetItem(itemCfg, {"bNumInterfaces", QString::number(cfgDesc.bNumInterfaces)});
    new QTreeWidgetItem(itemCfg, {"Max Power", QString("%1 mA").arg(cfgDesc.maxPowerMilliAmps())});

    for (const auto& iface : cfgDesc.interfaces) {
        auto itemIface = new QTreeWidgetItem(itemCfg, {
            QString("Interface %1 (Class %2)").arg(iface.bInterfaceNumber).arg(iface.className()),
            QString("%1 endpoints").arg(iface.endpoints.size())
        });
        itemIface->setExpanded(true);
        itemIface->setData(0, Qt::UserRole, iface.toByteArray());

        for (const auto& ep : iface.endpoints) {
            auto itemEp = new QTreeWidgetItem(itemIface, {
                QString("Endpoint 0x%1 (%2 %3)").arg(ep.bEndpointAddress, 2, 16, QChar('0')).toUpper().arg(ep.directionString()).arg(ep.transferTypeString()),
                QString("MaxPacket: %1, Interval: %2").arg(ep.wMaxPacketSize).arg(ep.bInterval)
            });
            itemEp->setData(0, Qt::UserRole, ep.toByteArray());
        }
    }

    // Default hex view to device descriptor
    m_hexView->setData(m_rawDeviceDesc);
}

void UsbTestView::onDescriptorItemClicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item) return;
    QByteArray raw = item->data(0, Qt::UserRole).toByteArray();
    if (!raw.isEmpty()) {
        m_hexView->setData(raw);
    }
}

void UsbTestView::onSendControlTransfer() {
    bool ok = false;
    UsbSetupPacket setup;
    setup.bmRequestType = static_cast<uint8_t>(m_editReqType->text().toInt(&ok, 16));
    setup.bRequest = static_cast<uint8_t>(m_editRequest->text().toInt(&ok, 16));
    setup.wValue = static_cast<uint16_t>(m_editValue->text().toInt(&ok, 16));
    setup.wIndex = static_cast<uint16_t>(m_editIndex->text().toInt(&ok, 16));
    setup.wLength = static_cast<uint16_t>(m_spinLength->value());

    QByteArray dataOut = QByteArray::fromHex(m_editDataOut->text().toUtf8());
    m_injectionEngine->executeControlTransfer(setup, dataOut);
}

void UsbTestView::onSendBulkTransfer() {
    uint8_t ep = static_cast<uint8_t>(m_spinBulkEp->value());
    bool isRead = (m_comboBulkDir->currentIndex() == 0);
    if (isRead) ep |= 0x80;

    QByteArray dataOut = QByteArray::fromHex(m_editBulkData->text().toUtf8());
    m_injectionEngine->executeBulkTransfer(ep, dataOut, isRead, 64);
}

void UsbTestView::onTransferExecuted(const TransactionResult& result) {
    if (!result.rxData.isEmpty()) {
        m_hexView->setData(result.rxData);
    } else if (!result.txData.isEmpty()) {
        m_hexView->setData(result.txData);
    }
}

void UsbTestView::onRunProbeSequence() {
    if (!m_usbHost->isDeviceOpen()) {
        QMessageBox::warning(this, "USB Sequence", "Please open a USB device before running sequences.");
        return;
    }

    m_txtSeqLog->clear();
    m_seqProgress->setValue(0);
    m_btnRunProbeSeq->setEnabled(false);

    auto seq = UsbTestSequence::standardUdooKeyProbeSequence();

    TaskManager::instance().submitTask("USB Compliance Probe", "Running standard UDOO KEY USB test sequence",
        [this, seq](std::shared_ptr<CancellationToken> token, std::function<void(int, const QString&)> progress) {
            auto results = const_cast<UsbTestSequence&>(seq).execute(
                m_injectionEngine.get(), token,
                [this, progress](int current, int total, const QString& status) {
                    int pct = (current * 100) / total;
                    progress(pct, status);
                    QMetaObject::invokeMethod(this, [this, pct, status]() {
                        m_seqProgress->setValue(pct);
                        m_txtSeqLog->append(status);
                    });
                }
            );

            bool allPassed = true;
            for (const auto& r : results) {
                if (!r.passed) allPassed = false;
                QMetaObject::invokeMethod(this, [this, r]() {
                    m_txtSeqLog->append(QString("  [%1] %2: %3")
                                        .arg(r.passed ? "PASS" : "FAIL")
                                        .arg(r.stepName)
                                        .arg(r.message));
                });
            }

            QMetaObject::invokeMethod(this, [this, allPassed]() {
                m_btnRunProbeSeq->setEnabled(true);
                m_seqProgress->setValue(100);
                m_txtSeqLog->append(QString("\n=== Sequence Completed: %1 ===").arg(allPassed ? "ALL TESTS PASSED" : "TEST FAILED"));
            });

            return allPassed;
        }
    );
}

void UsbTestView::onToggleEmulation() {
    if (m_emulator->isRunning()) {
        m_emulator->stop();
        m_btnStartEmu->setText("Start Emulation");
        m_btnStartEmu->setStyleSheet("background-color: #7A5E00; font-weight: bold; color: white;");
        m_lblEmuStats->setText("Status: Emulation Inactive");
    } else {
        auto mode = static_cast<UsbEmulatorMode>(m_comboEmuMode->currentData().toInt());
        m_emulator->start(mode, 25);
        m_btnStartEmu->setText("Stop Emulation");
        m_btnStartEmu->setStyleSheet("background-color: #9A2020; font-weight: bold; color: white;");
        m_lblEmuStats->setText("Status: Emulation Running...");
    }
}

void UsbTestView::onEmulatorTelemetryGenerated(const QByteArray& report) {
    m_hexView->setData(report);
}

void UsbTestView::onEmulatorStatsUpdated(uint64_t totalBytes, double throughputKBs) {
    m_lblEmuStats->setText(QString("Generated: %1 bytes | Throughput: %2 KB/s")
                           .arg(totalBytes)
                           .arg(throughputKBs, 0, 'f', 2));
}

} // namespace UdooKey
