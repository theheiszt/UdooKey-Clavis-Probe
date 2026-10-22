#pragma once

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include "TerminalClient.h"
#include "Types.h"

namespace UdooKey {

class MeterView : public QWidget {
    Q_OBJECT

public:
    explicit MeterView(TerminalClient* terminalClient, QWidget* parent = nullptr);
    ~MeterView() override = default;

    void setTerminalClient(TerminalClient* client);

private slots:
    void onPollTimerTick();
    void onAdcReadingReceived(const MeterReading& reading);
    void onContinuityResultReceived(bool connected, const QString& detail);
    void onResetStatsClicked();

private:
    void setupUi();

    TerminalClient* m_terminalClient;
    QTimer m_pollTimer;

    // UI elements
    QLabel* m_lblVoltageDigital;
    QLabel* m_lblRawAdc;
    QProgressBar* m_barVoltage;
    QLabel* m_lblContinuityBanner;

    QLabel* m_lblMinVoltage;
    QLabel* m_lblMaxVoltage;
    QLabel* m_lblAvgVoltage;

    QPushButton* m_btnAutoPoll;
    QPushButton* m_btnResetStats;

    // Statistics
    double m_minV = 999.0;
    double m_maxV = -999.0;
    double m_sumV = 0.0;
    uint32_t m_countV = 0;
};

} // namespace UdooKey
