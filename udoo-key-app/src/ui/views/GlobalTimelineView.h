#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QLabel>
#include <QTabWidget>
#include "TransactionModel.h"
#include "HexViewWidget.h"

namespace UdooKey {

class GlobalTimelineView : public QWidget {
    Q_OBJECT

public:
    explicit GlobalTimelineView(QWidget* parent = nullptr);
    ~GlobalTimelineView() override = default;

public slots:
    void refreshTimeline();
    void clearTimeline();
    void exportTimeline();

private slots:
    void onTransactionLogged(const TransactionResult& tx);
    void onTableRowSelected(int row, int column);
    void onFilterChanged();

private:
    void setupUi();
    void addTransactionRow(const TransactionResult& tx);

    QComboBox* m_comboSubsystem;
    QComboBox* m_comboStatus;
    QLineEdit* m_searchEdit;
    QPushButton* m_btnClear;
    QPushButton* m_btnExport;
    QLabel* m_lblCount;

    QTableWidget* m_table;
    QTabWidget* m_payloadTabs;
    HexViewWidget* m_hexTx;
    HexViewWidget* m_hexRx;
    QLabel* m_lblDetails;

    QList<TransactionResult> m_displayedTransactions;
};

} // namespace UdooKey
