#include "MeterView.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>

namespace UdooKey {

MeterView::MeterView(TerminalClient* terminalClient, QWidget* parent)
    : QWidget(parent)
    , m_terminalClient(terminalClient) {
    setupUi();
    setTerminalClient(m_terminalClient);

    connect(&m_pollTimer, &QTimer::timeout, this, &MeterView::onPollTimerTick);
}

void MeterView::setTerminalClient(TerminalClient* client) {
    m_terminalClient = client;
    if (m_terminalClient) {
        connect(m_terminalClient, &TerminalClient::adcReadingReceived, this, &MeterView::onAdcReadingReceived);
        connect(m_terminalClient, &TerminalClient::continuityResultReceived, this, &MeterView::onContinuityResultReceived);
    }
}

void MeterView::setupUi() {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);

    // Controls top bar
    auto topBar = new QHBoxLayout();
    m_btnAutoPoll = new QPushButton("▶ Start Live Monitoring (5 Hz)", this);
    m_btnAutoPoll->setCheckable(true);
    m_btnAutoPoll->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    connect(m_btnAutoPoll, &QPushButton::toggled, [this](bool checked) {
        if (checked) {
            m_btnAutoPoll->setText("⏹ Stop Live Monitoring");
            m_btnAutoPoll->setStyleSheet("font-weight: bold; background-color: #A03030; color: white;");
            m_pollTimer.start(200); // 5 Hz
        } else {
            m_btnAutoPoll->setText("▶ Start Live Monitoring (5 Hz)");
            m_btnAutoPoll->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
            m_pollTimer.stop();
        }
    });

    m_btnResetStats = new QPushButton("↺ Reset Min/Max", this);
    connect(m_btnResetStats, &QPushButton::clicked, this, &MeterView::onResetStatsClicked);

    auto btnSingleRead = new QPushButton("Read Now", this);
    connect(btnSingleRead, &QPushButton::clicked, this, &MeterView::onPollTimerTick);

    topBar->addWidget(m_btnAutoPoll);
    topBar->addWidget(btnSingleRead);
    topBar->addWidget(m_btnResetStats);
    topBar->addStretch();
    layout->addLayout(topBar);

    // Main Voltmeter Group
    auto meterBox = new QGroupBox("Digital Voltmeter (ADC Channel 0 / GP26)", this);
    auto meterLayout = new QVBoxLayout(meterBox);

    // Big 7-Segment style readout
    m_lblVoltageDigital = new QLabel("0.000 V", meterBox);
    m_lblVoltageDigital->setAlignment(Qt::AlignCenter);
    m_lblVoltageDigital->setStyleSheet(
        "font-family: 'Consolas', 'Courier New', monospace; "
        "font-size: 54px; font-weight: bold; color: #00FFCC; "
        "background-color: #121218; border: 2px solid #00D2A0; "
        "border-radius: 8px; padding: 18px;");
    meterLayout->addWidget(m_lblVoltageDigital);

    // Analog Bar Graph
    m_barVoltage = new QProgressBar(meterBox);
    m_barVoltage->setRange(0, 3300); // 0 to 3.3V in millivolts
    m_barVoltage->setValue(0);
    m_barVoltage->setTextVisible(false);
    m_barVoltage->setFixedHeight(22);
    m_barVoltage->setStyleSheet(
        "QProgressBar { background-color: #242430; border: 1px solid #3A3A4A; border-radius: 4px; } "
        "QProgressBar::chunk { background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #00D2A0, stop:1 #00FFCC); border-radius: 3px; }");
    meterLayout->addWidget(m_barVoltage);

    m_lblRawAdc = new QLabel("Raw 12-Bit ADC: 0 / 4095  |  Resolution: 0.805 mV / LSB", meterBox);
    m_lblRawAdc->setAlignment(Qt::AlignCenter);
    m_lblRawAdc->setStyleSheet("color: #8E8E9F; font-size: 12px;");
    meterLayout->addWidget(m_lblRawAdc);

    // Min / Max / Avg stats
    auto statsLayout = new QHBoxLayout();
    m_lblMinVoltage = new QLabel("Min: -- V", meterBox);
    m_lblMaxVoltage = new QLabel("Max: -- V", meterBox);
    m_lblAvgVoltage = new QLabel("Avg: -- V", meterBox);
    m_lblMinVoltage->setStyleSheet("color: #FFB020; font-size: 14px; font-weight: bold;");
    m_lblMaxVoltage->setStyleSheet("color: #FF5555; font-size: 14px; font-weight: bold;");
    m_lblAvgVoltage->setStyleSheet("color: #55AAFF; font-size: 14px; font-weight: bold;");

    statsLayout->addStretch();
    statsLayout->addWidget(m_lblMinVoltage);
    statsLayout->addSpacing(30);
    statsLayout->addWidget(m_lblMaxVoltage);
    statsLayout->addSpacing(30);
    statsLayout->addWidget(m_lblAvgVoltage);
    statsLayout->addStretch();
    meterLayout->addLayout(statsLayout);

    layout->addWidget(meterBox);

    // Continuity Tester Group
    auto contBox = new QGroupBox("Continuity Tester (Threshold: < 50 Ω)", this);
    auto contLayout = new QVBoxLayout(contBox);

    m_lblContinuityBanner = new QLabel("CONTINUITY: OPEN CIRCUIT", contBox);
    m_lblContinuityBanner->setAlignment(Qt::AlignCenter);
    m_lblContinuityBanner->setFixedHeight(50);
    m_lblContinuityBanner->setStyleSheet(
        "font-size: 18px; font-weight: bold; color: #E0E0E0; "
        "background-color: #2E2222; border: 2px solid #5A2A2A; border-radius: 6px;");
    contLayout->addWidget(m_lblContinuityBanner);

    layout->addWidget(contBox);
    layout->addStretch();
}

void MeterView::onPollTimerTick() {
    if (m_terminalClient) {
        m_terminalClient->readAdc();
        m_terminalClient->readContinuity();
    }
}

void MeterView::onAdcReadingReceived(const MeterReading& reading) {
    double v = reading.voltageV;
    m_lblVoltageDigital->setText(QString("%1 V").arg(v, 0, 'f', 3));
    m_barVoltage->setValue(static_cast<int>(v * 1000.0));
    m_lblRawAdc->setText(QString("Raw 12-Bit ADC: %1 / 4095  |  Resolution: 0.805 mV / LSB").arg(static_cast<int>(reading.rawAdc)));

    // Update stats
    if (v < m_minV) m_minV = v;
    if (v > m_maxV) m_maxV = v;
    m_sumV += v;
    m_countV++;

    m_lblMinVoltage->setText(QString("Min: %1 V").arg(m_minV, 0, 'f', 3));
    m_lblMaxVoltage->setText(QString("Max: %1 V").arg(m_maxV, 0, 'f', 3));
    m_lblAvgVoltage->setText(QString("Avg: %1 V").arg(m_sumV / m_countV, 0, 'f', 3));
}

void MeterView::onContinuityResultReceived(bool connected, const QString&) {
    if (connected) {
        m_lblContinuityBanner->setText("CONTINUITY: CLOSED (BEEP / SHORT)");
        m_lblContinuityBanner->setStyleSheet(
            "font-size: 18px; font-weight: bold; color: #FFFFFF; "
            "background-color: #007A5E; border: 2px solid #00FFCC; border-radius: 6px;");
    } else {
        m_lblContinuityBanner->setText("CONTINUITY: OPEN CIRCUIT");
        m_lblContinuityBanner->setStyleSheet(
            "font-size: 18px; font-weight: bold; color: #E0E0E0; "
            "background-color: #2E2222; border: 2px solid #5A2A2A; border-radius: 6px;");
    }
}

void MeterView::onResetStatsClicked() {
    m_minV = 999.0;
    m_maxV = -999.0;
    m_sumV = 0.0;
    m_countV = 0;
    m_lblMinVoltage->setText("Min: -- V");
    m_lblMaxVoltage->setText("Max: -- V");
    m_lblAvgVoltage->setText("Avg: -- V");
}

} // namespace UdooKey
