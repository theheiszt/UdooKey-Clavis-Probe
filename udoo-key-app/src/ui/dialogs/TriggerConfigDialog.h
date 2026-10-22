#pragma once

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include "TriggerExpression.h"

namespace UdooKey {

class TriggerConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit TriggerConfigDialog(const TriggerExpression& currentExpr, QWidget* parent = nullptr);
    ~TriggerConfigDialog() override = default;

    TriggerExpression expression() const { return m_expr; }

private slots:
    void onConditionChanged();
    void onBusToggled(bool checked);
    void onPatternEdited(const QString& text);

private:
    void setupUi();
    void updateSummary();

    TriggerExpression m_expr;

    QComboBox* m_channelCombos[8];
    QCheckBox* m_chkBusEnable;
    QLineEdit* m_editBusValue;
    QLineEdit* m_editBusMask;
    QLineEdit* m_editPatternString;
    QLabel* m_lblSummary;

    QPushButton* m_btnOk;
    QPushButton* m_btnCancel;
};

} // namespace UdooKey
