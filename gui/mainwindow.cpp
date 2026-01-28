#include "mainwindow.h"
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QAction>
#include <QMessageBox>
#include <QCloseEvent>
#include <QTimer>
#include <QSettings>
#include <QDateTime>
#include <memory>

namespace SysMon {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , isConnected_(false)
    , currentAgentStatus_("Disconnected") {
    
    setupUI();
    setupMenuBar();
    setupStatusBar();
    setupCentralWidget();
    setupTabs();
    
    // Create IPC client
    ipcClient_ = std::make_unique<IpcClient>(this);
    
    // Connect IPC client signals
    connect(ipcClient_.get(), &IpcClient::connected,
            this, [this]() { onConnectionStatusChanged(true); });
    connect(ipcClient_.get(), &IpcClient::disconnected,
            this, [this]() { onConnectionStatusChanged(false); });
    connect(ipcClient_.get(), &IpcClient::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    
    // Set up status timer
    statusTimer_ = std::make_unique<QTimer>(this);
    connect(statusTimer_.get(), &QTimer::timeout,
            this, &MainWindow::updateStatusBar);
    statusTimer_->start(STATUS_UPDATE_INTERVAL);
    
    // Set window properties
    setWindowTitle(WINDOW_TITLE);
    resize(1200, 800);
    
    // Try to connect to agent automatically
    QTimer::singleShot(1000, this, &MainWindow::connectToAgent);
}

MainWindow::~MainWindow() {
    // Disconnect from agent
    if (ipcClient_) {
        ipcClient_->disconnectFromAgent();
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Ask for confirmation if connected
    if (isConnected_) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Confirm Exit",
            "Are you sure you want to exit? You are currently connected to the agent.",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        
        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }
    }
    
    // Disconnect and close
    if (ipcClient_) {
        ipcClient_->disconnectFromAgent();
    }
    
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    // Handle resize if needed
}

void MainWindow::onConnectToAgent() {
    connectToAgent();
}

void MainWindow::onDisconnectFromAgent() {
    disconnectFromAgent();
}

void MainWindow::onExitApplication() {
    close();
}

void MainWindow::onAboutApplication() {
    QMessageBox::about(this, "About SysMon3",
        "SysMon3 - Cross-platform System Monitor\n\n"
        "Version: 1.0.0\n"
        "A comprehensive system monitoring and management tool\n\n"
        " 2025 SysMon Team");
}

void MainWindow::onSettings() {
    QMessageBox::information(this, "Settings", "Settings dialog not implemented yet");
}

void MainWindow::onConnectionStatusChanged(bool connected) {
    updateConnectionStatus(connected);
    
    if (connected) {
        showStatusMessage("Connected to agent");
    } else {
        showStatusMessage("Disconnected from agent");
    }
}

void MainWindow::onAgentStatusChanged(const std::string& status) {
    currentAgentStatus_ = status;
    updateStatusBar();
}

void MainWindow::onErrorOccurred(const std::string& error) {
    showStatusMessage("Error: " + error, 5000);
}

void MainWindow::setupUI() {
    // Main layout is set up in setupCentralWidget()
}

void MainWindow::setupMenuBar() {
    menuBar_ = menuBar();
    
    // File menu
    fileMenu_ = menuBar_->addMenu("&File");
    
    connectAction_ = new QAction("&Connect to Agent", this);
    connectAction_->setShortcut(QKeySequence("Ctrl+C"));
    connect(connectAction_, &QAction::triggered, this, &MainWindow::onConnectToAgent);
    fileMenu_->addAction(connectAction_);
    
    disconnectAction_ = new QAction("&Disconnect from Agent", this);
    disconnectAction_->setShortcut(QKeySequence("Ctrl+D"));
    connect(disconnectAction_, &QAction::triggered, this, &MainWindow::onDisconnectFromAgent);
    fileMenu_->addAction(disconnectAction_);
    
    fileMenu_->addSeparator();
    
    exitAction_ = new QAction("E&xit", this);
    exitAction_->setShortcut(QKeySequence("Ctrl+Q"));
    connect(exitAction_, &QAction::triggered, this, &MainWindow::onExitApplication);
    fileMenu_->addAction(exitAction_);
    
    // Connection menu
    connectionMenu_ = menuBar_->addMenu("&Connection");
    connectionMenu_->addAction(connectAction_);
    connectionMenu_->addAction(disconnectAction_);
    
    // Help menu
    helpMenu_ = menuBar_->addMenu("&Help");
    
    settingsAction_ = new QAction("&Settings", this);
    connect(settingsAction_, &QAction::triggered, this, &MainWindow::onSettings);
    helpMenu_->addAction(settingsAction_);
    
    helpMenu_->addSeparator();
    
    aboutAction_ = new QAction("&About", this);
    connect(aboutAction_, &QAction::triggered, this, &MainWindow::onAboutApplication);
    helpMenu_->addAction(aboutAction_);
}

void MainWindow::setupStatusBar() {
    statusBar_ = statusBar();
    
    connectionStatusLabel_ = new QLabel("Disconnected");
    statusBar_->addWidget(connectionStatusLabel_);
    
    agentStatusLabel_ = new QLabel("Agent: Unknown");
    statusBar_->addWidget(agentStatusLabel_);
    
    timeLabel_ = new QLabel();
    statusBar_->addPermanentWidget(timeLabel_);
}

void MainWindow::setupCentralWidget() {
    mainLayout_ = std::make_unique<QVBoxLayout>();
    
    tabWidget_ = std::make_unique<QTabWidget>();
    mainLayout_->addWidget(tabWidget_.get());
    
    QWidget* centralWidget = new QWidget();
    centralWidget->setLayout(mainLayout_.get());
    setCentralWidget(centralWidget);
}

void MainWindow::setupTabs() {
    createSystemMonitorTab();
    createDeviceManagerTab();
    createNetworkManagerTab();
    createProcessManagerTab();
    createAndroidTab();
    createAutomationTab();
}

void MainWindow::createSystemMonitorTab() {
    systemMonitorTab_ = std::make_unique<SystemMonitorTab>(ipcClient_.get());
    tabWidget_->addTab(systemMonitorTab_.get(), "System Monitor");
}

void MainWindow::createDeviceManagerTab() {
    deviceManagerTab_ = std::make_unique<DeviceManagerTab>(ipcClient_.get());
    tabWidget_->addTab(deviceManagerTab_.get(), "Device Manager");
}

void MainWindow::createNetworkManagerTab() {
    networkManagerTab_ = std::make_unique<NetworkManagerTab>(ipcClient_.get());
    tabWidget_->addTab(networkManagerTab_.get(), "Network Manager");
}

void MainWindow::createProcessManagerTab() {
    processManagerTab_ = std::make_unique<ProcessManagerTab>(ipcClient_.get());
    tabWidget_->addTab(processManagerTab_.get(), "Process Manager");
}

void MainWindow::createAndroidTab() {
    androidTab_ = std::make_unique<AndroidTab>(ipcClient_.get());
    tabWidget_->addTab(androidTab_.get(), "Android Manager");
}

void MainWindow::createAutomationTab() {
    automationTab_ = std::make_unique<AutomationTab>(ipcClient_.get());
    tabWidget_->addTab(automationTab_.get(), "Automation");
}

void MainWindow::connectToAgent() {
    if (!ipcClient_) {
        return;
    }
    
    if (isConnected_) {
        return;
    }
    
    showStatusMessage("Connecting to agent...");
    
    if (ipcClient_->connectToAgent("localhost", 12345)) {
        // Connection initiated
    } else {
        showStatusMessage("Failed to initiate connection to agent");
    }
}

void MainWindow::disconnectFromAgent() {
    if (!ipcClient_) {
        return;
    }
    
    if (!isConnected_) {
        return;
    }
    
    showStatusMessage("Disconnecting from agent...");
    ipcClient_->disconnectFromAgent();
}

void MainWindow::updateConnectionStatus(bool connected) {
    isConnected_ = connected;
    
    if (connected) {
        connectionStatusLabel_->setText("Connected");
        connectionStatusLabel_->setStyleSheet("color: green;");
        connectAction_->setEnabled(false);
        disconnectAction_->setEnabled(true);
    } else {
        connectionStatusLabel_->setText("Disconnected");
        connectionStatusLabel_->setStyleSheet("color: red;");
        connectAction_->setEnabled(true);
        disconnectAction_->setEnabled(false);
        currentAgentStatus_ = "Disconnected";
    }
}

void MainWindow::updateStatusBar() {
    // Update time
    QDateTime currentTime = QDateTime::currentDateTime();
    timeLabel_->setText(currentTime.toString("yyyy-MM-dd hh:mm:ss"));
    
    // Update agent status
    agentStatusLabel_->setText("Agent: " + QString::fromStdString(currentAgentStatus_));
}

void MainWindow::showStatusMessage(const std::string& message, int timeoutMs) {
    statusBar_->showMessage(QString::fromStdString(message), timeoutMs);
}

} // namespace SysMon
