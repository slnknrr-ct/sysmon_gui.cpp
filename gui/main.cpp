#include "mainwindow.h"
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
