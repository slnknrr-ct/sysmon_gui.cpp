#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>

namespace SysMon {

// Basic system information structures
struct SystemInfo {
    double cpuUsageTotal;
    std::vector<double> cpuCoresUsage;
    uint64_t memoryTotal;
    uint64_t memoryUsed;
    uint64_t memoryFree;
    uint64_t memoryCache;
    uint64_t memoryBuffers;
    uint32_t processCount;
    uint32_t threadCount;
    uint64_t contextSwitches;
    std::chrono::seconds uptime;
    
    SystemInfo();
};

struct ProcessInfo {
    uint32_t pid;
    std::string name;
    double cpuUsage;
    uint64_t memoryUsage;
    std::string status;
    uint32_t parentPid;
    std::string user;
    
    ProcessInfo();
};

struct NetworkInterface {
    std::string name;
    std::string ipv4;
    std::string ipv6;
    bool isEnabled;
    uint64_t rxBytes;
    uint64_t txBytes;
    double rxSpeed;
    double txSpeed;
    
    NetworkInterface();
};

struct UsbDevice {
    std::string vid;
    std::string pid;
    std::string name;
    std::string serialNumber;
    bool isConnected;
    bool isEnabled;
    
    UsbDevice();
};

struct AndroidDeviceInfo {
    std::string model;
    std::string androidVersion;
    std::string serialNumber;
    int batteryLevel;
    bool isScreenOn;
    bool isLocked;
    std::string foregroundApp;
    
    AndroidDeviceInfo();
};

struct AutomationRule {
    std::string id;
    std::string condition;
    std::string action;
    bool isEnabled;
    std::chrono::seconds duration;
    
    AutomationRule();
};

// Common enums
enum class LogLevel {
    INFO,
    WARNING,
    ERROR
};

enum class CommandStatus {
    SUCCESS,
    FAILED,
    PENDING
};

} // namespace SysMon
