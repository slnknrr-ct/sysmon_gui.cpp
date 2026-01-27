#include "processmanager.h"
#include "../shared/constants.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <dirent.h>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <userenv.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>
#include <cstring>
#endif

namespace SysMon {

ProcessManager::ProcessManager() 
    : running_(false)
    , initialized_(false)
    , updateInterval_(Constants::DEFAULT_UPDATE_INTERVAL)
    , hIphlpapi_(nullptr)
    , pGetAdaptersAddresses_(nullptr)
    , pGetIfEntry2_(nullptr)
    , pSetIfEntry_(nullptr)
    , netlinkSocket_(-1) {
    
#ifdef _WIN32
    processSnapshot_ = nullptr;
#else
    procStat_ = nullptr;
    procStatus_ = nullptr;
#endif
}

ProcessManager::~ProcessManager() {
    shutdown();
}

bool ProcessManager::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Initialize platform-specific resources
#ifdef _WIN32
    // No special initialization needed for Windows
    // ToolHelp32 will be used on-demand
#else
    // Open /proc files for Linux
    procStat_ = fopen("/proc/stat", "r");
    if (!procStat_) {
        std::cerr << "Failed to open /proc/stat" << std::endl;
        return false;
    }
    
    procStatus_ = fopen("/proc/self/status", "r");
    if (!procStatus_) {
        std::cerr << "Failed to open /proc/self/status" << std::endl;
        fclose(procStat_);
        procStat_ = nullptr;
        return false;
    }
#endif
    
    // Load critical processes list
#ifdef _WIN32
    criticalProcesses_ = getCriticalProcessesWindows();
#else
    criticalProcesses_ = getCriticalProcessesLinux();
#endif
    
    initialized_ = true;
    // Use cerr since logger might not be available yet
    std::cout << "ProcessManager initialized successfully" << std::endl;
    return true;
}

bool ProcessManager::start() {
    if (!initialized_) {
        return false;
    }
    
    if (running_) {
        return true;
    }
    
    running_ = true;
    monitoringThread_ = std::thread(&ProcessManager::processMonitoringThread, this);
    
    return true;
}

void ProcessManager::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
}

void ProcessManager::shutdown() {
    stop();
    
#ifdef _WIN32
    // Clean up Windows resources
    if (processSnapshot_ && processSnapshot_ != INVALID_HANDLE_VALUE) {
        CloseHandle(processSnapshot_);
        processSnapshot_ = nullptr;
    }
#else
    // Close /proc files
    if (procStat_) {
        fclose(procStat_);
        procStat_ = nullptr;
    }
    if (procStatus_) {
        fclose(procStatus_);
        procStatus_ = nullptr;
    }
#endif
    
    initialized_ = false;
}

bool ProcessManager::isRunning() const {
    return running_;
}

std::vector<ProcessInfo> ProcessManager::getProcessList() {
    std::shared_lock<std::shared_mutex> lock(processMutex_);
    return processList_;
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

bool ProcessManager::isCriticalProcess(uint32_t pid) const {
    std::shared_lock<std::shared_mutex> lock(processMutex_);
    return std::find(criticalProcesses_.begin(), criticalProcesses_.end(), pid) != criticalProcesses_.end();
}

void ProcessManager::processMonitoringThread() {
    while (running_) {
        try {
            updateProcessList();
            lastUpdate_ = std::chrono::steady_clock::now();
            
            std::this_thread::sleep_for(updateInterval_);
            
        } catch (const std::exception& e) {
            // Log error but continue
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void ProcessManager::updateProcessList() {
    std::vector<ProcessInfo> processes;
    
#ifdef _WIN32
    processes = getProcessListWindows();
#else
    processes = getProcessListLinux();
#endif
    
    std::unique_lock<std::shared_mutex> lock(processMutex_);
    processList_ = processes;
}

std::vector<ProcessInfo> ProcessManager::getProcessListLinux() {
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
        if (procInfo.pid > 0 && !procInfo.name.empty()) {
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
                
                // Get user
                HANDLE hToken;
                if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
                    TOKEN_USER* pTokenUser = nullptr;
                    DWORD dwSize = 0;
                    
                    if (GetTokenInformation(hToken, TokenUser, nullptr, 0, &dwSize) == ERROR_INSUFFICIENT_BUFFER) {
                        pTokenUser = (TOKEN_USER*)malloc(dwSize);
                        if (GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize)) {
                            if (pTokenUser->User.Sid) {
                                WCHAR userName[256];
                                DWORD userNameSize = sizeof(userName) / sizeof(WCHAR);
                                WCHAR domainName[256];
                                DWORD domainNameSize = sizeof(domainName) / sizeof(WCHAR);
                                SID_NAME_USE sidUse;
                                
                                if (LookupAccountSid(nullptr, pTokenUser->User.Sid, userName, &userNameSize,
                                                   domainName, &domainNameSize, &sidUse)) {
                                    WideCharToMultiByte(CP_ACP, 0, userName, -1, name, MAX_PATH, nullptr, nullptr);
                                    procInfo.user = name;
                                } else {
                                    procInfo.user = "UNKNOWN";
                                }
                            } else {
                                procInfo.user = "UNKNOWN";
                            }
                        }
                        free(pTokenUser);
                    }
                    CloseHandle(hToken);
                } else {
                    procInfo.user = "UNKNOWN";
                }
                
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

bool ProcessManager::terminateProcessLinux(uint32_t pid) {
    // Safe PID conversion check
    if (pid > std::numeric_limits<pid_t>::max()) {
        return false;
    }
    
    pid_t linuxPid = static_cast<pid_t>(pid);
    return kill(linuxPid, SIGTERM) == 0;
}

bool ProcessManager::terminateProcessWindows(uint32_t pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess) {
        bool result = TerminateProcess(hProcess, 1);
        DWORD error = GetLastError();
        CloseHandle(hProcess);
        return result;
    }
    return false;
}

bool ProcessManager::killProcessLinux(uint32_t pid) {
    // Safe PID conversion check
    if (pid > std::numeric_limits<pid_t>::max()) {
        return false;
    }
    
    pid_t linuxPid = static_cast<pid_t>(pid);
    return kill(linuxPid, SIGKILL) == 0;
}

bool ProcessManager::killProcessWindows(uint32_t pid) {
    // For Windows, kill is the same as terminate but more forceful
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess) {
        bool result = TerminateProcess(hProcess, 1);
        CloseHandle(hProcess);
        return result;
    }
    return false;
}

std::vector<uint32_t> ProcessManager::getCriticalProcessesLinux() {
    std::vector<uint32_t> critical;
    
    // Add critical Linux processes
    critical.push_back(1);    // init/systemd
    critical.push_back(2);    // kthreadd
    
    return critical;
}

std::vector<uint32_t> ProcessManager::getCriticalProcessesWindows() {
    std::vector<uint32_t> critical;
    
    // Add critical Windows processes
    critical.push_back(4);     // System
    critical.push_back(8);     // System process (varies)
    critical.push_back(12);    // Windows Session Manager
    critical.push_back(20);    // Winlogon
    critical.push_back(28);    // csrss.exe
    critical.push_back(36);    // services.exe
    critical.push_back(44);    // lsass.exe
    critical.push_back(52);    // svchost.exe instances
    critical.push_back(68);    // dwm.exe
    critical.push_back(76);    // explorer.exe (user shell)
    
    // Add common system process ranges
    for (uint32_t i = 1; i <= 200; ++i) {
        critical.push_back(i);
    }
    
    return critical;
}

bool ProcessManager::isSystemCritical(uint32_t pid) const {
    return isCriticalProcess(pid);
}

ProcessInfo ProcessManager::getProcessInfoLinux(uint32_t pid) {
    ProcessInfo info;
    info.pid = pid;
    
    std::string pidStr = std::to_string(pid);
    std::string statPath = "/proc/" + pidStr + "/stat";
    std::string statusPath = "/proc/" + pidStr + "/status";
    std::string cmdlinePath = "/proc/" + pidStr + "/cmdline";
    
    // Get process name from cmdline
    std::ifstream cmdlineFile(cmdlinePath);
    if (cmdlineFile.is_open()) {
        std::string cmdline;
        std::getline(cmdlineFile, cmdline);
        if (!cmdline.empty()) {
            // Extract first part of cmdline as name
            size_t pos = cmdline.find(' ');
            if (pos != std::string::npos) {
                info.name = cmdline.substr(0, pos);
            } else {
                info.name = cmdline;
            }
            // Remove path if present
            size_t slashPos = info.name.find_last_of('/');
            if (slashPos != std::string::npos) {
                info.name = info.name.substr(slashPos + 1);
            }
        }
        cmdlineFile.close();
    }
    
    // Get detailed info from stat
    std::ifstream statFile(statPath);
    if (statFile.is_open()) {
        std::string line;
        std::getline(statFile, line);
        std::istringstream iss(line);
        std::string token;
        
        // Skip pid (already have it)
        iss >> token;
        
        // Get comm (command name) if not found from cmdline
        if (info.name.empty()) {
            iss >> token;
            // Remove parentheses
            if (token.length() >= 2 && token.front() == '(' && token.back() == ')') {
                info.name = token.substr(1, token.length() - 2);
            }
        }
        
        // Get state
        char state;
        iss >> state;
        info.status = std::string(1, state);
        
        // Get parent PID
        iss >> info.parentPid;
        
        statFile.close();
    }
    
    // Get user info from status
    std::ifstream statusFile(statusPath);
    if (statusFile.is_open()) {
        std::string line;
        while (std::getline(statusFile, line)) {
            if (line.substr(0, 5) == "Uid:") {
                std::istringstream iss(line);
                std::string uidLabel;
                uint32_t uid;
                iss >> uidLabel >> uid;
                
                // Get username from UID
                struct passwd* pw = getpwuid(uid);
                if (pw) {
                    info.user = pw->pw_name;
                } else {
                    info.user = std::to_string(uid);
                }
                break;
            }
        }
        statusFile.close();
    }
    
    // Get memory usage from status
    std::ifstream memFile(statusPath);
    if (memFile.is_open()) {
        std::string line;
        while (std::getline(memFile, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                std::istringstream iss(line);
                std::string vmrssLabel;
                uint64_t memoryKb;
                iss >> vmrssLabel >> memoryKb;
                info.memoryUsage = memoryKb * 1024; // Convert to bytes
                break;
            }
        }
        memFile.close();
    }
    
    // Calculate CPU usage
    info.cpuUsage = calculateCpuUsage(pid);
    
    // Set default values if not found
    if (info.name.empty()) info.name = "unknown";
    if (info.status.empty()) info.status = "unknown";
    if (info.user.empty()) info.user = "unknown";
    
    return info;
}

ProcessInfo ProcessManager::getProcessInfoWindows(uint32_t pid) {
    ProcessInfo info;
    info.pid = pid;
    
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) {
        info.name = "unknown";
        info.cpuUsage = 0.0;
        info.memoryUsage = 0;
        info.status = "unknown";
        info.parentPid = 0;
        info.user = "unknown";
        return info;
    }
    
    // Get process name
    WCHAR processName[MAX_PATH];
    DWORD bufferSize = MAX_PATH;
    if (QueryFullProcessImageNameW(hProcess, 0, processName, &bufferSize)) {
        char name[MAX_PATH];
        WideCharToMultiByte(CP_ACP, 0, processName, -1, name, MAX_PATH, nullptr, nullptr);
        // Extract just the filename
        std::string fullName(name);
        size_t slashPos = fullName.find_last_of("\\/");
        if (slashPos != std::string::npos) {
            info.name = fullName.substr(slashPos + 1);
        } else {
            info.name = fullName;
        }
    } else {
        info.name = "unknown";
    }
    
    // Get memory usage
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        info.memoryUsage = pmc.WorkingSetSize;
    } else {
        info.memoryUsage = 0;
    }
    
    // Get CPU usage
    FILETIME ftCreate, ftExit, ftKernel, ftUser;
    if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
        ULARGE_INTEGER kernelTime, userTime;
        kernelTime.LowPart = ftKernel.dwLowDateTime;
        kernelTime.HighPart = ftKernel.dwHighDateTime;
        userTime.LowPart = ftUser.dwLowDateTime;
        userTime.HighPart = ftUser.dwHighDateTime;
        
        uint64_t totalTime = kernelTime.QuadPart + userTime.QuadPart;
        info.cpuUsage = static_cast<double>(totalTime) / 10000000.0; // Convert to seconds
    } else {
        info.cpuUsage = 0.0;
    }
    
    // Get process status
    DWORD exitCode;
    if (GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
        info.status = "Running";
    } else {
        info.status = "Zombie";
    }
    
    // Get parent PID from process snapshot
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        
        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (pe32.th32ProcessID == pid) {
                    info.parentPid = pe32.th32ParentProcessID;
                    break;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    
    // Get user information
    HANDLE hToken;
    if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        TOKEN_USER* pTokenUser = nullptr;
        DWORD dwSize = 0;
        
        if (GetTokenInformation(hToken, TokenUser, nullptr, 0, &dwSize) == ERROR_INSUFFICIENT_BUFFER) {
            pTokenUser = (TOKEN_USER*)malloc(dwSize);
            if (GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize)) {
                if (pTokenUser->User.Sid) {
                    WCHAR userName[256];
                    DWORD userNameSize = sizeof(userName) / sizeof(WCHAR);
                    WCHAR domainName[256];
                    DWORD domainNameSize = sizeof(domainName) / sizeof(WCHAR);
                    SID_NAME_USE sidUse;
                    
                    if (LookupAccountSid(nullptr, pTokenUser->User.Sid, userName, &userNameSize,
                                       domainName, &domainNameSize, &sidUse)) {
                        char name[256];
                        WideCharToMultiByte(CP_ACP, 0, userName, -1, name, 256, nullptr, nullptr);
                        info.user = name;
                    } else {
                        info.user = "UNKNOWN";
                    }
                } else {
                    info.user = "UNKNOWN";
                }
            }
            free(pTokenUser);
        }
        CloseHandle(hToken);
    } else {
        info.user = "UNKNOWN";
    }
    
    CloseHandle(hProcess);
    return info;
}

double ProcessManager::calculateCpuUsage(uint32_t pid) {
    std::string pidStr = std::to_string(pid);
    std::string statPath = "/proc/" + pidStr + "/stat";
    std::string uptimePath = "/proc/uptime";
    
    static std::unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> lastCpuTimes;
    static std::chrono::steady_clock::time_point lastUpdate = std::chrono::steady_clock::now();
    
    std::ifstream statFile(statPath);
    if (!statFile.is_open()) {
        return 0.0;
    }
    
    std::string line;
    std::getline(statFile, line);
    std::istringstream iss(line);
    
    // Skip pid and comm
    std::string token;
    iss >> token; // pid
    iss >> token; // comm (may contain spaces and parentheses)
    
    // Find the closing parenthesis
    if (token.front() == '(') {
        while (token.back() != ')' && iss >> token) {
            // Keep reading until we find the closing parenthesis
        }
    }
    
    // Skip state, ppid, pgrp, session, tty_nr, tpgid
    for (int i = 0; i < 6; ++i) {
        iss >> token;
    }
    
    // Read CPU times
    uint64_t utime, stime, cutime, cstime;
    iss >> utime >> stime >> cutime >> cstime;
    
    uint64_t totalTime = utime + stime + cutime + cstime;
    
    statFile.close();
    
    // Get system uptime
    std::ifstream uptimeFile(uptimePath);
    if (!uptimeFile.is_open()) {
        return 0.0;
    }
    
    double uptime;
    uptimeFile >> uptime;
    uptimeFile.close();
    
    auto now = std::chrono::steady_clock::now();
    auto timeDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();
    
    if (timeDiff < 1000) { // Less than 1 second since last update
        auto it = lastCpuTimes.find(pid);
        if (it != lastCpuTimes.end()) {
            uint64_t timeDiff = totalTime - it->second.first;
            double cpuPercent = (timeDiff * 100.0) / std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();
            return std::min(cpuPercent, 100.0);
        }
    }
    
    lastCpuTimes[pid] = {totalTime, static_cast<uint64_t>(uptime * 1000)};
    lastUpdate = now;
    
    return 0.0; // First call, return 0
}

uint64_t ProcessManager::getProcessMemoryUsage(uint32_t pid) {
    std::string pidStr = std::to_string(pid);
    std::string statusPath = "/proc/" + pidStr + "/status";
    
    std::ifstream statusFile(statusPath);
    if (!statusFile.is_open()) {
        return 0;
    }
    
    std::string line;
    while (std::getline(statusFile, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line);
            std::string vmrssLabel;
            uint64_t memoryKb;
            iss >> vmrssLabel >> memoryKb;
            statusFile.close();
            return memoryKb * 1024; // Convert KB to bytes
        }
    }
    
    statusFile.close();
    return 0;
}

std::string ProcessManager::getProcessStatus(uint32_t pid) {
    std::string pidStr = std::to_string(pid);
    std::string statPath = "/proc/" + pidStr + "/stat";
    
    std::ifstream statFile(statPath);
    if (!statFile.is_open()) {
        return "unknown";
    }
    
    std::string line;
    std::getline(statFile, line);
    std::istringstream iss(line);
    
    // Skip pid and comm
    std::string token;
    iss >> token; // pid
    iss >> token; // comm (may contain spaces and parentheses)
    
    // Find the closing parenthesis
    if (token.front() == '(') {
        while (token.back() != ')' && iss >> token) {
            // Keep reading until we find the closing parenthesis
        }
    }
    
    // Get state
    char state;
    iss >> state;
    
    statFile.close();
    
    // Convert state to human-readable format
    switch (state) {
        case 'R': return "Running";
        case 'S': return "Sleeping";
        case 'D': return "Disk Sleep";
        case 'Z': return "Zombie";
        case 'T': return "Stopped";
        case 't': return "Tracing Stop";
        case 'X': return "Dead";
        case 'x': return "Dead";
        case 'K': return "Wakekill";
        case 'W': return "Waking";
        case 'P': return "Parked";
        default: return std::string(1, state);
    }
}

std::string ProcessManager::getProcessUser(uint32_t pid) {
    std::string pidStr = std::to_string(pid);
    std::string statusPath = "/proc/" + pidStr + "/status";
    
    std::ifstream statusFile(statusPath);
    if (!statusFile.is_open()) {
        return "unknown";
    }
    
    std::string line;
    while (std::getline(statusFile, line)) {
        if (line.substr(0, 5) == "Uid:") {
            std::istringstream iss(line);
            std::string uidLabel;
            uint32_t uid;
            iss >> uidLabel >> uid;
            statusFile.close();
            
            // Get username from UID
            struct passwd* pw = getpwuid(uid);
            if (pw) {
                return std::string(pw->pw_name);
            } else {
                return std::to_string(uid);
            }
        }
    }
    
    statusFile.close();
    return "unknown";
}

uint32_t ProcessManager::getParentPid(uint32_t pid) {
    std::string pidStr = std::to_string(pid);
    std::string statPath = "/proc/" + pidStr + "/stat";
    
    std::ifstream statFile(statPath);
    if (!statFile.is_open()) {
        return 0;
    }
    
    std::string line;
    std::getline(statFile, line);
    std::istringstream iss(line);
    
    // Skip pid and comm
    std::string token;
    iss >> token; // pid
    iss >> token; // comm (may contain spaces and parentheses)
    
    // Find the closing parenthesis
    if (token.front() == '(') {
        while (token.back() != ')' && iss >> token) {
            // Keep reading until we find the closing parenthesis
        }
    }
    
    // Skip state
    iss >> token;
    
    // Get parent PID
    uint32_t ppid;
    iss >> ppid;
    
    statFile.close();
    return ppid;
}

} // namespace SysMon
