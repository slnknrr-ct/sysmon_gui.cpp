#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <memory>
#include "../shared/systemtypes.h"
#include "../shared/commands.h"
#include "ipcclient.h"
#include "systemmonitortab.h"
#include "devicemanagertab.h"
#include "networkmanagertab.h"
#include "processmanagertab.h"
#include "androidtab.h"
#include "automationtab.h"

QT_BEGIN_NAMESPACE
class QMenuBar;
class QStatusBar;
class QTabWidget;
QT_END_NAMESPACE

namespace SysMon {

// Main Window - main application window with tabs
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    // Event handling
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // Menu actions
    void onConnectToAgent();
    void onDisconnectFromAgent();
    void onExitApplication();
    void onAboutApplication();
    void onSettings();
    
    // Status updates
    void onConnectionStatusChanged(bool connected);
    void onAgentStatusChanged(const std::string& status);
    void onErrorOccurred(const std::string& error);

private:
    // UI initialization
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void setupCentralWidget();
    void setupTabs();
    
    // Tab creation
    void createSystemMonitorTab();
    void createDeviceManagerTab();
    void createNetworkManagerTab();
    void createProcessManagerTab();
    void createAndroidTab();
    void createAutomationTab();
    
    // Connection management
    void connectToAgent();
    void disconnectFromAgent();
    void updateConnectionStatus(bool connected);
    
    // Status bar updates
    void updateStatusBar();
    void showStatusMessage(const std::string& message, int timeoutMs = 3000);
    
    // UI components
    std::unique_ptr<QTabWidget> tabWidget_;
    std::unique_ptr<QVBoxLayout> mainLayout_;
    
    // Menu components
    QMenuBar* menuBar_;
    QMenu* fileMenu_;
    QMenu* connectionMenu_;
    QMenu* helpMenu_;
    QAction* connectAction_;
    QAction* disconnectAction_;
    QAction* exitAction_;
    QAction* settingsAction_;
    QAction* aboutAction_;
    
    // Status bar components
    QStatusBar* statusBar_;
    QLabel* connectionStatusLabel_;
    QLabel* agentStatusLabel_;
    QLabel* timeLabel_;
    
    // Tabs
    std::unique_ptr<SystemMonitorTab> systemMonitorTab_;
    std::unique_ptr<DeviceManagerTab> deviceManagerTab_;
    std::unique_ptr<NetworkManagerTab> networkManagerTab_;
    std::unique_ptr<ProcessManagerTab> processManagerTab_;
    std::unique_ptr<AndroidTab> androidTab_;
    std::unique_ptr<AutomationTab> automationTab_;
    
    // IPC client
    std::unique_ptr<IpcClient> ipcClient_;
    
    // Status
    bool isConnected_;
    std::string currentAgentStatus_;
    
    // Timer for status updates
    std::unique_ptr<QTimer> statusTimer_;
    
    // Constants
    static constexpr const char* WINDOW_TITLE = "SysMon3 - System Monitor";
    static constexpr int STATUS_UPDATE_INTERVAL = 1000; // 1 second
};

} // namespace SysMon
