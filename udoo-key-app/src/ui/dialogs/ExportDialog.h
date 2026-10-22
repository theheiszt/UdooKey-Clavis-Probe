#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include "CaptureBuffer.h"

namespace UdooKey {

class ExportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ExportDialog(const CaptureBuffer& buffer, QWidget* parent = nullptr);
    ~ExportDialog() override;

private slots:
    void onBrowseClicked();
    void onExportClicked();
    void onCancelOrCloseClicked();
    void onTaskProgress(uint64_t taskId, int percent, const QString& message);
    void onTaskFinished(uint64_t taskId, bool success, const QString& message);

private:
    void setupUi();

    CaptureBuffer m_buffer;

    QComboBox* m_comboFormat;
    QLineEdit* m_editFilePath;
    QPushButton* m_btnBrowse;
    QPushButton* m_btnExport;
    QPushButton* m_btnCancel;
    QLabel* m_lblInfo;
    QProgressBar* m_progressBar;

    uint64_t m_activeTaskId = 0;
    QString m_exportPath;
};

} // namespace UdooKey
