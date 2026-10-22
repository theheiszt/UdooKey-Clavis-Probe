#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include "ShortcutManager.h"

namespace UdooKey {

class CommandPaletteDialog : public QDialog {
    Q_OBJECT

public:
    explicit CommandPaletteDialog(QWidget* parent = nullptr);
    ~CommandPaletteDialog() override = default;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onSearchTextChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);

private:
    void setupUi();
    void populateList(const QString& query);

    QLineEdit* m_searchEdit;
    QListWidget* m_listWidget;
    QLabel* m_lblHint;
    QList<ShortcutItem> m_currentItems;
};

} // namespace UdooKey
