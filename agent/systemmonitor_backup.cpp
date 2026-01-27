#include "systemmonitor.h"
#include "logger.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <dirent.h>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <tlhelp32.h>
#else
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <signal.h>
#include <pwd.h>
#include <sys/stat.h>
#include <cstring>
#include <sys/utsname.h>
#include <sys/conf.h>
#endif

namespace SysMon {

SystemMonitor::SystemMonitor() 
    : running_(false)
    , initialized_(false)
    , updateInterval_(1000) {
    
#ifdef _WIN32
    cpuQueryHandle_ = nullptr;
    memoryCounterHandle_ = nullptr;
#else
    procStat_ = nullptr;
    procMeminfo_ = nullptr;
    procUptime_ = nullptr;
#endif
}

SystemMonitor::~SystemMonitor() {
    shutdown();
}

bool SystemMonitor::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Initialize platform-specific resources
#ifdef _WIN32
    // Initialize PDH queries for Windows
    PDH_STATUS status = PdhOpenQuery(nullptr, 0, &cpuQueryHandle_);
    if (status != ERROR_SUCCESS) {
        // Logger will be available after initialization, use cerr for now
        std::cerr << "Failed to open PDH query: " << status << std::endl;
#else
    // Linux initialization
    procStat_ = fopen("/proc/stat", "r");
    procMeminfo_ = fopen("/proc/meminfo", "r");
    procUptime_ = fopen("/proc/uptime", "r");
    
    }
    
    // Get number of CPU cores
    numCores_ = 0;
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "processor", 9) == 0) {
                numCores_++;
            }
        }
        fclose(cpuinfo);
    }
    
    if (numCores_ == 0) {
        numCores_ = 1; // Fallback
    }
    
    // Initialize previous CPU times
    prevCpuTimes_.resize(numCores_ + 1); // +1 for total
#endif
    
    initialized_ = true;
    std::cout << "SystemMonitor initialized successfully" << std::endl;
    return true;
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
    
    std::cout << "System Monitor started" << std::endl;
    return true;
}

void SystemMonitor::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
    
    std::cout << "System Monitor stopped" << std::endl;
}

void SystemMonitor::shutdown() {
    stop();
    
#ifdef _WIN32
    if (cpuQueryHandle_) {
        PdhCloseQuery(cpuQueryHandle_);
        cpuQueryHandle_ = nullptr;
    }
#else
    if (procStat_) {
        fclose(procStat_);
        procStat_ = nullptr;
    }
    if (procMeminfo_) {
        fclose(procMeminfo_);
        procMeminfo_ = nullptr;
    }
    if (procUptime_) {
        fclose(procUptime_);
        procUptime_ = nullptr;
    }
#endif
    
    initialized_ = false;
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

SystemInfo SystemMonitor::getCurrentSystemInfo() {
    std::shared_lock<std::shared_mutex> lock(dataMutex_);
    return currentSystemInfo_;
}

std::vector<ProcessInfo> SystemMonitor::getProcessList() {
    std::shared_lock<std::shared_mutex> lock(dataMutex_);
    return currentProcessList_;
}

void SystemMonitor::monitoringThread() {
    while (running_) {
        try {
            updateSystemInfo();
            updateProcessList();
            lastUpdate_ = std::chrono::steady_clock::now();
            
            std::this_thread::sleep_for(updateInterval_);
            
        } catch (const std::exception& e) {
            // Log error but continue
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
}

void SystemMonitor::updateSystemInfo() {
    SystemInfo info;
    
#ifdef _WIN32
    info = collectSystemInfoWindows();
#else
    info = collectSystemInfoLinux();
#endif
    
    std::unique_lock<std::shared_mutex> lock(dataMutex_);
    currentSystemInfo_ = info;
}

void SystemMonitor::updateProcessList() {
    std::vector<ProcessInfo> processes;
    
#ifdef _WIN32
    processes = collectProcessListWindows();
#else
    processes = collectProcessListLinux();
#endif
    
    std::unique_lock<std::shared_mutex> lock(dataMutex_);
    currentProcessList_ = processes;
}

SystemInfo SystemMonitor::collectSystemInfoLinux() {
    SystemInfo info;
    
    // Get CPU usage
    info.cpuUsageTotal = getCpuUsageTotal();
    info.cpuCoresUsage = getCpuCoresUsage();
    
    // Get memory information
    getMemoryInfo(info.memoryTotal, info.memoryUsed, info.memoryFree, 
                  info.memoryCache, info.memoryBuffers);
    
    // Get process and thread counts
    info.processCount = getProcessCount();
    info.threadCount = getThreadCount();
    
    // Get context switches
    info.contextSwitches = getContextSwitches();
    
    // Get system uptime
    info.uptime = getSystemUptime();
    
    return info;
}

SystemInfo SystemMonitor::collectSystemInfoWindows() {
    SystemInfo info;
    
    // Collect PDH data
    PdhCollectQueryData(cpuQueryHandle_);
    
    // Get CPU usage
    PDH_FMT_COUNTERVALUE counterValue;
    if (coreCounterHandles_[0] && PdhGetFormattedCounterValue(coreCounterHandles_[0], PDH_FMT_DOUBLE, nullptr, &counterValue) == ERROR_SUCCESS) {
        info.cpuUsageTotal = counterValue.doubleValue;
    }
    
    // Get CPU cores usage
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    int numCores = sysInfo.dwNumberOfProcessors;
    info.cpuCoresUsage.resize(numCores);
    
    for (int i = 0; i < numCores; ++i) {
        if (coreCounterHandles_[i] && PdhGetFormattedCounterValue(coreCounterHandles_[i], PDH_FMT_DOUBLE, nullptr, &counterValue) == ERROR_SUCCESS) {
            info.cpuCoresUsage[i] = counterValue.doubleValue;
        }
    }
    
    // Get memory information
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        info.memoryTotal = memStatus.ullTotalPhys;
        info.memoryUsed = memStatus.ullTotalPhys - memStatus.ullAvailPhys;
        info.memoryFree = memStatus.ullAvailPhys;
    }
    
    // Get cache and buffers (approximation for Windows)
    PERFORMANCE_INFORMATION perfInfo;
    if (GetPerformanceInfo(&perfInfo, sizeof(perfInfo))) {
        info.memoryCache = perfInfo.SystemCache * perfInfo.PageSize;
        info.memoryBuffers = 0; // Windows doesn't have separate buffers like Linux
    }
    
    // Get process and thread counts
    DWORD processCount = 0;
    DWORD threadCount = 0;
    GetProcessIdCount(&processCount);
    GetThreadIdCount(&threadCount);
    info.processCount = processCount;
    info.threadCount = threadCount;
    
    // Get context switches (approximation)
    info.contextSwitches = 0; // Not easily accessible on Windows without additional APIs
    
    // Get system uptime
    DWORD uptime = GetTickCount() / 1000;
    info.uptime = std::chrono::seconds(uptime);
    
    return info;
}

std::vector<ProcessInfo> SystemMonitor::collectProcessListLinux() {
    std::vector<ProcessInfo> processes;
    
    DIR* procDir = opendir("/proc");
    if (!procDir) {
        return processes;
    }
    
    struct dirent* entry;
    while ((entry = readdir(procDir)) != nullptr) {
        // Check if entry is a number (PID)
        if (!isdigit(entry->d_name[0])) {
            continue;
        }
        
        pid_t pid = atoi(entry->d_name);
        if (pid <= 0) {
            continue;
        }
        
        ProcessInfo procInfo = getProcessInfoLinux(pid);
        if (procInfo.pid > 0) {
            processes.push_back(procInfo);
        }
    }
    
    closedir(procDir);
    
    // Sort by PID
    std::sort(processes.begin(), processes.end(), 
              [](const ProcessInfo& a, const ProcessInfo& b) {
                  return a.pid < b.pid;
              });
    
    return processes;
}

std::vector<ProcessInfo> SystemMonitor::collectProcessListWindows() {
    std::vector<ProcessInfo> processes;
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return processes;
    }
    
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(hSnapshot, &pe32)) {
        do {
            ProcessInfo procInfo;
            procInfo.pid = pe32.th32ProcessID;
            procInfo.parentPid = pe32.th32ParentProcessID;
            
            // Convert wide char to narrow char for name
            char name[MAX_PATH];
            WideCharToMultiByte(CP_ACP, 0, pe32.szExeFile, -1, name, MAX_PATH, nullptr, nullptr);
            procInfo.name = name;
            
            // Get additional process information
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);
            if (hProcess) {
                // Get memory usage
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    procInfo.memoryUsage = pmc.WorkingSetSize;
                }
                
                // Get CPU usage (simplified)
                FILETIME ftCreate, ftExit, ftKernel, ftUser;
                if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
                    // Simplified CPU calculation
                    ULARGE_INTEGER kernelTime, userTime;
                    kernelTime.LowPart = ftKernel.dwLowDateTime;
                    kernelTime.HighPart = ftKernel.dwHighDateTime;
                    userTime.LowPart = ftUser.dwLowDateTime;
                    userTime.HighPart = ftUser.dwHighDateTime;
                    
                    uint64_t totalTime = kernelTime.QuadPart + userTime.QuadPart;
                    procInfo.cpuUsage = static_cast<double>(totalTime) / 10000000.0; // Convert to seconds
                }
                
                // Get process status
                DWORD exitCode;
                if (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                    procInfo.status = "Running";
                } else {
                    procInfo.status = "Zombie";
                }
                
                // Get user (simplified - would need more complex token handling for real user)
                procInfo.user = "UNKNOWN";
                
                CloseHandle(hProcess);
            } else {
                procInfo.memoryUsage = 0;
                procInfo.cpuUsage = 0.0;
                procInfo.status = "Unknown";
                procInfo.user = "UNKNOWN";
            }
            
            processes.push_back(procInfo);
        } while (Process32Next(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    
    return processes;
}

// Linux-specific helper functions
double SystemMonitor::getCpuUsageTotal() {
    if (!procStat_) {
        return 0.0;
    }
    
    rewind(procStat_);
    fflush(procStat_);
    
    char line[256];
    if (!fgets(line, sizeof(line), procStat_)) {
        return 0.0;
    }
    
    // Parse first line (cpu total)
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) != 8) {
        return 0.0;
    }
    
    unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
    unsigned long long work = user + nice + system + iowait + irq + softirq + steal;
    
    if (prevCpuTimes_[0].total == 0) {
        prevCpuTimes_[0].total = total;
        prevCpuTimes_[0].work = work;
        return 0.0;
    }
    
    unsigned long long totalDiff = total - prevCpuTimes_[0].total;
    unsigned long long workDiff = work - prevCpuTimes_[0].work;
    
    prevCpuTimes_[0].total = total;
    prevCpuTimes_[0].work = work;
    
    if (totalDiff == 0) {
        return 0.0;
    }
    
    return (100.0 * workDiff) / totalDiff;
}

std::vector<double> SystemMonitor::getCpuCoresUsage() {
    std::vector<double> coreUsage(numCores_, 0.0);
    
    if (!procStat_) {
        return coreUsage;
    }
    
    rewind(procStat_);
    fflush(procStat_);
    
    char line[256];
    // Skip first line (cpu total)
    if (!fgets(line, sizeof(line), procStat_)) {
        return coreUsage;
    }
    
    // Parse each CPU core
    for (int i = 0; i < numCores_; ++i) {
        if (!fgets(line, sizeof(line), procStat_)) {
            break;
        }
        
        if (strncmp(line, "cpu", 3) != 0) {
            break;
        }
        
        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
        if (sscanf(line, "cpu* %llu %llu %llu %llu %llu %llu %llu %llu",
                   &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) != 8) {
            continue;
        }
        
        unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
        unsigned long long work = user + nice + system + iowait + irq + softirq + steal;
        
        if (prevCpuTimes_[i + 1].total == 0) {
            prevCpuTimes_[i + 1].total = total;
            prevCpuTimes_[i + 1].work = work;
            continue;
        }
        
        unsigned long long totalDiff = total - prevCpuTimes_[i + 1].total;
        unsigned long long workDiff = work - prevCpuTimes_[i + 1].work;
        
        prevCpuTimes_[i + 1].total = total;
        prevCpuTimes_[i + 1].work = work;
        
        if (totalDiff > 0) {
            coreUsage[i] = (100.0 * workDiff) / totalDiff;
        }
    }
    
    return coreUsage;
}

void SystemMonitor::getMemoryInfo(uint64_t& total, uint64_t& used, uint64_t& free,
                                uint64_t& cache, uint64_t& buffers) {
    total = used = free = cache = buffers = 0;
    
    if (!procMeminfo_) {
        return;
    }
    
    rewind(procMeminfo_);
    fflush(procMeminfo_);
    
    char line[256];
    while (fgets(line, sizeof(line), procMeminfo_)) {
        unsigned long long value;
        if (sscanf(line, "MemTotal: %llu kB", &value) == 1) {
            total = value * 1024;
        } else if (sscanf(line, "MemFree: %llu kB", &value) == 1) {
            free = value * 1024;
        } else if (sscanf(line, "Buffers: %llu kB", &value) == 1) {
            buffers = value * 1024;
        } else if (sscanf(line, "Cached: %llu kB", &value) == 1) {
            cache = value * 1024;
        }
    }
    
    used = total - free - cache - buffers;
}

uint32_t SystemMonitor::getProcessCount() {
    DIR* procDir = opendir("/proc");
    if (!procDir) {
        return 0;
    }
    
    uint32_t count = 0;
    struct dirent* entry;
    while ((entry = readdir(procDir)) != nullptr) {
        if (isdigit(entry->d_name[0])) {
            count++;
        }
    }
    
    closedir(procDir);
    return count;
}

uint32_t SystemMonitor::getThreadCount() {
    // This is a simplified implementation
    // A full implementation would parse /proc/[pid]/status for each process
    return getProcessCount() * 2; // Rough estimate
}

uint64_t SystemMonitor::getContextSwitches() {
    if (!procStat_) {
        return 0;
    }
    
    rewind(procStat_);
    fflush(procStat_);
    
    char line[256];
    while (fgets(line, sizeof(line), procStat_)) {
        unsigned long long value;
        if (sscanf(line, "ctxt %llu", &value) == 1) {
            return value;
        }
    }
    
    return 0;
}

std::chrono::seconds SystemMonitor::getSystemUptime() {
    if (!procUptime_) {
        return std::chrono::seconds(0);
    }
    
    rewind(procUptime_);
    fflush(procUptime_);
    
    double uptime, idle;
    if (fscanf(procUptime_, "%lf %lf", &uptime, &idle) == 2) {
        return std::chrono::seconds(static_cast<long long>(uptime));
    }
    
    return std::chrono::seconds(0);
}

ProcessInfo SystemMonitor::getProcessInfoLinux(pid_t pid) {
    ProcessInfo info;
    info.pid = pid;
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    
    FILE* statFile = fopen(path, "r");
    if (!statFile) {
        return info;
    }
    
    // Parse /proc/[pid]/stat
    char comm[256];
    char state;
    unsigned long long utime, stime, starttime;
    long rss;
    pid_t ppid;
    
    if (fscanf(statFile, "%d %s %c %d %*d %*d %*d %*d %*d %*d %*d %*d %llu %llu %*d %*d %ld %*d %*d %*d %*d %*d %*d %llu",
               &info.pid, comm, &state, &ppid, &utime, &stime, &rss, &starttime) >= 8) {
        info.name = comm;
        info.status = state;
        info.parentPid = ppid;
        
        // Get memory usage (RSS in pages)
        long pageSize = sysconf(_SC_PAGESIZE);
        info.memoryUsage = rss * pageSize;
        
        // Get CPU usage (simplified)
        info.cpuUsage = 0.0; // Would need more complex calculation over time
        
        // Get user
        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        FILE* statusFile = fopen(path, "r");
        if (statusFile) {
            char line[256];
            while (fgets(line, sizeof(line), statusFile)) {
                if (strncmp(line, "Uid:", 4) == 0) {
                    unsigned int uid;
                    if (sscanf(line, "Uid: %u", &uid) == 1) {
                        struct passwd* pw = getpwuid(uid);
                        if (pw) {
                            info.user = pw->pw_name;
                        } else {
                            info.user = std::to_string(uid);
                        }
                    }
                    break;
                }
            }
            fclose(statusFile);
        }
    }
    
    fclose(statFile);
    return info;
}

} // namespace SysMon
