#pragma once

#include "systemtypes.h"
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <map>

namespace SysMon {

// IPC Command types
enum class CommandType {
    // System Monitor
    GET_SYSTEM_INFO,
    GET_PROCESS_LIST,
    
    // Device Manager
    GET_USB_DEVICES,
    ENABLE_USB_DEVICE,
    DISABLE_USB_DEVICE,
    
    // Network Manager
    GET_NETWORK_INTERFACES,
    ENABLE_NETWORK_INTERFACE,
    DISABLE_NETWORK_INTERFACE,
    SET_STATIC_IP,
    SET_DHCP_IP,
    
    // Process Manager
    TERMINATE_PROCESS,
    KILL_PROCESS,
    
    // Android Manager
    GET_ANDROID_DEVICES,
    ANDROID_SCREEN_ON,
    ANDROID_SCREEN_OFF,
    ANDROID_LOCK_DEVICE,
    ANDROID_GET_FOREGROUND_APP,
    ANDROID_LAUNCH_APP,
    ANDROID_STOP_APP,
    ANDROID_TAKE_SCREENSHOT,
    ANDROID_GET_ORIENTATION,
    ANDROID_GET_LOGCAT,
    
    // Automation
    GET_AUTOMATION_RULES,
    ADD_AUTOMATION_RULE,
    REMOVE_AUTOMATION_RULE,
    ENABLE_AUTOMATION_RULE,
    DISABLE_AUTOMATION_RULE,
    
    // Generic
    PING,
    SHUTDOWN
};

// Module identifiers
enum class Module {
    SYSTEM,
    DEVICE,
    NETWORK,
    PROCESS,
    ANDROID,
    AUTOMATION
};

// Command data structures
struct Command {
    CommandType type;
    Module module;
    std::string id;
    std::map<std::string, std::string> parameters;
    std::chrono::system_clock::time_point timestamp;
};

// Response structures
struct Response {
    std::string commandId;
    CommandStatus status;
    std::string message;
    std::map<std::string, std::string> data;
    std::chrono::system_clock::time_point timestamp;
};

// Event structures
struct Event {
    std::string id;
    Module module;
    std::string type;
    std::map<std::string, std::string> data;
    std::chrono::system_clock::time_point timestamp;
};

// Command factory functions
inline Command createCommand(CommandType type, Module module, const std::map<std::string, std::string>& params = {}) {
    Command cmd;
    cmd.type = type;
    cmd.module = module;
    cmd.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    cmd.parameters = params;
    cmd.timestamp = std::chrono::system_clock::now();
    return cmd;
}

inline Response createResponse(const std::string& commandId, CommandStatus status, const std::string& message = "", const std::map<std::string, std::string>& data = {}) {
    Response resp;
    resp.commandId = commandId;
    resp.status = status;
    resp.message = message;
    resp.data = data;
    resp.timestamp = std::chrono::system_clock::now();
    return resp;
}

inline Event createEvent(Module module, const std::string& type, const std::map<std::string, std::string>& data = {}) {
    Event evt;
    evt.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    evt.module = module;
    evt.type = type;
    evt.data = data;
    evt.timestamp = std::chrono::system_clock::now();
    return evt;
}

// Utility functions for command handling
std::string commandTypeToString(CommandType type);
CommandType stringToCommandType(const std::string& str);

} // namespace SysMon
