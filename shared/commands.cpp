#include "commands.h"
#include <sstream>
#include <iomanip>

namespace SysMon {

// Implementation of command factory functions are already in header as inline functions

// Additional utility functions for command handling
std::string commandTypeToString(CommandType type) {
    switch (type) {
        case CommandType::GET_SYSTEM_INFO: return "GET_SYSTEM_INFO";
        case CommandType::GET_PROCESS_LIST: return "GET_PROCESS_LIST";
        case CommandType::GET_USB_DEVICES: return "GET_USB_DEVICES";
        case CommandType::ENABLE_USB_DEVICE: return "ENABLE_USB_DEVICE";
        case CommandType::DISABLE_USB_DEVICE: return "DISABLE_USB_DEVICE";
        case CommandType::GET_NETWORK_INTERFACES: return "GET_NETWORK_INTERFACES";
        case CommandType::ENABLE_NETWORK_INTERFACE: return "ENABLE_NETWORK_INTERFACE";
        case CommandType::DISABLE_NETWORK_INTERFACE: return "DISABLE_NETWORK_INTERFACE";
        case CommandType::SET_STATIC_IP: return "SET_STATIC_IP";
        case CommandType::SET_DHCP_IP: return "SET_DHCP_IP";
        case CommandType::TERMINATE_PROCESS: return "TERMINATE_PROCESS";
        case CommandType::KILL_PROCESS: return "KILL_PROCESS";
        case CommandType::GET_ANDROID_DEVICES: return "GET_ANDROID_DEVICES";
        case CommandType::ANDROID_SCREEN_ON: return "ANDROID_SCREEN_ON";
        case CommandType::ANDROID_SCREEN_OFF: return "ANDROID_SCREEN_OFF";
        case CommandType::ANDROID_LOCK_DEVICE: return "ANDROID_LOCK_DEVICE";
        case CommandType::ANDROID_GET_FOREGROUND_APP: return "ANDROID_GET_FOREGROUND_APP";
        case CommandType::ANDROID_LAUNCH_APP: return "ANDROID_LAUNCH_APP";
        case CommandType::ANDROID_STOP_APP: return "ANDROID_STOP_APP";
        case CommandType::ANDROID_TAKE_SCREENSHOT: return "ANDROID_TAKE_SCREENSHOT";
        case CommandType::ANDROID_GET_ORIENTATION: return "ANDROID_GET_ORIENTATION";
        case CommandType::ANDROID_GET_LOGCAT: return "ANDROID_GET_LOGCAT";
        case CommandType::GET_AUTOMATION_RULES: return "GET_AUTOMATION_RULES";
        case CommandType::ADD_AUTOMATION_RULE: return "ADD_AUTOMATION_RULE";
        case CommandType::REMOVE_AUTOMATION_RULE: return "REMOVE_AUTOMATION_RULE";
        case CommandType::ENABLE_AUTOMATION_RULE: return "ENABLE_AUTOMATION_RULE";
        case CommandType::DISABLE_AUTOMATION_RULE: return "DISABLE_AUTOMATION_RULE";
        case CommandType::PING: return "PING";
        case CommandType::SHUTDOWN: return "SHUTDOWN";
        default: return "UNKNOWN";
    }
}

CommandType stringToCommandType(const std::string& str) {
    if (str == "GET_SYSTEM_INFO") return CommandType::GET_SYSTEM_INFO;
    if (str == "GET_PROCESS_LIST") return CommandType::GET_PROCESS_LIST;
    if (str == "GET_USB_DEVICES") return CommandType::GET_USB_DEVICES;
    if (str == "ENABLE_USB_DEVICE") return CommandType::ENABLE_USB_DEVICE;
    if (str == "DISABLE_USB_DEVICE") return CommandType::DISABLE_USB_DEVICE;
    if (str == "GET_NETWORK_INTERFACES") return CommandType::GET_NETWORK_INTERFACES;
    if (str == "ENABLE_NETWORK_INTERFACE") return CommandType::ENABLE_NETWORK_INTERFACE;
    if (str == "DISABLE_NETWORK_INTERFACE") return CommandType::DISABLE_NETWORK_INTERFACE;
    if (str == "SET_STATIC_IP") return CommandType::SET_STATIC_IP;
    if (str == "SET_DHCP_IP") return CommandType::SET_DHCP_IP;
    if (str == "TERMINATE_PROCESS") return CommandType::TERMINATE_PROCESS;
    if (str == "KILL_PROCESS") return CommandType::KILL_PROCESS;
    if (str == "GET_ANDROID_DEVICES") return CommandType::GET_ANDROID_DEVICES;
    if (str == "ANDROID_SCREEN_ON") return CommandType::ANDROID_SCREEN_ON;
    if (str == "ANDROID_SCREEN_OFF") return CommandType::ANDROID_SCREEN_OFF;
    if (str == "ANDROID_LOCK_DEVICE") return CommandType::ANDROID_LOCK_DEVICE;
    if (str == "ANDROID_GET_FOREGROUND_APP") return CommandType::ANDROID_GET_FOREGROUND_APP;
    if (str == "ANDROID_LAUNCH_APP") return CommandType::ANDROID_LAUNCH_APP;
    if (str == "ANDROID_STOP_APP") return CommandType::ANDROID_STOP_APP;
    if (str == "ANDROID_TAKE_SCREENSHOT") return CommandType::ANDROID_TAKE_SCREENSHOT;
    if (str == "ANDROID_GET_ORIENTATION") return CommandType::ANDROID_GET_ORIENTATION;
    if (str == "ANDROID_GET_LOGCAT") return CommandType::ANDROID_GET_LOGCAT;
    if (str == "GET_AUTOMATION_RULES") return CommandType::GET_AUTOMATION_RULES;
    if (str == "ADD_AUTOMATION_RULE") return CommandType::ADD_AUTOMATION_RULE;
    if (str == "REMOVE_AUTOMATION_RULE") return CommandType::REMOVE_AUTOMATION_RULE;
    if (str == "ENABLE_AUTOMATION_RULE") return CommandType::ENABLE_AUTOMATION_RULE;
    if (str == "DISABLE_AUTOMATION_RULE") return CommandType::DISABLE_AUTOMATION_RULE;
    if (str == "PING") return CommandType::PING;
    if (str == "SHUTDOWN") return CommandType::SHUTDOWN;
    return CommandType::PING; // default
}

std::string moduleToString(Module module) {
    switch (module) {
        case Module::SYSTEM: return "SYSTEM";
        case Module::DEVICE: return "DEVICE";
        case Module::NETWORK: return "NETWORK";
        case Module::PROCESS: return "PROCESS";
        case Module::ANDROID: return "ANDROID";
        case Module::AUTOMATION: return "AUTOMATION";
        default: return "UNKNOWN";
    }
}

Module stringToModule(const std::string& str) {
    if (str == "SYSTEM") return Module::SYSTEM;
    if (str == "DEVICE") return Module::DEVICE;
    if (str == "NETWORK") return Module::NETWORK;
    if (str == "PROCESS") return Module::PROCESS;
    if (str == "ANDROID") return Module::ANDROID;
    if (str == "AUTOMATION") return Module::AUTOMATION;
    return Module::SYSTEM; // default
}

} // namespace SysMon
