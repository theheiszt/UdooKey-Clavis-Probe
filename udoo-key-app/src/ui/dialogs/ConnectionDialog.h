#pragma once

#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include "DeviceManager.h"

namespace UdooKey {

class ConnectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConnectionDialog(QWidget* parent = nullptr);
    ~ConnectionDialog() override = default;

    QString selectedPort() const;
    int selectedBaudRate() const;
    bool isMockSelected() const;

private slots:
    void refreshPorts();
    void onConnectClicked();
    void onMockClicked();

private:
    void setupUi();

    QComboBox* m_comboPorts;
    QComboBox* m_comboBaudRate;
    QPushButton* m_btnRefresh;
    QPushButton* m_btnConnect;
    QPushButton* m_btnMock;
    QPushButton* m_btnCancel;
    QLabel* m_lblStatus;

    bool m_useMock = false;
};

} // namespace UdooKey
