#include "TriggerConfigDialog.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>

namespace UdooKey {

TriggerConfigDialog::TriggerConfigDialog(const TriggerExpression& currentExpr, QWidget* parent)
    : QDialog(parent)
    , m_expr(currentExpr) {
    setWindowTitle("Visual Trigger Expression Builder");
    setModal(true);
    resize(520, 360);
    setupUi();
    updateSummary();
}

void TriggerConfigDialog::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // 1. Channel Level & Edge Matrix
    auto matrixGroup = new QGroupBox("Per-Channel Trigger Conditions (CH0 .. CH7)", this);
    auto matrixLayout = new QGridLayout(matrixGroup);

    QStringList conditionLabels = {"X (Don't Care)", "0 (Low)", "1 (High)", "↑ (Rising Edge)", "↓ (Falling Edge)", "↕ (Any Edge)"};

    for (int ch = 0; ch < 8; ++ch) {
        auto lbl = new QLabel(QString("CH%1:").arg(ch), matrixGroup);
        lbl->setStyleSheet(QString("color: %1;").arg(Theme::channelColor(ch).name()));
        lbl->setFont(QFont("Consolas", 10, QFont::Bold));

        auto combo = new QComboBox(matrixGroup);
        for (int i = 0; i < conditionLabels.size(); ++i) {
            combo->addItem(conditionLabels[i], i);
        }
        combo->setCurrentIndex(static_cast<int>(m_expr.condition(static_cast<uint8_t>(ch))));
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TriggerConfigDialog::onConditionChanged);

        m_channelCombos[ch] = combo;

        int row = ch / 2;
        int col = (ch % 2) * 2;
        matrixLayout->addWidget(lbl, row, col);
        matrixLayout->addWidget(combo, row, col + 1);
    }
    mainLayout->addWidget(matrixGroup);

    // 2. Parallel Bus Pattern
    auto busGroup = new QGroupBox("Parallel Bus Trigger Pattern", this);
    auto busLayout = new QHBoxLayout(busGroup);

    m_chkBusEnable = new QCheckBox("Enable Bus Match", busGroup);
    m_chkBusEnable->setChecked(m_expr.isBusTriggerEnabled());
    connect(m_chkBusEnable, &QCheckBox::toggled, this, &TriggerConfigDialog::onBusToggled);
    busLayout->addWidget(m_chkBusEnable);

    busLayout->addWidget(new QLabel("Value (Hex):", busGroup));
    m_editBusValue = new QLineEdit(QString("%1").arg(m_expr.busValue(), 2, 16, QChar('0')).toUpper(), busGroup);
    m_editBusValue->setMaxLength(2);
    m_editBusValue->setFixedWidth(50);
    busLayout->addWidget(m_editBusValue);

    busLayout->addWidget(new QLabel("Mask (Hex):", busGroup));
    m_editBusMask = new QLineEdit(QString("%1").arg(m_expr.busMask(), 2, 16, QChar('0')).toUpper(), busGroup);
    m_editBusMask->setMaxLength(2);
    m_editBusMask->setFixedWidth(50);
    busLayout->addWidget(m_editBusMask);
    busLayout->addStretch();

    mainLayout->addWidget(busGroup);

    // 3. String representation
    auto patternLayout = new QHBoxLayout();
    patternLayout->addWidget(new QLabel("Pattern String (CH7..CH0):", this));
    m_editPatternString = new QLineEdit(m_expr.toPatternString(), this);
    m_editPatternString->setFont(QFont("Consolas", 10, QFont::Bold));
    connect(m_editPatternString, &QLineEdit::textEdited, this, &TriggerConfigDialog::onPatternEdited);
    patternLayout->addWidget(m_editPatternString);
    mainLayout->addLayout(patternLayout);

    // 4. Summary badge
    m_lblSummary = new QLabel(this);
    m_lblSummary->setStyleSheet("color: #00D2A0; font-family: Consolas; font-weight: bold; padding: 4px;");
    mainLayout->addWidget(m_lblSummary);

    mainLayout->addStretch();

    // 5. Buttons
    auto btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_btnCancel = new QPushButton("Cancel", this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_btnCancel);

    m_btnOk = new QPushButton("Apply Trigger", this);
    m_btnOk->setStyleSheet("font-weight: bold; background-color: #007A5E; color: white;");
    connect(m_btnOk, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_btnOk);

    mainLayout->addLayout(btnLayout);
}

void TriggerConfigDialog::onConditionChanged() {
    for (int ch = 0; ch < 8; ++ch) {
        int idx = m_channelCombos[ch]->currentIndex();
        m_expr.setCondition(static_cast<uint8_t>(ch), static_cast<ChannelCondition>(idx));
    }
    m_editPatternString->setText(m_expr.toPatternString());
    updateSummary();
}

void TriggerConfigDialog::onBusToggled(bool checked) {
    m_expr.setBusTriggerEnabled(checked);
    bool okV = false, okM = false;
    uint8_t v = static_cast<uint8_t>(m_editBusValue->text().toUInt(&okV, 16));
    uint8_t m = static_cast<uint8_t>(m_editBusMask->text().toUInt(&okM, 16));
    m_expr.setBusPattern(okV ? v : 0, okM ? m : 0xFF);
    updateSummary();
}

void TriggerConfigDialog::onPatternEdited(const QString& text) {
    if (m_expr.fromPatternString(text)) {
        for (int ch = 0; ch < 8; ++ch) {
            m_channelCombos[ch]->blockSignals(true);
            m_channelCombos[ch]->setCurrentIndex(static_cast<int>(m_expr.condition(static_cast<uint8_t>(ch))));
            m_channelCombos[ch]->blockSignals(false);
        }
        updateSummary();
    }
}

void TriggerConfigDialog::updateSummary() {
    m_lblSummary->setText("Expression: " + m_expr.summary());
}

} // namespace UdooKey
