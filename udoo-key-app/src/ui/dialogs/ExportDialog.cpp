#include "ExportDialog.h"
#include "VcdExporter.h"
#include "CsvExporter.h"
#include "BinaryExporter.h"
#include "TaskManager.h"
#include "ErrorManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>

namespace UdooKey {

ExportDialog::ExportDialog(const CaptureBuffer& buffer, QWidget* parent)
    : QDialog(parent)
    , m_buffer(buffer) {
    setWindowTitle("Export Waveform Capture");
    setModal(true);
    resize(540, 260);
    setupUi();

    connect(&TaskManager::instance(), &TaskManager::taskProgress, this, &ExportDialog::onTaskProgress);
    connect(&TaskManager::instance(), &TaskManager::taskFinished, this, &ExportDialog::onTaskFinished);
}

ExportDialog::~ExportDialog() {
    if (m_activeTaskId > 0) {
        TaskManager::instance().cancelTask(m_activeTaskId);
    }
}

void ExportDialog::setupUi() {
    auto layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto group = new QGroupBox("Export Parameters", this);
    auto grid = new QGridLayout(group);

    grid->addWidget(new QLabel("Format:"), 0, 0);
    m_comboFormat = new QComboBox(this);
    m_comboFormat->addItem("Value Change Dump (*.vcd) - GTKWave / PulseView", 0);
    m_comboFormat->addItem("Comma-Separated Values (*.csv) - Excel / MATLAB", 1);
    m_comboFormat->addItem("Raw Packed Binary (*.bin)", 2);
    grid->addWidget(m_comboFormat, 0, 1, 1, 2);

    grid->addWidget(new QLabel("Output File:"), 1, 0);
    m_editFilePath = new QLineEdit(this);
    m_editFilePath->setPlaceholderText("Select destination file path...");
    grid->addWidget(m_editFilePath, 1, 1);

    m_btnBrowse = new QPushButton("Browse...", this);
    connect(m_btnBrowse, &QPushButton::clicked, this, &ExportDialog::onBrowseClicked);
    grid->addWidget(m_btnBrowse, 1, 2);

    layout->addWidget(group);

    m_lblInfo = new QLabel(QString("Capture contains %1 samples (duration: %2 ms).")
                               .arg(m_buffer.sampleCount())
                               .arg(m_buffer.durationSec() * 1e3, 0, 'f', 2), this);
    m_lblInfo->setStyleSheet("color: #00D2A0;");
    layout->addWidget(m_lblInfo);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setVisible(false);
    layout->addWidget(m_progressBar);

    layout->addStretch();

    auto btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_btnCancel = new QPushButton("Cancel", this);
    connect(m_btnCancel, &QPushButton::clicked, this, &ExportDialog::onCancelOrCloseClicked);
    btnLayout->addWidget(m_btnCancel);

    m_btnExport = new QPushButton("Export Data", this);
    m_btnExport->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    connect(m_btnExport, &QPushButton::clicked, this, &ExportDialog::onExportClicked);
    btnLayout->addWidget(m_btnExport);

    layout->addLayout(btnLayout);
}

void ExportDialog::onBrowseClicked() {
    int fmt = m_comboFormat->currentIndex();
    QString filter;
    QString defaultExt;
    if (fmt == 0) {
        filter = "VCD Files (*.vcd);;All Files (*.*)";
        defaultExt = "vcd";
    } else if (fmt == 1) {
        filter = "CSV Files (*.csv);;All Files (*.*)";
        defaultExt = "csv";
    } else {
        filter = "Binary Files (*.bin);;All Files (*.*)";
        defaultExt = "bin";
    }

    QString path = QFileDialog::getSaveFileName(this, "Save Waveform Capture", QString("udoo_capture.%1").arg(defaultExt), filter);
    if (!path.isEmpty()) {
        m_editFilePath->setText(path);
    }
}

void ExportDialog::onExportClicked() {
    QString path = m_editFilePath->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "Export Error", "Please specify a destination file path.");
        return;
    }

    m_exportPath = path;
    int fmt = m_comboFormat->currentIndex();
    CaptureBuffer bufCopy = m_buffer; // Worker capture copy

    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_btnExport->setEnabled(false);
    m_btnBrowse->setEnabled(false);
    m_comboFormat->setEnabled(false);
    m_editFilePath->setEnabled(false);
    m_btnCancel->setText("Cancel Export");

    m_activeTaskId = TaskManager::instance().submitTask(
        "Export Waveform",
        QString("Exporting to %1").arg(QFileInfo(path).fileName()),
        [fmt, path, bufCopy](std::shared_ptr<CancellationToken> token, std::function<void(int, const QString&)> progress) {
            QString err;
            bool ok = false;
            if (fmt == 0) {
                VcdExporter exp;
                ok = exp.exportDataAsync(path, bufCopy, &err, token, progress);
            } else if (fmt == 1) {
                CsvExporter exp;
                ok = exp.exportDataAsync(path, bufCopy, &err, token, progress);
            } else {
                BinaryExporter exp;
                ok = exp.exportDataAsync(path, bufCopy, &err, token, progress);
            }

            if (!ok && !err.isEmpty()) {
                ErrorManager::instance().report(ErrorCategory::FileSystem, ErrorSeverity::Error, "Export failed: " + err);
            }
            return ok;
        }
    );
}

void ExportDialog::onCancelOrCloseClicked() {
    if (m_activeTaskId > 0) {
        TaskManager::instance().cancelTask(m_activeTaskId);
        m_activeTaskId = 0;
        m_progressBar->setVisible(false);
        m_btnExport->setEnabled(true);
        m_btnBrowse->setEnabled(true);
        m_comboFormat->setEnabled(true);
        m_editFilePath->setEnabled(true);
        m_btnCancel->setText("Cancel");
        m_lblInfo->setText("Export was cancelled.");
        m_lblInfo->setStyleSheet("color: #FFB020;");
    } else {
        reject();
    }
}

void ExportDialog::onTaskProgress(uint64_t taskId, int percent, const QString& message) {
    if (taskId == m_activeTaskId) {
        m_progressBar->setValue(percent);
        m_lblInfo->setText(message);
    }
}

void ExportDialog::onTaskFinished(uint64_t taskId, bool success, const QString& message) {
    if (taskId != m_activeTaskId) return;

    m_activeTaskId = 0;
    m_progressBar->setVisible(false);
    m_btnExport->setEnabled(true);
    m_btnBrowse->setEnabled(true);
    m_comboFormat->setEnabled(true);
    m_editFilePath->setEnabled(true);
    m_btnCancel->setText("Close");

    if (success) {
        QMessageBox::information(this, "Export Successful", QString("Waveform capture saved to:\n%1").arg(m_exportPath));
        accept();
    } else {
        QMessageBox::critical(this, "Export Failed", QString("Failed to export waveform:\n%1").arg(message));
        m_lblInfo->setText("Export failed: " + message);
        m_lblInfo->setStyleSheet("color: #FF5555;");
    }
}

} // namespace UdooKey
