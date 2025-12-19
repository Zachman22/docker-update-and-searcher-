#include <QApplication>
#include "ui/MainWindow.h"
#include "utils/Logger.h"
#include <iostream>

int main(int argc, char *argv[]) {
    // Initialize logger
    utils::Logger::getInstance().setLogLevel(utils::LogLevel::Info);
    utils::Logger::getInstance().enableConsoleOutput(true);
    utils::Logger::getInstance().enableFileOutput(true);
    utils::Logger::getInstance().setLogFile("docker_homelab_manager.log");

    LOG_INFO("Docker Homelab Manager starting...");

    // Create Qt application
    QApplication app(argc, argv);

    // Set application metadata
    app.setApplicationName("Docker Homelab Manager");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("Homelab Tools");

    try {
        // Create and show main window
        ui::MainWindow mainWindow;
        mainWindow.setWindowTitle("Docker Homelab Manager v0.1.0");
        mainWindow.resize(1200, 800);
        mainWindow.show();

        LOG_INFO("Application initialized successfully");

        // Run application event loop
        return app.exec();
    }
    catch (const std::exception& e) {
        LOG_CRITICAL(std::string("Fatal error: ") + e.what());
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
