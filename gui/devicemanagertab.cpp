#include "devicemanagertab.h"
#include "../shared/commands.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QShowEvent>
#include <QHideEvent>
#include <sstream>
#include <algorithm>
#include <QDebug>

namespace SysMon {

DeviceManagerTab::DeviceManagerTab(IpcClient* ipcClient, QWidget* parent)
    : QWidget(parent)
    , ipcClient_(ipcClient)
    , isActive_(false) {
    
    setupUI();
    
    // Create refresh timer
    refreshTimer_ = std::make_unique<QTimer>(this);
    connect(refreshTimer_.get(), &QTimer::timeout,
            this, &DeviceManagerTab::refreshDevices);
    
    // Connect IPC client signals
    connect(ipcClient_, &IpcClient::responseReceived,
            this, &DeviceManagerTab::onUsbDevicesResponse);
    connect(ipcClient_, &IpcClient::errorOccurred,
            this, &DeviceManagerTab::onError);
}

DeviceManagerTab::~DeviceManagerTab() {
    // Timer is automatically deleted by Qt
}

void DeviceManagerTab::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    
    if (!isActive_) {
        isActive_ = true;
        
        // Start timer
        refreshTimer_->start(REFRESH_INTERVAL);
        
        // Request initial data
        refreshDevices();
    }
}

void DeviceManagerTab::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    
    if (isActive_) {
        isActive_ = false;
        
        // Stop timer
        refreshTimer_->stop();
    }
}

void DeviceManagerTab::refreshDevices() {
    if (!ipcClient_ || !isActive_) {
        return;
    }
    
    // Create command to get USB devices
    Command command = createCommand(CommandType::GET_USB_DEVICES, Module::DEVICE);
    
    // Send command with response handler
    ipcClient_->sendCommand(command, [this](const Response& response) {
        onUsbDevicesResponse(response);
    });
}

void DeviceManagerTab::enableDevice() {
    if (!hasValidSelection()) {
        return;
    }
    
    UsbDevice device = getSelectedDevice();
    
    // Create command to enable device
    Command command = createCommand(CommandType::ENABLE_USB_DEVICE, Module::DEVICE);
    command.parameters["vid"] = device.vid;
    command.parameters["pid"] = device.pid;
    
    // Send command
    ipcClient_->sendCommand(command, [this](const Response& response) {
        onDeviceOperationResponse(response);
    });
}

void DeviceManagerTab::disableDevice() {
    if (!hasValidSelection()) {
        return;
    }
    
    UsbDevice device = getSelectedDevice();
    
    // Create command to disable device
    Command command = createCommand(CommandType::DISABLE_USB_DEVICE, Module::DEVICE);
    command.parameters["vid"] = device.vid;
    command.parameters["pid"] = device.pid;
    
    // Send command
    ipcClient_->sendCommand(command, [this](const Response& response) {
        onDeviceOperationResponse(response);
    });
}

void DeviceManagerTab::toggleAutoConnect() {
    if (!hasValidSelection()) {
        return;
    }
    
    UsbDevice device = getSelectedDevice();
    
    // Check current auto-connect status
    bool isPrevented = isDevicePrevented(device.vid, device.pid);
    
    // Create command to toggle auto-connect
    Command command = createCommand(CommandType::ENABLE_USB_DEVICE, Module::DEVICE);
    command.parameters["vid"] = device.vid;
    command.parameters["pid"] = device.pid;
    command.parameters["prevent_auto"] = isPrevented ? "false" : "true";
    
    // Send command
    ipcClient_->sendCommand(command, [this](const Response& response) {
        onDeviceOperationResponse(response);
    });
}

void DeviceManagerTab::onDeviceSelectionChanged() {
    updateButtonStates();
}

void DeviceManagerTab::onUsbDevicesResponse(const Response& response) {
    if (response.status != CommandStatus::SUCCESS) {
        onError(QString("Failed to get USB devices: %1").arg(QString::fromStdString(response.message)));
        return;
    }
    
    // Parse USB devices from response data
    std::vector<UsbDevice> devices;
    
    try {
        // Expect device data in format: "vid1,pid1,name1,serial1,status1,enabled1;vid2,pid2,name2,serial2,status2,enabled2;..."
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
                
                if (fields.size() >= 6) {
                    UsbDevice device;
                    device.vid = fields[0];
                    device.pid = fields[1];
                    device.name = fields[2];
                    device.serialNumber = fields[3];
                    device.isConnected = (fields[4] == "1" || fields[4] == "true");
                    device.isEnabled = (fields[5] == "1" || fields[5] == "true");
                    
                    devices.push_back(device);
                }
            }
        }
    } catch (const std::exception& e) {
        onError(QString("Failed to parse device data: %1").arg(e.what()));
        return;
    }
    
    // If no devices from IPC, use placeholder data for development
    if (devices.empty()) {
        UsbDevice device;
        device.vid = "1234";
        device.pid = "5678";
        device.name = "Example USB Device";
        device.serialNumber = "ABC123";
        device.isConnected = true;
        device.isEnabled = true;
        devices.push_back(device);
        
        device.vid = "ABCD";
        device.pid = "EF12";
        device.name = "Another USB Device";
        device.serialNumber = "DEF456";
        device.isConnected = true;
        device.isEnabled = false;
        devices.push_back(device);
    }
    
    currentDevices_ = devices;
    
    // Update display
    updateDeviceTable(devices);
    updateButtonStates();
}

void DeviceManagerTab::onDeviceOperationResponse(const Response& response) {
    QString operation = "Unknown operation";
    
    if (response.status == CommandStatus::SUCCESS) {
        operation = QString::fromStdString(response.message);
        showDeviceOperationResult(operation, true);
        
        // Refresh device list
        refreshDevices();
    } else {
        showDeviceOperationResult(operation, false, QString::fromStdString(response.message));
    }
}

void DeviceManagerTab::onError(const QString& error) {
    qWarning() << "DeviceManagerTab error:" << error;
}

void DeviceManagerTab::setupUI() {
    mainLayout_ = std::make_unique<QVBoxLayout>();
    
    setupDeviceTable();
    setupControlButtons();
    setupStatusBar();
    
    mainLayout_->addWidget(deviceGroup_.get());
    mainLayout_->addWidget(controlGroup_.get());
    mainLayout_->addStretch();
    
    setLayout(mainLayout_.get());
}

void DeviceManagerTab::setupDeviceTable() {
    deviceGroup_ = std::make_unique<QGroupBox("USB Devices");
    deviceLayout_ = std::make_unique<QVBoxLayout>();
    
    deviceTable_ = std::make_unique<QTableWidget>();
    setupDeviceTableHeaders();
    
    // Connect selection change signal
    connect(deviceTable_.get(), &QTableWidget::itemSelectionChanged,
            this, &DeviceManagerTab::onDeviceSelectionChanged);
    
    deviceLayout_->addWidget(deviceTable_.get());
    deviceGroup_->setLayout(deviceLayout_.get());
}

void DeviceManagerTab::setupControlButtons() {
    controlGroup_ = std::make_unique<QGroupBox("Device Control");
    controlLayout_ = std::make_unique<QHBoxLayout>();
    
    refreshButton_ = std::make_unique<QPushButton("Refresh");
    connect(refreshButton_.get(), &QPushButton::clicked,
            this, &DeviceManagerTab::refreshDevices);
    
    enableButton_ = std::make_unique<QPushButton(ENABLE_TEXT);
    connect(enableButton_.get(), &QPushButton::clicked,
            this, &DeviceManagerTab::enableDevice);
    
    disableButton_ = std::make_unique<QPushButton(DISABLE_TEXT);
    connect(disableButton_.get(), &QPushButton::clicked,
            this, &DeviceManagerTab::disableDevice);
    
    autoConnectButton_ = std::make_unique<QPushButton(PREVENT_AUTO_TEXT);
    connect(autoConnectButton_.get(), &QPushButton::clicked,
            this, &DeviceManagerTab::toggleAutoConnect);
    
    controlLayout_->addWidget(refreshButton_.get());
    controlLayout_->addWidget(enableButton_.get());
    controlLayout_->addWidget(disableButton_.get());
    controlLayout_->addWidget(autoConnectButton_.get());
    controlLayout_->addStretch();
    
    controlGroup_->setLayout(controlLayout_.get());
}

void DeviceManagerTab::setupStatusBar() {
    // Create status bar at bottom of tab
    auto statusLayout = std::make_unique<QHBoxLayout>();
    
    statusLabel_ = std::make_unique<QLabel("Ready");
    deviceCountLabel_ = std::make_unique<QLabel("Devices: 0");
    
    statusLayout->addWidget(statusLabel_.get());
    statusLayout->addStretch();
    statusLayout->addWidget(deviceCountLabel_.get());
    
    mainLayout_->addLayout(statusLayout.release());
}

void DeviceManagerTab::setupDeviceTableHeaders() {
    QStringList headers;
    headers << "VID" << "PID" << "Name" << "Serial" << "Status" << "Auto-Connect";
    
    deviceTable_->setColumnCount(headers.size());
    deviceTable_->setHorizontalHeaderLabels(headers);
    deviceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    deviceTable_->setSortingEnabled(true);
    
    // Adjust column widths
    deviceTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // VID
    deviceTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents); // PID
    deviceTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch); // Name
    deviceTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents); // Serial
    deviceTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents); // Status
    deviceTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents); // Auto-Connect
}

void DeviceManagerTab::updateDeviceTable(const std::vector<UsbDevice>& devices) {
    deviceTable_->setRowCount(static_cast<int>(devices.size()));
    
    for (int row = 0; row < static_cast<int>(devices.size()); ++row) {
        const UsbDevice& device = devices[row];
        
        deviceTable_->setItem(row, COLUMN_VID, new QTableWidgetItem(QString::fromStdString(device.vid)));
        deviceTable_->setItem(row, COLUMN_PID, new QTableWidgetItem(QString::fromStdString(device.pid)));
        deviceTable_->setItem(row, COLUMN_NAME, new QTableWidgetItem(QString::fromStdString(device.name)));
        deviceTable_->setItem(row, COLUMN_SERIAL, new QTableWidgetItem(QString::fromStdString(device.serialNumber)));
        
        QString status = device.isConnected ? (device.isEnabled ? "Connected" : "Disabled") : "Disconnected";
        deviceTable_->setItem(row, COLUMN_STATUS, new QTableWidgetItem(status));
        
        QString autoConnect = isDevicePrevented(device.vid, device.pid) ? "Prevented" : "Allowed";
        deviceTable_->setItem(row, COLUMN_AUTO_CONNECT, new QTableWidgetItem(autoConnect));
    }
    
    deviceCountLabel_->setText(QString("Devices: %1").arg(devices.size()));
}

void DeviceManagerTab::addDeviceToTable(const UsbDevice& device, int row) {
    deviceTable_->setItem(row, COLUMN_VID, new QTableWidgetItem(QString::fromStdString(device.vid)));
    deviceTable_->setItem(row, COLUMN_PID, new QTableWidgetItem(QString::fromStdString(device.pid)));
    deviceTable_->setItem(row, COLUMN_NAME, new QTableWidgetItem(QString::fromStdString(device.name)));
    deviceTable_->setItem(row, COLUMN_SERIAL, new QTableWidgetItem(QString::fromStdString(device.serialNumber)));
    
    QString status = device.isConnected ? (device.isEnabled ? "Connected" : "Disabled") : "Disconnected";
    deviceTable_->setItem(row, COLUMN_STATUS, new QTableWidgetItem(status));
    
    QString autoConnect = isDevicePrevented(device.vid, device.pid) ? "Prevented" : "Allowed";
    deviceTable_->setItem(row, COLUMN_AUTO_CONNECT, new QTableWidgetItem(autoConnect));
}

bool DeviceManagerTab::isDevicePrevented(const std::string& vid, const std::string& pid) const {
    // This would normally check with the agent for prevented devices
    // For now, return false (allowed)
    return false;
}

void DeviceManagerTab::clearDeviceTable() {
    deviceTable_->setRowCount(0);
}

UsbDevice DeviceManagerTab::getSelectedDevice() const {
    QList<QTableWidgetItem*> selectedItems = deviceTable_->selectedItems();
    if (selectedItems.isEmpty()) {
        return UsbDevice{};
    }
    
    int row = selectedItems.first()->row();
    if (row >= 0 && row < static_cast<int>(currentDevices_.size())) {
        return currentDevices_[row];
    }
    
    return UsbDevice{};
}

bool DeviceManagerTab::hasValidSelection() const {
    return deviceTable_->currentRow() >= 0 && deviceTable_->currentRow() < static_cast<int>(currentDevices_.size());
}

void DeviceManagerTab::updateButtonStates() {
    bool hasSelection = hasValidSelection();
    
    enableButton_->setEnabled(hasSelection);
    disableButton_->setEnabled(hasSelection);
    autoConnectButton_->setEnabled(hasSelection);
    
    if (hasSelection) {
        UsbDevice device = getSelectedDevice();
        
        // Update button text based on device state
        if (device.isEnabled) {
            disableButton_->setText(DISABLE_TEXT);
            enableButton_->setEnabled(false);
        } else {
            enableButton_->setText(ENABLE_TEXT);
            disableButton_->setEnabled(false);
        }
        
        // Update auto-connect button text
        bool isPrevented = isDevicePrevented(device.vid, device.pid);
        if (isPrevented) {
            autoConnectButton_->setText(ALLOW_AUTO_TEXT);
        } else {
            autoConnectButton_->setText(PREVENT_AUTO_TEXT);
        }
    }
}

void DeviceManagerTab::showDeviceOperationResult(const QString& operation, bool success, const QString& message) {
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

} // namespace SysMon

#include "devicemanagertab.moc"
