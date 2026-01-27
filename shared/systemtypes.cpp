#include "systemtypes.h"
#include <sstream>
#include <iomanip>

namespace SysMon {

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

// Implementation of ProcessInfo methods
ProcessInfo::ProcessInfo()
    : pid(0)
    , cpuUsage(0.0)
    , memoryUsage(0)
    , parentPid(0) {
}

// Implementation of NetworkInterface methods
NetworkInterface::NetworkInterface()
    : isEnabled(false)
    , rxBytes(0)
    , txBytes(0)
    , rxSpeed(0.0)
    , txSpeed(0.0) {
}

// Implementation of UsbDevice methods
UsbDevice::UsbDevice()
    : isConnected(false)
    , isEnabled(false) {
}

// Implementation of AndroidDeviceInfo methods
AndroidDeviceInfo::AndroidDeviceInfo()
    : batteryLevel(0)
    , isScreenOn(false)
    , isLocked(false) {
}

// Implementation of AutomationRule methods
AutomationRule::AutomationRule()
    : isEnabled(false)
    , duration(0) {
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
