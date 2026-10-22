#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QTextEdit>
#include <memory>
#include "JtagClient.h"
#include "SwdClient.h"
#include "MockDebugTransport.h"
#include "SerialDebugTransport.h"
#include "ITransport.h"

namespace UdooKey {

class DebugView : public QWidget {
    Q_OBJECT

public:
    explicit DebugView(QWidget* parent = nullptr);
    ~DebugView() override = default;

    void setLiveTransport(ITransport* transport);

private slots:
    // JTAG Slots
    void onJtagResetClicked();
    void onJtagScanChainClicked();
    void onJtagNavigateStateClicked();
    void onJtagSetClockClicked();
    void onJtagShiftClicked();
    void onJtagTapStateChanged(udoo::debug::JtagTapState state);
    void onJtagChainScanned(const std::vector<udoo::debug::JtagDevice>& devices);
    void onJtagTransactionCompleted(const udoo::debug::JtagTransactionResult& result);
    void onJtagTargetDeviceChanged(int index);
    void onJtagStandardInstructionSelected(int index);
    void onJtagExecuteTargetedShiftClicked();
    void onJtagExecuteBatchClicked();
    void onJtagBatchCompleted(const udoo::debug::JtagBatchResult& result);

    // SWD Slots
    void onSwdResetClicked();
    void onSwdConnectClicked();
    void onSwdSetClockClicked();
    void onSwdReadDpClicked();
    void onSwdWriteDpClicked();
    void onSwdReadApClicked();
    void onSwdWriteApClicked();
    void onSwdReadMemClicked();
    void onSwdWriteMemClicked();
    void onSwdTargetConnected(const udoo::debug::SwdDpidr& dpidr);
    void onSwdTargetDisconnected();

    // Mode / Transport Slots
    void onTransportModeChanged(int index);
    void onLogMessage(const QString& msg);

private:
    void setupUi();
    QWidget* createJtagTab();
    QWidget* createSwdTab();
    void updateTapStateBadge(udoo::debug::JtagTapState state);

    // Transport & Clients
    std::unique_ptr<udoo::debug::MockDebugTransport> m_mockTransport;
    std::unique_ptr<udoo::debug::SerialDebugTransport> m_serialTransport;
    udoo::debug::IDebugTransport* m_activeTransport = nullptr;

    std::unique_ptr<udoo::debug::JtagClient> m_jtagClient;
    std::unique_ptr<udoo::debug::SwdClient> m_swdClient;

    // Transport Mode Controls
    QComboBox* m_comboTransportMode;
    QLabel* m_lblTransportStatus;

    // JTAG UI Elements
    QLabel* m_lblTapState;
    QComboBox* m_comboJtagClock;
    QPushButton* m_btnJtagReset;
    QPushButton* m_btnJtagScanChain;
    QComboBox* m_comboTargetTapState;
    QPushButton* m_btnJtagNavigate;
    QTreeWidget* m_treeJtagChain;
    QLabel* m_lblChainTopology;
    QComboBox* m_comboTargetDevice;
    QComboBox* m_comboStdInstruction;
    QPushButton* m_btnExecuteTargetedShift;
    QPushButton* m_btnExecuteBatch;
    QComboBox* m_comboShiftType;
    QSpinBox* m_spinShiftBits;
    QLineEdit* m_editShiftTdi;
    QComboBox* m_comboShiftExitState;
    QPushButton* m_btnExecuteShift;
    QLineEdit* m_editShiftTdoHex;
    QLineEdit* m_editShiftTdoBin;

    // SWD UI Elements
    QLabel* m_lblSwdTargetStatus;
    QLabel* m_lblSwdDpidr;
    QLabel* m_lblSwdTargetInfo;
    QComboBox* m_comboSwdClock;
    QPushButton* m_btnSwdReset;
    QPushButton* m_btnSwdConnect;
    QComboBox* m_comboDpReg;
    QLineEdit* m_editDpVal;
    QPushButton* m_btnReadDp;
    QPushButton* m_btnWriteDp;
    QComboBox* m_comboApReg;
    QLineEdit* m_editApVal;
    QPushButton* m_btnReadAp;
    QPushButton* m_btnWriteAp;
    QLineEdit* m_editMemAddr;
    QLineEdit* m_editMemVal;
    QPushButton* m_btnReadMem;
    QPushButton* m_btnWriteMem;

    // Log Widget
    QTextEdit* m_txtDebugLog;
};

} // namespace UdooKey
