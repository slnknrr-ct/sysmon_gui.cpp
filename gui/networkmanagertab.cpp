#include "networkmanagertab.h"
#include "../shared/commands.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QGroupBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QShowEvent>
#include <QHideEvent>
#include <sstream>
#include <algorithm>
#include <QDebug>

namespace SysMon {

NetworkManagerTab::NetworkManagerTab(IpcClient* ipcClient, QWidget* parent)
    : QWidget(parent)
    , ipcClient_(ipcClient)
    , isActive_(false) {
    
    setupUI();
    
    // Create refresh timer
    refreshTimer_ = std::make_unique<QTimer>(this);
    connect(refreshTimer_.get(), &QTimer::timeout,
            this, &NetworkManagerTab::refreshInterfaces);
    
    // Connect IPC client signals
    connect(ipcClient_, &IpcClient::responseReceived,
            this, &NetworkManagerTab::onNetworkInterfacesResponse);
    connect(ipcClient_, &IpcClient::errorOccurred,
            this, &NetworkManagerTab::onError);
}

NetworkManagerTab::~NetworkManagerTab() {
    // Timer is automatically deleted by Qt
}

void NetworkManagerTab::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    
    if (!isActive_) {
        isActive_ = true;
        
        // Start timer
        refreshTimer_->start(REFRESH_INTERVAL);
        
        // Request initial data
        refreshInterfaces();
    }
}

void NetworkManagerTab::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    
    if (isActive_) {
        isActive_ = false;
        
        // Stop timer
        refreshTimer_->stop();
    }
}

void NetworkManagerTab::refreshInterfaces() {
    if (!ipcClient_ || !isActive_) {
        return;
    }
    
    // Create command to get network interfaces
    Command command = createCommand(CommandType::GET_NETWORK_INTERFACES, Module::NETWORK);
    
    // Send command with response handler
    ipcClient_->sendCommand(command, [this](const Response& response) {
        onNetworkInterfacesResponse(response);
    });
}

void NetworkManagerTab::enableInterface() {
    if (!hasValidSelection()) {
        return;
    }
    
    NetworkInterface interface = getSelectedInterface();
    
    // Create command to enable interface
    Command command = createCommand(CommandType::ENABLE_NETWORK_INTERFACE, Module::NETWORK);
    command.parameters["interface"] = interface.name;
    
    // Send command
    ipcClient_->sendCommand(command, [this](const Response& response) {
        onInterfaceOperationResponse(response);
    });
}

void NetworkManagerTab::disableInterface() {
    if (!hasValidSelection()) {
        return;
    }
    
    NetworkInterface interface = getSelectedInterface();
    
    // Create command to disable interface
    Command command = createCommand(CommandType::DISABLE_NETWORK_INTERFACE, Module::NETWORK);
    command.parameters["interface"] = interface.name;
    
    // Send command
    ipcClient_->sendCommand(command, [this](const Response& response) {
        onInterfaceOperationResponse(response);
    });
}

void NetworkManagerTab::setStaticIp() {
    if (!hasValidSelection()) {
        return;
    }
    
    showStaticIpDialog();
}

void NetworkManagerTab::setDhcpIp() {
    if (!hasValidSelection()) {
        return;
    }
    
    NetworkInterface interface = getSelectedInterface();
    
    // Create command to set DHCP
    Command command = createCommand(CommandType::SET_DHCP_IP, Module::NETWORK);
    command.parameters["interface"] = interface.name;
    
    // Send command
    ipcClient_->sendCommand(command, [this](const Response& response) {
        onInterfaceOperationResponse(response);
    });
}

void NetworkManagerTab::onInterfaceSelectionChanged() {
    updateButtonStates();
}

void NetworkManagerTab::onNetworkInterfacesResponse(const Response& response) {
    if (response.status != CommandStatus::SUCCESS) {
        onError(QString("Failed to get network interfaces: %1").arg(QString::fromStdString(response.message)));
        return;
    }
    
    // Parse network interfaces from response data
    std::vector<NetworkInterface> interfaces;
    
    try {
        // Expect interface data in format: "name1,ipv4_1,ipv6_1,status1,rx1,tx1;name2,ipv4_2,ipv6_2,status2,rx2,tx2;..."
        std::string interfaceData = response.data.count("interfaces") ? response.data.at("interfaces") : "";
        
        if (!interfaceData.empty()) {
            std::stringstream ss(interfaceData);
            std::string interfaceEntry;
            
            while (std::getline(ss, interfaceEntry, ';')) {
                std::stringstream entryStream(interfaceEntry);
                std::string field;
                std::vector<std::string> fields;
                
                while (std::getline(entryStream, field, ',')) {
                    fields.push_back(field);
                }
                
                if (fields.size() >= 6) {
                    NetworkInterface interface;
                    interface.name = fields[0];
                    interface.ipv4 = fields[1];
                    interface.ipv6 = fields[2];
                    interface.isEnabled = (fields[3] == "1" || fields[3] == "true");
                    interface.rxBytes = std::stoull(fields[4]);
                    interface.txBytes = std::stoull(fields[5]);
                    
                    // Calculate speeds (will be updated by monitoring thread)
                    interface.rxSpeed = 0.0;
                    interface.txSpeed = 0.0;
                    
                    interfaces.push_back(interface);
                }
            }
        }
    } catch (const std::exception& e) {
        onError(QString("Failed to parse interface data: %1").arg(e.what()));
        return;
    }
    
    // If no interfaces from IPC, use placeholder data for development
    if (interfaces.empty()) {
        NetworkInterface interface;
        interface.name = "eth0";
        interface.ipv4 = "192.168.1.100";
        interface.ipv6 = "fe80::1";
        interface.isEnabled = true;
        interface.rxBytes = 1024 * 1024;
        interface.txBytes = 512 * 1024;
        interface.rxSpeed = 1024.0;
        interface.txSpeed = 512.0;
        interfaces.push_back(interface);
        
        interface.name = "wlan0";
        interface.ipv4 = "192.168.1.101";
        interface.ipv6 = "fe80::2";
        interface.isEnabled = false;
        interface.rxBytes = 2048 * 1024;
        interface.txBytes = 1024 * 1024;
        interface.rxSpeed = 2048.0;
        interface.txSpeed = 1024.0;
        interfaces.push_back(interface);
    }
    
    currentInterfaces_ = interfaces;
    
    // Update display
    updateInterfaceTable(interfaces);
    updateButtonStates();
}

void NetworkManagerTab::onInterfaceOperationResponse(const Response& response) {
    QString operation = "Unknown operation";
    
    if (response.status == CommandStatus::SUCCESS) {
        operation = QString::fromStdString(response.message);
        showInterfaceOperationResult(operation, true);
        
        // Refresh interface list
        refreshInterfaces();
    } else {
        showInterfaceOperationResult(operation, false, QString::fromStdString(response.message));
    }
}

void NetworkManagerTab::onError(const QString& error) {
    qWarning() << "NetworkManagerTab error:" << error;
}

void NetworkManagerTab::setupUI() {
    mainLayout_ = std::make_unique<QVBoxLayout>();
    
    setupInterfaceTable();
    setupControlButtons();
    setupStatusBar();
    
    mainLayout_->addWidget(interfaceGroup_.get());
    mainLayout_->addWidget(controlGroup_.get());
    mainLayout_->addStretch();
    
    setLayout(mainLayout_.get());
}

void NetworkManagerTab::setupInterfaceTable() {
    interfaceGroup_ = std::make_unique<QGroupBox("Network Interfaces");
    interfaceLayout_ = std::make_unique<QVBoxLayout>();
    
    interfaceTable_ = std::make_unique<QTableWidget>();
    setupInterfaceTableHeaders();
    
    // Connect selection change signal
    connect(interfaceTable_.get(), &QTableWidget::itemSelectionChanged,
            this, &NetworkManagerTab::onInterfaceSelectionChanged);
    
    interfaceLayout_->addWidget(interfaceTable_.get());
    interfaceGroup_->setLayout(interfaceLayout_.get());
}

void NetworkManagerTab::setupControlButtons() {
    controlGroup_ = std::make_unique<QGroupBox("Interface Control");
    controlLayout_ = std::make_unique<QHBoxLayout>();
    
    refreshButton_ = std::make_unique<QPushButton("Refresh");
    connect(refreshButton_.get(), &QPushButton::clicked,
            this, &NetworkManagerTab::refreshInterfaces);
    
    enableButton_ = std::make_unique<QPushButton(ENABLE_TEXT);
    connect(enableButton_.get(), &QPushButton::clicked,
            this, &NetworkManagerTab::enableInterface);
    
    disableButton_ = std::make_unique<QPushButton(DISABLE_TEXT);
    connect(disableButton_.get(), &QPushButton::clicked,
            this, &NetworkManagerTab::disableInterface);
    
    staticIpButton_ = std::make_unique<QPushButton(STATIC_IP_TEXT);
    connect(staticIpButton_.get(), &QPushButton::clicked,
            this, &NetworkManagerTab::setStaticIp);
    
    dhcpButton_ = std::make_unique<QPushButton(DHCP_IP_TEXT);
    connect(dhcpButton_.get(), &QPushButton::clicked,
            this, &NetworkManagerTab::setDhcpIp);
    
    controlLayout_->addWidget(refreshButton_.get());
    controlLayout_->addWidget(enableButton_.get());
    controlLayout_->addWidget(disableButton_.get());
    controlLayout_->addWidget(staticIpButton_.get());
    controlLayout_->addWidget(dhcpButton_.get());
    controlLayout_->addStretch();
    
    controlGroup_->setLayout(controlLayout_.get());
}

void NetworkManagerTab::setupStatusBar() {
    auto statusLayout = std::make_unique<QHBoxLayout>();
    
    statusLabel_ = std::make_unique<QLabel("Ready");
    interfaceCountLabel_ = std::make_unique<QLabel("Interfaces: 0");
    
    statusLayout->addWidget(statusLabel_.get());
    statusLayout->addStretch();
    statusLayout->addWidget(interfaceCountLabel_.get());
    
    mainLayout_->addLayout(statusLayout.release());
}

void NetworkManagerTab::setupInterfaceTableHeaders() {
    QStringList headers;
    headers << "Name" << "IPv4" << "IPv6" << "Status" << "Rx Speed" << "Tx Speed";
    
    interfaceTable_->setColumnCount(headers.size());
    interfaceTable_->setHorizontalHeaderLabels(headers);
    interfaceTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    interfaceTable_->setSortingEnabled(true);
    
    // Adjust column widths
    interfaceTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // Name
    interfaceTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents); // IPv4
    interfaceTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents); // IPv6
    interfaceTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents); // Status
    interfaceTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents); // Rx Speed
    interfaceTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents); // Tx Speed
}

void NetworkManagerTab::updateInterfaceTable(const std::vector<NetworkInterface>& interfaces) {
    interfaceTable_->setRowCount(static_cast<int>(interfaces.size()));
    
    for (int row = 0; row < static_cast<int>(interfaces.size()); ++row) {
        addInterfaceToTable(interfaces[row], row);
    }
    
    interfaceCountLabel_->setText(QString("Interfaces: %1").arg(interfaces.size()));
}

QString NetworkManagerTab::formatSpeed(double speed) const {
    if (speed >= 1024.0 * 1024.0) {
        return QString("%1 GB/s").arg(speed / (1024.0 * 1024.0), 0, 'f', 2);
    } else if (speed >= 1024.0) {
        return QString("%1 MB/s").arg(speed / 1024.0, 0, 'f', 2);
    } else {
        return QString("%1 KB/s").arg(speed, 0, 'f', 1);
    }
}

void NetworkManagerTab::addInterfaceToTable(const NetworkInterface& interface, int row) {
    interfaceTable_->setItem(row, COLUMN_NAME, new QTableWidgetItem(QString::fromStdString(interface.name)));
    interfaceTable_->setItem(row, COLUMN_IPV4, new QTableWidgetItem(QString::fromStdString(interface.ipv4)));
    interfaceTable_->setItem(row, COLUMN_IPV6, new QTableWidgetItem(QString::fromStdString(interface.ipv6)));
    
    QString status = interface.isEnabled ? "Enabled" : "Disabled";
    interfaceTable_->setItem(row, COLUMN_STATUS, new QTableWidgetItem(status));
    
    QString rxSpeed = formatSpeed(interface.rxSpeed);
    interfaceTable_->setItem(row, COLUMN_RX_SPEED, new QTableWidgetItem(rxSpeed));
    
    QString txSpeed = formatSpeed(interface.txSpeed);
    interfaceTable_->setItem(row, COLUMN_TX_SPEED, new QTableWidgetItem(txSpeed));
}

void NetworkManagerTab::clearInterfaceTable() {
    interfaceTable_->setRowCount(0);
}

NetworkInterface NetworkManagerTab::getSelectedInterface() const {
    QList<QTableWidgetItem*> selectedItems = interfaceTable_->selectedItems();
    if (selectedItems.isEmpty()) {
        return NetworkInterface{};
    }
    
    int row = selectedItems.first()->row();
    if (row >= 0 && row < static_cast<int>(currentInterfaces_.size())) {
        return currentInterfaces_[row];
    }
    
    return NetworkInterface{};
}

bool NetworkManagerTab::hasValidSelection() const {
    return interfaceTable_->currentRow() >= 0 && interfaceTable_->currentRow() < static_cast<int>(currentInterfaces_.size());
}

void NetworkManagerTab::updateButtonStates() {
    bool hasSelection = hasValidSelection();
    
    enableButton_->setEnabled(hasSelection);
    disableButton_->setEnabled(hasSelection);
    staticIpButton_->setEnabled(hasSelection);
    dhcpButton_->setEnabled(hasSelection);
    
    if (hasSelection) {
        NetworkInterface interface = getSelectedInterface();
        
        // Update button text based on interface state
        if (interface.isEnabled) {
            disableButton_->setText(DISABLE_TEXT);
            enableButton_->setEnabled(false);
        } else {
            enableButton_->setText(ENABLE_TEXT);
            disableButton_->setEnabled(false);
        }
    }
}

void NetworkManagerTab::showInterfaceOperationResult(const QString& operation, bool success, const QString& message) {
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

void NetworkManagerTab::showStaticIpDialog() {
    createStaticIpDialog();
    
    // Reset dialog fields
    resetStaticIpDialog();
    
    // Show dialog
    if (staticIpDialog_->exec() == QDialog::Accepted) {
        QString ip = ipEdit_->text();
        QString netmask = netmaskEdit_->text();
        QString gateway = gatewayEdit_->text();
        
        setStaticIpForSelectedInterface(ip, netmask, gateway);
    }
}

void NetworkManagerTab::createStaticIpDialog() {
    if (staticIpDialog_) {
        return;
    }
    
    staticIpDialog_ = std::make_unique<QDialog>(this);
    staticIpDialog_->setWindowTitle("Set Static IP");
    
    dialogLayout_ = std::make_unique<QFormLayout>(staticIpDialog_.get());
    
    ipEdit_ = std::make_unique<QLineEdit>();
    netmaskEdit_ = std::make_unique<QLineEdit>();
    gatewayEdit_ = std::make_unique<QLineEdit>();
    
    dialogLayout_->addRow("IP Address:", ipEdit_.get());
    dialogLayout_->addRow("Netmask:", netmaskEdit_.get());
    dialogLayout_->addRow("Gateway:", gatewayEdit_.get());
    
    dialogButtons_ = std::make_unique<QDialogButtonBox>(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, staticIpDialog_.get());
    
    dialogLayout_->addRow(dialogButtons_.get());
    
    connect(dialogButtons_.get(), &QDialogButtonBox::accepted,
            staticIpDialog_.get(), &QDialog::accept);
    connect(dialogButtons_.get(), &QDialogButtonBox::rejected,
            staticIpDialog_.get(), &QDialog::reject);
}

void NetworkManagerTab::resetStaticIpDialog() {
    ipEdit_->clear();
    netmaskEdit_->clear();
    gatewayEdit_->clear();
}

void NetworkManagerTab::setStaticIpForSelectedInterface(const QString& ip, const QString& netmask, const QString& gateway) {
    NetworkInterface interface = getSelectedInterface();
    
    // Create command to set static IP
    Command command = createCommand(CommandType::SET_STATIC_IP, Module::NETWORK);
    command.parameters["interface"] = interface.name;
    command.parameters["ip"] = ip.toStdString();
    command.parameters["netmask"] = netmask.toStdString();
    command.parameters["gateway"] = gateway.toStdString();
    
    // Send command
    ipcClient_->sendCommand(command, [this](const Response& response) {
        onInterfaceOperationResponse(response);
    });
}

} // namespace SysMon

#include "networkmanagertab.moc"
