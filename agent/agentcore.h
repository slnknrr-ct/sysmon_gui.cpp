#pragma once

#include "../shared/systemtypes.h"
#include "../shared/commands.h"
#include "../shared/ipcprotocol.h"
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <shared_mutex>

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
    
    // Component access (for automation engine)
    SystemMonitor* getSystemMonitor() const { return systemMonitor_.get(); }
    DeviceManager* getDeviceManager() const { return deviceManager_.get(); }
    NetworkManager* getNetworkManager() const { return networkManager_.get(); }
    ProcessManager* getProcessManager() const { return processManager_.get(); }
    AndroidManager* getAndroidManager() const { return androidManager_.get(); }
    Logger* getLogger() const { return logger_.get(); }

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
    
    // Helper methods
    std::string serializeSystemInfo(const SystemInfo& info);
    std::string serializeProcessList(const std::vector<ProcessInfo>& processes);
    std::string serializeDeviceList(const std::vector<UsbDevice>& devices);
    std::string serializeNetworkInterfaces(const std::vector<NetworkInterface>& interfaces);
    std::string serializeAndroidDevices(const std::vector<AndroidDeviceInfo>& devices);
    std::string serializeAutomationRules(const std::vector<AutomationRule>& rules);
    
    // Data validation
    bool validateParameters(const Command& command, const std::vector<std::string>& requiredParams);
};

} // namespace SysMon
