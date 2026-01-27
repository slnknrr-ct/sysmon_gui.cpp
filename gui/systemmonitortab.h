#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QTimer>
#include <QGroupBox>
#include <QScrollArea>
#include <memory>
#include "../shared/systemtypes.h"

QT_BEGIN_NAMESPACE
class QTableWidget;
class QProgressBar;
class QLabel;
class QTimer;
class QGroupBox;
QT_END_NAMESPACE

// Forward declarations
class IpcClient;

namespace SysMon {

// System Monitor Tab - displays real-time system information
class SystemMonitorTab : public QWidget {
    Q_OBJECT

public:
    SystemMonitorTab(IpcClient* ipcClient, QWidget* parent = nullptr);
    ~SystemMonitorTab();

protected:
    // Event handling
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    // Data updates
    void updateSystemInfo();
    void updateProcessList();
    void onSystemInfoResponse(const Response& response);
    void onProcessListResponse(const Response& response);
    void onError(const QString& error);

private:
    // UI initialization
    void setupUI();
    void setupSystemInfoSection();
    void setupCpuSection();
    void setupMemorySection();
    void setupProcessSection();
    void setupSystemStatsSection();
    
    // System info display
    void updateCpuDisplay(const SystemInfo& info);
    void updateMemoryDisplay(const SystemInfo& info);
    void updateSystemStats(const SystemInfo& info);
    void updateProcessTable(const std::vector<ProcessInfo>& processes);
    
    // Formatters
    QString formatBytes(uint64_t bytes) const;
    QString formatPercentage(double value) const;
    QString formatUptime(std::chrono::seconds uptime) const;
    QString formatCoresUsage(const std::vector<double>& cores) const;
    
    // UI components
    std::unique_ptr<QVBoxLayout> mainLayout_;
    std::unique_ptr<QScrollArea> scrollArea_;
    QWidget* scrollWidget_;
    std::unique_ptr<QVBoxLayout> scrollLayout_;
    
    // System Info section
    std::unique_ptr<QGroupBox> systemInfoGroup_;
    std::unique_ptr<QGridLayout> systemInfoLayout_;
    std::unique_ptr<QLabel> uptimeLabel_;
    std::unique_ptr<QLabel> uptimeValue_;
    std::unique_ptr<QLabel> processCountLabel_;
    std::unique_ptr<QLabel> processCountValue_;
    std::unique_ptr<QLabel> threadCountLabel_;
    std::unique_ptr<QLabel> threadCountValue_;
    std::unique_ptr<QLabel> contextSwitchesLabel_;
    std::unique_ptr<QLabel> contextSwitchesValue_;
    
    // CPU section
    std::unique_ptr<QGroupBox> cpuGroup_;
    std::unique_ptr<QVBoxLayout> cpuLayout_;
    std::unique_ptr<QLabel> cpuTotalLabel_;
    std::unique_ptr<QProgressBar> cpuTotalBar_;
    std::unique_ptr<QLabel> cpuTotalValue_;
    std::unique_ptr<QLabel> cpuCoresLabel_;
    std::unique_ptr<QWidget> cpuCoresWidget_;
    std::unique_ptr<QVBoxLayout> cpuCoresLayout_;
    std::vector<std::unique_ptr<QProgressBar>> cpuCoreBars_;
    std::vector<std::unique_ptr<QLabel>> cpuCoreLabels_;
    
    // Memory section
    std::unique_ptr<QGroupBox> memoryGroup_;
    std::unique_ptr<QVBoxLayout> memoryLayout_;
    std::unique_ptr<QLabel> memoryTotalLabel_;
    std::unique_ptr<QLabel> memoryTotalValue_;
    std::unique_ptr<QProgressBar> memoryUsedBar_;
    std::unique_ptr<QLabel> memoryUsedValue_;
    std::unique_ptr<QLabel> memoryFreeValue_;
    std::unique_ptr<QLabel> memoryCacheValue_;
    std::unique_ptr<QLabel> memoryBuffersValue_;
    
    // Process section
    std::unique_ptr<QGroupBox> processGroup_;
    std::unique_ptr<QVBoxLayout> processLayout_;
    std::unique_ptr<QTableWidget> processTable_;
    
    // IPC client
    IpcClient* ipcClient_;
    
    // Update timers
    std::unique_ptr<QTimer> systemInfoTimer_;
    std::unique_ptr<QTimer> processListTimer_;
    
    // Data
    SystemInfo currentSystemInfo_;
    std::vector<ProcessInfo> currentProcessList_;
    
    // Status
    bool isActive_;
    
    // Constants
    static constexpr int SYSTEM_UPDATE_INTERVAL = 1000; // 1 second
    static constexpr int PROCESS_UPDATE_INTERVAL = 2000; // 2 seconds
    static constexpr int MAX_PROCESSES_DISPLAY = 100;
};

} // namespace SysMon
