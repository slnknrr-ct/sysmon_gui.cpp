#include "systemtypes.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <chrono>

namespace SysMon {

// Validation namespace implementation
namespace Validation {

bool isValidCpuUsage(double usage) {
    return usage >= 0.0 && usage <= 100.0;
}

bool isValidMemoryValue(uint64_t value) {
    return value <= (1ULL << 40); // Max 1TB
}

bool isValidProcessId(uint32_t pid) {
    return pid > 0 && pid <= 4294967295U;
}

bool isValidPercentage(double value) {
    return value >= 0.0 && value <= 100.0;
}

bool isValidPort(int port) {
    return port > 0 && port <= 65535;
}

bool isValidNonEmptyString(const std::string& str) {
    return !str.empty() && str.length() <= 256;
}

bool isValidVidPid(const std::string& vidPid) {
    std::regex pattern("^[0-9A-Fa-f]{4}:[0-9A-Fa-f]{4}$");
    return std::regex_match(vidPid, pattern);
}

bool isValidIpAddress(const std::string& ip) {
    std::regex ipv4Pattern("^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
    std::regex ipv6Pattern("^(?:[0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$");
    return std::regex_match(ip, ipv4Pattern) || std::regex_match(ip, ipv6Pattern);
}

bool isValidAndroidSerial(const std::string& serial) {
    return !serial.empty() && serial.length() <= 32 && 
           std::all_of(serial.begin(), serial.end(), ::isalnum);
}

bool isValidRuleId(const std::string& ruleId) {
    return !ruleId.empty() && ruleId.length() <= 64 &&
           std::all_of(ruleId.begin(), ruleId.end(), [](char c) {
               return std::isalnum(c) || c == '_' || c == '-';
           });
}

} // namespace Validation

// Implementation of SystemInfo methods
SystemInfo::SystemInfo() 
    : cpuUsageTotal(0.0)
    , memoryTotal(0)
    , memoryUsed(0)
    , memoryFree(0)
    , memoryCache(0)
    , memoryBuffers(0)
    , processCount(0)
    , threadCount(0)
    , contextSwitches(0)
    , uptime(0) {
}

bool SystemInfo::isValid() const {
    return Validation::isValidCpuUsage(cpuUsageTotal) &&
           Validation::isValidMemoryValue(memoryTotal) &&
           Validation::isValidMemoryValue(memoryUsed) &&
           Validation::isValidMemoryValue(memoryFree) &&
           Validation::isValidMemoryValue(memoryCache) &&
           Validation::isValidMemoryValue(memoryBuffers) &&
           processCount <= 100000 && // Reasonable limit
           threadCount <= 1000000 &&
           memoryUsed <= memoryTotal; // Logical consistency
}

void SystemInfo::sanitize() {
    // Clamp CPU usage to valid range
    cpuUsageTotal = std::max(0.0, std::min(100.0, cpuUsageTotal));
    
    // Ensure memory consistency
    if (memoryUsed > memoryTotal) {
        memoryUsed = memoryTotal;
    }
    
    // Clamp counts to reasonable limits
    processCount = std::min(100000U, processCount);
    threadCount = std::min(1000000U, threadCount);
    
    // Sanitize CPU cores usage
    for (auto& usage : cpuCoresUsage) {
        usage = std::max(0.0, std::min(100.0, usage));
    }
}

// Implementation of ProcessInfo methods
ProcessInfo::ProcessInfo()
    : pid(0)
    , cpuUsage(0.0)
    , memoryUsage(0)
    , parentPid(0) {
}

bool ProcessInfo::isValid() const {
    return Validation::isValidProcessId(pid) &&
           Validation::isValidCpuUsage(cpuUsage) &&
           Validation::isValidMemoryValue(memoryUsage) &&
           Validation::isValidProcessId(parentPid) &&
           Validation::isValidNonEmptyString(name) &&
           Validation::isValidNonEmptyString(status);
}

void ProcessInfo::sanitize() {
    // Clamp CPU usage
    cpuUsage = std::max(0.0, std::min(100.0, cpuUsage));
    
    // Ensure valid PIDs
    if (pid == 0) pid = 1;
    if (parentPid == 0) parentPid = 1;
    
    // Truncate strings if too long
    if (name.length() > 256) name = name.substr(0, 256);
    if (status.length() > 64) status = status.substr(0, 64);
    if (user.length() > 64) user = user.substr(0, 64);
}

// Implementation of NetworkInterface methods
NetworkInterface::NetworkInterface()
    : isEnabled(false)
    , rxBytes(0)
    , txBytes(0)
    , rxSpeed(0.0)
    , txSpeed(0.0) {
}

bool NetworkInterface::isValid() const {
    return Validation::isValidNonEmptyString(name) &&
           (ipv4.empty() || Validation::isValidIpAddress(ipv4)) &&
           (ipv6.empty() || Validation::isValidIpAddress(ipv6)) &&
           rxSpeed >= 0.0 && txSpeed >= 0.0;
}

void NetworkInterface::sanitize() {
    // Clamp speeds to reasonable values
    rxSpeed = std::max(0.0, rxSpeed);
    txSpeed = std::max(0.0, txSpeed);
    
    // Truncate interface name
    if (name.length() > 32) name = name.substr(0, 32);
}

// Implementation of UsbDevice methods
UsbDevice::UsbDevice()
    : isConnected(false)
    , isEnabled(false) {
}

bool UsbDevice::isValid() const {
    return Validation::isValidVidPid(vid + ":" + pid) &&
           Validation::isValidNonEmptyString(name) &&
           Validation::isValidNonEmptyString(serialNumber);
}

void UsbDevice::sanitize() {
    // Truncate strings if too long
    if (name.length() > 128) name = name.substr(0, 128);
    if (serialNumber.length() > 64) serialNumber = serialNumber.substr(0, 64);
    
    // Ensure VID/PID format
    if (vid.length() != 4) vid = "0000";
    if (pid.length() != 4) pid = "0000";
}

// Implementation of AndroidDeviceInfo methods
AndroidDeviceInfo::AndroidDeviceInfo()
    : batteryLevel(0)
    , isScreenOn(false)
    , isLocked(false) {
}

bool AndroidDeviceInfo::isValid() const {
    return Validation::isValidAndroidSerial(serialNumber) &&
           Validation::isValidNonEmptyString(model) &&
           Validation::isValidNonEmptyString(androidVersion) &&
           batteryLevel >= 0 && batteryLevel <= 100;
}

void AndroidDeviceInfo::sanitize() {
    // Clamp battery level to valid range
    batteryLevel = std::max(0, std::min(100, batteryLevel));
    
    // Truncate strings if too long
    if (model.length() > 64) model = model.substr(0, 64);
    if (androidVersion.length() > 32) androidVersion = androidVersion.substr(0, 32);
    if (foregroundApp.length() > 128) foregroundApp = foregroundApp.substr(0, 128);
}

// Implementation of AutomationRule methods
AutomationRule::AutomationRule()
    : isEnabled(false)
    , duration(0) {
}

bool AutomationRule::isValid() const {
    return Validation::isValidRuleId(id) &&
           Validation::isValidNonEmptyString(condition) &&
           Validation::isValidNonEmptyString(action);
}

void AutomationRule::sanitize() {
    // Truncate strings if too long
    if (condition.length() > 512) condition = condition.substr(0, 512);
    if (action.length() > 512) action = action.substr(0, 512);
    
    // Ensure valid rule ID
    if (id.empty()) {
        id = "rule_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    }
}

// Utility functions for string conversion
std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string commandStatusToString(CommandStatus status) {
    switch (status) {
        case CommandStatus::SUCCESS: return "SUCCESS";
        case CommandStatus::FAILED: return "FAILED";
        case CommandStatus::PENDING: return "PENDING";
        default: return "UNKNOWN";
    }
}

LogLevel stringToLogLevel(const std::string& str) {
    if (str == "INFO") return LogLevel::INFO;
    if (str == "WARNING") return LogLevel::WARNING;
    if (str == "ERROR") return LogLevel::ERROR;
    return LogLevel::INFO; // default
}

CommandStatus stringToCommandStatus(const std::string& str) {
    if (str == "SUCCESS") return CommandStatus::SUCCESS;
    if (str == "FAILED") return CommandStatus::FAILED;
    if (str == "PENDING") return CommandStatus::PENDING;
    return CommandStatus::PENDING; // default
}

} // namespace SysMon
