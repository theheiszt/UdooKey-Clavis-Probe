#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QList>
#include <QMutex>

namespace UdooKey {

enum class ErrorCategory {
    General,
    Transport,
    Protocol,
    Device,
    Acquisition,
    Decoder,
    Usb,
    FileSystem
};

enum class ErrorSeverity {
    Info,
    Warning,
    Error,
    Critical
};

struct ErrorEvent {
    uint64_t id = 0;
    QDateTime timestamp;
    ErrorCategory category = ErrorCategory::General;
    ErrorSeverity severity = ErrorSeverity::Error;
    int errorCode = 0;
    QString message;
    QString suggestion;
    QString context;

    QString categoryString() const;
    QString severityString() const;
};

class ErrorManager : public QObject {
    Q_OBJECT

public:
    static ErrorManager& instance();

    void report(ErrorCategory category,
                ErrorSeverity severity,
                const QString& message,
                const QString& suggestion = "",
                const QString& context = "",
                int errorCode = 0);

    QList<ErrorEvent> recentErrors(int maxCount = 100) const;
    void clear();

signals:
    void errorOccurred(const ErrorEvent& event);

private:
    ErrorManager();
    ~ErrorManager() override = default;
    ErrorManager(const ErrorManager&) = delete;
    ErrorManager& operator=(const ErrorManager&) = delete;

    mutable QMutex m_mutex;
    uint64_t m_nextId = 1;
    QList<ErrorEvent> m_history;
    static constexpr int MAX_HISTORY = 1000;
};

} // namespace UdooKey
