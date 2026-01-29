#include "systemmonitor.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <shared_mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <tlhelp32.h>
#include <ws2tcpip.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/utsname.h>
#endif

namespace SysMon {

SystemMonitor::SystemMonitor() 
    : running_(false)
    , initialized_(false)
    , fallbackMode_(false)
    , updateInterval_(1000) {
    
#ifdef _WIN32
    cpuQueryHandle_ = nullptr;
    memoryCounterHandle_ = nullptr;
#else
    procStat_ = nullptr;
    procMeminfo_ = nullptr;
    procUptime_ = nullptr;
    numCores_ = 0;
#endif
}

SystemMonitor::~SystemMonitor() {
    shutdown();
}

bool SystemMonitor::initialize() {
    if (initialized_) {
        return true;
    }
    
    initialized_ = true;
    return true;
}

void SystemMonitor::shutdown() {
    if (!initialized_) {
        return;
    }
    
    running_ = false;
    
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
    
    initialized_ = false;
}

bool SystemMonitor::start() {
    if (!initialized_) {
        return false;
    }
    
    if (running_) {
        return true;
    }
    
    running_ = true;
    monitoringThread_ = std::thread(&SystemMonitor::monitoringThread, this);
    
    return true;
}

void SystemMonitor::stop() {
    running_ = false;
    
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
}

void SystemMonitor::monitoringThread() {
    while (running_) {
        try {
            updateSystemInfo();
            updateProcessList();
            
            std::this_thread::sleep_for(updateInterval_);
            
        } catch (const std::exception& e) {
            // Log error but continue
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void SystemMonitor::updateSystemInfo() {
    // Temporary simplified implementation
    SystemInfo info;
    info.cpuUsageTotal = 0.0;
    info.cpuCoresUsage = {0.0};
    info.memoryTotal = 8589934592; // 8GB
    info.memoryUsed = 4294967296;  // 4GB
    info.memoryFree = 4294967296;  // 4GB
    info.memoryCache = 0;
    info.memoryBuffers = 0;
    info.processCount = 100;
    info.threadCount = 200;
    info.contextSwitches = 0;
    info.uptime = std::chrono::seconds(3600);
    
    std::unique_lock<std::shared_mutex> lock(dataMutex_);
    currentSystemInfo_ = info;
    lastUpdate_ = std::chrono::steady_clock::now();
}

void SystemMonitor::updateProcessList() {
    // Temporary simplified implementation
    std::vector<ProcessInfo> processes;
    
    ProcessInfo proc;
    proc.pid = 1;
    proc.name = "init";
    proc.cpuUsage = 0.1;
    proc.memoryUsage = 1024 * 1024; // 1MB
    proc.status = "S";
    proc.parentPid = 0;
    proc.user = "root";
    processes.push_back(proc);
    
    std::unique_lock<std::shared_mutex> lock(dataMutex_);
    currentProcessList_ = processes;
}

SystemInfo SystemMonitor::getCurrentSystemInfo() {
    std::shared_lock<std::shared_mutex> lock(dataMutex_);
    return currentSystemInfo_;
}

std::vector<ProcessInfo> SystemMonitor::getProcessList() {
    std::shared_lock<std::shared_mutex> lock(dataMutex_);
    return currentProcessList_;
}

bool SystemMonitor::isRunning() const {
    return running_;
}

std::chrono::milliseconds SystemMonitor::getUpdateInterval() const {
    return updateInterval_;
}

void SystemMonitor::setUpdateInterval(std::chrono::milliseconds interval) {
    updateInterval_ = interval;
}

// Platform-specific implementations (simplified)
SystemInfo SystemMonitor::collectSystemInfoLinux() {
    SystemInfo info;
    
    // Get CPU usage from /proc/stat
    std::ifstream statFile("/proc/stat");
    if (statFile.is_open()) {
        std::string line;
        if (std::getline(statFile, line)) {
            std::istringstream iss(line);
            std::string cpu;
            unsigned long long user, nice, system, idle, iowait, irq, softirq;
            if (iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq) {
                static unsigned long long lastIdle = 0, lastTotal = 0;
                
                unsigned long long total = user + nice + system + idle + iowait + irq + softirq;
                unsigned long long idleDiff = idle - lastIdle;
                unsigned long long totalDiff = total - lastTotal;
                
                if (totalDiff > 0) {
                    info.cpuUsageTotal = 100.0 * (1.0 - (double)idleDiff / totalDiff);
                }
                
                lastIdle = idle;
                lastTotal = total;
            }
        }
    }
    
    // Get memory info from /proc/meminfo
    std::ifstream memFile("/proc/meminfo");
    if (memFile.is_open()) {
        std::string line;
        while (std::getline(memFile, line)) {
            if (line.find("MemTotal:") == 0) {
                std::istringstream iss(line);
                std::string label;
                unsigned long long value;
                std::string unit;
                iss >> label >> value >> unit;
                info.memoryTotal = value * 1024; // Convert KB to bytes
            } else if (line.find("MemAvailable:") == 0) {
                std::istringstream iss(line);
                std::string label;
                unsigned long long value;
                std::string unit;
                iss >> label >> value >> unit;
                info.memoryFree = value * 1024; // Convert KB to bytes
            }
        }
        info.memoryUsed = info.memoryTotal - info.memoryFree;
    }
    
    // Get CPU cores
    info.cpuCoresUsage.resize(std::thread::hardware_concurrency(), info.cpuUsageTotal / std::thread::hardware_concurrency());
    
    // Get uptime from /proc/uptime
    std::ifstream uptimeFile("/proc/uptime");
    if (uptimeFile.is_open()) {
        double uptimeSeconds;
        uptimeFile >> uptimeSeconds;
        info.uptime = std::chrono::seconds(static_cast<long long>(uptimeSeconds));
    }
    
    return info;
}

SystemInfo SystemMonitor::collectSystemInfoWindows() {
    SystemInfo info;
    
    // Get CPU usage
    ULARGE_INTEGER idleTime, kernelTime, userTime;
    if (GetSystemTimes((FILETIME*)&idleTime, (FILETIME*)&kernelTime, (FILETIME*)&userTime)) {
        static ULARGE_INTEGER lastIdle, lastKernel, lastUser;
        
        ULARGE_INTEGER idleDiff, kernelDiff, userDiff;
        idleDiff.QuadPart = idleTime.QuadPart - lastIdle.QuadPart;
        kernelDiff.QuadPart = kernelTime.QuadPart - lastKernel.QuadPart;
        userDiff.QuadPart = userTime.QuadPart - lastUser.QuadPart;
        
        ULARGE_INTEGER totalDiff;
        totalDiff.QuadPart = kernelDiff.QuadPart + userDiff.QuadPart;
        
        if (totalDiff.QuadPart > 0) {
            double cpuUsage = 100.0 * (1.0 - (double)idleDiff.QuadPart / totalDiff.QuadPart);
            info.cpuUsageTotal = std::max(0.0, std::min(100.0, cpuUsage));
        }
        
        lastIdle = idleTime;
        lastKernel = kernelTime;
        lastUser = userTime;
    }
    
    // Get memory info
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        info.memoryTotal = memInfo.ullTotalPhys;
        info.memoryUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        info.memoryFree = memInfo.ullAvailPhys;
    }
    
    // Get CPU cores
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    info.cpuCoresUsage.resize(sysInfo.dwNumberOfProcessors, info.cpuUsageTotal / sysInfo.dwNumberOfProcessors);
    
    // Get process count
    DWORD processes = 0;
    DWORD processesReturned = 0;
    if (EnumProcesses(&processes, sizeof(processes), &processesReturned)) {
        info.processCount = processesReturned / sizeof(DWORD);
    }
    
    // Get uptime
    info.uptime = std::chrono::seconds(GetTickCount64() / 1000);
    
    return info;
}

std::vector<ProcessInfo> SystemMonitor::collectProcessListLinux() {
    std::vector<ProcessInfo> processes;
    // Simplified implementation
    return processes;
}

std::vector<ProcessInfo> SystemMonitor::collectProcessListWindows() {
    std::vector<ProcessInfo> processes;
    // Simplified implementation
    return processes;
}

double SystemMonitor::getCpuUsageTotal() {
    return 0.0;
}

std::vector<double> SystemMonitor::getCpuCoresUsage() {
    return std::vector<double>{0.0};
}

void SystemMonitor::getMemoryInfo(uint64_t& total, uint64_t& used, uint64_t& free, 
                                  uint64_t& cache, uint64_t& buffers) {
    total = used = free = cache = buffers = 0;
}

uint32_t SystemMonitor::getProcessCount() {
    return 0;
}

uint32_t SystemMonitor::getThreadCount() {
    return 0;
}

uint64_t SystemMonitor::getContextSwitches() {
    return 0;
}

std::chrono::seconds SystemMonitor::getSystemUptime() {
    return std::chrono::seconds(0);
}

ProcessInfo SystemMonitor::getProcessInfoLinux(pid_t pid) {
    (void)pid;
    return ProcessInfo();
}

// Fallback mode support
void SystemMonitor::enableFallbackMode() {
    fallbackMode_ = true;
    initialized_ = true;
    
    // Create fallback system info
    SystemInfo fallbackInfo;
    fallbackInfo.cpuUsageTotal = 0.0;
    fallbackInfo.cpuCoresUsage = {0.0};
    fallbackInfo.memoryTotal = 1024 * 1024 * 1024; // 1GB
    fallbackInfo.memoryUsed = 512 * 1024 * 1024; // 512MB
    fallbackInfo.memoryFree = 512 * 1024 * 1024;
    fallbackInfo.memoryCache = 0;
    fallbackInfo.memoryBuffers = 0;
    fallbackInfo.processCount = 0;
    fallbackInfo.threadCount = 0;
    fallbackInfo.contextSwitches = 0;
    fallbackInfo.uptime = std::chrono::seconds(0);
    
    std::lock_guard<std::shared_mutex> lock(dataMutex_);
    currentSystemInfo_ = fallbackInfo;
    currentProcessList_.clear();
}

bool SystemMonitor::isFallbackMode() const {
    return fallbackMode_;
}

} // namespace SysMon
