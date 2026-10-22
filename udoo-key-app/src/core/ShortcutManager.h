#pragma once

#include <QObject>
#include <QString>
#include <QKeySequence>
#include <QAction>
#include <QList>
#include <functional>
#include <map>

namespace UdooKey {

struct ShortcutItem {
    QString id;
    QString category;    // "View", "Capture", "Device", "Tools", "Help"
    QString name;        // e.g. "Switch to Logic Analyzer"
    QString description;
    QKeySequence shortcut;
    std::function<void()> triggerCallback;
};

class ShortcutManager : public QObject {
    Q_OBJECT

public:
    static ShortcutManager& instance();

    void registerAction(const QString& id,
                        const QString& category,
                        const QString& name,
                        const QString& description,
                        const QKeySequence& shortcut,
                        std::function<void()> callback);

    bool trigger(const QString& id);
    QList<ShortcutItem> allItems() const;
    QList<ShortcutItem> filter(const QString& query) const;

signals:
    void actionTriggered(const QString& id);

private:
    ShortcutManager() = default;
    ~ShortcutManager() override = default;
    ShortcutManager(const ShortcutManager&) = delete;
    ShortcutManager& operator=(const ShortcutManager&) = delete;

    std::map<QString, ShortcutItem> m_items;
};

} // namespace UdooKey
