#include "Application.h"
#include "Logger.h"
#include "SettingsManager.h"
#include <QThreadPool>
#include <QIcon>

namespace UdooKey {

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv) {
    setApplicationName("UDOO KEY Companion");
    setApplicationVersion("1.2.0");
    setOrganizationName("UDOO KEY Project");
    setOrganizationDomain("udoo-key-instrument.org");
    setWindowIcon(QIcon(":/icons/app-icon.svg"));
}

Application::~Application() {
    shutdown();
}

Application* Application::instance() {
    return qobject_cast<Application*>(QApplication::instance());
}

void Application::initialize() {
    LOG_INFO("Initializing UDOO KEY Companion v1.0.0");
    // Ensure thread pool has ample threads for background acquisition, LOD, and decoders
    int idealThreads = QThreadPool::globalInstance()->maxThreadCount();
    if (idealThreads < 4) {
        QThreadPool::globalInstance()->setMaxThreadCount(4);
    }
    LOG_INFO(QString("Worker thread pool initialized with %1 threads").arg(QThreadPool::globalInstance()->maxThreadCount()));
}

void Application::shutdown() {
    LOG_INFO("Shutting down UDOO KEY Companion");
    QThreadPool::globalInstance()->waitForDone(1000);
}

} // namespace UdooKey
