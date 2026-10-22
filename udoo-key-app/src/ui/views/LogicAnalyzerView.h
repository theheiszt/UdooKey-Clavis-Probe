#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QTreeWidget>
#include "WaveformWidget.h"
#include "BinaryLogicClient.h"
#include "CaptureProcessor.h"
#include "DecoderRegistry.h"
#include "TriggerExpression.h"

namespace UdooKey {

class LogicAnalyzerView : public QWidget {
    Q_OBJECT

public:
    explicit LogicAnalyzerView(BinaryLogicClient* client, QWidget* parent = nullptr);
    ~LogicAnalyzerView() override = default;

    void setClient(BinaryLogicClient* client);
    const CaptureBuffer& currentCapture() const { return m_processor.captureBuffer(); }
    const CaptureProcessor& processor() const { return m_processor; }
    const WaveformWidget* waveformWidget() const { return m_waveformWidget; }
    const TriggerExpression& triggerExpression() const { return m_triggerExpr; }

signals:
    void exportRequested();

private slots:
    void onStartClicked();
    void onSingleClicked();
    void onStopClicked();
    void onCaptureComplete(const QByteArray& samples, uint32_t sampleRateHz);
    void onProcessingComplete();
    void onRunDecoderClicked();
    void onDecoderFinished(ProtocolType type, const QVector<DecodedPacket>& packets);
    void onCursorChanged(double cursorASec, double cursorBSec, double deltaSec, double freqHz);
    void onTriggerConfigClicked();

private:
    void setupUi();
    void updateMetricsTable();

    BinaryLogicClient* m_client;
    CaptureProcessor m_processor;
    TriggerExpression m_triggerExpr;

    // Controls
    QPushButton* m_btnRun;
    QPushButton* m_btnSingle;
    QPushButton* m_btnStop;
    QComboBox* m_comboSampleRate;
    QComboBox* m_comboSampleDepth;
    QComboBox* m_comboTriggerChannel;
    QComboBox* m_comboTriggerMode;
    QPushButton* m_btnTriggerConfig;
    QPushButton* m_btnExport;

    // Waveform
    WaveformWidget* m_waveformWidget;

    // Bottom panels
    QTableWidget* m_metricsTable;
    QComboBox* m_comboDecoder;
    QPushButton* m_btnRunDecoder;
    QTreeWidget* m_decoderTree;
    QLabel* m_lblMeasurements;
};

} // namespace UdooKey
