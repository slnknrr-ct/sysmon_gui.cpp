#pragma once

#include "../shared/systemtypes.h"
#include <memory>
#include <thread>
#include <atomic>
#include <shared_mutex>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#else
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/sysinfo.h>
#endif

namespace SysMon {

// System Monitor - collects system information
class SystemMonitor {
public:
    SystemMonitor();
    ~SystemMonitor();
    
    // Lifecycle
    bool initialize();
    bool start();
    void stop();
    void shutdown();
    
    // Data collection
    SystemInfo getCurrentSystemInfo();
    std::vector<ProcessInfo> getProcessList();
    
    // Status
    bool isRunning() const;
    std::chrono::milliseconds getUpdateInterval() const;
    void setUpdateInterval(std::chrono::milliseconds interval);

private:
    // Monitoring thread
    void monitoringThread();
    
    // System data collection (platform-specific)
    void updateSystemInfo();
    void updateProcessList();
    
    // Platform-specific implementations
    SystemInfo collectSystemInfoLinux();
    SystemInfo collectSystemInfoWindows();
    std::vector<ProcessInfo> collectProcessListLinux();
    std::vector<ProcessInfo> collectProcessListWindows();
    
    // CPU monitoring
    double getCpuUsageTotal();
    std::vector<double> getCpuCoresUsage();
    
    // Memory monitoring
    void getMemoryInfo(uint64_t& total, uint64_t& used, uint64_t& free, 
                      uint64_t& cache, uint64_t& buffers);
    
    // Process monitoring
    uint32_t getProcessCount();
    uint32_t getThreadCount();
    uint64_t getContextSwitches();
    std::chrono::seconds getSystemUptime();
    
    // Linux-specific helper
    ProcessInfo getProcessInfoLinux(pid_t pid);
    
    // Thread management
    std::thread monitoringThread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // Data storage
    SystemInfo currentSystemInfo_;
    std::vector<ProcessInfo> currentProcessList_;
    mutable std::shared_mutex dataMutex_;
    
    // Timing
    std::chrono::milliseconds updateInterval_;
    std::chrono::steady_clock::time_point lastUpdate_;
    
    // Platform-specific data
#ifdef _WIN32
    PDH_HQUERY cpuQueryHandle_;
    PDH_HCOUNTER memoryCounterHandle_;
    std::vector<PDH_HCOUNTER> coreCounterHandles_;
#else
    FILE* procStat_;
    FILE* procMeminfo_;
    FILE* procUptime_;
    int numCores_;
    
    // CPU time tracking for Linux
    struct CpuTime {
        unsigned long long total;
        unsigned long long work;
    };
    std::vector<CpuTime> prevCpuTimes_;
#endif
};

} // namespace SysMon
