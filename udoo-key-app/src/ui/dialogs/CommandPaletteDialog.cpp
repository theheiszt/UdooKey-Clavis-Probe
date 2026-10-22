#include "CommandPaletteDialog.h"
#include <QKeyEvent>

namespace UdooKey {

CommandPaletteDialog::CommandPaletteDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Command Palette");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose, false);
    resize(550, 360);

    setStyleSheet(
        "QDialog {"
        "  background-color: #1A1A22;"
        "  border: 2px solid #00FFCC;"
        "  border-radius: 8px;"
        "}"
        "QLineEdit {"
        "  background-color: #121218;"
        "  color: #FFFFFF;"
        "  border: 1px solid #333344;"
        "  border-radius: 4px;"
        "  padding: 8px 12px;"
        "  font-size: 14px;"
        "}"
        "QListWidget {"
        "  background-color: #121218;"
        "  color: #CCCCCC;"
        "  border: 1px solid #2B2B36;"
        "  border-radius: 4px;"
        "  outline: none;"
        "  font-size: 13px;"
        "}"
        "QListWidget::item {"
        "  padding: 8px 10px;"
        "  border-bottom: 1px solid #1E1E28;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: #007A5E;"
        "  color: #FFFFFF;"
        "}"
    );

    setupUi();
    populateList("");
}

void CommandPaletteDialog::setupUi() {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Type a command or jump to view (e.g., 'Logic', 'USB', 'Capture')...");
    m_searchEdit->installEventFilter(this);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &CommandPaletteDialog::onSearchTextChanged);
    layout->addWidget(m_searchEdit);

    m_listWidget = new QListWidget(this);
    connect(m_listWidget, &QListWidget::itemActivated, this, &CommandPaletteDialog::onItemActivated);
    layout->addWidget(m_listWidget);

    m_lblHint = new QLabel("↑/↓ to navigate, Enter to execute, Esc to dismiss", this);
    m_lblHint->setStyleSheet("color: #7E7E8F; font-size: 11px;");
    layout->addWidget(m_lblHint);
}

void CommandPaletteDialog::populateList(const QString& query) {
    m_listWidget->clear();
    m_currentItems = ShortcutManager::instance().filter(query);

    for (const auto& item : m_currentItems) {
        QString scStr = item.shortcut.isEmpty() ? "" : QString(" (%1)").arg(item.shortcut.toString());
        QString label = QString("[%1] %2%3 — %4")
                            .arg(item.category)
                            .arg(item.name)
                            .arg(scStr)
                            .arg(item.description);

        auto listItem = new QListWidgetItem(label, m_listWidget);
        listItem->setData(Qt::UserRole, item.id);
    }

    if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
    }
}

void CommandPaletteDialog::onSearchTextChanged(const QString& text) {
    populateList(text);
}

void CommandPaletteDialog::onItemActivated(QListWidgetItem* item) {
    if (!item) return;
    QString id = item->data(Qt::UserRole).toString();
    ShortcutManager::instance().trigger(id);
    accept();
}

bool CommandPaletteDialog::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_searchEdit && event->type() == QEvent::KeyPress) {
        auto keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Down) {
            int row = m_listWidget->currentRow();
            if (row < m_listWidget->count() - 1) {
                m_listWidget->setCurrentRow(row + 1);
            }
            return true;
        } else if (keyEvent->key() == Qt::Key_Up) {
            int row = m_listWidget->currentRow();
            if (row > 0) {
                m_listWidget->setCurrentRow(row - 1);
            }
            return true;
        } else if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            onItemActivated(m_listWidget->currentItem());
            return true;
        } else if (keyEvent->key() == Qt::Key_Escape) {
            reject();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

} // namespace UdooKey
