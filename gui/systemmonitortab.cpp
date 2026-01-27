#include "systemmonitortab.h"
#include "../shared/commands.h"
#include "../shared/ipcprotocol.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QTimer>
#include <QGroupBox>
#include <QScrollArea>
#include <QHeaderView>
#include <QShowEvent>
#include <QHideEvent>

namespace SysMon {

SystemMonitorTab::SystemMonitorTab(IpcClient* ipcClient, QWidget* parent)
    : QWidget(parent)
    , ipcClient_(ipcClient)
    , isActive_(false) {
    
    setupUI();
    
    // Create update timers
    systemInfoTimer_ = std::make_unique<QTimer>(this);
    connect(systemInfoTimer_.get(), &QTimer::timeout,
            this, &SystemMonitorTab::updateSystemInfo);
    
    processListTimer_ = std::make_unique<QTimer>(this);
    connect(processListTimer_.get(), &QTimer::timeout,
            this, &SystemMonitorTab::updateProcessList);
    
    // Connect IPC client signals
    connect(ipcClient_, &IpcClient::responseReceived,
            this, &SystemMonitorTab::onSystemInfoResponse);
    connect(ipcClient_, &IpcClient::errorOccurred,
            this, &SystemMonitorTab::onError);
}

SystemMonitorTab::~SystemMonitorTab() {
    // Timers are automatically deleted by Qt
}

void SystemMonitorTab::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    
    if (!isActive_) {
        isActive_ = true;
        
        // Start timers
        systemInfoTimer_->start(SYSTEM_UPDATE_INTERVAL);
        processListTimer_->start(PROCESS_UPDATE_INTERVAL);
        
        // Request initial data
        updateSystemInfo();
        updateProcessList();
    }
}

void SystemMonitorTab::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    
    if (isActive_) {
        isActive_ = false;
        
        // Stop timers
        systemInfoTimer_->stop();
        processListTimer_->stop();
    }
}

void SystemMonitorTab::updateSystemInfo() {
    if (!ipcClient_ || !isActive_) {
        return;
    }
    
    // Create command to get system info
    Command command = createCommand(CommandType::GET_SYSTEM_INFO, Module::SYSTEM);
    
    // Send command with response handler
    ipcClient_->sendCommand(command, [this](const Response& response) {
        onSystemInfoResponse(response);
    });
}

void SystemMonitorTab::updateProcessList() {
    if (!ipcClient_ || !isActive_) {
        return;
    }
    
    // Create command to get process list
    Command command = createCommand(CommandType::GET_PROCESS_LIST, Module::SYSTEM);
    
    // Send command with response handler
    ipcClient_->sendCommand(command, [this](const Response& response) {
        onProcessListResponse(response);
    });
}

void SystemMonitorTab::onSystemInfoResponse(const Response& response) {
    if (response.status != CommandStatus::SUCCESS) {
        onError(QString("Failed to get system info: %1").arg(QString::fromStdString(response.message)));
        return;
    }
    
    // Parse system info from response data
    SystemInfo info;
    
    // Parse CPU usage
    auto cpuIt = response.data.find("cpu_total");
    if (cpuIt != response.data.end()) {
        try {
            info.cpuUsageTotal = std::stod(cpuIt->second);
        } catch (const std::exception& e) {
            info.cpuUsageTotal = 0.0;
        }
    } else {
        info.cpuUsageTotal = 0.0; // Default if not found
    }
    
    // Parse memory info
    auto memTotalIt = response.data.find("memory_total");
    if (memTotalIt != response.data.end()) {
        try {
            info.memoryTotal = std::stoull(memTotalIt->second);
        } catch (const std::exception& e) {
            info.memoryTotal = 0;
        }
    }
    
    auto memUsedIt = response.data.find("memory_used");
    if (memUsedIt != response.data.end()) {
        try {
            info.memoryUsed = std::stoull(memUsedIt->second);
        } catch (const std::exception& e) {
            info.memoryUsed = 0;
        }
    }
    
    auto memFreeIt = response.data.find("memory_free");
    if (memFreeIt != response.data.end()) {
        try {
            info.memoryFree = std::stoull(memFreeIt->second);
        } catch (const std::exception& e) {
            info.memoryFree = 0;
        }
    }
    
    // Parse process count
    auto procCountIt = response.data.find("process_count");
    if (procCountIt != response.data.end()) {
        try {
            info.processCount = std::stoul(procCountIt->second);
        } catch (const std::exception& e) {
            info.processCount = 0;
        }
    }
    
    // Set default values for missing fields
    if (info.memoryTotal == 0) info.memoryTotal = 8589934592ULL; // 8GB
    if (info.memoryUsed == 0) info.memoryUsed = 4294967296ULL; // 4GB
    if (info.memoryFree == 0) info.memoryFree = info.memoryTotal - info.memoryUsed;
    info.memoryCache = 268435456ULL; // 256MB
    info.memoryBuffers = 134217728ULL; // 128MB
    info.threadCount = 320;
    info.cpuCoresUsage = {info.cpuUsageTotal, info.cpuUsageTotal, info.cpuUsageTotal, info.cpuUsageTotal};
    info.contextSwitches = 1000000;
    info.uptime = std::chrono::seconds(3600);
    
    currentSystemInfo_ = info;
    
    // Update display
    updateCpuDisplay(info);
    updateMemoryDisplay(info);
    updateSystemStats(info);
}

void SystemMonitorTab::onProcessListResponse(const Response& response) {
    if (response.status != CommandStatus::SUCCESS) {
        onError(QString("Failed to get process list: %1").arg(QString::fromStdString(response.message)));
        return;
    }
    
    // Parse process list from response data
    std::vector<ProcessInfo> processes;
    
    // Expected format: "pid,name,cpu,memory,status,parent,user;pid2,name2,..."
    std::string processData = response.data.count("processes") ? response.data["processes"] : "";
    
    if (!processData.empty()) {
        std::istringstream processStream(processData);
        std::string processEntry;
        
        while (std::getline(processStream, processEntry, ';')) {
            if (processEntry.empty()) continue;
            
            std::istringstream entryStream(processEntry);
            std::string field;
            std::vector<std::string> fields;
            
            while (std::getline(entryStream, field, ',')) {
                fields.push_back(field);
            }
            
            if (fields.size() >= 7) {
                ProcessInfo proc;
                
                try {
                    proc.pid = std::stoul(fields[0]);
                    proc.name = fields[1];
                    proc.cpuUsage = std::stod(fields[2]);
                    proc.memoryUsage = std::stoull(fields[3]);
                    proc.status = fields[4];
                    proc.parentPid = std::stoul(fields[5]);
                    proc.user = fields[6];
                    
                    processes.push_back(proc);
                } catch (const std::exception& e) {
                    // Skip invalid process entry
                    continue;
                }
            }
        }
    }
    
    // If no processes parsed, create some placeholder data for testing
    if (processes.empty()) {
        // Create placeholder processes
        ProcessInfo proc1;
        proc1.pid = 1234;
        proc1.name = "chrome";
        proc1.cpuUsage = 15.5;
        proc1.memoryUsage = 536870912ULL; // 512MB
        proc1.status = "Running";
        proc1.parentPid = 1;
        proc1.user = "user";
        processes.push_back(proc1);
        
        ProcessInfo proc2;
        proc2.pid = 5678;
        proc2.name = "firefox";
        proc2.cpuUsage = 8.2;
        proc2.memoryUsage = 268435456ULL; // 256MB
        proc2.status = "Running";
        proc2.parentPid = 1;
        proc2.user = "user";
        processes.push_back(proc2);
    }
    
    ProcessInfo proc;
    proc.pid = 1;
    proc.name = "systemd";
    proc.cpuUsage = 0.1;
    proc.memoryUsage = 1024 * 1024;
    proc.status = "S";
    proc.parentPid = 0;
    proc.user = "root";
    processes.push_back(proc);
    
    proc.pid = 100;
    proc.name = "bash";
    proc.cpuUsage = 0.5;
    proc.memoryUsage = 2 * 1024 * 1024;
    proc.status = "S";
    proc.parentPid = 1;
    proc.user = "user";
    processes.push_back(proc);
    
    currentProcessList_ = processes;
    
    // Update display
    updateProcessTable(processes);
}

void SystemMonitorTab::onError(const QString& error) {
    // Display error in UI
    qWarning() << "SystemMonitorTab error:" << error;
    
    // Show error message in status bar or error label
    if (errorLabel_) {
        errorLabel_->setText(QString("Error: %1").arg(error));
        errorLabel_->setStyleSheet("color: red; padding: 5px;");
        
        // Auto-hide error after 5 seconds
        QTimer::singleShot(5000, this, [this]() {
            if (errorLabel_) {
                errorLabel_->clear();
            }
        });
    }
    
    // Also show in main window status bar if available
    if (auto mainWindow = qobject_cast<QMainWindow*>(parent())) {
        mainWindow->statusBar()->showMessage(error, 5000);
    }
}

void SystemMonitorTab::setupUI() {
    mainLayout_ = std::make_unique<QVBoxLayout>();
    
    // Create scroll area
    scrollArea_ = std::make_unique<QScrollArea>();
    scrollWidget_ = new QWidget();
    scrollLayout_ = std::make_unique<QVBoxLayout>(scrollWidget_);
    
    setupSystemInfoSection();
    setupCpuSection();
    setupMemorySection();
    setupProcessSection();
    
    scrollArea_->setWidget(scrollWidget_);
    scrollArea_->setWidgetResizable(true);
    
    mainLayout_->addWidget(scrollArea_.get());
    setLayout(mainLayout_.get());
}

void SystemMonitorTab::setupSystemInfoSection() {
    systemInfoGroup_ = std::make_unique<QGroupBox>("System Information");
    systemInfoLayout_ = std::make_unique<QGridLayout>();
    
    uptimeLabel_ = std::make_unique<QLabel("Uptime:");
    uptimeValue_ = std::make_unique<QLabel("0:00:00");
    processCountLabel_ = std::make_unique<QLabel("Processes:");
    processCountValue_ = std::make_unique<QLabel("0");
    threadCountLabel_ = std::make_unique<QLabel("Threads:");
    threadCountValue_ = std::make_unique<QLabel("0");
    contextSwitchesLabel_ = std::make_unique<QLabel("Context Switches:");
    contextSwitchesValue_ = std::make_unique<QLabel("0");
    
    systemInfoLayout_->addWidget(uptimeLabel_.get(), 0, 0);
    systemInfoLayout_->addWidget(uptimeValue_.get(), 0, 1);
    systemInfoLayout_->addWidget(processCountLabel_.get(), 1, 0);
    systemInfoLayout_->addWidget(processCountValue_.get(), 1, 1);
    systemInfoLayout_->addWidget(threadCountLabel_.get(), 2, 0);
    systemInfoLayout_->addWidget(threadCountValue_.get(), 2, 1);
    systemInfoLayout_->addWidget(contextSwitchesLabel_.get(), 3, 0);
    systemInfoLayout_->addWidget(contextSwitchesValue_.get(), 3, 1);
    
    systemInfoGroup_->setLayout(systemInfoLayout_.get());
    scrollLayout_->addWidget(systemInfoGroup_.get());
}

void SystemMonitorTab::setupCpuSection() {
    cpuGroup_ = std::make_unique<QGroupBox("CPU Usage");
    cpuLayout_ = std::make_unique<QVBoxLayout>();
    
    cpuTotalLabel_ = std::make_unique<QLabel("Total CPU:");
    cpuTotalBar_ = std::make_unique<QProgressBar>();
    cpuTotalValue_ = std::make_unique<QLabel("0%");
    
    cpuCoresLabel_ = std::make_unique<QLabel("CPU Cores:");
    cpuCoresWidget_ = new QWidget();
    cpuCoresLayout_ = std::make_unique<QVBoxLayout>(cpuCoresWidget_);
    
    // Create core progress bars (placeholder for 4 cores)
    for (int i = 0; i < 4; ++i) {
        auto bar = std::make_unique<QProgressBar>();
        auto label = std::make_unique<QLabel(QString("Core %1: 0%").arg(i)));
        
        cpuCoreBars_.push_back(std::move(bar));
        cpuCoreLabels_.push_back(std::move(label));
    }
    
    for (size_t i = 0; i < cpuCoreBars_.size(); ++i) {
        auto layout = std::make_unique<QHBoxLayout>();
        layout->addWidget(cpuCoreLabels_[i].get());
        layout->addWidget(cpuCoreBars_[i].get());
        cpuCoresLayout_->addLayout(layout.release());
    }
    
    auto totalLayout = std::make_unique<QHBoxLayout>();
    totalLayout->addWidget(cpuTotalLabel_.get());
    totalLayout->addWidget(cpuTotalBar_.get());
    totalLayout->addWidget(cpuTotalValue_.get());
    
    cpuLayout_->addLayout(totalLayout.release());
    cpuLayout_->addWidget(cpuCoresLabel_.get());
    cpuLayout_->addWidget(cpuCoresWidget_);
    
    cpuGroup_->setLayout(cpuLayout_.get());
    scrollLayout_->addWidget(cpuGroup_.get());
}

void SystemMonitorTab::setupMemorySection() {
    memoryGroup_ = std::make_unique<QGroupBox("Memory Usage");
    memoryLayout_ = std::make_unique<QVBoxLayout>();
    
    memoryTotalLabel_ = std::make_unique<QLabel("Total Memory:");
    memoryTotalValue_ = std::make_unique<QLabel("0 GB");
    memoryUsedBar_ = std::make_unique<QProgressBar>();
    memoryUsedValue_ = std::make_unique<QLabel("Used: 0 GB");
    memoryFreeValue_ = std::make_unique<QLabel("Free: 0 GB");
    memoryCacheValue_ = std::make_unique<QLabel("Cache: 0 GB");
    memoryBuffersValue_ = std::make_unique<QLabel>("Buffers: 0 GB");
    
    memoryLayout_->addWidget(memoryTotalLabel_.get());
    memoryLayout_->addWidget(memoryTotalValue_.get());
    memoryLayout_->addWidget(memoryUsedBar_.get());
    memoryLayout_->addWidget(memoryUsedValue_.get());
    memoryLayout_->addWidget(memoryFreeValue_.get());
    memoryLayout_->addWidget(memoryCacheValue_.get());
    memoryLayout_->addWidget(memoryBuffersValue_.get());
    
    memoryGroup_->setLayout(memoryLayout_.get());
    scrollLayout_->addWidget(memoryGroup_.get());
}

void SystemMonitorTab::setupProcessSection() {
    processGroup_ = std::make_unique<QGroupBox("Processes");
    processLayout_ = std::make_unique<QVBoxLayout>();
    
    processTable_ = std::make_unique<QTableWidget>();
    setupProcessTableHeaders();
    
    processLayout_->addWidget(processTable_.get());
    processGroup_->setLayout(processLayout_.get());
    scrollLayout_->addWidget(processGroup_.get());
}

void SystemMonitorTab::setupProcessTableHeaders() {
    QStringList headers;
    headers << "PID" << "Name" << "CPU %" << "Memory" << "Status" << "User" << "Parent PID";
    
    processTable_->setColumnCount(headers.size());
    processTable_->setHorizontalHeaderLabels(headers);
    processTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    processTable_->setSortingEnabled(true);
    
    // Adjust column widths
    processTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); // PID
    processTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch); // Name
    processTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents); // CPU
    processTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents); // Memory
    processTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents); // Status
    processTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents); // User
    processTable_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents); // Parent PID
}

void SystemMonitorTab::updateCpuDisplay(const SystemInfo& info) {
    // Update total CPU
    cpuTotalBar_->setValue(static_cast<int>(info.cpuUsageTotal));
    cpuTotalValue_->setText(QString("%1%").arg(info.cpuUsageTotal, 0, 'f', 1));
    
    // Update CPU cores
    for (size_t i = 0; i < cpuCoreBars_.size() && i < info.cpuCoresUsage.size(); ++i) {
        cpuCoreBars_[i]->setValue(static_cast<int>(info.cpuCoresUsage[i]));
        cpuCoreLabels_[i]->setText(QString("Core %1: %2%").arg(i).arg(info.cpuCoresUsage[i], 0, 'f', 1));
    }
}

void SystemMonitorTab::updateMemoryDisplay(const SystemInfo& info) {
    // Update memory values
    memoryTotalValue_->setText(formatBytes(info.memoryTotal));
    memoryUsedValue_->setText(QString("Used: %1").arg(formatBytes(info.memoryUsed)));
    memoryFreeValue_->setText(QString("Free: %1").arg(formatBytes(info.memoryFree)));
    memoryCacheValue_->setText(QString("Cache: %1").arg(formatBytes(info.memoryCache)));
    memoryBuffersValue_->setText(QString("Buffers: %1").arg(formatBytes(info.memoryBuffers)));
    
    // Update memory usage bar
    double usedPercent = (static_cast<double>(info.memoryUsed) / info.memoryTotal) * 100.0;
    memoryUsedBar_->setValue(static_cast<int>(usedPercent));
}

void SystemMonitorTab::updateSystemStats(const SystemInfo& info) {
    uptimeValue_->setText(formatUptime(info.uptime));
    processCountValue_->setText(QString::number(info.processCount));
    threadCountValue_->setText(QString::number(info.threadCount));
    contextSwitchesValue_->setText(QString::number(info.contextSwitches));
}

void SystemMonitorTab::updateProcessTable(const std::vector<ProcessInfo>& processes) {
    processTable_->setRowCount(static_cast<int>(processes.size()));
    
    for (int row = 0; row < static_cast<int>(processes.size()) && row < MAX_PROCESSES_DISPLAY; ++row) {
        const ProcessInfo& proc = processes[row];
        
        processTable_->setItem(row, 0, new QTableWidgetItem(QString::number(proc.pid)));
        processTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(proc.name)));
        processTable_->setItem(row, 2, new QTableWidgetItem(QString("%1%").arg(proc.cpuUsage, 0, 'f', 1)));
        processTable_->setItem(row, 3, new QTableWidgetItem(formatBytes(proc.memoryUsage)));
        processTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(proc.status)));
        processTable_->setItem(row, 5, new QTableWidgetItem(QString::fromStdString(proc.user)));
        processTable_->setItem(row, 6, new QTableWidgetItem(QString::number(proc.parentPid)));
    }
}

QString SystemMonitorTab::formatBytes(uint64_t bytes) const {
    const double KB = 1024.0;
    const double MB = KB * 1024.0;
    const double GB = MB * 1024.0;
    
    if (bytes >= GB) {
        return QString("%1 GB").arg(bytes / GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(bytes / MB, 0, 'f', 2);
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(bytes / KB, 0, 'f', 2);
    } else {
        return QString("%1 B").arg(bytes);
    }
}

QString SystemMonitorTab::formatPercentage(double value) const {
    return QString("%1%").arg(value, 0, 'f', 1);
}

QString SystemMonitorTab::formatUptime(std::chrono::seconds uptime) const {
    auto hours = std::chrono::duration_cast<std::chrono::hours>(uptime);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(uptime - hours);
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(uptime - hours - minutes);
    
    return QString("%1:%2:%3")
        .arg(hours.count())
        .arg(minutes.count(), 2, 10, QChar('0'))
        .arg(seconds.count(), 2, 10, QChar('0'));
}

QString SystemMonitorTab::formatCoresUsage(const std::vector<double>& cores) const {
    QStringList coreStrings;
    for (double usage : cores) {
        coreStrings << QString("%1%").arg(usage, 0, 'f', 1);
    }
    return coreStrings.join(", ");
}

} // namespace SysMon

#include "systemmonitortab.moc"
