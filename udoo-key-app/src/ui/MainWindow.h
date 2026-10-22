#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <memory>
#include "DeviceManager.h"
#include "StreamDemux.h"
#include "BinaryLogicClient.h"
#include "SumpClient.h"
#include "TerminalClient.h"
#include "Logger.h"
#include "DeviceCapabilities.h"

namespace UdooKey {

class LogicAnalyzerView;
class BusToolsView;
class MeterView;
class TerminalConsoleView;
class DebugView;
class UsbTestView;
class GlobalTimelineView;
class CommunicationDiagnosticsView;

class BusToolsClient;
class ModeClient;
class GpioClient;
class SyntaxClient;
class WirelessClient;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnectActionTriggered();
    void onDisconnectActionTriggered();
    void onDeviceConnected(const QString& portName, uint32_t capabilities);
    void onDeviceConnectedWithCaps(const QString& portName, const DeviceCapabilities& caps);
    void onDeviceDisconnected();
    void onDataReceivedFromTransport(const QByteArray& data);
    void onExportRequested();
    void onLogMessage(LogLevel level, const QString& message);
    void onOpenCommandPalette();
    void onShowDeviceInfo();
    void onSaveSessionRequested();
    void onLoadSessionRequested();
    void onGenerateReportRequested();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUi();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void wireSubsystems();
    void registerShortcuts();

    // Protocol Clients
    std::unique_ptr<StreamDemux> m_demux;
    std::unique_ptr<BinaryLogicClient> m_logicClient;
    std::unique_ptr<SumpClient> m_sumpClient;
    std::unique_ptr<TerminalClient> m_terminalClient;
    std::unique_ptr<BusToolsClient> m_busToolsClient;
    std::unique_ptr<ModeClient>     m_modeClient;
    std::unique_ptr<GpioClient>     m_gpioClient;
    std::unique_ptr<SyntaxClient>   m_syntaxClient;
    std::unique_ptr<WirelessClient> m_wirelessClient;

    // Instrument Views
    QTabWidget* m_tabWidget;
    LogicAnalyzerView* m_logicView;
    BusToolsView* m_busToolsView;
    MeterView* m_meterView;
    TerminalConsoleView* m_terminalView;
    DebugView* m_debugView;
    UsbTestView* m_usbTestView;
    GlobalTimelineView* m_timelineView;
    CommunicationDiagnosticsView* m_commDiagView;
    QTextEdit* m_txtLogView;

    // UI Badges & Status
    QLabel* m_lblConnStatus;
    QLabel* m_lblCapabilities;
    QLabel* m_lblWifiStatus;
    QPushButton* m_btnConnect;
    QPushButton* m_btnDisconnect;
    QPushButton* m_btnPalette;
    QPushButton* m_btnInfo;
};

} // namespace UdooKey
