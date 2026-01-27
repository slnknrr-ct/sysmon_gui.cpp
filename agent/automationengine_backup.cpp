#include "automationengine.h"
#include "agentcore.h"
#include <thread>
#include <chrono>
#include <regex>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace SysMon {

AutomationEngine::AutomationEngine() 
    : running_(false)
    , initialized_(false)
    , core_(nullptr)
    , evaluationInterval_(1000) {
}

AutomationEngine::~AutomationEngine() {
    shutdown();
}

bool AutomationEngine::initialize(AgentCore* core) {
    if (initialized_) {
        return true;
    }
    
    if (!core) {
        return false;
    }
    
    core_ = core;
    
    // Load rules from config
    loadRulesFromConfiguration();
    
    initialized_ = true;
    return true;
}

bool AutomationEngine::start() {
    if (!initialized_) {
        return false;
    }
    
    if (running_) {
        return true;
    }
    
    running_ = true;
    automationThread_ = std::thread(&AutomationEngine::automationThread, this);
    
    return true;
}

void AutomationEngine::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (automationThread_.joinable()) {
        automationThread_.join();
    }
}

void AutomationEngine::shutdown() {
    stop();
    initialized_ = false;
}

std::vector<AutomationRule> AutomationEngine::getRules() const {
    std::shared_lock<std::shared_mutex> lock(rulesMutex_);
    return rules_;
}

bool AutomationEngine::addRule(const AutomationRule& rule) {
    std::unique_lock<std::shared_mutex> lock(rulesMutex_);
    
    if (rules_.size() >= MAX_RULES) {
        return false;
    }
    
    if (!isValidCondition(rule.condition) || !isValidAction(rule.action)) {
        return false;
    }
    
    rules_.push_back(rule);
    return true;
}

bool AutomationEngine::removeRule(const std::string& ruleId) {
    std::unique_lock<std::shared_mutex> lock(rulesMutex_);
    
    auto it = std::remove_if(rules_.begin(), rules_.end(),
        [&ruleId](const AutomationRule& rule) {
            return rule.id == ruleId;
        });
    
    if (it != rules_.end()) {
        rules_.erase(it, rules_.end());
        return true;
    }
    
    return false;
}

bool AutomationEngine::enableRule(const std::string& ruleId) {
    std::unique_lock<std::shared_mutex> lock(rulesMutex_);
    
    for (auto& rule : rules_) {
        if (rule.id == ruleId) {
            rule.isEnabled = true;
            return true;
        }
    }
    
    return false;
}

bool AutomationEngine::disableRule(const std::string& ruleId) {
    std::unique_lock<std::shared_mutex> lock(rulesMutex_);
    
    for (auto& rule : rules_) {
        if (rule.id == ruleId) {
            rule.isEnabled = false;
            return true;
        }
    }
    
    return false;
}

bool AutomationEngine::isRuleEnabled(const std::string& ruleId) const {
    std::shared_lock<std::shared_mutex> lock(rulesMutex_);
    
    for (const auto& rule : rules_) {
        if (rule.id == ruleId) {
            return rule.isEnabled;
        }
    }
    
    return false;
}

bool AutomationEngine::isRunning() const {
    return running_;
}

size_t AutomationEngine::getActiveRulesCount() const {
    std::shared_lock<std::shared_mutex> lock(rulesMutex_);
    
    return std::count_if(rules_.begin(), rules_.end(),
        [](const AutomationRule& rule) {
            return rule.isEnabled;
        });
}

void AutomationEngine::automationThread() {
    while (running_) {
        try {
            evaluateRules();
            lastEvaluation_ = std::chrono::steady_clock::now();
            
            std::this_thread::sleep_for(evaluationInterval_);
            
        } catch (const std::exception& e) {
            // Log error but continue
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void AutomationEngine::evaluateRules() {
    std::shared_lock<std::shared_mutex> lock(rulesMutex_);
    
    for (const auto& rule : rules_) {
        if (!rule.isEnabled) {
            continue;
        }
        
        try {
            bool conditionMet = evaluateCondition(rule.condition);
            
            if (conditionMet) {
                if (checkDurationCondition(rule.condition)) {
                    executeAction(rule.action);
                }
            } else {
                // Reset condition timer if condition is not met
                stopConditionTimer(rule.condition);
            }
            
        } catch (const std::exception& e) {
            // Log error for this rule but continue with others
        }
    }
}

bool AutomationEngine::evaluateCondition(const std::string& condition) {
    std::string conditionType = extractConditionType(condition);
    
    if (conditionType == "CPU_LOAD") {
        return evaluateCpuCondition(condition);
    } else if (conditionType == "MEMORY") {
        return evaluateMemoryCondition(condition);
    } else if (conditionType == "PROCESS") {
        return evaluateProcessCondition(condition);
    } else if (conditionType == "DEVICE") {
        return evaluateDeviceCondition(condition);
    } else if (conditionType == "NETWORK") {
        return evaluateNetworkCondition(condition);
    } else if (conditionType == "ANDROID") {
        return evaluateAndroidCondition(condition);
    }
    
    return false;
}

void AutomationEngine::executeAction(const std::string& action) {
    std::string actionType = extractActionType(action);
    
    if (actionType == "DISABLE_USB") {
        executeDeviceAction(action);
    } else if (actionType == "ENABLE_USB") {
        executeDeviceAction(action);
    } else if (actionType == "LOCK_ANDROID") {
        executeAndroidAction(action);
    } else if (actionType == "TERMINATE_PROCESS") {
        executeProcessAction(action);
    } else if (actionType == "DISABLE_NETWORK") {
        executeNetworkAction(action);
    } else if (actionType == "ENABLE_NETWORK") {
        executeNetworkAction(action);
    }
}

bool AutomationEngine::evaluateCpuCondition(const std::string& condition) {
    // Parse CPU condition: "CPU_LOAD > 85%"
    std::regex cpuRegex(R"(CPU_LOAD\s*([><=]+)\s*(\d+)%?)");
    std::smatch match;
    
    if (std::regex_search(condition, match, cpuRegex)) {
        std::string op = match[1];
        int threshold;
        
        try {
            threshold = std::stoi(match[2]);
        } catch (const std::exception& e) {
            return false;
        }
        
        // Get actual CPU usage from system monitor
        if (core_) {
            Command command = createCommand(CommandType::GET_SYSTEM_INFO, Module::SYSTEM);
            // In production, this would be async with callback
            // For now, simulate getting actual CPU usage
            double currentCpuUsage = getCpuUsageFromSystem();
            
            if (op == ">") return currentCpuUsage > threshold;
            if (op == "<") return currentCpuUsage < threshold;
            if (op == ">=") return currentCpuUsage >= threshold;
            if (op == "<=") return currentCpuUsage <= threshold;
            if (op == "==") return currentCpuUsage == threshold;
        }
    }
    
    return false;
}

bool AutomationEngine::evaluateMemoryCondition(const std::string& condition) {
    // Parse memory condition: "MEMORY > 80%" or "MEMORY < 50%"
    std::regex memoryRegex(R"(MEMORY\s*([><=]+)\s*(\d+)%?)");
    std::smatch match;
    
    if (std::regex_search(condition, match, memoryRegex)) {
        std::string op = match[1];
        int threshold;
        
        try {
            threshold = std::stoi(match[2]);
        } catch (const std::exception& e) {
            return false;
        }
        
        // Get current memory usage from system monitor
        if (core_) {
            // Request system info from system monitor
            Command command = createCommand(CommandType::GET_SYSTEM_INFO, Module::SYSTEM);
            // Note: This is a simplified approach - in production, we'd use callbacks
            // For now, use placeholder value
            double currentMemoryUsage = 60.0; // Placeholder
            
            if (op == ">") return currentMemoryUsage > threshold;
            if (op == "<") return currentMemoryUsage < threshold;
            if (op == ">=") return currentMemoryUsage >= threshold;
            if (op == "<=") return currentMemoryUsage <= threshold;
            if (op == "==") return currentMemoryUsage == threshold;
        }
    }
    
    return false;
}

bool AutomationEngine::evaluateProcessCondition(const std::string& condition) {
    // Parse process condition: "PROCESS firefox RUNNING" or "PROCESS chrome STOPPED"
    std::regex processRegex(R"(PROCESS\s+(\w+)\s+(\w+))");
    std::smatch match;
    
    if (std::regex_search(condition, match, processRegex)) {
        std::string processName = match[1];
        std::string status = match[2];
        
        // Get current process list from process manager
        if (core_) {
            // Request process list from process manager
            Command command = createCommand(CommandType::GET_PROCESS_LIST, Module::PROCESS);
            // Note: This is a simplified approach - in production, we'd use callbacks
            // For now, use placeholder logic
            
            // Check if process is in desired state
            if (status == "RUNNING") {
                // Check if process is running (placeholder logic)
                return processName == "firefox"; // Placeholder
            } else if (status == "STOPPED") {
                // Check if process is stopped (placeholder logic)
                return processName != "firefox"; // Placeholder
            }
        }
    }
    
    return false;
}

bool AutomationEngine::evaluateDeviceCondition(const std::string& condition) {
    // Parse device condition: "DEVICE 1234:5678 CONNECTED" or "DEVICE ABCD:EF12 DISCONNECTED"
    std::regex deviceRegex(R"(DEVICE\s+([0-9A-Fa-f]{4}:[0-9A-Fa-f]{4})\s+(\w+))");
    std::smatch match;
    
    if (std::regex_search(condition, match, deviceRegex)) {
        std::string vid = match[1];
        std::string pid = match[2];
        std::string deviceKey = vid + ":" + pid;
        std::string desiredState = match[3];
        
        // Get current device list from device manager
        if (core_) {
            // Request device list from device manager
            Command command = createCommand(CommandType::GET_USB_DEVICES, Module::DEVICE);
            // Note: This is a simplified approach - in production, we'd use callbacks
            // For now, use placeholder logic
            
            // Check if device is in desired state
            if (desiredState == "CONNECTED") {
                // Check if device is connected (placeholder logic)
                return deviceKey == "1234:5678"; // Placeholder
            } else if (desiredState == "DISCONNECTED") {
                // Check if device is disconnected (placeholder logic)
                return deviceKey != "1234:5678"; // Placeholder
            }
        }
    }
    
    return false;
}

bool AutomationEngine::evaluateNetworkCondition(const std::string& condition) {
    // Parse network condition: "NETWORK eth0 DOWN" or "NETWORK wlan0 UP"
    std::regex networkRegex(R"(NETWORK\s+(\w+)\s+(\w+))");
    std::smatch match;
    
    if (std::regex_search(condition, match, networkRegex)) {
        std::string interfaceName = match[1];
        std::string desiredState = match[2];
        
        // Get current network interfaces from network manager
        if (core_) {
            // Request network interfaces from network manager
            Command command = createCommand(CommandType::GET_NETWORK_INTERFACES, Module::NETWORK);
            // Note: This is a simplified approach - in production, we'd use callbacks
            // For now, use placeholder logic
            
            // Check if interface is in desired state
            if (desiredState == "UP") {
                // Check if interface is up (placeholder logic)
                return interfaceName == "eth0"; // Placeholder
            } else if (desiredState == "DOWN") {
                // Check if interface is down (placeholder logic)
                return interfaceName != "eth0"; // Placeholder
            }
        }
    }
    
    return false;
}

bool AutomationEngine::evaluateAndroidCondition(const std::string& condition) {
    // Parse Android condition: "ANDROID BATTERY < 20%" or "ANDROID SCREEN OFF"
    std::regex androidRegex(R"(ANDROID\s+(\w+)\s*([><=]+\s*\d+%)?)");
    std::smatch match;
    
    if (std::regex_search(condition, match, androidRegex)) {
        std::string metric = match[1];
        std::string op = match.size() > 2 ? match[2] : "";
        int threshold = 0;
        
        try {
            threshold = match.size() > 3 ? std::stoi(match[3]) : 0;
        } catch (const std::exception& e) {
            threshold = 0;
        }
        
        // Get current Android device info from android manager
        if (core_) {
            // Request Android device info from android manager
            Command command = createCommand(CommandType::GET_ANDROID_DEVICES, Module::ANDROID);
            // Note: This is a simplified approach - in production, we'd use callbacks
            // For now, use placeholder logic
            
            if (metric == "BATTERY") {
                // Check battery level (placeholder)
                int currentBattery = 85; // Placeholder
                
                if (op == "<") return currentBattery < threshold;
                if (op == ">") return currentBattery > threshold;
                if (op == "<=") return currentBattery <= threshold;
                if (op == ">=") return currentBattery >= threshold;
                if (op == "==") return currentBattery == threshold;
            } else if (metric == "SCREEN") {
                // Check screen state (placeholder)
                bool screenOn = true; // Placeholder
                
                if (op == "OFF") return !screenOn;
                if (op == "ON") return screenOn;
            }
        }
    }
    
    // Also handle simple conditions like "ANDROID SCREEN OFF"
    if (condition.find("ANDROID SCREEN OFF") != std::string::npos) {
        // Check if screen is off (placeholder)
        return false; // Placeholder
    } else if (condition.find("ANDROID SCREEN ON") != std::string::npos) {
        // Check if screen is on (placeholder)
        return true; // Placeholder
    }
    
    return false;
}

void AutomationEngine::executeSystemAction(const std::string& action) {
    // Parse system action: "REBOOT_SYSTEM", "SHUTDOWN_SYSTEM", "LOGOUT_USER"
    std::regex systemRegex(R"((REBOOT_SYSTEM|SHUTDOWN_SYSTEM|LOGOUT_USER|KILL_ALL_PROCESSES))");
    std::smatch match;
    
    if (std::regex_search(action, match, systemRegex)) {
        std::string operation = match[1];
        
        if (core_) {
            Command command;
            
            if (operation == "REBOOT_SYSTEM") {
                command = createCommand(CommandType::SYSTEM_REBOOT, Module::SYSTEM);
            } else if (operation == "SHUTDOWN_SYSTEM") {
                command = createCommand(CommandType::SYSTEM_SHUTDOWN, Module::SYSTEM);
            } else if (operation == "LOGOUT_USER") {
                command = createCommand(CommandType::SYSTEM_LOGOUT, Module::SYSTEM);
            } else if (operation == "KILL_ALL_PROCESSES") {
                command = createCommand(CommandType::KILL_ALL_PROCESSES, Module::PROCESS);
            }
            
            // Execute action through system manager
            core_->handleSystemCommand(command);
        }
    }
}

void AutomationEngine::executeDeviceAction(const std::string& action) {
    // Parse device action: "DISABLE_USB 1234:5678" or "ENABLE_USB ABCD:EF12"
    std::regex deviceRegex(R"((ENABLE|DISABLE)_USB\s+([0-9A-Fa-f]{4}:[0-9A-Fa-f]{4}))");
    std::smatch match;
    
    if (std::regex_search(action, match, deviceRegex)) {
        std::string operation = match[1];
        std::string vidPid = match[2];
        
        // Extract VID and PID
        size_t colonPos = vidPid.find(':');
        if (colonPos != std::string::npos) {
            std::string vid = vidPid.substr(0, colonPos);
            std::string pid = vidPid.substr(colonPos + 1);
            
            if (core_) {
                if (operation == "DISABLE") {
                    // Send command to disable USB device
                    Command command = createCommand(CommandType::DISABLE_USB_DEVICE, Module::DEVICE);
                    command.parameters["vid"] = vid;
                    command.parameters["pid"] = pid;
                    
                    // Execute action through device manager
                    core_->handleDeviceCommand(command);
                } else if (operation == "ENABLE") {
                    // Send command to enable USB device
                    Command command = createCommand(CommandType::ENABLE_USB_DEVICE, Module::DEVICE);
                    command.parameters["vid"] = vid;
                    command.parameters["pid"] = pid;
                    
                    // Execute action through device manager
                    core_->handleDeviceCommand(command);
                }
            }
        }
    }
}

void AutomationEngine::executeNetworkAction(const std::string& action) {
    // Parse network action: "DISABLE_NETWORK eth0" or "ENABLE_NETWORK wlan0"
    std::regex networkRegex(R"((ENABLE|DISABLE)_NETWORK\s+(\w+))");
    std::smatch match;
    
    if (std::regex_search(action, match, networkRegex)) {
        std::string operation = match[1];
        std::string interfaceName = match[2];
        
        if (core_) {
            if (operation == "DISABLE") {
                // Send command to disable network interface
                Command command = createCommand(CommandType::DISABLE_NETWORK_INTERFACE, Module::NETWORK);
                command.parameters["interface"] = interfaceName;
                
                // Execute action through network manager
                core_->handleNetworkCommand(command);
            } else if (operation == "ENABLE") {
                // Send command to enable network interface
                Command command = createCommand(CommandType::ENABLE_NETWORK_INTERFACE, Module::NETWORK);
                command.parameters["interface"] = interfaceName;
                
                // Execute action through network manager
                core_->handleNetworkCommand(command);
            }
        }
    }
}

void AutomationEngine::executeProcessAction(const std::string& action) {
    // Parse process action: "TERMINATE_PROCESS 1234" or "KILL_PROCESS 5678"
    std::regex processRegex(R"((TERMINATE|KILL)_PROCESS\s+(\d+))");
    std::smatch match;
    
    if (std::regex_search(action, match, processRegex)) {
        std::string operation = match[1];
        uint32_t pid = std::stoul(match[2]);
        
        if (core_) {
            if (operation == "TERMINATE") {
                // Send command to terminate process
                Command command = createCommand(CommandType::TERMINATE_PROCESS, Module::PROCESS);
                command.parameters["pid"] = std::to_string(pid);
                
                // Execute action through process manager
                core_->handleProcessCommand(command);
            } else if (operation == "KILL") {
                // Send command to kill process
                Command command = createCommand(CommandType::KILL_PROCESS, Module::PROCESS);
                command.parameters["pid"] = std::to_string(pid);
                
                // Execute action through process manager
                core_->handleProcessCommand(command);
            }
        }
    }
}

void AutomationEngine::executeAndroidAction(const std::string& action) {
    // Parse Android action: "LOCK_ANDROID_DEVICE" or "SCREEN_ON_ANDROID"
    std::regex androidRegex(R"((LOCK_ANDROID_DEVICE|SCREEN_ON_ANDROID|SCREEN_OFF_ANDROID|TURN_ON_ANDROID|TURN_OFF_ANDROID))");
    std::smatch match;
    
    if (std::regex_search(action, match, androidRegex)) {
        std::string operation = match[1];
        
        if (core_) {
            Command command;
            
            if (operation == "LOCK_ANDROID_DEVICE") {
                command = createCommand(CommandType::ANDROID_LOCK_DEVICE, Module::ANDROID);
            } else if (operation == "SCREEN_ON_ANDROID") {
                command = createCommand(CommandType::ANDROID_SCREEN_ON, Module::ANDROID);
            } else if (operation == "SCREEN_OFF_ANDROID") {
                command = createCommand(CommandType::ANDROID_SCREEN_OFF, Module::ANDROID);
            } else if (operation == "TURN_ON_ANDROID") {
                command = createCommand(CommandType::ANDROID_SCREEN_ON, Module::ANDROID);
            } else if (operation == "TURN_OFF_ANDROID") {
                command = createCommand(CommandType::ANDROID_SCREEN_OFF, Module::ANDROID);
            }
            
            // Execute action through android manager
            if (core_ && core_->getAndroidManager()) {
                // For now, just log the action - actual implementation would use AndroidManager methods
                std::cout << "Executing Android action: " << command.toString() << std::endl;
            }
        }
    }
}

std::string AutomationEngine::extractConditionType(const std::string& condition) {
    // Extract the first word before any operators
    std::regex typeRegex(R"(\w+)");
    std::smatch match;
    
    if (std::regex_search(condition, match, typeRegex)) {
        return match[0];
    }
    
    return "";
}

std::string AutomationEngine::extractConditionValue(const std::string& condition) {
    // Extract value after operator
    std::regex valueRegex(R"([><=]+\s*\d+%?)");
    std::smatch match;
    
    if (std::regex_search(condition, match, valueRegex)) {
        return match[0];
    }
    
    return "";
}

std::string AutomationEngine::extractActionType(const std::string& action) {
    // Extract the first word
    std::regex typeRegex(R"(\w+)");
    std::smatch match;
    
    if (std::regex_search(action, match, typeRegex)) {
        return match[0];
    }
    
    return "";
}

std::string AutomationEngine::extractActionValue(const std::string& action) {
    // Extract value after action type
    std::regex valueRegex(R"(\w+)\s+(.+)");
    std::smatch match;
    
    if (std::regex_search(action, match, valueRegex)) {
        return match[2];
    }
    
    return "";
}

bool AutomationEngine::checkDurationCondition(const std::string& condition) {
    // Check if condition has been met for the required duration
    std::lock_guard<std::mutex> lock(timersMutex_);
    
    for (auto& timer : conditionTimers_) {
        if (timer.condition == condition && timer.active) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - timer.startTime);
            
            // Get duration from condition if specified
            std::regex durationRegex(R"(FOR\s+(\d+)s)");
            std::smatch match;
            
            if (std::regex_search(condition, match, durationRegex)) {
                int requiredDuration;
                
                try {
                    requiredDuration = std::stoi(match[1]);
                } catch (const std::exception& e) {
                    requiredDuration = 5; // Default duration
                }
                
                return elapsed.count() >= requiredDuration;
            }
            
            // Default duration of 5 seconds if not specified
            return elapsed.count() >= 5;
        }
    }
    
    return false;
}

void AutomationEngine::startConditionTimer(const std::string& condition) {
    std::lock_guard<std::mutex> lock(timersMutex_);
    
    // Check if timer already exists
    for (auto& timer : conditionTimers_) {
        if (timer.condition == condition) {
            timer.startTime = std::chrono::steady_clock::now();
            timer.active = true;
            return;
        }
    }
    
    // Create new timer
    ConditionTimer timer;
    timer.condition = condition;
    timer.startTime = std::chrono::steady_clock::now();
    timer.active = true;
    
    conditionTimers_.push_back(timer);
}

void AutomationEngine::stopConditionTimer(const std::string& condition) {
    std::lock_guard<std::mutex> lock(timersMutex_);
    
    for (auto& timer : conditionTimers_) {
        if (timer.condition == condition) {
            timer.active = false;
        }
    }
}

bool AutomationEngine::isValidCondition(const std::string& condition) const {
    // Check if condition has valid format
    std::vector<std::string> validPrefixes = {"CPU_LOAD", "MEMORY", "PROCESS", "DEVICE", "NETWORK", "ANDROID"};
    
    for (const auto& prefix : validPrefixes) {
        if (condition.find(prefix) == 0) {
            // Check if condition has valid operator
            if (condition.find(">") != std::string::npos ||
                condition.find("<") != std::string::npos ||
                condition.find("=") != std::string::npos) {
                return true;
            }
        }
    }
    
    return false;
}

bool AutomationEngine::isValidAction(const std::string& action) const {
    // Check if action has valid format
    std::vector<std::string> validPrefixes = {
        "DISABLE_USB", "ENABLE_USB", "LOCK_ANDROID_DEVICE", "SCREEN_ON_ANDROID", 
        "SCREEN_OFF_ANDROID", "TERMINATE_PROCESS", "KILL_PROCESS",
        "DISABLE_NETWORK", "ENABLE_NETWORK", "REBOOT_SYSTEM", "SHUTDOWN_SYSTEM",
        "LOGOUT_USER", "KILL_ALL_PROCESSES"
    };
    
    for (const auto& prefix : validPrefixes) {
        if (action.find(prefix) == 0) {
            return true;
        }
    }
    
    return false;
}

void AutomationEngine::loadRulesFromConfiguration() {
    // Load automation rules from configuration file
    std::string configPath = "sysmon_agent.conf";
    std::ifstream configFile(configPath);
    
    if (!configFile.is_open()) {
        // Create default rules if config doesn't exist
        createDefaultRules();
        return;
    }
    
    std::string line;
    std::string currentSection;
    
    while (std::getline(configFile, line)) {
        // Remove comments and trim
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty()) continue;
        
        // Check for section header
        if (line.front() == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }
        
        // Parse automation rules
        if (currentSection == "automation") {
            parseAutomationRule(line);
        }
    }
    
    configFile.close();
}

double AutomationEngine::getCpuUsageFromSystem() {
    if (!core_) {
        return 0.0;
    }
    
    // Get system info from system monitor
    Command command = createCommand(CommandType::GET_SYSTEM_INFO, Module::SYSTEM);
    
    // In production, this would be async with callback
    // For now, simulate getting actual CPU usage from system
#ifdef _WIN32
    // Windows CPU usage calculation
    static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
    static int numProcessors = 0;
    static HANDLE self = GetCurrentProcess();
    
    if (numProcessors == 0) {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        numProcessors = sysInfo.dwNumberOfProcessors;
        
        FILETIME ftime, fsys, fuser;
        GetSystemTimeAsFileTime(&ftime);
        memcpy(&lastCPU, &ftime, sizeof(FILETIME));
        
        GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
        memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
        memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
    }
    
    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;
    
    GetSystemTimeAsFileTime(&ftime);
    memcpy(&now, &ftime, sizeof(FILETIME));
    
    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&sys, &fsys, sizeof(FILETIME));
    memcpy(&user, &fuser, sizeof(FILETIME));
    
    double percent = (sys.QuadPart - lastSysCPU.QuadPart) + (user.QuadPart - lastUserCPU.QuadPart);
    percent /= (now.QuadPart - lastCPU.QuadPart);
    percent /= numProcessors;
    
    lastCPU = now;
    lastUserCPU = user;
    lastSysCPU = sys;
    
    return percent * 100;
#else
    // Linux CPU usage calculation
    std::ifstream statFile("/proc/stat");
    if (!statFile.is_open()) {
        return 0.0;
    }
    
    std::string line;
    if (std::getline(statFile, line)) {
        std::istringstream iss(line);
        std::string cpuLabel;
        uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
        
        if (iss >> cpuLabel >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal) {
            uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
            uint64_t work = user + nice + system + irq + softirq + steal;
            
            static uint64_t lastTotal = 0, lastWork = 0;
            
            if (lastTotal != 0) {
                double cpuUsage = (double)(work - lastWork) / (double)(total - lastTotal) * 100.0;
                lastTotal = total;
                lastWork = work;
                return cpuUsage;
            }
            
            lastTotal = total;
            lastWork = work;
        }
    }
    
    statFile.close();
    return 0.0;
#endif
}

void AutomationEngine::createDefaultRules() {
    // Create some default automation rules for demonstration
    AutomationRule rule1;
    rule1.id = "default_cpu_high";
    rule1.condition = "CPU_LOAD > 80% FOR 10s";
    rule1.action = "LOGOUT_USER";
    rule1.isEnabled = false; // Disabled by default for safety
    rule1.duration = std::chrono::seconds(10);
    
    AutomationRule rule2;
    rule2.id = "default_memory_low";
    rule2.condition = "MEMORY > 90% FOR 5s";
    rule2.action = "KILL_ALL_PROCESSES";
    rule2.isEnabled = false; // Disabled by default for safety
    rule2.duration = std::chrono::seconds(5);
    addRule(rule1);
    addRule(rule2);
}

void AutomationEngine::parseAutomationRule(const std::string& line) {
    // Parse rule format: id=rule1;name=High CPU;condition=CPU_LOAD > 80%;action=LOGOUT_USER;enabled=true
    std::istringstream iss(line);
    std::string token;
    AutomationRule rule;
    
    while (std::getline(iss, token, ';')) {
        size_t equalPos = token.find('=');
        if (equalPos != std::string::npos) {
            std::string key = token.substr(0, equalPos);
            std::string value = token.substr(equalPos + 1);
            
            if (key == "id") {
                rule.id = value;
            } else if (key == "condition") {
                rule.condition = value;
            } else if (key == "action") {
                rule.action = value;
            } else if (key == "enabled") {
                rule.isEnabled = (value == "true" || value == "1");
            }
        }
    }
    
    // Only add rule if it has required fields
    if (!rule.id.empty() && !rule.condition.empty() && !rule.action.empty()) {
        if (isValidCondition(rule.condition) && isValidAction(rule.action)) {
            addRule(rule);
        }
    }
}

} // namespace SysMon
