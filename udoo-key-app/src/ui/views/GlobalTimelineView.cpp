#include "GlobalTimelineView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>

namespace UdooKey {

GlobalTimelineView::GlobalTimelineView(QWidget* parent)
    : QWidget(parent) {
    setupUi();

    connect(&TransactionLogger::instance(), &TransactionLogger::transactionLogged,
            this, &GlobalTimelineView::onTransactionLogged);
    connect(&TransactionLogger::instance(), &TransactionLogger::cleared,
            this, &GlobalTimelineView::refreshTimeline);

    refreshTimeline();
}

void GlobalTimelineView::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    // Filter bar
    auto barLayout = new QHBoxLayout();

    barLayout->addWidget(new QLabel("Subsystem:", this));
    m_comboSubsystem = new QComboBox(this);
    m_comboSubsystem->addItems({"All", "Logic", "Bus Tools", "JTAG", "SWD", "USB", "Terminal"});
    connect(m_comboSubsystem, &QComboBox::currentTextChanged, this, &GlobalTimelineView::onFilterChanged);
    barLayout->addWidget(m_comboSubsystem);

    barLayout->addWidget(new QLabel("Status:", this));
    m_comboStatus = new QComboBox(this);
    m_comboStatus->addItems({"All", "Success", "Failed"});
    connect(m_comboStatus, &QComboBox::currentTextChanged, this, &GlobalTimelineView::onFilterChanged);
    barLayout->addWidget(m_comboStatus);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("🔍 Filter summary or error...");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &GlobalTimelineView::onFilterChanged);
    barLayout->addWidget(m_searchEdit);

    barLayout->addStretch();

    m_lblCount = new QLabel("0 events", this);
    m_lblCount->setStyleSheet("color: #8E8E9F;");
    barLayout->addWidget(m_lblCount);

    m_btnClear = new QPushButton("Clear", this);
    connect(m_btnClear, &QPushButton::clicked, this, &GlobalTimelineView::clearTimeline);
    barLayout->addWidget(m_btnClear);

    m_btnExport = new QPushButton("Export CSV...", this);
    connect(m_btnExport, &QPushButton::clicked, this, &GlobalTimelineView::exportTimeline);
    barLayout->addWidget(m_btnExport);

    mainLayout->addLayout(barLayout);

    // Splitter: Table above, Hex / Details below
    auto splitter = new QSplitter(Qt::Vertical, this);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({"ID", "Timestamp", "Subsystem", "Duration (ms)", "Status", "Summary"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_table, &QTableWidget::cellClicked, this, &GlobalTimelineView::onTableRowSelected);
    splitter->addWidget(m_table);

    // Bottom pane: Tabs for Tx Hex, Rx Hex, Details
    m_payloadTabs = new QTabWidget(this);
    m_hexTx = new HexViewWidget(this);
    m_hexRx = new HexViewWidget(this);
    m_lblDetails = new QLabel("Select a transaction to inspect payload and metadata.", this);
    m_lblDetails->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_lblDetails->setWordWrap(true);
    m_lblDetails->setStyleSheet("padding: 8px; color: #CCCCCC; font-family: 'Consolas', monospace;");

    m_payloadTabs->addTab(m_hexTx, "TX Payload");
    m_payloadTabs->addTab(m_hexRx, "RX Payload");
    m_payloadTabs->addTab(m_lblDetails, "Transaction Details");
    splitter->addWidget(m_payloadTabs);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    mainLayout->addWidget(splitter);
}

void GlobalTimelineView::refreshTimeline() {
    m_table->setRowCount(0);
    m_displayedTransactions.clear();

    QString subFilter = m_comboSubsystem->currentText();
    if (subFilter == "All") subFilter = "";

    auto allTx = TransactionLogger::instance().getTransactions(subFilter, 2000);
    QString statusFilter = m_comboStatus->currentText();
    QString query = m_searchEdit->text().trimmed().toLower();

    for (const auto& tx : allTx) {
        if (statusFilter == "Success" && !tx.success) continue;
        if (statusFilter == "Failed" && tx.success) continue;
        if (!query.isEmpty() && !tx.summary.toLower().contains(query) && !tx.errorMessage.toLower().contains(query)) {
            continue;
        }

        m_displayedTransactions.append(tx);
        addTransactionRow(tx);
    }

    m_lblCount->setText(QString("%1 events").arg(m_displayedTransactions.size()));
}

void GlobalTimelineView::addTransactionRow(const TransactionResult& tx) {
    int row = m_table->rowCount();
    m_table->insertRow(row);

    m_table->setItem(row, 0, new QTableWidgetItem(QString::number(tx.id)));
    m_table->setItem(row, 1, new QTableWidgetItem(tx.timestamp.toString("HH:mm:ss.zzz")));
    m_table->setItem(row, 2, new QTableWidgetItem(tx.subsystem));
    m_table->setItem(row, 3, new QTableWidgetItem(QString::number(tx.durationMs(), 'f', 2)));

    auto itemStatus = new QTableWidgetItem(tx.success ? "✔ OK" : "✖ FAIL");
    itemStatus->setForeground(tx.success ? QColor("#00FFCC") : QColor("#FF5555"));
    m_table->setItem(row, 4, itemStatus);

    m_table->setItem(row, 5, new QTableWidgetItem(tx.summary));
}

void GlobalTimelineView::onTransactionLogged(const TransactionResult& tx) {
    // If matching filters, append
    QString subFilter = m_comboSubsystem->currentText();
    if (subFilter != "All" && tx.subsystem.compare(subFilter, Qt::CaseInsensitive) != 0) return;

    QString statusFilter = m_comboStatus->currentText();
    if (statusFilter == "Success" && !tx.success) return;
    if (statusFilter == "Failed" && tx.success) return;

    QString query = m_searchEdit->text().trimmed().toLower();
    if (!query.isEmpty() && !tx.summary.toLower().contains(query) && !tx.errorMessage.toLower().contains(query)) return;

    m_displayedTransactions.append(tx);
    addTransactionRow(tx);
    m_lblCount->setText(QString("%1 events").arg(m_displayedTransactions.size()));
}

void GlobalTimelineView::onTableRowSelected(int row, int /*column*/) {
    if (row < 0 || row >= m_displayedTransactions.size()) return;
    const auto& tx = m_displayedTransactions.at(row);

    m_hexTx->setData(tx.txData);
    m_hexRx->setData(tx.rxData);

    QString details = QString(
        "ID: %1\n"
        "Subsystem: %2\n"
        "Timestamp: %3\n"
        "Duration: %4 ms (%5 ns)\n"
        "Status: %6\n"
        "Summary: %7\n"
    ).arg(tx.id)
     .arg(tx.subsystem)
     .arg(tx.timestamp.toString(Qt::ISODateWithMs))
     .arg(tx.durationMs(), 0, 'f', 3)
     .arg(tx.durationNs)
     .arg(tx.success ? "SUCCESS" : "FAILED")
     .arg(tx.summary);

    if (!tx.errorMessage.isEmpty()) {
        details += QString("Error Message: %1\n").arg(tx.errorMessage);
    }

    if (!tx.metadata.isEmpty()) {
        details += "\nMetadata:\n";
        for (auto it = tx.metadata.begin(); it != tx.metadata.end(); ++it) {
            details += QString("  • %1: %2\n").arg(it.key(), it.value().toString());
        }
    }

    m_lblDetails->setText(details);
}

void GlobalTimelineView::onFilterChanged() {
    refreshTimeline();
}

void GlobalTimelineView::clearTimeline() {
    TransactionLogger::instance().clear();
}

void GlobalTimelineView::exportTimeline() {
    if (m_displayedTransactions.isEmpty()) {
        QMessageBox::information(this, "Export Timeline", "No transaction events to export.");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, "Export Timeline to CSV", "timeline.csv", "CSV Files (*.csv)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Error", "Failed to open file for writing.");
        return;
    }

    QTextStream out(&file);
    out << "ID,Timestamp,Subsystem,DurationMs,Status,Summary,ErrorMessage,TxBytes,RxBytes\n";
    for (const auto& tx : m_displayedTransactions) {
        out << tx.id << ","
            << "\"" << tx.timestamp.toString(Qt::ISODateWithMs) << "\","
            << "\"" << tx.subsystem << "\","
            << tx.durationMs() << ","
            << (tx.success ? "SUCCESS" : "FAILED") << ","
            << "\"" << QString(tx.summary).replace("\"", "\"\"") << "\","
            << "\"" << QString(tx.errorMessage).replace("\"", "\"\"") << "\","
            << tx.txData.size() << ","
            << tx.rxData.size() << "\n";
    }

    QMessageBox::information(this, "Export Complete", QString("Successfully exported %1 transactions.").arg(m_displayedTransactions.size()));
}

} // namespace UdooKey
