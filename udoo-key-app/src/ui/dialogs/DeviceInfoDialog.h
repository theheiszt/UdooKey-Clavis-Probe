#pragma once

#include <QDialog>
#include <QLabel>
#include <QTreeWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include "DeviceCapabilities.h"

namespace UdooKey {

class DeviceInfoDialog : public QDialog {
    Q_OBJECT

public:
    explicit DeviceInfoDialog(QWidget* parent = nullptr);
    ~DeviceInfoDialog() override = default;

private:
    void setupUi();
    void populateInfo();

    QTreeWidget* m_treeWidget;
    QLabel* m_lblHeader;
    QPushButton* m_btnClose;
};

} // namespace UdooKey
