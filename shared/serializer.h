#pragma once

#include "../shared/systemtypes.h"
#include "../shared/commands.h"
#include "../shared/security.h"
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <memory>

namespace SysMon {
namespace Serialization {

// Efficient serializer with memory pooling
class Serializer {
public:
    static Serializer& getInstance();
    
    // Serialization methods with memory efficiency
    std::string serializeSystemInfo(const SystemInfo& info);
    std::string serializeProcessList(const std::vector<ProcessInfo>& processes);
    std::string serializeDeviceList(const std::vector<UsbDevice>& devices);
    std::string serializeNetworkInterfaces(const std::vector<NetworkInterface>& interfaces);
    std::string serializeAndroidDevices(const std::vector<AndroidDeviceInfo>& devices);
    std::string serializeAutomationRules(const std::vector<AutomationRule>& rules);
    
    // JSON utilities with escape handling
    std::string escapeJsonString(const std::string& str);
    std::string formatJsonValue(const std::string& key, const std::string& value);
    std::string formatJsonValue(const std::string& key, double value);
    std::string formatJsonValue(const std::string& key, uint64_t value);
    std::string formatJsonValue(const std::string& key, uint32_t value);
    std::string formatJsonValue(const std::string& key, bool value);
    
    // Memory pool management
    void clearCache();
    size_t getCacheSize() const;
    
private:
    Serializer() = default;
    ~Serializer() = default;
    Serializer(const Serializer&) = delete;
    Serializer& operator=(const Serializer&) = delete;
    
    // Memory pool for string operations
    mutable std::vector<std::string> stringPool_;
    mutable size_t poolIndex_ = 0;
    static constexpr size_t POOL_SIZE = 100;
    
    // Efficient string building
    std::string& getPooledString() const;
    void resetPool();
    
    // Validation helpers
    bool validateSystemInfo(const SystemInfo& info) const;
    bool validateProcessInfo(const ProcessInfo& process) const;
    bool validateUsbDevice(const UsbDevice& device) const;
    bool validateNetworkInterface(const NetworkInterface& interface) const;
    bool validateAndroidDeviceInfo(const AndroidDeviceInfo& device) const;
    bool validateAutomationRule(const AutomationRule& rule) const;
};

// RAII string builder for efficient concatenation
class StringBuilder {
public:
    StringBuilder(size_t initialCapacity = 1024);
    ~StringBuilder() = default;
    
    StringBuilder& append(const std::string& str);
    StringBuilder& append(const char* str);
    StringBuilder& append(double value);
    StringBuilder& append(uint64_t value);
    StringBuilder& append(uint32_t value);
    StringBuilder& append(bool value);
    
    StringBuilder& escapeAndAppend(const std::string& str);
    
    std::string toString();
    void clear();
    
private:
    std::unique_ptr<std::ostringstream> stream_;
    size_t capacity_;
};

} // namespace Serialization
} // namespace SysMon
