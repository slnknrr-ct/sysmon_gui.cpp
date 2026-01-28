#include "serializer.h"
#include "../shared/constants.h"
#include <iomanip>
#include <algorithm>

namespace SysMon {
namespace Serialization {

// Serializer implementation
Serializer& Serializer::getInstance() {
    static Serializer instance;
    return instance;
}

std::string Serializer::serializeSystemInfo(const SystemInfo& info) {
    if (!validateSystemInfo(info)) {
        return "{}";
    }
    
    StringBuilder builder(512);
    builder.append("{");
    
    builder.escapeAndAppend("\"cpu_total\":").append(info.cpuUsageTotal).append(",");
    builder.escapeAndAppend("\"memory_total\":").append(info.memoryTotal).append(",");
    builder.escapeAndAppend("\"memory_used\":").append(info.memoryUsed).append(",");
    builder.escapeAndAppend("\"memory_free\":").append(info.memoryFree).append(",");
    builder.escapeAndAppend("\"memory_cache\":").append(info.memoryCache).append(",");
    builder.escapeAndAppend("\"memory_buffers\":").append(info.memoryBuffers).append(",");
    builder.escapeAndAppend("\"process_count\":").append(info.processCount).append(",");
    builder.escapeAndAppend("\"thread_count\":").append(info.threadCount).append(",");
    builder.escapeAndAppend("\"context_switches\":").append(info.contextSwitches).append(",");
    builder.escapeAndAppend("\"uptime_seconds\":").append(static_cast<uint64_t>(info.uptime.count())).append(",");
    
    // CPU cores usage
    builder.escapeAndAppend("\"cpu_cores\":[");
    for (size_t i = 0; i < info.cpuCoresUsage.size(); ++i) {
        if (i > 0) builder.append(",");
        builder.append(info.cpuCoresUsage[i]);
    }
    builder.append("]}");
    
    return builder.toString();
}

std::string Serializer::serializeProcessList(const std::vector<ProcessInfo>& processes) {
    StringBuilder builder(4096);
    builder.append("{");
    builder.escapeAndAppend("\"process_count\":").append(processes.size()).append(",");
    builder.escapeAndAppend("\"processes\":[");
    
    const size_t maxProcesses = std::min(processes.size(), static_cast<size_t>(100)); // Limit to 100
    for (size_t i = 0; i < maxProcesses; ++i) {
        const auto& proc = processes[i];
        if (!validateProcessInfo(proc)) continue;
        
        if (i > 0) builder.append(",");
        builder.append("{");
        builder.escapeAndAppend("\"pid\":").append(proc.pid).append(",");
        builder.escapeAndAppend("\"name\":\"").escapeAndAppend(proc.name).append("\",");
        builder.escapeAndAppend("\"cpu_usage\":").append(proc.cpuUsage).append(",");
        builder.escapeAndAppend("\"memory_usage\":").append(proc.memoryUsage).append(",");
        builder.escapeAndAppend("\"status\":\"").escapeAndAppend(proc.status).append("\",");
        builder.escapeAndAppend("\"parent_pid\":").append(proc.parentPid).append(",");
        builder.escapeAndAppend("\"user\":\"").escapeAndAppend(proc.user).append("\"");
        builder.append("}");
    }
    builder.append("]}");
    
    return builder.toString();
}

std::string Serializer::serializeDeviceList(const std::vector<UsbDevice>& devices) {
    StringBuilder builder(2048);
    builder.append("{");
    builder.escapeAndAppend("\"device_count\":").append(devices.size()).append(",");
    builder.escapeAndAppend("\"devices\":[");
    
    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& device = devices[i];
        if (!validateUsbDevice(device)) continue;
        
        if (i > 0) builder.append(",");
        builder.append("{");
        builder.escapeAndAppend("\"vid\":\"").escapeAndAppend(device.vid).append("\",");
        builder.escapeAndAppend("\"pid\":\"").escapeAndAppend(device.pid).append("\",");
        builder.escapeAndAppend("\"name\":\"").escapeAndAppend(device.name).append("\",");
        builder.escapeAndAppend("\"serial\":\"").escapeAndAppend(device.serialNumber).append("\",");
        builder.escapeAndAppend("\"connected\":").append(device.isConnected).append(",");
        builder.escapeAndAppend("\"enabled\":").append(device.isEnabled);
        builder.append("}");
    }
    builder.append("]}");
    
    return builder.toString();
}

std::string Serializer::serializeNetworkInterfaces(const std::vector<NetworkInterface>& interfaces) {
    StringBuilder builder(2048);
    builder.append("{");
    builder.escapeAndAppend("\"interface_count\":").append(interfaces.size()).append(",");
    builder.escapeAndAppend("\"interfaces\":[");
    
    for (size_t i = 0; i < interfaces.size(); ++i) {
        const auto& iface = interfaces[i];
        if (!validateNetworkInterface(iface)) continue;
        
        if (i > 0) builder.append(",");
        builder.append("{");
        builder.escapeAndAppend("\"name\":\"").escapeAndAppend(iface.name).append("\",");
        builder.escapeAndAppend("\"ipv4\":\"").escapeAndAppend(iface.ipv4).append("\",");
        builder.escapeAndAppend("\"ipv6\":\"").escapeAndAppend(iface.ipv6).append("\",");
        builder.escapeAndAppend("\"enabled\":").append(iface.isEnabled).append(",");
        builder.escapeAndAppend("\"rx_bytes\":").append(iface.rxBytes).append(",");
        builder.escapeAndAppend("\"tx_bytes\":").append(iface.txBytes).append(",");
        builder.escapeAndAppend("\"rx_speed\":").append(iface.rxSpeed).append(",");
        builder.escapeAndAppend("\"tx_speed\":").append(iface.txSpeed);
        builder.append("}");
    }
    builder.append("]}");
    
    return builder.toString();
}

std::string Serializer::serializeAndroidDevices(const std::vector<AndroidDeviceInfo>& devices) {
    StringBuilder builder(2048);
    builder.append("{");
    builder.escapeAndAppend("\"device_count\":").append(devices.size()).append(",");
    builder.escapeAndAppend("\"devices\":[");
    
    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& device = devices[i];
        if (!validateAndroidDeviceInfo(device)) continue;
        
        if (i > 0) builder.append(",");
        builder.append("{");
        builder.escapeAndAppend("\"model\":\"").escapeAndAppend(device.model).append("\",");
        builder.escapeAndAppend("\"version\":\"").escapeAndAppend(device.androidVersion).append("\",");
        builder.escapeAndAppend("\"serial\":\"").escapeAndAppend(device.serialNumber).append("\",");
        builder.escapeAndAppend("\"battery\":").append(static_cast<uint32_t>(device.batteryLevel)).append(",");
        builder.escapeAndAppend("\"screen_on\":").append(device.isScreenOn).append(",");
        builder.escapeAndAppend("\"locked\":").append(device.isLocked).append(",");
        builder.escapeAndAppend("\"foreground_app\":\"").escapeAndAppend(device.foregroundApp).append("\"");
        builder.append("}");
    }
    builder.append("]}");
    
    return builder.toString();
}

std::string Serializer::serializeAutomationRules(const std::vector<AutomationRule>& rules) {
    StringBuilder builder(2048);
    builder.append("{");
    builder.escapeAndAppend("\"rule_count\":").append(rules.size()).append(",");
    builder.escapeAndAppend("\"rules\":[");
    
    for (size_t i = 0; i < rules.size(); ++i) {
        const auto& rule = rules[i];
        if (!validateAutomationRule(rule)) continue;
        
        if (i > 0) builder.append(",");
        builder.append("{");
        builder.escapeAndAppend("\"id\":\"").escapeAndAppend(rule.id).append("\",");
        builder.escapeAndAppend("\"condition\":\"").escapeAndAppend(rule.condition).append("\",");
        builder.escapeAndAppend("\"action\":\"").escapeAndAppend(rule.action).append("\",");
        builder.escapeAndAppend("\"enabled\":").append(rule.isEnabled).append(",");
        builder.escapeAndAppend("\"duration\":").append(static_cast<uint64_t>(rule.duration.count()));
        builder.append("}");
    }
    builder.append("]}");
    
    return builder.toString();
}

std::string Serializer::escapeJsonString(const std::string& str) {
    return Security::Validation::escapeJsonString(str);
}

std::string Serializer::formatJsonValue(const std::string& key, const std::string& value) {
    StringBuilder builder(256);
    builder.escapeAndAppend("\"").escapeAndAppend(key).escapeAndAppend("\":\"").escapeAndAppend(value).append("\"");
    return builder.toString();
}

std::string Serializer::formatJsonValue(const std::string& key, double value) {
    StringBuilder builder(128);
    builder.escapeAndAppend("\"").escapeAndAppend(key).escapeAndAppend("\":").append(value);
    return builder.toString();
}

std::string Serializer::formatJsonValue(const std::string& key, uint64_t value) {
    StringBuilder builder(128);
    builder.escapeAndAppend("\"").escapeAndAppend(key).escapeAndAppend("\":").append(value);
    return builder.toString();
}

std::string Serializer::formatJsonValue(const std::string& key, uint32_t value) {
    StringBuilder builder(128);
    builder.escapeAndAppend("\"").escapeAndAppend(key).escapeAndAppend("\":").append(value);
    return builder.toString();
}

std::string Serializer::formatJsonValue(const std::string& key, bool value) {
    StringBuilder builder(128);
    builder.escapeAndAppend("\"").escapeAndAppend(key).escapeAndAppend("\":").append(value ? "true" : "false");
    return builder.toString();
}

void Serializer::clearCache() {
    resetPool();
}

size_t Serializer::getCacheSize() const {
    return stringPool_.size();
}

std::string& Serializer::getPooledString() const {
    if (stringPool_.size() <= poolIndex_) {
        stringPool_.emplace_back();
        poolIndex_ = stringPool_.size() - 1;
    }
    
    auto& str = stringPool_[poolIndex_];
    str.clear();
    str.reserve(1024); // Reserve space to avoid reallocations
    
    poolIndex_ = (poolIndex_ + 1) % POOL_SIZE;
    return str;
}

void Serializer::resetPool() {
    stringPool_.clear();
    poolIndex_ = 0;
}

bool Serializer::validateSystemInfo(const SystemInfo& info) const {
    return info.isValid();
}

bool Serializer::validateProcessInfo(const ProcessInfo& process) const {
    return process.isValid();
}

bool Serializer::validateUsbDevice(const UsbDevice& device) const {
    return device.isValid();
}

bool Serializer::validateNetworkInterface(const NetworkInterface& interface) const {
    return interface.isValid();
}

bool Serializer::validateAndroidDeviceInfo(const AndroidDeviceInfo& device) const {
    return device.isValid();
}

bool Serializer::validateAutomationRule(const AutomationRule& rule) const {
    return rule.isValid();
}

// StringBuilder implementation
StringBuilder::StringBuilder(size_t initialCapacity) 
    : capacity_(initialCapacity) {
    stream_ = std::make_unique<std::ostringstream>();
    stream_->str().reserve(initialCapacity);
}

StringBuilder& StringBuilder::append(const std::string& str) {
    *stream_ << str;
    return *this;
}

StringBuilder& StringBuilder::append(const char* str) {
    *stream_ << str;
    return *this;
}

StringBuilder& StringBuilder::append(double value) {
    *stream_ << std::fixed << std::setprecision(2) << value;
    return *this;
}

StringBuilder& StringBuilder::append(uint64_t value) {
    *stream_ << value;
    return *this;
}

StringBuilder& StringBuilder::append(uint32_t value) {
    *stream_ << value;
    return *this;
}

StringBuilder& StringBuilder::append(bool value) {
    *stream_ << (value ? "true" : "false");
    return *this;
}

StringBuilder& StringBuilder::escapeAndAppend(const std::string& str) {
    *stream_ << Serialization::Serializer::getInstance().escapeJsonString(str);
    return *this;
}

std::string StringBuilder::toString() {
    return stream_->str();
}

void StringBuilder::clear() {
    stream_->str("");
    stream_->clear();
}

} // namespace Serialization
} // namespace SysMon
