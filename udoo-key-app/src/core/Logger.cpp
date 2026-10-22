#include "Logger.h"
#include <QDateTime>
#include <iostream>

namespace UdooKey {

Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

void Logger::log(LogLevel level, const QString& message, const char* file, int line) {
    QMutexLocker locker(&m_mutex);

    const char* lvlStr = "INFO";
    switch (level) {
        case LogLevel::Debug:   lvlStr = "DEBUG"; break;
        case LogLevel::Info:    lvlStr = "INFO "; break;
        case LogLevel::Warning: lvlStr = "WARN "; break;
        case LogLevel::Error:   lvlStr = "ERROR"; break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString location = "";
    if (file) {
        QString fStr(file);
        int slashIdx = fStr.lastIndexOf('/');
        if (slashIdx == -1) slashIdx = fStr.lastIndexOf('\\');
        if (slashIdx != -1) fStr = fStr.mid(slashIdx + 1);
        location = QString(" [%1:%2]").arg(fStr).arg(line);
    }

    QString formatted = QString("[%1] [%2]%3 %4").arg(timestamp, lvlStr, location, message);

    if (level == LogLevel::Error) {
        std::cerr << formatted.toStdString() << std::endl;
    } else {
        std::cout << formatted.toStdString() << std::endl;
    }

    emit logMessageEmitted(level, formatted);
}

} // namespace UdooKey
