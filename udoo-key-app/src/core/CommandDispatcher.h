#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <functional>
#include <map>

namespace UdooKey {

class CommandDispatcher : public QObject {
    Q_OBJECT

public:
    using Handler = std::function<QString(const QStringList& args)>;

    static CommandDispatcher& instance();

    void registerCommand(const QString& name, const QString& helpText, Handler handler);
    QString execute(const QString& commandLine);
    QJsonObject executeJsonRpc(const QJsonObject& request);

    QStringList availableCommands() const;
    QString help(const QString& cmd = QString()) const;

private:
    CommandDispatcher();
    ~CommandDispatcher() override = default;
    CommandDispatcher(const CommandDispatcher&) = delete;
    CommandDispatcher& operator=(const CommandDispatcher&) = delete;

    void registerBuiltins();

    struct CommandEntry {
        QString name;
        QString help;
        Handler handler;
    };

    std::map<QString, CommandEntry> m_commands;
};

} // namespace UdooKey
