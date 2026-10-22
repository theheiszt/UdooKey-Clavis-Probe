#include "ErrorManager.h"
#include "Logger.h"

namespace UdooKey {

QString ErrorEvent::categoryString() const {
    switch (category) {
        case ErrorCategory::Transport:   return "Transport";
        case ErrorCategory::Protocol:    return "Protocol";
        case ErrorCategory::Device:      return "Device";
        case ErrorCategory::Acquisition: return "Acquisition";
        case ErrorCategory::Decoder:     return "Decoder";
        case ErrorCategory::Usb:         return "USB";
        case ErrorCategory::FileSystem:  return "FileSystem";
        case ErrorCategory::General:
        default:                         return "General";
    }
}

QString ErrorEvent::severityString() const {
    switch (severity) {
        case ErrorSeverity::Info:     return "Info";
        case ErrorSeverity::Warning:  return "Warning";
        case ErrorSeverity::Critical: return "Critical";
        case ErrorSeverity::Error:
        default:                      return "Error";
    }
}

ErrorManager::ErrorManager() {
}

ErrorManager& ErrorManager::instance() {
    static ErrorManager s_instance;
    return s_instance;
}

void ErrorManager::report(ErrorCategory category,
                          ErrorSeverity severity,
                          const QString& message,
                          const QString& suggestion,
                          const QString& context,
                          int errorCode) {
    ErrorEvent ev;
    {
        QMutexLocker locker(&m_mutex);
        ev.id = m_nextId++;
        ev.timestamp = QDateTime::currentDateTime();
        ev.category = category;
        ev.severity = severity;
        ev.errorCode = errorCode;
        ev.message = message;
        ev.suggestion = suggestion;
        ev.context = context;

        m_history.append(ev);
        if (m_history.size() > MAX_HISTORY) {
            m_history.removeFirst();
        }
    }

    // Also log to Logger
    QString logText = QString("[%1] %2").arg(ev.categoryString(), message);
    if (!suggestion.isEmpty()) {
        logText += QString(" (Recommendation: %1)").arg(suggestion);
    }

    if (severity == ErrorSeverity::Critical || severity == ErrorSeverity::Error) {
        LOG_ERROR(logText);
    } else if (severity == ErrorSeverity::Warning) {
        LOG_WARN(logText);
    } else {
        LOG_INFO(logText);
    }

    emit errorOccurred(ev);
}

QList<ErrorEvent> ErrorManager::recentErrors(int maxCount) const {
    QMutexLocker locker(&m_mutex);
    if (m_history.size() <= maxCount) {
        return m_history;
    }
    return m_history.mid(m_history.size() - maxCount);
}

void ErrorManager::clear() {
    QMutexLocker locker(&m_mutex);
    m_history.clear();
}

} // namespace UdooKey
