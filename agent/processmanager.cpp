#include "processmanager.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <tlhelp32.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

namespace SysMon {

ProcessManager::ProcessManager() 
    : running_(false)
    , initialized_(false)
    , fallbackMode_(false)
    , updateInterval_(2000) {
}

ProcessManager::~ProcessManager() {
    shutdown();
}

bool ProcessManager::initialize() {
    if (initialized_) {
        return true;
    }
    
    initialized_ = true;
    return true;
}

void ProcessManager::shutdown() {
    if (!initialized_) {
        return;
    }
    
    running_ = false;
    
    if (processMonitoringThread_.joinable()) {
        processMonitoringThread_.join();
    }
    
    initialized_ = false;
}

bool ProcessManager::start() {
    if (!initialized_) {
        return false;
    }
    
    if (running_) {
        return true;
    }
    
    running_ = true;
    processMonitoringThread_ = std::thread(&ProcessManager::processMonitoringThread, this);
    
    return true;
}

void ProcessManager::stop() {
    running_ = false;
    
    if (processMonitoringThread_.joinable()) {
        processMonitoringThread_.join();
    }
}

void ProcessManager::processMonitoringThread() {
    while (running_) {
        try {
            updateProcessList();
            
            std::this_thread::sleep_for(updateInterval_);
            
        } catch (const std::exception& e) {
            // Log error but continue
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void ProcessManager::updateProcessList() {
    // Temporary simplified implementation
    std::vector<ProcessInfo> processes;
    
    ProcessInfo proc;
    proc.pid = GetCurrentProcessId();
    proc.name = "sysmon_agent";
    proc.cpuUsage = 0.1;
    proc.memoryUsage = 1024 * 1024; // 1MB
    proc.status = "R";
    proc.parentPid = 0;
    proc.user = "system";
    processes.push_back(proc);
    
    std::unique_lock<std::shared_mutex> lock(processMutex_);
    currentProcessList_ = processes;
    lastUpdate_ = std::chrono::steady_clock::now();
}

std::vector<ProcessInfo> ProcessManager::getProcessList() {
    std::shared_lock<std::shared_mutex> lock(processMutex_);
    return currentProcessList_;
}

bool ProcessManager::terminateProcess(uint32_t pid) {
    if (isCriticalProcess(pid)) {
        return false; // Don't allow terminating critical processes
    }
    
#ifdef _WIN32
    return terminateProcessWindows(pid);
#else
    return terminateProcessLinux(pid);
#endif
}

bool ProcessManager::killProcess(uint32_t pid) {
    if (isCriticalProcess(pid)) {
        return false; // Don't allow killing critical processes
    }
    
#ifdef _WIN32
    return killProcessWindows(pid);
#else
    return killProcessLinux(pid);
#endif
}

bool ProcessManager::isRunning() const {
    return running_;
}

bool ProcessManager::isCriticalProcess(uint32_t pid) const {
    // Simplified implementation - protect system processes
    return pid < 100;
}

// Platform-specific implementations (simplified)
std::vector<ProcessInfo> ProcessManager::getProcessListWindows() {
    std::vector<ProcessInfo> processes;
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return processes;
    }
    
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(hSnapshot, &pe32)) {
        do {
            ProcessInfo info;
            info.pid = pe32.th32ProcessID;
            info.name = pe32.szExeFile;
            info.cpuUsage = 0.0; // Would need performance counters for real CPU usage
            info.memoryUsage = 0; // Would need additional queries
            info.status = "Running";
            
            processes.push_back(info);
        } while (Process32Next(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    return processes;
}

#ifndef _WIN32
std::vector<ProcessInfo> ProcessManager::getProcessListLinux() {
    std::vector<ProcessInfo> processes;
    
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        return processes;
    }
    
    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != nullptr) {
        if (!isdigit(entry->d_name[0])) {
            continue;
        }
        
        uint32_t pid = std::stoul(entry->d_name);
        
        // Read process name from /proc/[pid]/comm
        std::string comm_path = "/proc/" + std::string(entry->d_name) + "/comm";
        std::ifstream comm_file(comm_path);
        std::string name;
        if (comm_file.is_open()) {
            std::getline(comm_file, name);
            // Remove trailing newline
            if (!name.empty() && name.back() == '\n') {
                name.pop_back();
            }
        } else {
            name = "Unknown";
        }
        
        ProcessInfo info;
        info.pid = pid;
        info.name = name.empty() ? "Unknown" : name;
        info.cpuUsage = 0.0; // Would need /proc/stat for real CPU usage
        info.memoryUsage = 0; // Would need /proc/[pid]/statm for real memory
        info.status = "Running";
        
        processes.push_back(info);
    }
    
    closedir(proc_dir);
    return processes;
}
#endif

bool ProcessManager::terminateProcessWindows(uint32_t pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) {
        return false;
    }
    
    bool result = TerminateProcess(hProcess, 1);
    DWORD error = GetLastError();
    CloseHandle(hProcess);
    
    return result;
}

#ifndef _WIN32
bool ProcessManager::terminateProcessLinux(uint32_t pid) {
    // Safe PID conversion check
    if (pid > std::numeric_limits<pid_t>::max()) {
        return false;
    }
    
    pid_t linuxPid = static_cast<pid_t>(pid);
    return kill(linuxPid, SIGTERM) == 0;
}
#endif

bool ProcessManager::killProcessWindows(uint32_t pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) {
        return false;
    }
    
    bool result = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return result;
}

#ifndef _WIN32
bool ProcessManager::killProcessLinux(uint32_t pid) {
    // Safe PID conversion check
    if (pid > std::numeric_limits<pid_t>::max()) {
        return false;
    }
    
    pid_t linuxPid = static_cast<pid_t>(pid);
    return kill(linuxPid, SIGKILL) == 0;
}
#endif

ProcessInfo ProcessManager::getProcessInfoWindows(uint32_t pid) {
    ProcessInfo info;
    info.pid = pid;
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        info.name = "Unknown";
        info.status = "Error";
        return info;
    }
    
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == pid) {
                info.name = pe32.szExeFile;
                info.cpuUsage = 0.0;
                info.memoryUsage = 0;
                info.status = "Running";
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    
    if (info.name.empty()) {
        info.name = "Unknown";
        info.status = "Not Found";
    }
    
    return info;
}

ProcessInfo ProcessManager::getProcessInfoLinux(uint32_t pid) {
    ProcessInfo info;
    info.pid = pid;
    
    // Read process name from /proc/[pid]/comm
    std::string comm_path = "/proc/" + std::to_string(pid) + "/comm";
    std::ifstream comm_file(comm_path);
    if (comm_file.is_open()) {
        std::getline(comm_file, info.name);
        if (!info.name.empty() && info.name.back() == '\n') {
            info.name.pop_back();
        }
        info.status = "Running";
    } else {
        info.name = "Unknown";
        info.status = "Not Found";
    }
    
    info.cpuUsage = 0.0;
    info.memoryUsage = 0;
    
    return info;
}

std::string ProcessManager::getProcessUser(uint32_t pid) {
    (void)pid;
    return "system";
}

// Fallback mode support
void ProcessManager::enableFallbackMode() {
    fallbackMode_ = true;
    initialized_ = true;
    
    // Clear process list in fallback mode
    std::lock_guard<std::shared_mutex> lock(processMutex_);
    currentProcessList_.clear();
    criticalProcesses_.clear();
}

bool ProcessManager::isFallbackMode() const {
    return fallbackMode_;
}

} // namespace SysMon
