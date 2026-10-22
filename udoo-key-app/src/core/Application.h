#pragma once

#include <QApplication>
#include <memory>

namespace UdooKey {

class Application : public QApplication {
    Q_OBJECT

public:
    Application(int& argc, char** argv);
    ~Application() override;

    static Application* instance();

    QString version() const { return "1.0.0"; }
    QString appName() const { return "UDOO KEY Companion"; }

    void initialize();
    void shutdown();
};

} // namespace UdooKey
