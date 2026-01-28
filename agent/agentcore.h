#pragma once

#include "../shared/systemtypes.h"
#include "../shared/commands.h"
#include "../shared/ipcprotocol.h"
#include "../shared/serializer.h"
#include "../shared/security.h"
#include "../shared/logger.h"
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <shared_mutex>
#include <mutex>

namespace SysMon {

// Forward declarations
class IpcServer;
class SystemMonitor;
class DeviceManager;
class NetworkManager;
class ProcessManager;
class AndroidManager;
class AutomationEngine;
class Logger;
class ConfigManager;

// Agent core - main orchestrator
class AgentCore {
public:
    AgentCore();
    ~AgentCore();
    
    // Lifecycle management
    bool initialize();
    bool start();
    void stop();
    void shutdown();
    
    // Status
    bool isRunning() const;
    std::string getStatus() const;
    
    // Event handling
    void handleEvent(const Event& event);
    void sendEventToClients(const Event& event);
    
    // Component access interface (encapsulated)
    bool getSystemInfo(SystemInfo& info) const;
    std::vector<ProcessInfo> getProcessList() const;
    std::vector<UsbDevice> getUsbDevices() const;
    std::vector<NetworkInterface> getNetworkInterfaces() const;
    std::vector<AndroidDeviceInfo> getAndroidDevices() const;
    std::vector<AutomationRule> getAutomationRules() const;
    
    // Component control interface
    bool terminateProcess(uint32_t pid);
    bool enableUsbDevice(const std::string& vidPid);
    bool disableUsbDevice(const std::string& vidPid);
    bool enableNetworkInterface(const std::string& name);
    bool disableNetworkInterface(const std::string& name);
    bool androidScreenOn(const std::string& serial);
    bool androidScreenOff(const std::string& serial);
    bool androidLockDevice(const std::string& serial);
    std::string androidTakeScreenshot(const std::string& serial);
    bool addAutomationRule(const AutomationRule& rule);
    bool removeAutomationRule(const std::string& ruleId);
    bool enableAutomationRule(const std::string& ruleId);
    bool disableAutomationRule(const std::string& ruleId);

private:
    // Component initialization
    bool initializeComponents();
    void cleanupComponents();
    
    // Main worker thread
    void workerThread();
    
    // Component management
    std::unique_ptr<IpcServer> ipcServer_;
    std::unique_ptr<SystemMonitor> systemMonitor_;
    std::unique_ptr<DeviceManager> deviceManager_;
    std::unique_ptr<NetworkManager> networkManager_;
    std::unique_ptr<ProcessManager> processManager_;
    std::unique_ptr<AndroidManager> androidManager_;
    std::unique_ptr<AutomationEngine> automationEngine_;
    std::unique_ptr<Logger> logger_;
    std::unique_ptr<ConfigManager> configManager_;
    
    // Thread management
    std::thread workerThread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // Thread safety
    mutable std::shared_mutex componentsMutex_;
    mutable std::mutex commandMutex_;
    
    // Serialization
    Serialization::Serializer* serializer_;
    Security::SecurityManager* securityManager_;
    
    // Command processing
    void processCommand(const Command& command);
    Response handleCommand(const Command& command);
    
    // Module-specific command handlers
    Response handleSystemCommand(const Command& command);
    Response handleDeviceCommand(const Command& command);
    Response handleNetworkCommand(const Command& command);
    Response handleProcessCommand(const Command& command);
    Response handleAndroidCommand(const Command& command);
    Response handleAutomationCommand(const Command& command);
    Response handleGenericCommand(const Command& command);
    
    // Helper methods - removed serialize methods (now using Serializer)
    bool validateParameters(const Command& command, const std::vector<std::string>& requiredParams);
    void logCommand(const Command& command, const std::string& status);
    void logError(const std::string& function, const std::exception& e);
};

} // namespace SysMon
