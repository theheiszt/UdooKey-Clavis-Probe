#include "ShortcutManager.h"

namespace UdooKey {

ShortcutManager& ShortcutManager::instance() {
    static ShortcutManager s_instance;
    return s_instance;
}

void ShortcutManager::registerAction(const QString& id,
                                     const QString& category,
                                     const QString& name,
                                     const QString& description,
                                     const QKeySequence& shortcut,
                                     std::function<void()> callback) {
    ShortcutItem item;
    item.id = id;
    item.category = category;
    item.name = name;
    item.description = description;
    item.shortcut = shortcut;
    item.triggerCallback = callback;
    m_items[id] = item;
}

bool ShortcutManager::trigger(const QString& id) {
    auto it = m_items.find(id);
    if (it != m_items.end() && it->second.triggerCallback) {
        it->second.triggerCallback();
        emit actionTriggered(id);
        return true;
    }
    return false;
}

QList<ShortcutItem> ShortcutManager::allItems() const {
    QList<ShortcutItem> list;
    for (const auto& pair : m_items) {
        list.append(pair.second);
    }
    return list;
}

QList<ShortcutItem> ShortcutManager::filter(const QString& query) const {
    QList<ShortcutItem> results;
    QString cleanQuery = query.trimmed().toLower();

    for (const auto& pair : m_items) {
        const auto& item = pair.second;
        if (cleanQuery.isEmpty() ||
            item.name.toLower().contains(cleanQuery) ||
            item.category.toLower().contains(cleanQuery) ||
            item.description.toLower().contains(cleanQuery) ||
            item.id.toLower().contains(cleanQuery)) {
            results.append(item);
        }
    }
    return results;
}

} // namespace UdooKey
