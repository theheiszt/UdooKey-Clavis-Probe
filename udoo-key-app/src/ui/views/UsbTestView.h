#pragma once

#include <QWidget>
#include <QListWidget>
#include <QTreeWidget>
#include <QTabWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QProgressBar>
#include <memory>

#include "IUsbHost.h"
#include "MockUsbHost.h"
#include "UsbInjectionEngine.h"
#include "UsbTestSequence.h"
#include "UsbDeviceEmulator.h"
#include "HexViewWidget.h"

namespace UdooKey {

class UsbTestView : public QWidget {
    Q_OBJECT

public:
    explicit UsbTestView(QWidget* parent = nullptr);
    ~UsbTestView() override;

public slots:
    void refreshDeviceList();
    void onOpenDeviceClicked();
    void onCloseDeviceClicked();
    void onSendControlTransfer();
    void onSendBulkTransfer();
    void onRunProbeSequence();
    void onToggleEmulation();

private slots:
    void onDeviceSelected(int row);
    void onDescriptorItemClicked(QTreeWidgetItem* item, int column);
    void onTransferExecuted(const TransactionResult& result);
    void onEmulatorTelemetryGenerated(const QByteArray& report);
    void onEmulatorStatsUpdated(uint64_t totalBytes, double throughputKBs);

private:
    void setupUi();
    void populateDescriptorTree();

    std::unique_ptr<IUsbHost> m_usbHost;
    std::unique_ptr<UsbInjectionEngine> m_injectionEngine;
    std::unique_ptr<UsbDeviceEmulator> m_emulator;

    // UI elements - Left Pane
    QListWidget* m_deviceList;
    QPushButton* m_btnRefresh;
    QPushButton* m_btnOpen;
    QPushButton* m_btnClose;
    QLabel* m_lblDeviceStatus;

    // UI elements - Center Pane
    QTreeWidget* m_descriptorTree;
    HexViewWidget* m_hexView;

    // UI elements - Right Pane (Control & Tests)
    QTabWidget* m_consoleTabs;

    // Manual Control Tab
    QLineEdit* m_editReqType;
    QLineEdit* m_editRequest;
    QLineEdit* m_editValue;
    QLineEdit* m_editIndex;
    QSpinBox*  m_spinLength;
    QLineEdit* m_editDataOut;
    QPushButton* m_btnSendControl;

    // Manual Bulk Tab
    QSpinBox*  m_spinBulkEp;
    QComboBox* m_comboBulkDir;
    QLineEdit* m_editBulkData;
    QPushButton* m_btnSendBulk;

    // Sequences Tab
    QPushButton* m_btnRunProbeSeq;
    QProgressBar* m_seqProgress;
    QTextEdit* m_txtSeqLog;

    // Emulation Tab
    QComboBox* m_comboEmuMode;
    QPushButton* m_btnStartEmu;
    QLabel* m_lblEmuStats;

    // Stored raw descriptors for hex viewing
    QByteArray m_rawDeviceDesc;
    QByteArray m_rawConfigDesc;
    QList<UsbDeviceInfo> m_cachedDevices;
};

} // namespace UdooKey
