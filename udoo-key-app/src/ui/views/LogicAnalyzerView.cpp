#include "LogicAnalyzerView.h"
#include "Theme.h"
#include "Logger.h"
#include "TriggerConfigDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QHeaderView>

namespace UdooKey {

LogicAnalyzerView::LogicAnalyzerView(BinaryLogicClient* client, QWidget* parent)
    : QWidget(parent)
    , m_client(client) {
    setupUi();
    setClient(m_client);
}

void LogicAnalyzerView::setClient(BinaryLogicClient* client) {
    m_client = client;
    if (m_client) {
        connect(m_client, &BinaryLogicClient::captureComplete, this, &LogicAnalyzerView::onCaptureComplete);
    }
}

void LogicAnalyzerView::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 1. Top Control Toolbar
    auto toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);

    m_btnRun = new QPushButton("▶ Run (Continuous)", this);
    m_btnRun->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    m_btnSingle = new QPushButton("⏵ Single Capture", this);
    m_btnStop = new QPushButton("⏹ Stop", this);

    connect(m_btnRun, &QPushButton::clicked, this, &LogicAnalyzerView::onStartClicked);
    connect(m_btnSingle, &QPushButton::clicked, this, &LogicAnalyzerView::onSingleClicked);
    connect(m_btnStop, &QPushButton::clicked, this, &LogicAnalyzerView::onStopClicked);

    toolbarLayout->addWidget(m_btnRun);
    toolbarLayout->addWidget(m_btnSingle);
    toolbarLayout->addWidget(m_btnStop);

    // Sample Rate
    toolbarLayout->addWidget(new QLabel("Rate:", this));
    m_comboSampleRate = new QComboBox(this);
    m_comboSampleRate->addItem("100 kHz", 100000);
    m_comboSampleRate->addItem("500 kHz", 500000);
    m_comboSampleRate->addItem("1 MHz", 1000000);
    m_comboSampleRate->addItem("5 MHz", 5000000);
    m_comboSampleRate->addItem("10 MHz", 10000000);
    m_comboSampleRate->addItem("25 MHz", 25000000);
    m_comboSampleRate->addItem("50 MHz", 50000000);
    m_comboSampleRate->addItem("100 MHz", 100000000);
    m_comboSampleRate->setCurrentIndex(4); // 10 MHz
    toolbarLayout->addWidget(m_comboSampleRate);

    // Sample Depth
    toolbarLayout->addWidget(new QLabel("Depth:", this));
    m_comboSampleDepth = new QComboBox(this);
    m_comboSampleDepth->addItem("1,024", 1024);
    m_comboSampleDepth->addItem("4,096", 4096);
    m_comboSampleDepth->addItem("16,384", 16384);
    m_comboSampleDepth->addItem("32,768", 32768);
    m_comboSampleDepth->addItem("65,536 (64 KiB)", 65536);
    m_comboSampleDepth->setCurrentIndex(4);
    toolbarLayout->addWidget(m_comboSampleDepth);

    // Trigger Channel
    toolbarLayout->addWidget(new QLabel("Trigger:", this));
    m_comboTriggerChannel = new QComboBox(this);
    for (int i = 0; i < 8; ++i) {
        m_comboTriggerChannel->addItem(QString("CH%1").arg(i), i);
    }
    toolbarLayout->addWidget(m_comboTriggerChannel);

    // Trigger Mode
    m_comboTriggerMode = new QComboBox(this);
    m_comboTriggerMode->addItem("Immediate", static_cast<int>(TriggerMode::Immediate));
    m_comboTriggerMode->addItem("Rising Edge", static_cast<int>(TriggerMode::EdgeRising));
    m_comboTriggerMode->addItem("Falling Edge", static_cast<int>(TriggerMode::EdgeFalling));
    toolbarLayout->addWidget(m_comboTriggerMode);

    m_btnTriggerConfig = new QPushButton("⚙ Trigger...", this);
    connect(m_btnTriggerConfig, &QPushButton::clicked, this, &LogicAnalyzerView::onTriggerConfigClicked);
    toolbarLayout->addWidget(m_btnTriggerConfig);

    toolbarLayout->addStretch();

    // Zoom Buttons
    auto btnZoomIn = new QPushButton("🔍+", this);
    auto btnZoomOut = new QPushButton("🔍-", this);
    auto btnZoomFit = new QPushButton("⛶ Fit", this);
    toolbarLayout->addWidget(btnZoomIn);
    toolbarLayout->addWidget(btnZoomOut);
    toolbarLayout->addWidget(btnZoomFit);

    auto btnToggleCD = new QPushButton("Markers C/D (On)", this);
    btnToggleCD->setCheckable(true);
    btnToggleCD->setChecked(true);
    toolbarLayout->addWidget(btnToggleCD);

    // Export Button
    m_btnExport = new QPushButton("💾 Export...", this);
    connect(m_btnExport, &QPushButton::clicked, this, &LogicAnalyzerView::exportRequested);
    toolbarLayout->addWidget(m_btnExport);

    mainLayout->addLayout(toolbarLayout);

    // 2. Central Splitter: Waveform on top, Inspectors below
    auto splitter = new QSplitter(Qt::Vertical, this);

    // Waveform Widget
    m_waveformWidget = new WaveformWidget(this);
    connect(btnZoomIn, &QPushButton::clicked, m_waveformWidget, &WaveformWidget::zoomIn);
    connect(btnZoomOut, &QPushButton::clicked, m_waveformWidget, &WaveformWidget::zoomOut);
    connect(btnZoomFit, &QPushButton::clicked, m_waveformWidget, &WaveformWidget::zoomToFit);
    connect(m_waveformWidget, &WaveformWidget::cursorsChanged, this, &LogicAnalyzerView::onCursorChanged);
    connect(m_waveformWidget, &WaveformWidget::cursorsCDChanged, [this](double, double, double, double) {
        onCursorChanged(m_waveformWidget->cursorAPosSec(), m_waveformWidget->cursorBPosSec(),
                        m_waveformWidget->cursorDeltaABSec(),
                        m_waveformWidget->cursorDeltaABSec() > 0 ? 1.0 / m_waveformWidget->cursorDeltaABSec() : 0.0);
    });
    connect(btnToggleCD, &QPushButton::toggled, [this, btnToggleCD](bool checked) {
        m_waveformWidget->setEnableMarkersCD(checked);
        btnToggleCD->setText(checked ? "Markers C/D (On)" : "Markers C/D (Off)");
        onCursorChanged(m_waveformWidget->cursorAPosSec(), m_waveformWidget->cursorBPosSec(),
                        m_waveformWidget->cursorDeltaABSec(),
                        m_waveformWidget->cursorDeltaABSec() > 0 ? 1.0 / m_waveformWidget->cursorDeltaABSec() : 0.0);
    });

    splitter->addWidget(m_waveformWidget);
    splitter->setStretchFactor(0, 3);

    // Bottom Panels in horizontal splitter
    auto bottomSplitter = new QSplitter(Qt::Horizontal, this);

    // Left Bottom: Channel Metrics & Mute/Solo
    auto metricsBox = new QGroupBox("Channel Statistics & Controls", this);
    auto metricsLayout = new QVBoxLayout(metricsBox);
    m_metricsTable = new QTableWidget(8, 5, this);
    m_metricsTable->setHorizontalHeaderLabels({"Channel", "Show", "Freq", "Duty", "Edges"});
    m_metricsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_metricsTable->verticalHeader()->setVisible(false);

    for (int ch = 0; ch < 8; ++ch) {
        auto itemCh = new QTableWidgetItem(QString("CH%1").arg(ch));
        itemCh->setForeground(Theme::channelColor(ch));
        m_metricsTable->setItem(ch, 0, itemCh);

        auto itemShow = new QTableWidgetItem("✔");
        itemShow->setTextAlignment(Qt::AlignCenter);
        m_metricsTable->setItem(ch, 1, itemShow);

        m_metricsTable->setItem(ch, 2, new QTableWidgetItem("--"));
        m_metricsTable->setItem(ch, 3, new QTableWidgetItem("--"));
        m_metricsTable->setItem(ch, 4, new QTableWidgetItem("--"));
    }

    connect(m_metricsTable, &QTableWidget::cellClicked, [this](int row, int col) {
        if (col == 1 && row >= 0 && row < 8) {
            auto item = m_metricsTable->item(row, col);
            bool isShown = (item->text() == "✔");
            item->setText(isShown ? "✖" : "✔");
            item->setForeground(isShown ? QColor(100, 100, 100) : QColor(0, 220, 255));
            m_waveformWidget->setChannelEnabled(static_cast<uint8_t>(row), !isShown);
        }
    });

    metricsLayout->addWidget(m_metricsTable);
    bottomSplitter->addWidget(metricsBox);

    // Right Bottom: Protocol Decoder Inspector
    auto decoderBox = new QGroupBox("Protocol Decoder Inspector", this);
    auto decoderLayout = new QVBoxLayout(decoderBox);

    auto decTopLayout = new QHBoxLayout();
    m_comboDecoder = new QComboBox(this);
    m_comboDecoder->addItem("UART (CH2, 115200 8N1)", static_cast<int>(ProtocolType::UART));
    m_comboDecoder->addItem("I2C (SCL=CH6, SDA=CH7)", static_cast<int>(ProtocolType::I2C));
    m_comboDecoder->addItem("SPI (SCK=CH4, MOSI=CH5, CS=CH3)", static_cast<int>(ProtocolType::SPI));
    decTopLayout->addWidget(m_comboDecoder);

    m_btnRunDecoder = new QPushButton("Decode Stream", this);
    connect(m_btnRunDecoder, &QPushButton::clicked, this, &LogicAnalyzerView::onRunDecoderClicked);
    decTopLayout->addWidget(m_btnRunDecoder);
    decoderLayout->addLayout(decTopLayout);

    m_decoderTree = new QTreeWidget(this);
    m_decoderTree->setHeaderLabels({"Time (s)", "Field / Data", "ASCII", "Status"});
    m_decoderTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    decoderLayout->addWidget(m_decoderTree);

    bottomSplitter->addWidget(decoderBox);
    bottomSplitter->setStretchFactor(1, 2);

    splitter->addWidget(bottomSplitter);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);

    // Cursor measurement bar at bottom
    m_lblMeasurements = new QLabel("Cursor A: 0.000 ms | Cursor B: 0.000 ms | ΔT: 0.000 ms | Freq: 0.000 Hz", this);
    m_lblMeasurements->setStyleSheet("color: #00D2A0; font-family: Consolas; font-weight: bold; padding: 4px;");
    mainLayout->addWidget(m_lblMeasurements);

    connect(&m_processor, &CaptureProcessor::processingComplete, this, &LogicAnalyzerView::onProcessingComplete);
    connect(&DecoderRegistry::instance(), &DecoderRegistry::decoderFinished, this, &LogicAnalyzerView::onDecoderFinished);
}

void LogicAnalyzerView::onStartClicked() {
    if (!m_client) return;
    AcquisitionConfig cfg;
    cfg.sampleRateHz = m_comboSampleRate->currentData().toUInt();
    cfg.sampleDepth = m_comboSampleDepth->currentData().toUInt();
    cfg.triggerChannel = static_cast<uint8_t>(m_comboTriggerChannel->currentData().toUInt());
    cfg.triggerMode = static_cast<TriggerMode>(m_comboTriggerMode->currentData().toUInt());
    m_client->armCapture(cfg);
}

void LogicAnalyzerView::onSingleClicked() {
    onStartClicked();
}

void LogicAnalyzerView::onStopClicked() {
    if (m_client) {
        m_client->abortCapture();
    }
}

void LogicAnalyzerView::onCaptureComplete(const QByteArray& samples, uint32_t sampleRateHz) {
    m_processor.processCapture(samples, sampleRateHz);
}

void LogicAnalyzerView::onProcessingComplete() {
    m_waveformWidget->setCaptureData(m_processor.sharedBuffer(), m_processor.sharedDecimator());
    updateMetricsTable();
    // Auto run current decoder
    onRunDecoderClicked();
}

void LogicAnalyzerView::updateMetricsTable() {
    for (uint8_t ch = 0; ch < 8; ++ch) {
        auto m = m_processor.metrics(ch);
        
        QString freqStr;
        if (m.estimatedFreqHz >= 1e6) freqStr = QString("%1 MHz").arg(m.estimatedFreqHz / 1e6, 0, 'f', 2);
        else if (m.estimatedFreqHz >= 1e3) freqStr = QString("%1 kHz").arg(m.estimatedFreqHz / 1e3, 0, 'f', 1);
        else freqStr = QString("%1 Hz").arg(m.estimatedFreqHz, 0, 'f', 0);

        m_metricsTable->item(ch, 2)->setText(m.estimatedFreqHz > 0 ? freqStr : "--");
        m_metricsTable->item(ch, 3)->setText(QString("%1%").arg(m.dutyCyclePercent, 0, 'f', 1));
        m_metricsTable->item(ch, 4)->setText(QString("%1 ↑ / %2 ↓").arg(m.risingEdges).arg(m.fallingEdges));
    }
}

void LogicAnalyzerView::onRunDecoderClicked() {
    if (m_processor.captureBuffer().isEmpty()) return;
    m_decoderTree->clear();
    DecoderRegistry::instance().runDecodersAsync(m_processor.sharedBuffer());
}

void LogicAnalyzerView::onDecoderFinished(ProtocolType type, const QVector<DecodedPacket>& packets) {
    auto currentType = static_cast<ProtocolType>(m_comboDecoder->currentData().toInt());
    if (type != currentType) return;

    m_decoderTree->clear();
    for (const auto& pkt : packets) {
        for (const auto& tok : pkt.tokens) {
            auto item = new QTreeWidgetItem(m_decoderTree);
            item->setText(0, QString::number(tok.startTimeSec, 'f', 6));
            item->setText(1, tok.text);
            item->setText(2, pkt.title);
            item->setText(3, tok.isWarning ? "WARN / NACK" : "OK");
            if (tok.isWarning) {
                item->setForeground(3, QColor(255, 80, 80));
            } else {
                item->setForeground(3, QColor(80, 255, 120));
            }
        }
    }

    m_waveformWidget->setDecodedPackets(packets);
}

void LogicAnalyzerView::onCursorChanged(double cursorASec, double cursorBSec, double deltaSec, double freqHz) {
    QString freqStr;
    if (freqHz >= 1e6) freqStr = QString("%1 MHz").arg(freqHz / 1e6, 0, 'f', 3);
    else if (freqHz >= 1e3) freqStr = QString("%1 kHz").arg(freqHz / 1e3, 0, 'f', 3);
    else freqStr = QString("%1 Hz").arg(freqHz, 0, 'f', 2);

    QString text = QString("A: %1 ms | B: %2 ms | ΔAB: %3 ms (%4) ")
        .arg(cursorASec * 1e3, 0, 'f', 3)
        .arg(cursorBSec * 1e3, 0, 'f', 3)
        .arg(deltaSec * 1e3, 0, 'f', 3)
        .arg(freqStr);

    if (m_waveformWidget && m_waveformWidget->markersCDEnabled()) {
        double dCD = m_waveformWidget->cursorDeltaCDSec();
        double fCD = dCD > 0.0 ? (1.0 / dCD) : 0.0;
        QString fCDStr = (fCD >= 1e6) ? QString("%1 MHz").arg(fCD / 1e6, 0, 'f', 2) :
                         (fCD >= 1e3) ? QString("%1 kHz").arg(fCD / 1e3, 0, 'f', 2) :
                                        QString("%1 Hz").arg(fCD, 0, 'f', 1);
        text += QString("| C: %1 ms | D: %2 ms | ΔCD: %3 ms (%4)")
            .arg(m_waveformWidget->cursorCPosSec() * 1e3, 0, 'f', 3)
            .arg(m_waveformWidget->cursorDPosSec() * 1e3, 0, 'f', 3)
            .arg(dCD * 1e3, 0, 'f', 3)
            .arg(fCDStr);
    }

    m_lblMeasurements->setText(text);
}

void LogicAnalyzerView::onTriggerConfigClicked() {
    TriggerConfigDialog dlg(m_triggerExpr, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_triggerExpr = dlg.expression();
        LOG_INFO("Configured visual trigger expression: " + m_triggerExpr.summary());
    }
}

} // namespace UdooKey
