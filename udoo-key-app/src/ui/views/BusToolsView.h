#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include <QCheckBox>
#include <QListWidget>

#include "BusToolsClient.h"
#include "ModeClient.h"
#include "GpioClient.h"
#include "SyntaxClient.h"
#include "WirelessClient.h"
#include "TerminalClient.h"

namespace UdooKey {

/**
 * @brief Comprehensive Interactive Bus Tools Workspace View (Phase 6)
 *
 * Unifies all Phase 1–5 Bus Pirate services into a professional desktop workspace:
 *   1. Mode Multiplexer & Invariants Controller (Safe Hi-Z)
 *   2. I2C Bus Explorer & 7-Bit Address Matrix Scanner
 *   3. SPI Transaction Builder & JEDEC SFDP Flash Probe
 *   4. Dallas 1-Wire Master Engine & Search ROM Tree
 *   5. Secondary UART Bridge Monitor & Log
 *   6. Digital I/O (DIO) 8-Pin Probe Bus Controller
 *   7. Interactive Bus Syntax & Macro Execution Shell
 *   8. Wireless Subsystem: Infrared (IR) & nRF24L01 2.4 GHz Transceiver
 */
class BusToolsView : public QWidget {
    Q_OBJECT

public:
    explicit BusToolsView(BusToolsClient* busTools = nullptr,
                         ModeClient* mode = nullptr,
                         GpioClient* gpio = nullptr,
                         SyntaxClient* syntax = nullptr,
                         WirelessClient* wireless = nullptr,
                         TerminalClient* terminal = nullptr,
                         QWidget* parent = nullptr);
    ~BusToolsView() override = default;

    void setClients(BusToolsClient* busTools,
                    ModeClient* mode,
                    GpioClient* gpio,
                    SyntaxClient* syntax,
                    WirelessClient* wireless,
                    TerminalClient* terminal = nullptr);

private slots:
    // Mode Multiplexer
    void onModeChangeRequested(int index);
    void onReleaseHiZClicked();
    void onModeStatusReceived(const InstrumentModeStatusPayload& status);

    // I2C Explorer
    void onScanI2cClicked();
    void onI2cScanCompleted(const QVector<uint8_t>& addrs);
    void onSendI2cTransferClicked();
    void onI2cTransferCompleted(uint8_t status, const QByteArray& readData);

    // SPI Builder & Flash Probe
    void onSendSpiClicked();
    void onSpiTransferCompleted(const QByteArray& rxData);
    void onProbeFlashClicked();
    void onSpiFlashInfoReceived(const InstrumentFlashInfoPayload& flashInfo);

    // 1-Wire Search
    void onSearch1WireClicked();
    void onOneWireSearchCompleted(const QVector<QByteArray>& romIds);
    void onOneWireTransferClicked();
    void onOneWireTransferCompleted(bool presenceOk, const QByteArray& readData);

    // UART Bridge
    void onSendUartClicked();
    void onUartDataReceived(const QByteArray& data);

    // GPIO Probe Bus
    void onGpioPinToggled(int pinIndex);
    void onGpioReadAllClicked();
    void onGpioWriteMaskClicked();
    void onGpioStatusReceived(const InstrumentGpioStatusPayload& status);

    // Syntax & Macros
    void onExecSyntaxClicked();
    void onMacroClicked(int macroId);
    void onSyntaxResultReceived(uint8_t status, const QString& result);

    // Wireless IR & nRF24
    void onSendIrNecClicked();
    void onToggleIrRx(bool checked);
    void onIrRxEventReceived(const InstrumentIrRxEventPayload& ev);
    void onSendNrf24Clicked();
    void onQueryNrf24StatusClicked();
    void onNrf24StatusReceived(const InstrumentNrf24StatusPayload& st);
    void onNrf24RxEventReceived(const InstrumentNrf24RxEventPayload& ev);

private:
    void setupUi();
    QWidget* createModeBar();
    QWidget* createI2cTab();
    QWidget* createSpiTab();
    QWidget* createOneWireTab();
    QWidget* createUartTab();
    QWidget* createGpioTab();
    QWidget* createSyntaxTab();
    QWidget* createWirelessTab();

    // Protocol Clients
    BusToolsClient* m_busToolsClient{nullptr};
    ModeClient*     m_modeClient{nullptr};
    GpioClient*     m_gpioClient{nullptr};
    SyntaxClient*   m_syntaxClient{nullptr};
    WirelessClient* m_wirelessClient{nullptr};
    TerminalClient* m_terminalClient{nullptr};

    // Mode Bar Widgets
    QComboBox*   m_comboMode{nullptr};
    QPushButton* m_btnReleaseHiZ{nullptr};
    QLabel*      m_lblActiveMode{nullptr};
    QLabel*      m_lblPinStatus{nullptr};

    // I2C Widgets
    QTableWidget* m_i2cGridTable{nullptr};
    QPushButton*  m_btnScanI2c{nullptr};
    QLabel*       m_lblI2cFound{nullptr};
    QLineEdit*    m_editI2cAddr{nullptr};
    QLineEdit*    m_editI2cWriteData{nullptr};
    QLineEdit*    m_editI2cReadLen{nullptr};
    QPushButton*  m_btnI2cXfer{nullptr};
    QTextEdit*    m_txtI2cLog{nullptr};

    // SPI Widgets
    QLineEdit*    m_editSpiHex{nullptr};
    QComboBox*    m_comboSpiMode{nullptr};
    QLineEdit*    m_editSpiSpeed{nullptr};
    QPushButton*  m_btnSendSpi{nullptr};
    QPushButton*  m_btnProbeFlash{nullptr};
    QTextEdit*    m_txtSpiLog{nullptr};

    // 1-Wire Widgets
    QPushButton*  m_btnSearch1Wire{nullptr};
    QListWidget*  m_list1WireRoms{nullptr};
    QLineEdit*    m_edit1WireWrite{nullptr};
    QLineEdit*    m_edit1WireReadLen{nullptr};
    QPushButton*  m_btn1WireXfer{nullptr};
    QTextEdit*    m_txt1WireLog{nullptr};

    // UART Widgets
    QComboBox*    m_comboUartBaud{nullptr};
    QLineEdit*    m_editUartSend{nullptr};
    QPushButton*  m_btnSendUart{nullptr};
    QTextEdit*    m_txtUartLog{nullptr};

    // GPIO Widgets
    std::array<QPushButton*, 8> m_btnGpioPins{};
    std::array<QCheckBox*, 8>   m_chkGpioDirs{};
    QLineEdit*    m_editGpioMask{nullptr};
    QPushButton*  m_btnGpioWriteMask{nullptr};
    QPushButton*  m_btnGpioReadAll{nullptr};
    QLabel*       m_lblGpioLevels{nullptr};

    // Syntax Widgets
    QLineEdit*    m_editSyntaxInput{nullptr};
    QPushButton*  m_btnExecSyntax{nullptr};
    QTextEdit*    m_txtSyntaxLog{nullptr};

    // Wireless Widgets
    QLineEdit*    m_editIrAddr{nullptr};
    QLineEdit*    m_editIrCmd{nullptr};
    QPushButton*  m_btnSendIrNec{nullptr};
    QCheckBox*    m_chkIrRx{nullptr};
    QTextEdit*    m_txtIrLog{nullptr};

    QLineEdit*    m_editNrf24Channel{nullptr};
    QLineEdit*    m_editNrf24Addr{nullptr};
    QLineEdit*    m_editNrf24Data{nullptr};
    QPushButton*  m_btnSendNrf24{nullptr};
    QPushButton*  m_btnNrf24Status{nullptr};
    QTextEdit*    m_txtNrf24Log{nullptr};
};

} // namespace UdooKey
