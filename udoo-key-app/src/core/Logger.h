#pragma once

#include <QObject>
#include <QString>
#include <QMutex>

namespace UdooKey {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger : public QObject {
    Q_OBJECT

public:
    static Logger& instance();

    void log(LogLevel level, const QString& message, const char* file = nullptr, int line = 0);

    void debug(const QString& msg, const char* f = nullptr, int l = 0) { log(LogLevel::Debug, msg, f, l); }
    void info(const QString& msg, const char* f = nullptr, int l = 0)  { log(LogLevel::Info, msg, f, l); }
    void warn(const QString& msg, const char* f = nullptr, int l = 0)  { log(LogLevel::Warning, msg, f, l); }
    void error(const QString& msg, const char* f = nullptr, int l = 0) { log(LogLevel::Error, msg, f, l); }

signals:
    void logMessageEmitted(LogLevel level, const QString& formattedMessage);

private:
    Logger() = default;
    ~Logger() override = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    QMutex m_mutex;
};

#define LOG_DEBUG(msg) UdooKey::Logger::instance().debug((msg), __FILE__, __LINE__)
#define LOG_INFO(msg)  UdooKey::Logger::instance().info((msg), __FILE__, __LINE__)
#define LOG_WARN(msg)  UdooKey::Logger::instance().warn((msg), __FILE__, __LINE__)
#define LOG_ERROR(msg) UdooKey::Logger::instance().error((msg), __FILE__, __LINE__)

} // namespace UdooKey
