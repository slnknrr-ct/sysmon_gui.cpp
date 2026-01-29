#include "androidtab.h"
#include "../shared/commands.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QGroupBox>
#include <QHeaderView>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QProgressBar>
#include <QDialog>
#include <QListWidgetItem>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPixmap>
#include <QTableWidgetItem>
#include <QShowEvent>
#include <QHideEvent>
#include <QMessageBox>
#include <sstream>
#include <algorithm>
#include <QDebug>

namespace SysMon {

AndroidTab::AndroidTab(IpcClient* ipcClient, QWidget* parent)
    : QWidget(parent)
    , ipcClient_(ipcClient)
    , isActive_(false) {
    
    setupUI();
    
    // Create update timers
    deviceRefreshTimer_ = std::make_unique<QTimer>(this);
    connect(deviceRefreshTimer_.get(), &QTimer::timeout,
            this, &AndroidTab::refreshDevices);
    
    infoRefreshTimer_ = std::make_unique<QTimer>(this);
    connect(infoRefreshTimer_.get(), &QTimer::timeout,
            this, &AndroidTab::refreshDeviceInfo);
    
    // Connect IPC client signals
    connect(ipcClient_, &IpcClient::responseReceived,
            this, [this](const Response& response) {
                // Handle different response types
                if (response.commandId.find("devices") != std::string::npos) {
                    onAndroidDevicesResponse(response);
                } else if (response.commandId.find("device_info") != std::string::npos) {
                    onDeviceInfoResponse(response);
                } else if (response.commandId.find("apps") != std::string::npos) {
                    onAppListResponse(response);
                } else if (response.commandId.find("screenshot") != std::string::npos) {
                    onScreenshotResponse(response);
                } else if (response.commandId.find("orientation") != std::string::npos) {
                    onOrientationResponse(response);
                } else if (response.commandId.find("logcat") != std::string::npos) {
                    onLogcatResponse(response);
                } else {
                    onDeviceOperationResponse(response);
                }
            });
    connect(ipcClient_, &IpcClient::errorOccurred,
            this, [this](const std::string& error) {
                onError(QString::fromStdString(error));
            });
}

AndroidTab::~AndroidTab() {
    // Timers are automatically deleted by Qt
}

void AndroidTab::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    
    if (!isActive_) {
        isActive_ = true;
        
        // Start timers
        deviceRefreshTimer_->start(DEVICE_REFRESH_INTERVAL);
        infoRefreshTimer_->start(INFO_REFRESH_INTERVAL);
        
        // Request initial data
        refreshDevices();
    }
}

void AndroidTab::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    
    if (isActive_) {
        isActive_ = false;
        
        // Stop timers
        deviceRefreshTimer_->stop();
        infoRefreshTimer_->stop();
    }
}

void AndroidTab::refreshDevices() {
    if (!ipcClient_ || !isActive_) {
        return;
    }
    
    // Create command to get Android devices
    Command command = createCommand(CommandType::GET_ANDROID_DEVICES, Module::ANDROID);
    command.id = "devices_" + command.id;
    
    // Send command
    ipcClient_->sendCommand(command);
}

void AndroidTab::onDeviceSelectionChanged() {
    updateButtonStates();
    
    // Refresh device info for selected device
    if (hasValidDeviceSelection()) {
        refreshDeviceInfo();
    } else {
        clearDeviceInfo();
    }
}

void AndroidTab::turnScreenOn() {
    if (!hasValidDeviceSelection()) {
        return;
    }
    
    AndroidDeviceInfo device = getSelectedDevice();
    
    // Create command to turn screen on
    Command command = createCommand(CommandType::ANDROID_SCREEN_ON, Module::ANDROID);
    command.parameters["serial"] = device.serialNumber;
    
    // Send command
    ipcClient_->sendCommand(command);
}

void AndroidTab::turnScreenOff() {
    if (!hasValidDeviceSelection()) {
        return;
    }
    
    AndroidDeviceInfo device = getSelectedDevice();
    
    // Create command to turn screen off
    Command command = createCommand(CommandType::ANDROID_SCREEN_OFF, Module::ANDROID);
    command.parameters["serial"] = device.serialNumber;
    
    // Send command
    ipcClient_->sendCommand(command);
}

void AndroidTab::lockDevice() {
    if (!hasValidDeviceSelection()) {
        return;
    }
    
    AndroidDeviceInfo device = getSelectedDevice();
    
    // Create command to lock device
    Command command = createCommand(CommandType::ANDROID_LOCK_DEVICE, Module::ANDROID);
    command.parameters["serial"] = device.serialNumber;
    
    // Send command
    ipcClient_->sendCommand(command);
}

void AndroidTab::refreshDeviceInfo() {
    if (!hasValidDeviceSelection()) {
        return;
    }
    
    AndroidDeviceInfo device = getSelectedDevice();
    
    // Create command to get device info
    Command command = createCommand(CommandType::GET_ANDROID_DEVICES, Module::ANDROID);
    command.parameters["serial"] = device.serialNumber;
    command.id = "device_info_" + command.id;
    
    // Send command
    ipcClient_->sendCommand(command);
}

void AndroidTab::refreshApps() {
    if (!hasValidDeviceSelection()) {
        return;
    }
    
    AndroidDeviceInfo device = getSelectedDevice();
    
    // Create command to get installed apps
    Command command = createCommand(CommandType::ANDROID_GET_FOREGROUND_APP, Module::ANDROID);
    command.parameters["serial"] = device.serialNumber;
    command.id = "apps_" + command.id;
    
    // Send command
    ipcClient_->sendCommand(command);
}

void AndroidTab::launchApp() {
    if (!hasValidDeviceSelection()) {
        return;
    }
    
    QString packageName = appEdit_->text();
    if (packageName.isEmpty()) {
        return;
    }
    
    AndroidDeviceInfo device = getSelectedDevice();
    
    // Create command to launch app
    Command command = createCommand(CommandType::ANDROID_LAUNCH_APP, Module::ANDROID);
    command.parameters["serial"] = device.serialNumber;
    command.parameters["package"] = packageName.toStdString();
    
    // Send command
    ipcClient_->sendCommand(command);
}

void AndroidTab::stopApp() {
    if (!hasValidDeviceSelection()) {
        return;
    }
    
    QString packageName = appEdit_->text();
    if (packageName.isEmpty()) {
        return;
    }
    
    AndroidDeviceInfo device = getSelectedDevice();
    
    // Create command to stop app
    Command command = createCommand(CommandType::ANDROID_STOP_APP, Module::ANDROID);
    command.parameters["serial"] = device.serialNumber;
    command.parameters["package"] = packageName.toStdString();
    
    // Send command
    ipcClient_->sendCommand(command);
}

void AndroidTab::takeScreenshot() {
    if (!hasValidDeviceSelection()) {
        return;
    }
    
    AndroidDeviceInfo device = getSelectedDevice();
    
    // Create command to take screenshot
    Command command = createCommand(CommandType::ANDROID_TAKE_SCREENSHOT, Module::ANDROID);
    command.parameters["serial"] = device.serialNumber;
    command.id = "screenshot_" + command.id;
    
    // Send command
    ipcClient_->sendCommand(command);
}

void AndroidTab::getOrientation() {
    if (!hasValidDeviceSelection()) {
        return;
    }
    
    AndroidDeviceInfo device = getSelectedDevice();
    
    // Create command to get orientation
    Command command = createCommand(CommandType::ANDROID_GET_ORIENTATION, Module::ANDROID);
    command.parameters["serial"] = device.serialNumber;
    command.id = "orientation_" + command.id;
    
    // Send command
    ipcClient_->sendCommand(command);
}

void AndroidTab::getLogcat() {
    if (!hasValidDeviceSelection()) {
        return;
    }
    
    AndroidDeviceInfo device = getSelectedDevice();
    
    // Create command to get logcat
    Command command = createCommand(CommandType::ANDROID_GET_LOGCAT, Module::ANDROID);
    command.parameters["serial"] = device.serialNumber;
    command.parameters["lines"] = std::to_string(MAX_LOGCAT_LINES);
    command.id = "logcat_" + command.id;
    
    // Send command
    ipcClient_->sendCommand(command);
}

void AndroidTab::onAndroidDevicesResponse(const Response& response) {
    if (response.status != CommandStatus::SUCCESS) {
        onError(QString("Failed to get Android devices: %1").arg(QString::fromStdString(response.message)));
        return;
    }
    
    // Parse Android devices from response data
    std::vector<AndroidDeviceInfo> devices;
    
    try {
        // Expect device data in format: "model1,version1,serial1,battery1,screen1,lock1,app1;model2,version2,serial2,battery2,screen2,lock2,app2;..."
        std::string deviceData = response.data.count("devices") ? response.data.at("devices") : "";
        
        if (!deviceData.empty()) {
            std::stringstream ss(deviceData);
            std::string deviceEntry;
            
            while (std::getline(ss, deviceEntry, ';')) {
                std::stringstream entryStream(deviceEntry);
                std::string field;
                std::vector<std::string> fields;
                
                while (std::getline(entryStream, field, ',')) {
                    fields.push_back(field);
                }
                
                if (fields.size() >= 7) {
                    AndroidDeviceInfo device;
                    device.model = fields[0];
                    device.androidVersion = fields[1];
                    device.serialNumber = fields[2];
                    
                    try {
                        device.batteryLevel = std::stoi(fields[3]);
                    } catch (const std::exception& e) {
                        device.batteryLevel = 0; // Default battery level
                    }
                    
                    device.isScreenOn = (fields[4] == "1" || fields[4] == "true");
                    device.isLocked = (fields[5] == "1" || fields[5] == "true");
                    device.foregroundApp = fields[6];
                    
                    devices.push_back(device);
                }
            }
        }
    } catch (const std::exception& e) {
        onError(QString("Failed to parse device data: %1").arg(e.what()));
        return;
    }
    
    // Display actual device count
    deviceCountLabel_->setText(QString("Devices: %1").arg(devices.size()));
    
    // If no devices, show appropriate message
    if (devices.empty()) {
        // Show no device message in device combo
        deviceCombo_->clear();
        deviceCombo_->addItem("No Android devices connected");
        return;
    }
    
    currentDevices_ = devices;
    
    // Update device list
    updateDeviceList(devices);
    updateButtonStates();
}

void AndroidTab::onDeviceInfoResponse(const Response& response) {
    if (response.status != CommandStatus::SUCCESS) {
        onError(QString("Failed to get device info: %1").arg(QString::fromStdString(response.message)));
        return;
    }
    
    // Parse device info from response data
    try {
        std::string deviceData = response.data.count("device_info") ? response.data.at("device_info") : "";
        
        if (!deviceData.empty()) {
            std::stringstream ss(deviceData);
            std::string field;
            std::vector<std::string> fields;
            
            while (std::getline(ss, field, ',')) {
                fields.push_back(field);
            }
            
            if (fields.size() >= 7) {
                AndroidDeviceInfo device;
                device.model = fields[0];
                device.androidVersion = fields[1];
                device.serialNumber = fields[2];
                
                try {
                    device.batteryLevel = std::stoi(fields[3]);
                } catch (const std::exception& e) {
                    device.batteryLevel = 0; // Default battery level
                }
                
                device.isScreenOn = (fields[4] == "1" || fields[4] == "true");
                device.isLocked = (fields[5] == "1" || fields[5] == "true");
                device.foregroundApp = fields[6];
                
                currentDeviceInfo_ = device;
                updateDeviceInfo(device);
                return;
            }
        }
    } catch (const std::exception& e) {
        onError(QString("Failed to parse device info: %1").arg(e.what()));
        return;
    }
    
    // If no device info received, show error
    if (response.data.empty()) {
        onError("No device information available");
        return;
    }
    
    // Device info was already parsed and updated in the try block above
    // No need to update again here
}

void AndroidTab::onAppListResponse(const Response& response) {
    if (response.status != CommandStatus::SUCCESS) {
        onError(QString("Failed to get app list: %1").arg(QString::fromStdString(response.message)));
        return;
    }
    
    // Parse app list from response data
    std::vector<std::string> apps;
    
    try {
        std::string appData = response.data.count("apps") ? response.data.at("apps") : "";
        
        if (!appData.empty()) {
            std::stringstream ss(appData);
            std::string app;
            
            while (std::getline(ss, app, ';')) {
                // Remove whitespace
                app.erase(0, app.find_first_not_of(" \t\n\r"));
                app.erase(app.find_last_not_of(" \t\n\r") + 1);
                
                if (!app.empty()) {
                    apps.push_back(app);
                }
            }
        }
    } catch (const std::exception& e) {
        onError(QString("Failed to parse app list: %1").arg(e.what()));
        return;
    }
    
    // If no apps, show appropriate message
    if (apps.empty()) {
        // Show no apps message in app table
        appTable_->setRowCount(1);
        appTable_->setColumnCount(1);
        appTable_->setItem(0, 0, new QTableWidgetItem("No apps found on device"));
        currentApps_.clear();
        return;
    }
    
    currentApps_ = apps;
    updateAppList(apps);
}

void AndroidTab::onDeviceOperationResponse(const Response& response) {
    QString operation = "Unknown operation";
    
    if (response.status == CommandStatus::SUCCESS) {
        operation = QString::fromStdString(response.message);
        showDeviceOperationResult(operation, true);
        
        // Refresh device info
        refreshDeviceInfo();
    } else {
        showDeviceOperationResult(operation, false, QString::fromStdString(response.message));
    }
}

void AndroidTab::onScreenshotResponse(const Response& response) {
    if (response.status != CommandStatus::SUCCESS) {
        onError(QString("Failed to take screenshot: %1").arg(QString::fromStdString(response.message)));
        return;
    }
    
    // Parse screenshot data from response
    QString imagePath = "screenshot.png"; // Default fallback
    
    try {
        std::string screenshotData = response.data.count("screenshot") ? response.data.at("screenshot") : "";
        if (!screenshotData.empty()) {
            imagePath = QString::fromStdString(screenshotData);
        }
    } catch (const std::exception& e) {
        onError(QString("Failed to parse screenshot data: %1").arg(e.what()));
    }
    
    showScreenshot(imagePath);
}

void AndroidTab::onOrientationResponse(const Response& response) {
    if (response.status != CommandStatus::SUCCESS) {
        onError(QString("Failed to get orientation: %1").arg(QString::fromStdString(response.message)));
        return;
    }
    
    // Parse orientation from response data
    QString orientation = "unknown"; // Default fallback
    
    try {
        std::string orientationData = response.data.count("orientation") ? response.data.at("orientation") : "";
        if (!orientationData.empty()) {
            orientation = QString::fromStdString(orientationData);
        }
    } catch (const std::exception& e) {
        onError(QString("Failed to parse orientation data: %1").arg(e.what()));
    }
    
    showOrientation(orientation);
}

void AndroidTab::onLogcatResponse(const Response& response) {
    if (response.status != CommandStatus::SUCCESS) {
        onError(QString("Failed to get logcat: %1").arg(QString::fromStdString(response.message)));
        return;
    }
    
    // Parse logcat data from response
    std::vector<std::string> logcat;
    
    try {
        std::string logcatData = response.data.count("logcat") ? response.data.at("logcat") : "";
        
        if (!logcatData.empty()) {
            std::stringstream ss(logcatData);
            std::string line;
            
            while (std::getline(ss, line, '|')) {
                // Remove whitespace
                line.erase(0, line.find_first_not_of(" \t\n\r"));
                line.erase(line.find_last_not_of(" \t\n\r") + 1);
                
                if (!line.empty()) {
                    logcat.push_back(line);
                }
            }
        }
    } catch (const std::exception& e) {
        onError(QString("Failed to parse logcat data: %1").arg(e.what()));
        return;
    }
    
    // If no logcat, show appropriate message
    if (logcat.empty()) {
        logcatEdit_->clear();
        logcatEdit_->append("No log data available from device");
        logcatEdit_->append("Make sure the device is connected and ADB debugging is enabled");
        return;
    }
    
    showLogcat(logcat);
}

void AndroidTab::onError(const QString& error) {
    qWarning() << "AndroidTab error:" << error;
}

void AndroidTab::setupUI() {
    mainLayout_ = std::make_unique<QVBoxLayout>();
    
    setupDeviceSection();
    setupDeviceInfoSection();
    setupDeviceControlSection();
    setupAppManagementSection();
    setupSystemOperationsSection();
    setupStatusBar();
    
    mainLayout_->addWidget(deviceGroup_.get());
    mainLayout_->addWidget(infoGroup_.get());
    mainLayout_->addWidget(controlGroup_.get());
    mainLayout_->addWidget(appGroup_.get());
    mainLayout_->addWidget(systemGroup_.get());
    mainLayout_->addStretch();
    
    setLayout(mainLayout_.get());
}

void AndroidTab::setupDeviceSection() {
    deviceGroup_ = std::make_unique<QGroupBox>();
    deviceGroup_->setTitle("Device Selection");
    deviceLayout_ = std::make_unique<QHBoxLayout>();
    
    deviceLabel_ = std::make_unique<QLabel>();
    deviceLabel_->setText("Device:");
    deviceCombo_ = std::make_unique<QComboBox>();
    refreshDevicesButton_ = std::make_unique<QPushButton>();
    refreshDevicesButton_->setText("Refresh");
    
    // Connect signals
    connect(deviceCombo_.get(), QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AndroidTab::onDeviceSelectionChanged);
    connect(refreshDevicesButton_.get(), &QPushButton::clicked,
            this, &AndroidTab::refreshDevices);
    
    deviceLayout_->addWidget(deviceLabel_.get());
    deviceLayout_->addWidget(deviceCombo_.get());
    deviceLayout_->addWidget(refreshDevicesButton_.get());
    deviceLayout_->addStretch();
    
    deviceGroup_->setLayout(deviceLayout_.get());
}

void AndroidTab::setupDeviceInfoSection() {
    infoGroup_ = std::make_unique<QGroupBox>();
    infoGroup_->setTitle("Device Information");
    infoLayout_ = std::make_unique<QFormLayout>();
    
    modelLabel_ = std::make_unique<QLabel>();
    modelLabel_->setText("Unknown");
    versionLabel_ = std::make_unique<QLabel>();
    versionLabel_->setText("Unknown");
    serialLabel_ = std::make_unique<QLabel>();
    serialLabel_->setText("Unknown");
    batteryLabel_ = std::make_unique<QLabel>();
    batteryLabel_->setText("Unknown");
    batteryBar_ = std::make_unique<QProgressBar>();
    screenLabel_ = std::make_unique<QLabel>();
    screenLabel_->setText("Unknown");
    lockLabel_ = std::make_unique<QLabel>();
    lockLabel_->setText("Unknown");
    foregroundLabel_ = std::make_unique<QLabel>();
    foregroundLabel_->setText("Unknown");
    
    infoLayout_->addRow("Model:", modelLabel_.get());
    infoLayout_->addRow("Android Version:", versionLabel_.get());
    infoLayout_->addRow("Serial Number:", serialLabel_.get());
    infoLayout_->addRow("Battery:", batteryBar_.get());
    infoLayout_->addRow("", batteryLabel_.get());
    infoLayout_->addRow("Screen:", screenLabel_.get());
    infoLayout_->addRow("Lock Status:", lockLabel_.get());
    infoLayout_->addRow("Foreground App:", foregroundLabel_.get());
    
    infoGroup_->setLayout(infoLayout_.get());
}

void AndroidTab::setupDeviceControlSection() {
    controlGroup_ = std::make_unique<QGroupBox>();
    controlGroup_->setTitle("Device Control");
    controlLayout_ = std::make_unique<QHBoxLayout>();
    
    screenOnButton_ = std::make_unique<QPushButton>();
    screenOnButton_->setText("Screen On");
    screenOffButton_ = std::make_unique<QPushButton>();
    screenOffButton_->setText("Screen Off");
    lockButton_ = std::make_unique<QPushButton>();
    lockButton_->setText("Lock Device");
    refreshInfoButton_ = std::make_unique<QPushButton>();
    refreshInfoButton_->setText("Refresh Info");
    
    // Connect signals
    connect(screenOnButton_.get(), &QPushButton::clicked,
            this, &AndroidTab::turnScreenOn);
    connect(screenOffButton_.get(), &QPushButton::clicked,
            this, &AndroidTab::turnScreenOff);
    connect(lockButton_.get(), &QPushButton::clicked,
            this, &AndroidTab::lockDevice);
    connect(refreshInfoButton_.get(), &QPushButton::clicked,
            this, &AndroidTab::refreshDeviceInfo);
    
    controlLayout_->addWidget(screenOnButton_.get());
    controlLayout_->addWidget(screenOffButton_.get());
    controlLayout_->addWidget(lockButton_.get());
    controlLayout_->addWidget(refreshInfoButton_.get());
    controlLayout_->addStretch();
    
    controlGroup_->setLayout(controlLayout_.get());
}

void AndroidTab::setupAppManagementSection() {
    appGroup_ = std::make_unique<QGroupBox>();
    appGroup_->setTitle("App Management");
    appLayout_ = std::make_unique<QVBoxLayout>();
    
    // App control row
    appControlLayout_ = std::make_unique<QHBoxLayout>();
    refreshAppsButton_ = std::make_unique<QPushButton>();
    refreshAppsButton_->setText("Refresh Apps");
    appEdit_ = std::make_unique<QLineEdit>();
    appEdit_->setPlaceholderText("Package name (e.g., com.example.app)");
    launchAppButton_ = std::make_unique<QPushButton>();
    launchAppButton_->setText("Launch");
    stopAppButton_ = std::make_unique<QPushButton>();
    stopAppButton_->setText("Stop");
    
    // Connect signals
    connect(refreshAppsButton_.get(), &QPushButton::clicked,
            this, &AndroidTab::refreshApps);
    connect(launchAppButton_.get(), &QPushButton::clicked,
            this, &AndroidTab::launchApp);
    connect(stopAppButton_.get(), &QPushButton::clicked,
            this, &AndroidTab::stopApp);
    
    appControlLayout_->addWidget(refreshAppsButton_.get());
    appControlLayout_->addWidget(appEdit_.get());
    appControlLayout_->addWidget(launchAppButton_.get());
    appControlLayout_->addWidget(stopAppButton_.get());
    
    // App table
    appTable_ = std::make_unique<QTableWidget>();
    appTable_->setColumnCount(1);
    appTable_->setHorizontalHeaderLabels({"Package Name"});
    appTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    appTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    
    appLayout_->addLayout(appControlLayout_.release());
    appLayout_->addWidget(appTable_.get());
    
    appGroup_->setLayout(appLayout_.get());
}

void AndroidTab::setupSystemOperationsSection() {
    systemGroup_ = std::make_unique<QGroupBox>();
    systemGroup_->setTitle("System Operations");
    systemLayout_ = std::make_unique<QHBoxLayout>();
    
    screenshotButton_ = std::make_unique<QPushButton>();
    screenshotButton_->setText("Screenshot");
    orientationButton_ = std::make_unique<QPushButton>();
    orientationButton_->setText("Get Orientation");
    logcatButton_ = std::make_unique<QPushButton>();
    logcatButton_->setText("Get Logcat");
    
    // Connect signals
    connect(screenshotButton_.get(), &QPushButton::clicked,
            this, &AndroidTab::takeScreenshot);
    connect(orientationButton_.get(), &QPushButton::clicked,
            this, &AndroidTab::getOrientation);
    connect(logcatButton_.get(), &QPushButton::clicked,
            this, &AndroidTab::getLogcat);
    
    systemLayout_->addWidget(screenshotButton_.get());
    systemLayout_->addWidget(orientationButton_.get());
    systemLayout_->addWidget(logcatButton_.get());
    systemLayout_->addStretch();
    
    systemGroup_->setLayout(systemLayout_.get());
}

void AndroidTab::setupStatusBar() {
    auto statusLayout = std::make_unique<QHBoxLayout>();
    
    statusLabel_ = std::make_unique<QLabel>();
    statusLabel_->setText("Ready");
    deviceCountLabel_ = std::make_unique<QLabel>();
    deviceCountLabel_->setText("Devices: 0");
    
    statusLayout->addWidget(statusLabel_.get());
    statusLayout->addStretch();
    statusLayout->addWidget(deviceCountLabel_.get());
    
    mainLayout_->addLayout(statusLayout.release());
}

void AndroidTab::updateDeviceList(const std::vector<AndroidDeviceInfo>& devices) {
    deviceCombo_->clear();
    
    for (const auto& device : devices) {
        QString deviceText = QString("%1 (%2) - %3")
            .arg(QString::fromStdString(device.model))
            .arg(QString::fromStdString(device.serialNumber))
            .arg(QString::fromStdString(device.androidVersion));
        
        deviceCombo_->addItem(deviceText, QString::fromStdString(device.serialNumber));
    }
    
    deviceCountLabel_->setText(QString("Devices: %1").arg(devices.size()));
}

void AndroidTab::updateDeviceInfo(const AndroidDeviceInfo& device) {
    modelLabel_->setText(QString::fromStdString(device.model));
    versionLabel_->setText(QString::fromStdString(device.androidVersion));
    serialLabel_->setText(QString::fromStdString(device.serialNumber));
    
    batteryBar_->setValue(device.batteryLevel);
    batteryLabel_->setText(formatBatteryLevel(device.batteryLevel));
    
    screenLabel_->setText(formatBool(device.isScreenOn));
    lockLabel_->setText(formatBool(device.isLocked));
    foregroundLabel_->setText(QString::fromStdString(device.foregroundApp));
}

void AndroidTab::updateAppList(const std::vector<std::string>& apps) {
    appTable_->setRowCount(static_cast<int>(apps.size()));
    
    for (int row = 0; row < static_cast<int>(apps.size()); ++row) {
        appTable_->setItem(row, COLUMN_PACKAGE_NAME, new QTableWidgetItem(QString::fromStdString(apps[row])));
    }
}

AndroidDeviceInfo AndroidTab::getSelectedDevice() const {
    if (deviceCombo_->currentIndex() < 0 || deviceCombo_->currentIndex() >= static_cast<int>(currentDevices_.size())) {
        return AndroidDeviceInfo{};
    }
    
    return currentDevices_[deviceCombo_->currentIndex()];
}

bool AndroidTab::hasValidDeviceSelection() const {
    return deviceCombo_->currentIndex() >= 0 && deviceCombo_->currentIndex() < static_cast<int>(currentDevices_.size());
}

void AndroidTab::updateButtonStates() {
    bool hasDevice = hasValidDeviceSelection();
    
    // Enable/disable buttons based on device selection
    screenOnButton_->setEnabled(hasDevice);
    screenOffButton_->setEnabled(hasDevice);
    lockButton_->setEnabled(hasDevice);
    refreshInfoButton_->setEnabled(hasDevice);
    refreshAppsButton_->setEnabled(hasDevice);
    launchAppButton_->setEnabled(hasDevice);
    stopAppButton_->setEnabled(hasDevice);
    screenshotButton_->setEnabled(hasDevice);
    orientationButton_->setEnabled(hasDevice);
    logcatButton_->setEnabled(hasDevice);
}

void AndroidTab::showDeviceOperationResult(const QString& operation, bool success, const QString& message) {
    if (success) {
        statusLabel_->setText(QString("%1 successful").arg(operation));
        statusLabel_->setStyleSheet("color: green;");
    } else {
        statusLabel_->setText(QString("%1 failed: %2").arg(operation, message));
        statusLabel_->setStyleSheet("color: red;");
    }
    
    // Clear status after 5 seconds
    QTimer::singleShot(5000, [this]() {
        statusLabel_->setText("Ready");
        statusLabel_->setStyleSheet("");
    });
}

void AndroidTab::clearDeviceInfo() {
    modelLabel_->setText("No device selected");
    versionLabel_->setText("Unknown");
    serialLabel_->setText("Unknown");
    batteryBar_->setValue(0);
    batteryLabel_->setText("Unknown");
    screenLabel_->setText("Unknown");
    lockLabel_->setText("Unknown");
    foregroundLabel_->setText("Unknown");
}

void AndroidTab::clearAppList() {
    appTable_->setRowCount(0);
}

void AndroidTab::showScreenshot(const QString& imageData) {
    createScreenshotDialog();
    
    // Try to load and display screenshot image
    QPixmap pixmap;
    if (pixmap.load(imageData)) {
        screenshotLabel_->setPixmap(pixmap.scaled(400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        screenshotLabel_->setText(""); // Clear text
    } else {
        screenshotLabel_->setText("Screenshot captured: " + imageData + "\n(Unable to display image)");
    }
    
    screenshotDialog_->show();
}

void AndroidTab::showOrientation(const QString& orientation) {
    QMessageBox::information(this, "Screen Orientation", 
        "Current screen orientation: " + orientation);
}

void AndroidTab::showLogcat(const std::vector<std::string>& logcat) {
    createLogcatDialog();
    
    QString logcatText;
    for (const auto& line : logcat) {
        logcatText += QString::fromStdString(line) + "\n";
    }
    
    logcatEdit_->setText(logcatText);
    logcatDialog_->show();
}

void AndroidTab::createScreenshotDialog() {
    if (screenshotDialog_) {
        return;
    }
    
    screenshotDialog_ = std::make_unique<QDialog>(this);
    screenshotDialog_->setWindowTitle("Screenshot");
    screenshotDialog_->resize(400, 300);
    
    auto layout = std::make_unique<QVBoxLayout>(screenshotDialog_.get());
    
    screenshotLabel_ = std::make_unique<QLabel>();
    screenshotLabel_->setAlignment(Qt::AlignCenter);
    
    auto closeButton = std::make_unique<QPushButton>("Close");
    connect(closeButton.get(), &QPushButton::clicked,
            screenshotDialog_.get(), &QDialog::close);
    
    layout->addWidget(screenshotLabel_.get());
    layout->addWidget(closeButton.release());
}

void AndroidTab::createLogcatDialog() {
    if (logcatDialog_) {
        return;
    }
    
    logcatDialog_ = std::make_unique<QDialog>(this);
    logcatDialog_->setWindowTitle("Logcat");
    logcatDialog_->resize(600, 400);
    
    auto layout = std::make_unique<QVBoxLayout>(logcatDialog_.get());
    
    logcatEdit_ = std::make_unique<QTextEdit>();
    logcatEdit_->setReadOnly(true);
    
    auto closeButton = std::make_unique<QPushButton>("Close");
    connect(closeButton.get(), &QPushButton::clicked,
            logcatDialog_.get(), &QDialog::close);
    
    layout->addWidget(logcatEdit_.get());
    layout->addWidget(closeButton.release());
}

QString AndroidTab::formatBatteryLevel(int level) const {
    return QString("%1%").arg(level);
}

QString AndroidTab::formatBool(bool value) const {
    return value ? "Yes" : "No";
}

} // namespace SysMon
