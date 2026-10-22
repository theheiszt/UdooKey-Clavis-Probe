#include "ConnectionDialog.h"
#include "SettingsManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>

namespace UdooKey {

ConnectionDialog::ConnectionDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Connect to UDOO KEY Instrument");
    setModal(true);
    resize(480, 260);
    setupUi();
    refreshPorts();
}

void ConnectionDialog::setupUi() {
    auto layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto group = new QGroupBox("Serial Port Configuration", this);
    auto grid = new QGridLayout(group);

    grid->addWidget(new QLabel("Port:"), 0, 0);
    m_comboPorts = new QComboBox(this);
    connect(m_comboPorts, &QComboBox::currentIndexChanged, this, [this](int) {
        auto ports = DeviceManager::instance().availablePorts();
        int idx = m_comboPorts->currentIndex();
        if (idx >= 0 && idx < ports.size()) {
            const auto& selected = ports[idx];
            if (!selected.isWritable) {
                m_btnConnect->setEnabled(false);
                m_lblStatus->setText("DEVICE FOUND BUT ACCESS DENIED: Add user to dialout group:\nsudo usermod -a -G dialout $USER");
                m_lblStatus->setStyleSheet("color: #FF9900;");
            } else {
                m_btnConnect->setEnabled(true);
                m_lblStatus->setText(QString("Selected: %1 (%2)").arg(selected.portName, selected.description));
                m_lblStatus->setStyleSheet("color: #00FFCC;");
            }
        }
    });
    grid->addWidget(m_comboPorts, 0, 1);

    m_btnRefresh = new QPushButton("🔄 Refresh", this);
    connect(m_btnRefresh, &QPushButton::clicked, this, &ConnectionDialog::refreshPorts);
    grid->addWidget(m_btnRefresh, 0, 2);

    grid->addWidget(new QLabel("Baud Rate:"), 1, 0);
    m_comboBaudRate = new QComboBox(this);
    const QList<int> bauds = {115200, 230400, 460800, 921600, 1000000, 2000000};
    for (int b : bauds) {
        m_comboBaudRate->addItem(QString("%1 bps").arg(b), b);
    }
    m_comboBaudRate->setCurrentIndex(0); // 115200 default (standard across all UDOO KEY tools)
    grid->addWidget(m_comboBaudRate, 1, 1, 1, 2);

    layout->addWidget(group);

    m_lblStatus = new QLabel(this);
    m_lblStatus->setStyleSheet("color: #8E8E9F;");
    layout->addWidget(m_lblStatus);

    layout->addStretch();

    // Buttons
    auto btnLayout = new QHBoxLayout();
    m_btnMock = new QPushButton("🧪 Offline Simulation Mode", this);
    m_btnMock->setStyleSheet("background-color: #38304E; border-color: #7B5EA7;");
    connect(m_btnMock, &QPushButton::clicked, this, &ConnectionDialog::onMockClicked);
    btnLayout->addWidget(m_btnMock);

    btnLayout->addStretch();

    m_btnCancel = new QPushButton("Cancel", this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_btnCancel);

    m_btnConnect = new QPushButton("Connect", this);
    m_btnConnect->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    connect(m_btnConnect, &QPushButton::clicked, this, &ConnectionDialog::onConnectClicked);
    btnLayout->addWidget(m_btnConnect);

    layout->addLayout(btnLayout);
}

void ConnectionDialog::refreshPorts() {
    m_comboPorts->clear();
    auto ports = DeviceManager::instance().availablePorts();

    int defaultIndex = -1;
    QString lastPort = SettingsManager::instance().lastPort();

    bool hasPermissionIssue = false;
    for (int i = 0; i < ports.size(); ++i) {
        const auto& p = ports[i];
        QString label = QString("%1 (%2)").arg(p.portName, p.description.isEmpty() ? "Serial Port" : p.description);
        
        if (!p.isWritable) {
            label = "🔒 " + label + " [PERMISSION DENIED - dialout required]";
            hasPermissionIssue = true;
        } else if (p.isBusy) {
            label = "⏳ " + label + " [PORT BUSY]";
        } else if (p.isUdooKey) {
            label = "★ " + label + " [UDOO KEY / RP2040]";
            if (defaultIndex == -1) defaultIndex = i;
        }

        m_comboPorts->addItem(label, p.portName);

        if (p.portName == lastPort && p.isWritable) {
            defaultIndex = i;
        }
    }

    if (m_comboPorts->count() == 0) {
        m_comboPorts->addItem("No serial ports detected", "");
        m_btnConnect->setEnabled(false);
        m_lblStatus->setText("DEVICE NOT FOUND: Connect UDOO KEY or launch Simulation Mode.");
        m_lblStatus->setStyleSheet("color: #FF5555;");
    } else {
        if (defaultIndex != -1) {
            m_comboPorts->setCurrentIndex(defaultIndex);
        }
        
        const auto& selected = ports[m_comboPorts->currentIndex()];
        if (!selected.isWritable) {
            m_btnConnect->setEnabled(false);
            m_lblStatus->setText("DEVICE FOUND BUT ACCESS DENIED: Current user lacks permission (check dialout group).");
            m_lblStatus->setStyleSheet("color: #FF9900;");
        } else {
            m_btnConnect->setEnabled(true);
            m_lblStatus->setText(QString("Found %1 serial port(s). Ready to connect.").arg(ports.size()));
            m_lblStatus->setStyleSheet("color: #00FFCC;");
        }
    }
}

void ConnectionDialog::onConnectClicked() {
    m_useMock = false;
    accept();
}

void ConnectionDialog::onMockClicked() {
    m_useMock = true;
    accept();
}

QString ConnectionDialog::selectedPort() const {
    return m_comboPorts->currentData().toString();
}

int ConnectionDialog::selectedBaudRate() const {
    return m_comboBaudRate->currentData().toInt();
}

bool ConnectionDialog::isMockSelected() const {
    return m_useMock;
}

} // namespace UdooKey
