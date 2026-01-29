#include "agentcore.h"
#include "ipcserver.h"
#include "systemmonitor.h"
#include "devicemanager.h"
#include "networkmanager.h"
#include "processmanager.h"
#include "androidmanager.h"
#include "automationengine.h"
#include "logger.h"
#include "configmanager.h"
#include "../shared/ipcprotocol.h"
#include "../shared/commands.h"
#include "../shared/systemtypes.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <sstream>
#include <iomanip>

namespace SysMon {

AgentCore::AgentCore() 
    : running_(false)
    , initialized_(false)
    , serializer_(&Serialization::Serializer::getInstance())
    , securityManager_(&Security::SecurityManager::getInstance()) {
    
    // Initialize logging
    Logging::LogManager::getInstance().initialize("sysmon_agent.log", Logging::LogLevel::INFO);
    LOG_INFO_CAT("AgentCore", "AgentCore constructor completed");
}

AgentCore::~AgentCore() {
    shutdown();
}

bool AgentCore::initialize() {
    LOG_FUNCTION_INFO("Initializing AgentCore");
    
    if (initialized_) {
        LOG_WARNING_CAT("AgentCore", "AgentCore already initialized");
        return true;
    }
    
    try {
        LOG_INFO_CAT("AgentCore", "Creating logger");
        logger_ = std::make_unique<Logger>();
        if (!logger_->initialize("sysmon_agent.log", LogLevel::INFO)) {
            LOG_ERROR_CAT("AgentCore", "Failed to initialize logger");
            return false;
        }
        
        LOG_INFO_CAT("AgentCore", "Initializing components");
        if (!initializeComponents()) {
            LOG_ERROR_CAT("AgentCore", "Failed to initialize components");
            return false;
        }
        
        initialized_ = true;
        LOG_INFO_CAT("AgentCore", "AgentCore initialization completed successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("AgentCore", "Initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool AgentCore::start() {
    LOG_FUNCTION_INFO("Starting AgentCore");
    
    if (!initialized_) {
        LOG_ERROR_CAT("AgentCore", "AgentCore not initialized");
        return false;
    }
    
    if (running_) {
        LOG_WARNING_CAT("AgentCore", "AgentCore already running");
        return true;
    }
    
    try {
        LOG_INFO_CAT("AgentCore", "Starting IPC server");
        if (!ipcServer_->start()) {
            LOG_ERROR_CAT("AgentCore", "Failed to start IPC server");
            return false;
        }
        
        LOG_INFO_CAT("AgentCore", "Starting worker thread");
        running_ = true;
        workerThread_ = std::thread(&AgentCore::workerThread, this);
        
        LOG_INFO_CAT("AgentCore", "AgentCore started successfully");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR_CAT("AgentCore", "Failed to start AgentCore: " + std::string(e.what()));
        running_ = false;
        return false;
    }
}

void AgentCore::stop() {
    LOG_FUNCTION_INFO("Stopping AgentCore");
    
    if (!running_) {
        LOG_WARNING_CAT("AgentCore", "AgentCore not running");
        return;
    }
    
    LOG_INFO_CAT("AgentCore", "Stopping AgentCore...");
    running_ = false;
    
    // Stop components
    if (automationEngine_) automationEngine_->stop();
    if (androidManager_) androidManager_->stop();
    if (processManager_) processManager_->stop();
    if (networkManager_) networkManager_->stop();
    if (deviceManager_) deviceManager_->stop();
    if (systemMonitor_) systemMonitor_->stop();
    if (ipcServer_) ipcServer_->stop();
    
    // Wait for worker thread
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    
    LOG_INFO_CAT("AgentCore", "AgentCore stopped successfully");
}

void AgentCore::shutdown() {
    LOG_FUNCTION_INFO("Shutting down AgentCore");
    stop();
    cleanupComponents();
    initialized_ = false;
    
    // Shutdown logging
    Logging::LogManager::getInstance().shutdown();
    LOG_INFO_CAT("AgentCore", "AgentCore shutdown completed");
}

bool AgentCore::isRunning() const {
    return running_;
}

std::string AgentCore::getStatus() const {
    if (running_) {
        return "Running";
    } else if (initialized_) {
        return "Stopped";
    } else {
        return "Not initialized";
    }
}

bool AgentCore::initializeComponents() {
    logger_->info("Initializing components...");
    
    // Initialize configuration manager first
    configManager_ = std::make_unique<ConfigManager>();
    if (!configManager_->initialize("sysmon_agent.conf")) {
        logger_->warning("Failed to load configuration, using defaults");
    }
    
    // Initialize IPC server (critical)
    ipcServer_ = std::make_unique<IpcServer>();
    int ipcPort = configManager_->getInt("agent.ipc_port", Constants::DEFAULT_IPC_PORT);
    if (!ipcServer_->initialize(ipcPort)) {
        logger_->error("Failed to initialize IPC server on port " + std::to_string(ipcPort));
        return false;
    }
    logger_->info("IPC server initialized on port " + std::to_string(ipcPort));
    
    // Initialize system monitor with fallback
    systemMonitor_ = std::make_unique<SystemMonitor>();
    if (!systemMonitor_->initialize()) {
        logger_->warning("Failed to initialize system monitor, using fallback mode");
        systemMonitor_->enableFallbackMode();
    }
    
    // Initialize device manager with fallback
    deviceManager_ = std::make_unique<DeviceManager>();
    if (!deviceManager_->initialize()) {
        logger_->warning("Failed to initialize device manager, using fallback mode");
        deviceManager_->enableFallbackMode();
    }
    
    // Initialize network manager with fallback
    networkManager_ = std::make_unique<NetworkManager>();
    if (!networkManager_->initialize()) {
        logger_->warning("Failed to initialize network manager, using fallback mode");
        networkManager_->enableFallbackMode();
    }
    
    // Initialize process manager with fallback
    processManager_ = std::make_unique<ProcessManager>();
    if (!processManager_->initialize()) {
        logger_->warning("Failed to initialize process manager, using fallback mode");
        processManager_->enableFallbackMode();
    }
    
    // Initialize android manager (optional)
    androidManager_ = std::make_unique<AndroidManager>();
    if (!androidManager_->initialize()) {
        logger_->warning("Failed to initialize android manager, Android features disabled");
        androidManager_.reset(); // Optional component, can be disabled
    }
    
    // Initialize automation engine (always works)
    automationEngine_ = std::make_unique<AutomationEngine>();
    if (!automationEngine_->initialize(this)) {
        logger_->error("Failed to initialize automation engine");
        return false;
    }
    
    // Set up command handler
    ipcServer_->setCommandHandler([this](const Command& cmd) {
        return handleCommand(cmd);
    });
    
    // Set up logger
    ipcServer_->setLogger(logger_.get());
    
    logger_->info("Components initialized with fallback support");
    return true;
}

void AgentCore::cleanupComponents() {
    logger_->info("Cleaning up components...");
    
    // Cleanup in reverse order
    if (automationEngine_) {
        automationEngine_->shutdown();
        automationEngine_.reset();
    }
    
    if (androidManager_) {
        androidManager_->shutdown();
        androidManager_.reset();
    }
    
    if (processManager_) {
        processManager_->shutdown();
        processManager_.reset();
    }
    
    if (networkManager_) {
        networkManager_->shutdown();
        networkManager_.reset();
    }
    
    if (deviceManager_) {
        deviceManager_->shutdown();
        deviceManager_.reset();
    }
    
    if (systemMonitor_) {
        systemMonitor_->shutdown();
        systemMonitor_.reset();
    }
    
    if (ipcServer_) {
        ipcServer_->shutdown();
        ipcServer_.reset();
    }
    
    if (configManager_) {
        configManager_.reset();
    }
    
    if (logger_) {
        logger_->info("Components cleanup complete");
        logger_->shutdown();
        logger_.reset();
    }
}

void AgentCore::workerThread() {
    logger_->info("Worker thread started");
    
    while (running_) {
        try {
            // Process any pending commands or events
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Cleanup serializer cache periodically
            static int cleanupCounter = 0;
            if (++cleanupCounter >= 600) { // Every minute
                serializer_->clearCache();
                cleanupCounter = 0;
            }
            
        } catch (const std::exception& e) {
            logError("workerThread", e);
        }
    }
    
    logger_->info("Worker thread stopped");
}

Response AgentCore::handleCommand(const Command& command) {
    std::lock_guard<std::mutex> lock(commandMutex_);
    
    logCommand(command, "started");
    
    try {
        // Validate command parameters
        if (!securityManager_->validateCommand(IpcProtocol::serializeCommand(command))) {
            logCommand(command, "invalid_command");
            return createResponse(command.id, CommandStatus::FAILED, "Invalid command format");
        }
        
        switch (command.module) {
            case Module::SYSTEM:
                return handleSystemCommand(command);
            case Module::DEVICE:
                return handleDeviceCommand(command);
            case Module::NETWORK:
                return handleNetworkCommand(command);
            case Module::PROCESS:
                return handleProcessCommand(command);
            case Module::ANDROID:
                return handleAndroidCommand(command);
            case Module::AUTOMATION:
                return handleAutomationCommand(command);
            default:
                logCommand(command, "unknown_module");
                return handleGenericCommand(command);
        }
    } catch (const std::exception& e) {
        logError("handleCommand", e);
        logCommand(command, "exception");
        return createResponse(command.id, CommandStatus::FAILED, e.what());
    }
}

Response AgentCore::handleSystemCommand(const Command& command) {
    std::shared_lock<std::shared_mutex> lock(componentsMutex_);
    
    try {
        switch (command.type) {
            case CommandType::GET_SYSTEM_INFO: {
                if (!systemMonitor_) {
                    logCommand(command, "system_monitor_unavailable");
                    return createResponse(command.id, CommandStatus::FAILED, "System monitor not available");
                }
                
                auto info = systemMonitor_->getCurrentSystemInfo();
                
                // Check if in fallback mode and add special message
                if (systemMonitor_->isFallbackMode()) {
                    logCommand(command, "system_monitor_fallback");
                    std::string serializedData = serializer_->serializeSystemInfo(info);
                    Response response = createResponse(command.id, CommandStatus::SUCCESS, serializedData);
                    response.message = "System info in fallback mode - limited functionality";
                    return response;
                }
                
                std::string serializedData = serializer_->serializeSystemInfo(info);
                logCommand(command, "success");
                return createResponse(command.id, CommandStatus::SUCCESS, serializedData);
            }
            
            case CommandType::GET_PROCESS_LIST: {
                if (!processManager_) {
                    logCommand(command, "process_manager_unavailable");
                    return createResponse(command.id, CommandStatus::FAILED, "Process manager not available");
                }
                
                auto processes = processManager_->getProcessList();
                
                // Check if in fallback mode and add special message
                if (processManager_->isFallbackMode()) {
                    logCommand(command, "process_manager_fallback");
                    std::string serializedData = serializer_->serializeProcessList(processes);
                    Response response = createResponse(command.id, CommandStatus::SUCCESS, serializedData);
                    response.message = "Process list in fallback mode - limited functionality";
                    return response;
                }
                
                std::string serializedData = serializer_->serializeProcessList(processes);
                
                logCommand(command, "success");
                return createResponse(command.id, CommandStatus::SUCCESS, "Process list retrieved", 
                                    {{"data", serializedData}});
            }
            
            default:
                logCommand(command, "unknown_system_command");
                return createResponse(command.id, CommandStatus::FAILED, "Unknown system command");
        }
    } catch (const std::exception& e) {
        logError("handleSystemCommand", e);
        logCommand(command, "exception");
        return createResponse(command.id, CommandStatus::FAILED, e.what());
    }
}

Response AgentCore::handleDeviceCommand(const Command& command) {
    try {
        if (!deviceManager_) {
            return createResponse(command.id, CommandStatus::FAILED, "Device manager not available");
        }
        
        switch (command.type) {
            case CommandType::GET_USB_DEVICES: {
                auto devices = deviceManager_->getUsbDevices();
                std::map<std::string, std::string> data;
                
                data["device_count"] = std::to_string(devices.size());
                
                for (size_t i = 0; i < devices.size(); ++i) {
                    const auto& device = devices[i];
                    std::string prefix = "usb_" + std::to_string(i) + "_";
                    data[prefix + "vid"] = device.vid;
                    data[prefix + "pid"] = device.pid;
                    data[prefix + "name"] = device.name;
                    data[prefix + "serial"] = device.serialNumber;
                    data[prefix + "connected"] = device.isConnected ? "1" : "0";
                    data[prefix + "enabled"] = device.isEnabled ? "1" : "0";
                }
                
                // Check if in fallback mode and add special message
                if (deviceManager_->isFallbackMode()) {
                    Response response = createResponse(command.id, CommandStatus::SUCCESS, "USB devices retrieved", data);
                    response.message = "USB devices in fallback mode - limited functionality";
                    return response;
                }
                
                return createResponse(command.id, CommandStatus::SUCCESS, "USB devices retrieved", data);
            }
            
            case CommandType::ENABLE_USB_DEVICE:
            case CommandType::DISABLE_USB_DEVICE: {
                auto it = command.parameters.find("device_id");
                if (it == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, "Missing device_id parameter");
                }
                
                std::string deviceId = it->second;
                bool enable = (command.type == CommandType::ENABLE_USB_DEVICE);
                
                // Parse deviceId which should be in format "vid,pid"
                size_t commaPos = deviceId.find(',');
                if (commaPos == std::string::npos) {
                    return createResponse(command.id, CommandStatus::FAILED, "Invalid device_id format, expected vid,pid");
                }
                
                std::string vid = deviceId.substr(0, commaPos);
                std::string pid = deviceId.substr(commaPos + 1);
                
                bool success = enable ? 
                    deviceManager_->enableUsbDevice(vid, pid) : 
                    deviceManager_->disableUsbDevice(vid, pid);
                
                if (success) {
                    std::string action = enable ? "enabled" : "disabled";
                    return createResponse(command.id, CommandStatus::SUCCESS, 
                        "Device " + action + " successfully");
                } else {
                    return createResponse(command.id, CommandStatus::FAILED, 
                        "Failed to " + std::string(enable ? "enable" : "disable") + " device");
                }
            }
            
            default:
                return createResponse(command.id, CommandStatus::FAILED, "Unknown device command");
        }
    } catch (const std::exception& e) {
        logger_->error("Exception in handleDeviceCommand: " + std::string(e.what()));
        return createResponse(command.id, CommandStatus::FAILED, e.what());
    }
}

Response AgentCore::handleNetworkCommand(const Command& command) {
    try {
        if (!networkManager_) {
            return createResponse(command.id, CommandStatus::FAILED, "Network manager not available");
        }
        
        switch (command.type) {
            case CommandType::GET_NETWORK_INTERFACES: {
                auto interfaces = networkManager_->getNetworkInterfaces();
                std::map<std::string, std::string> data;
                
                data["interface_count"] = std::to_string(interfaces.size());
                
                for (size_t i = 0; i < interfaces.size(); ++i) {
                    const auto& iface = interfaces[i];
                    std::string prefix = "iface_" + std::to_string(i) + "_";
                    data[prefix + "name"] = iface.name;
                    data[prefix + "ipv4"] = iface.ipv4;
                    data[prefix + "ipv6"] = iface.ipv6;
                    data[prefix + "enabled"] = iface.isEnabled ? "1" : "0";
                    data[prefix + "rx_bytes"] = std::to_string(iface.rxBytes);
                    data[prefix + "tx_bytes"] = std::to_string(iface.txBytes);
                    data[prefix + "rx_speed"] = std::to_string(iface.rxSpeed);
                    data[prefix + "tx_speed"] = std::to_string(iface.txSpeed);
                }
                
                // Check if in fallback mode and add special message
                if (networkManager_->isFallbackMode()) {
                    Response response = createResponse(command.id, CommandStatus::SUCCESS, "Network interfaces retrieved", data);
                    response.message = "Network interfaces in fallback mode - limited functionality";
                    return response;
                }
                
                return createResponse(command.id, CommandStatus::SUCCESS, "Network interfaces retrieved", data);
            }
            
            case CommandType::ENABLE_NETWORK_INTERFACE:
            case CommandType::DISABLE_NETWORK_INTERFACE: {
                auto it = command.parameters.find("interface_name");
                if (it == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, "Missing interface_name parameter");
                }
                
                std::string interfaceName = it->second;
                bool enable = (command.type == CommandType::ENABLE_NETWORK_INTERFACE);
                
                bool success = enable ? 
                    networkManager_->enableInterface(interfaceName) : 
                    networkManager_->disableInterface(interfaceName);
                
                if (success) {
                    std::string action = enable ? "enabled" : "disabled";
                    return createResponse(command.id, CommandStatus::SUCCESS, 
                        "Interface " + action + " successfully");
                } else {
                    return createResponse(command.id, CommandStatus::FAILED, 
                        "Failed to " + std::string(enable ? "enable" : "disable") + " interface");
                }
            }
            
            case CommandType::SET_STATIC_IP: {
                auto nameIt = command.parameters.find("interface_name");
                auto ipIt = command.parameters.find("ip");
                auto maskIt = command.parameters.find("netmask");
                auto gwIt = command.parameters.find("gateway");
                
                if (nameIt == command.parameters.end() || ipIt == command.parameters.end() || 
                    maskIt == command.parameters.end() || gwIt == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, 
                        "Missing required parameters: interface_name, ip, netmask, gateway");
                }
                
                bool success = networkManager_->setStaticIp(
                    nameIt->second, ipIt->second, maskIt->second, gwIt->second);
                
                return success ? 
                    createResponse(command.id, CommandStatus::SUCCESS, "Static IP configured successfully") :
                    createResponse(command.id, CommandStatus::FAILED, "Failed to configure static IP");
            }
            
            case CommandType::SET_DHCP_IP: {
                auto it = command.parameters.find("interface_name");
                if (it == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, "Missing interface_name parameter");
                }
                
                bool success = networkManager_->setDhcpIp(it->second);
                
                return success ? 
                    createResponse(command.id, CommandStatus::SUCCESS, "DHCP configured successfully") :
                    createResponse(command.id, CommandStatus::FAILED, "Failed to configure DHCP");
            }
            
            default:
                return createResponse(command.id, CommandStatus::FAILED, "Unknown network command");
        }
    } catch (const std::exception& e) {
        logger_->error("Exception in handleNetworkCommand: " + std::string(e.what()));
        return createResponse(command.id, CommandStatus::FAILED, e.what());
    }
}

Response AgentCore::handleProcessCommand(const Command& command) {
    try {
        if (!processManager_) {
            return createResponse(command.id, CommandStatus::FAILED, "Process manager not available");
        }
        
        switch (command.type) {
            case CommandType::TERMINATE_PROCESS:
            case CommandType::KILL_PROCESS: {
                auto it = command.parameters.find("pid");
                if (it == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, "Missing pid parameter");
                }
                
                try {
                    uint32_t pid = std::stoul(it->second);
                    bool forceKill = (command.type == CommandType::KILL_PROCESS);
                    
                    // Check if process is critical
                    if (processManager_->isCriticalProcess(pid)) {
                        return createResponse(command.id, CommandStatus::FAILED, 
                            "Cannot terminate critical process");
                    }
                    
                    bool success = forceKill ? 
                        processManager_->killProcess(pid) : 
                        processManager_->terminateProcess(pid);
                    
                    if (success) {
                        std::string action = forceKill ? "killed" : "terminated";
                        return createResponse(command.id, CommandStatus::SUCCESS, 
                            "Process " + action + " successfully");
                    } else {
                        return createResponse(command.id, CommandStatus::FAILED, 
                            "Failed to " + std::string(forceKill ? "kill" : "terminate") + " process");
                    }
                } catch (const std::exception& e) {
                    return createResponse(command.id, CommandStatus::FAILED, "Invalid PID format");
                }
            }
            
            default:
                return createResponse(command.id, CommandStatus::FAILED, "Unknown process command");
        }
    } catch (const std::exception& e) {
        logger_->error("Exception in handleProcessCommand: " + std::string(e.what()));
        return createResponse(command.id, CommandStatus::FAILED, e.what());
    }
}

Response AgentCore::handleAndroidCommand(const Command& command) {
    try {
        if (!androidManager_) {
            return createResponse(command.id, CommandStatus::FAILED, "Android manager not available - ADB not found");
        }
        
        switch (command.type) {
            case CommandType::GET_ANDROID_DEVICES: {
                auto devices = androidManager_->getConnectedDevices();
                std::map<std::string, std::string> data;
                
                data["device_count"] = std::to_string(devices.size());
                
                for (size_t i = 0; i < devices.size(); ++i) {
                    const auto& device = devices[i];
                    std::string prefix = "android_" + std::to_string(i) + "_";
                    data[prefix + "model"] = device.model;
                    data[prefix + "serial"] = device.serialNumber;
                    data[prefix + "android_version"] = device.androidVersion;
                    data[prefix + "battery"] = std::to_string(device.batteryLevel);
                    data[prefix + "screen_on"] = device.isScreenOn ? "1" : "0";
                    data[prefix + "locked"] = device.isLocked ? "1" : "0";
                }
                
                return createResponse(command.id, CommandStatus::SUCCESS, "Android devices retrieved", data);
            }
            
            case CommandType::ANDROID_SCREEN_ON:
            case CommandType::ANDROID_SCREEN_OFF: {
                auto it = command.parameters.find("device_serial");
                if (it == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, "Missing device_serial parameter");
                }
                
                std::string deviceSerial = it->second;
                bool turnOn = (command.type == CommandType::ANDROID_SCREEN_ON);
                
                bool success = turnOn ? 
                    androidManager_->turnScreenOn(deviceSerial) : 
                    androidManager_->turnScreenOff(deviceSerial);
                
                if (success) {
                    std::string action = turnOn ? "turned on" : "turned off";
                    return createResponse(command.id, CommandStatus::SUCCESS, 
                        "Screen " + action + " successfully");
                } else {
                    return createResponse(command.id, CommandStatus::FAILED, 
                        "Failed to " + std::string(turnOn ? "turn on" : "turn off") + " screen");
                }
            }
            
            case CommandType::ANDROID_LOCK_DEVICE: {
                auto it = command.parameters.find("device_serial");
                if (it == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, "Missing device_serial parameter");
                }
                
                bool success = androidManager_->lockDevice(it->second);
                
                return success ? 
                    createResponse(command.id, CommandStatus::SUCCESS, "Device locked successfully") :
                    createResponse(command.id, CommandStatus::FAILED, "Failed to lock device");
            }
            
            case CommandType::ANDROID_GET_FOREGROUND_APP: {
                auto it = command.parameters.find("device_serial");
                if (it == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, "Missing device_serial parameter");
                }
                
                std::string app = androidManager_->getForegroundApp(it->second);
                std::map<std::string, std::string> data;
                data["foreground_app"] = app;
                
                return createResponse(command.id, CommandStatus::SUCCESS, 
                    "Foreground app retrieved", data);
            }
            
            case CommandType::ANDROID_LAUNCH_APP:
            case CommandType::ANDROID_STOP_APP: {
                auto deviceIt = command.parameters.find("device_serial");
                auto appIt = command.parameters.find("package_name");
                
                if (deviceIt == command.parameters.end() || appIt == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, 
                        "Missing device_serial or package_name parameter");
                }
                
                bool launch = (command.type == CommandType::ANDROID_LAUNCH_APP);
                bool success = launch ? 
                    androidManager_->launchApp(deviceIt->second, appIt->second) :
                    androidManager_->stopApp(deviceIt->second, appIt->second);
                
                if (success) {
                    std::string action = launch ? "launched" : "stopped";
                    return createResponse(command.id, CommandStatus::SUCCESS, 
                        "App " + action + " successfully");
                } else {
                    return createResponse(command.id, CommandStatus::FAILED, 
                        "Failed to " + std::string(launch ? "launch" : "stop") + " app");
                }
            }
            
            case CommandType::ANDROID_TAKE_SCREENSHOT: {
                auto it = command.parameters.find("device_serial");
                if (it == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, "Missing device_serial parameter");
                }
                
                std::string screenshotPath = androidManager_->takeScreenshot(it->second);
                std::map<std::string, std::string> data;
                data["screenshot_path"] = screenshotPath;
                
                return createResponse(command.id, CommandStatus::SUCCESS, 
                    "Screenshot taken", data);
            }
            
            case CommandType::ANDROID_GET_ORIENTATION: {
                auto it = command.parameters.find("device_serial");
                if (it == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, "Missing device_serial parameter");
                }
                
                std::string orientation = androidManager_->getScreenOrientation(it->second);
                std::map<std::string, std::string> data;
                data["orientation"] = orientation;
                
                return createResponse(command.id, CommandStatus::SUCCESS, 
                    "Orientation retrieved", data);
            }
            
            case CommandType::ANDROID_GET_LOGCAT: {
                auto it = command.parameters.find("device_serial");
                if (it == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, "Missing device_serial parameter");
                }
                
                auto logLines = androidManager_->getLogcat(it->second);
                std::string logs;
                for (const auto& line : logLines) {
                    logs += line + "\n";
                }
                std::map<std::string, std::string> data;
                data["logs"] = logs;
                
                return createResponse(command.id, CommandStatus::SUCCESS, 
                    "Logcat retrieved", data);
            }
            
            default:
                return createResponse(command.id, CommandStatus::FAILED, "Unknown android command");
        }
    } catch (const std::exception& e) {
        logger_->error("Exception in handleAndroidCommand: " + std::string(e.what()));
        return createResponse(command.id, CommandStatus::FAILED, e.what());
    }
}

Response AgentCore::handleAutomationCommand(const Command& command) {
    try {
        if (!automationEngine_) {
            return createResponse(command.id, CommandStatus::FAILED, "Automation engine not available");
        }
        
        switch (command.type) {
            case CommandType::GET_AUTOMATION_RULES: {
                auto rules = automationEngine_->getRules();
                std::map<std::string, std::string> data;
                
                data["rule_count"] = std::to_string(rules.size());
                
                for (size_t i = 0; i < rules.size(); ++i) {
                    const auto& rule = rules[i];
                    std::string prefix = "rule_" + std::to_string(i) + "_";
                    data[prefix + "id"] = rule.id;
                    data[prefix + "condition"] = rule.condition;
                    data[prefix + "action"] = rule.action;
                    data[prefix + "enabled"] = rule.isEnabled ? "1" : "0";
                    data[prefix + "duration"] = std::to_string(rule.duration.count());
                }
                
                return createResponse(command.id, CommandStatus::SUCCESS, "Automation rules retrieved", data);
            }
            
            case CommandType::ADD_AUTOMATION_RULE: {
                auto conditionIt = command.parameters.find("condition");
                auto actionIt = command.parameters.find("action");
                
                if (conditionIt == command.parameters.end() || actionIt == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, 
                        "Missing condition or action parameter");
                }
                
                AutomationRule rule;
                rule.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                rule.condition = conditionIt->second;
                rule.action = actionIt->second;
                rule.isEnabled = true;
                
                auto durationIt = command.parameters.find("duration");
                if (durationIt != command.parameters.end()) {
                    try {
                        rule.duration = std::chrono::seconds(std::stoul(durationIt->second));
                    } catch (...) {
                        rule.duration = std::chrono::seconds(0);
                    }
                }
                
                bool success = automationEngine_->addRule(rule);
                
                return success ? 
                    createResponse(command.id, CommandStatus::SUCCESS, 
                        "Automation rule added with ID: " + rule.id) :
                    createResponse(command.id, CommandStatus::FAILED, "Failed to add automation rule");
            }
            
            case CommandType::REMOVE_AUTOMATION_RULE: {
                auto it = command.parameters.find("rule_id");
                if (it == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, "Missing rule_id parameter");
                }
                
                bool success = automationEngine_->removeRule(it->second);
                
                return success ? 
                    createResponse(command.id, CommandStatus::SUCCESS, "Automation rule removed") :
                    createResponse(command.id, CommandStatus::FAILED, "Failed to remove automation rule");
            }
            
            case CommandType::ENABLE_AUTOMATION_RULE:
            case CommandType::DISABLE_AUTOMATION_RULE: {
                auto it = command.parameters.find("rule_id");
                if (it == command.parameters.end()) {
                    return createResponse(command.id, CommandStatus::FAILED, "Missing rule_id parameter");
                }
                
                bool enable = (command.type == CommandType::ENABLE_AUTOMATION_RULE);
                bool success = enable ? 
                    automationEngine_->enableRule(it->second) : 
                    automationEngine_->disableRule(it->second);
                
                if (success) {
                    std::string action = enable ? "enabled" : "disabled";
                    return createResponse(command.id, CommandStatus::SUCCESS, 
                        "Automation rule " + action);
                } else {
                    return createResponse(command.id, CommandStatus::FAILED, 
                        "Failed to " + std::string(enable ? "enable" : "disable") + " automation rule");
                }
            }
            
            default:
                return createResponse(command.id, CommandStatus::FAILED, "Unknown automation command");
        }
    } catch (const std::exception& e) {
        logger_->error("Exception in handleAutomationCommand: " + std::string(e.what()));
        return createResponse(command.id, CommandStatus::FAILED, e.what());
    }
}

Response AgentCore::handleGenericCommand(const Command& command) {
    switch (command.type) {
        case CommandType::PING:
            return createResponse(command.id, CommandStatus::SUCCESS, "PONG");
        case CommandType::SHUTDOWN:
            logger_->info("Shutdown command received");
            running_ = false;
            return createResponse(command.id, CommandStatus::SUCCESS, "Shutting down");
        default:
            return createResponse(command.id, CommandStatus::FAILED, "Unknown command");
    }
}

void AgentCore::handleEvent(const Event& event) {
    logger_->info("Handling event: " + event.type + " from module: " + std::to_string(static_cast<int>(event.module)));
    
    // Forward event to all connected clients
    sendEventToClients(event);
}

void AgentCore::sendEventToClients(const Event& event) {
    if (ipcServer_) {
        ipcServer_->broadcastEvent(event);
    }
}

bool AgentCore::validateParameters(const Command& command, const std::vector<std::string>& requiredParams) {
    for (const auto& param : requiredParams) {
        if (command.parameters.find(param) == command.parameters.end()) {
            logger_->warning("Missing required parameter: " + param);
            return false;
        }
    }
    return true;
}

void AgentCore::logCommand(const Command& command, const std::string& status) {
    if (logger_) {
        logger_->info("Command " + commandTypeToString(command.type) + 
                     " [" + command.id + "] - " + status);
    }
}

void AgentCore::logError(const std::string& function, const std::exception& e) {
    if (logger_) {
        logger_->error("Exception in " + function + ": " + std::string(e.what()));
    }
}

Response AgentCore::createFallbackResponse(const std::string& commandId, const std::string& data, const std::string& componentName) {
    Response response = createResponse(commandId, CommandStatus::SUCCESS, data);
    response.message = componentName + " in fallback mode - limited functionality due to missing Windows SDK";
    return response;
}

} // namespace SysMon
