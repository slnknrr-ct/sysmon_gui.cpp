#pragma once

#include "../shared/systemtypes.h"
#include <memory>
#include <thread>
#include <atomic>
#include <shared_mutex>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#endif

namespace SysMon {

// Process Manager - manages system processes
class ProcessManager {
public:
    ProcessManager();
    ~ProcessManager();
    
    // Lifecycle
    bool initialize();
    bool start();
    void stop();
    void shutdown();
    
    // Process operations
    std::vector<ProcessInfo> getProcessList();
    bool terminateProcess(uint32_t pid);
    bool killProcess(uint32_t pid);
    bool isCriticalProcess(uint32_t pid) const;
    
    // Status
    bool isRunning() const;

private:
    // Process monitoring
    void processMonitoringThread();
    void updateProcessList();
    
    // Platform-specific implementations
    std::vector<ProcessInfo> getProcessListLinux();
    std::vector<ProcessInfo> getProcessListWindows();
    bool terminateProcessLinux(uint32_t pid);
    bool terminateProcessWindows(uint32_t pid);
    bool killProcessLinux(uint32_t pid);
    bool killProcessWindows(uint32_t pid);
    
    // Critical process detection
    std::vector<uint32_t> getCriticalProcessesLinux();
    std::vector<uint32_t> getCriticalProcessesWindows();
    bool isSystemCritical(uint32_t pid) const;
    
    // Process information helpers
    ProcessInfo getProcessInfoLinux(uint32_t pid);
    ProcessInfo getProcessInfoWindows(uint32_t pid);
    double calculateCpuUsage(uint32_t pid);
    uint64_t getProcessMemoryUsage(uint32_t pid);
    std::string getProcessStatus(uint32_t pid);
    std::string getProcessUser(uint32_t pid);
    uint32_t getParentPid(uint32_t pid);
    
    // Thread management
    std::thread monitoringThread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // Data storage
    std::vector<ProcessInfo> processList_;
    std::vector<uint32_t> criticalProcesses_;
    mutable std::shared_mutex processMutex_;
    
    // Timing
    std::chrono::steady_clock::time_point lastUpdate_;
    std::chrono::milliseconds updateInterval_;
    
    // Platform-specific data
#ifdef _WIN32
    HANDLE processSnapshot_;
#else
    FILE* procStat_;
    FILE* procStatus_;
#endif
};

} // namespace SysMon
