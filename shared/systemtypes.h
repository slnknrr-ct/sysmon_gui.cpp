#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>

namespace SysMon {

// Validation utilities
namespace Validation {
    bool isValidCpuUsage(double usage);
    bool isValidMemoryValue(uint64_t value);
    bool isValidProcessId(uint32_t pid);
    bool isValidPercentage(double value);
    bool isValidPort(int port);
    bool isValidNonEmptyString(const std::string& str);
    bool isValidVidPid(const std::string& vidPid);
    bool isValidIpAddress(const std::string& ip);
    bool isValidAndroidSerial(const std::string& serial);
    bool isValidRuleId(const std::string& ruleId);
}

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
    
    // Validation
    bool isValid() const;
    void sanitize(); // Fix invalid values
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
    
    // Validation
    bool isValid() const;
    void sanitize();
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
    
    // Validation
    bool isValid() const;
    void sanitize();
};

struct UsbDevice {
    std::string vid;
    std::string pid;
    std::string name;
    std::string serialNumber;
    bool isConnected;
    bool isEnabled;
    
    UsbDevice();
    
    // Validation
    bool isValid() const;
    void sanitize();
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
    
    // Validation
    bool isValid() const;
    void sanitize();
};

struct AutomationRule {
    std::string id;
    std::string condition;
    std::string action;
    bool isEnabled;
    std::chrono::seconds duration;
    
    AutomationRule();
    
    // Validation
    bool isValid() const;
    void sanitize();
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
