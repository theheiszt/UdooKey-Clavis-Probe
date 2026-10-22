#include "Application.h"
#include "MainWindow.h"
#include "Logger.h"

int main(int argc, char* argv[]) {
    UdooKey::Application app(argc, argv);
    app.initialize();

    LOG_INFO("Starting UDOO KEY Desktop Companion...");

    UdooKey::MainWindow mainWindow;
    mainWindow.show();

    int exitCode = app.exec();

    LOG_INFO("Exiting UDOO KEY Desktop Companion.");
    return exitCode;
}
