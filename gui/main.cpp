#include "mainwindow.h"
#include "gui_config.h"
#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <memory>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Set application properties
    app.setApplicationName("SysMon3");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("SysMon");
    app.setOrganizationDomain("sysmon.local");
    
    // Set application style
    app.setStyle(QStyleFactory::create("Fusion"));
    
    // Set application icon
    app.setWindowIcon(QIcon("../sysmon.png"));
    
    // Load GUI configuration
    auto& config = SysMon::GuiConfig::getInstance();
    std::string configFile = "sysmon_gui.conf";
    if (!config.loadFromFile(configFile)) {
        qDebug() << "GUI config file not found, using defaults. Created:" << QString::fromStdString(configFile);
    } else {
        qDebug() << "GUI configuration loaded successfully";
        qDebug() << "Agent host:" << QString::fromStdString(config.getAgentHost());
        qDebug() << "Agent port:" << config.getAgentPort();
    }
    
    qDebug() << "SysMon3 GUI starting...";
    
    try {
        // Create main window
        auto mainWindow = std::make_unique<SysMon::MainWindow>();
        
        // Show main window
        mainWindow->show();
        
        qDebug() << "SysMon3 GUI started successfully";
        
        // Run application
        int result = app.exec();
        
        qDebug() << "SysMon3 GUI exited with code:" << result;
        return result;
        
    } catch (const std::exception& e) {
        qCritical() << "GUI failed with exception:" << e.what();
        return 1;
    }
}
