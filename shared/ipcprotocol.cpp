#include "ipcprotocol.h"
#include "commands.h"
#include <sstream>
#include <regex>
#include <algorithm>

namespace SysMon {

thread_local std::string IpcProtocol::lastError_;

// Serialize Command to JSON
std::string IpcProtocol::serializeCommand(const Command& command) {
    std::map<std::string, std::string> data;
    data["type"] = "command";
    data["id"] = command.id;
    data["module"] = moduleToString(command.module);
    data["action"] = commandTypeToString(command.type);
    
    // Add timestamp
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        command.timestamp.time_since_epoch()).count();
    data["timestamp"] = std::to_string(timestamp);
    
    // Add parameters
    for (const auto& param : command.parameters) {
        data["param_" + param.first] = param.second;
    }
    
    return createJson(data);
}

// Serialize Response to JSON
std::string IpcProtocol::serializeResponse(const Response& response) {
    std::map<std::string, std::string> data;
    data["type"] = "response";
    data["commandId"] = response.commandId;
    data["status"] = commandStatusToString(response.status);
    data["message"] = response.message;
    
    // Add timestamp
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        response.timestamp.time_since_epoch()).count();
    data["timestamp"] = std::to_string(timestamp);
    
    // Add data fields
    for (const auto& field : response.data) {
        data["data_" + field.first] = field.second;
    }
    
    return createJson(data);
}

// Serialize Event to JSON
std::string IpcProtocol::serializeEvent(const Event& event) {
    std::map<std::string, std::string> data;
    data["type"] = "event";
    data["id"] = event.id;
    data["module"] = moduleToString(event.module);
    data["eventType"] = event.type;
    
    // Add timestamp
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        event.timestamp.time_since_epoch()).count();
    data["timestamp"] = std::to_string(timestamp);
    
    // Add event data
    for (const auto& field : event.data) {
        data["event_" + field.first] = field.second;
    }
    
    return createJson(data);
}

// Deserialize Command from JSON
Command IpcProtocol::deserializeCommand(const std::string& json) {
    Command command;
    std::map<std::string, std::string> data;
    
    if (!parseJson(json, data)) {
        lastError_ = "Failed to parse JSON";
        return command;
    }
    
    command.id = data["id"];
    command.module = stringToModule(data["module"]);
    command.type = stringToCommandType(data["action"]);
    
    // Parse timestamp
    if (data.find("timestamp") != data.end()) {
        try {
            auto ms = std::stoll(data["timestamp"]);
            command.timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(ms));
        } catch (const std::exception& e) {
            // Use current time if timestamp parsing fails
            command.timestamp = std::chrono::system_clock::now();
        }
    }
    
    // Extract parameters
    for (const auto& pair : data) {
        if (pair.first.substr(0, 6) == "param_") {
            std::string paramName = pair.first.substr(6);
            command.parameters[paramName] = pair.second;
        }
    }
    
    return command;
}

// Deserialize Response from JSON
Response IpcProtocol::deserializeResponse(const std::string& json) {
    Response response;
    std::map<std::string, std::string> data;
    
    if (!parseJson(json, data)) {
        lastError_ = "Failed to parse JSON";
        return response;
    }
    
    response.commandId = data["commandId"];
    response.status = stringToCommandStatus(data["status"]);
    response.message = data["message"];
    
    // Parse timestamp
    if (data.find("timestamp") != data.end()) {
        try {
            auto ms = std::stoll(data["timestamp"]);
            response.timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(ms));
        } catch (const std::exception& e) {
            // Use current time if timestamp parsing fails
            response.timestamp = std::chrono::system_clock::now();
        }
    }
    
    // Extract data fields
    for (const auto& pair : data) {
        if (pair.first.substr(0, 5) == "data_") {
            std::string fieldName = pair.first.substr(5);
            response.data[fieldName] = pair.second;
        }
    }
    
    return response;
}

// Deserialize Event from JSON
Event IpcProtocol::deserializeEvent(const std::string& json) {
    Event event;
    std::map<std::string, std::string> data;
    
    if (!parseJson(json, data)) {
        lastError_ = "Failed to parse JSON";
        return event;
    }
    
    event.id = data["id"];
    event.module = stringToModule(data["module"]);
    event.type = data["eventType"];
    
    // Parse timestamp
    if (data.find("timestamp") != data.end()) {
        try {
            auto ms = std::stoll(data["timestamp"]);
            event.timestamp = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(ms));
        } catch (const std::exception& e) {
            // Use current time if timestamp parsing fails
            event.timestamp = std::chrono::system_clock::now();
        }
    }
    
    // Extract event data
    for (const auto& pair : data) {
        if (pair.first.substr(0, 6) == "event_") {
            std::string fieldName = pair.first.substr(6);
            event.data[fieldName] = pair.second;
        }
    }
    
    return event;
}

// Get message type from JSON
IpcProtocol::MessageType IpcProtocol::getMessageType(const std::string& json) {
    std::map<std::string, std::string> data;
    if (!parseJson(json, data)) {
        return MessageType::UNKNOWN;
    }
    
    std::string type = data["type"];
    if (type == "command") return MessageType::COMMAND;
    if (type == "response") return MessageType::RESPONSE;
    if (type == "event") return MessageType::EVENT;
    
    return MessageType::UNKNOWN;
}

// Get last error
std::string IpcProtocol::getLastError() {
    return lastError_;
}

// Check if there was an error
bool IpcProtocol::hasError() {
    return !lastError_.empty();
}

// Robust JSON parsing with validation
bool IpcProtocol::parseJson(const std::string& json, std::map<std::string, std::string>& result) {
    lastError_.clear();
    result.clear();
    
    // Size validation - prevent DoS
    if (json.empty() || json.size() > MAX_MESSAGE_SIZE) {
        lastError_ = "JSON size out of bounds (0 < size <= " + std::to_string(MAX_MESSAGE_SIZE) + ")";
        return false;
    }
    
    // Basic JSON structure validation
    if (json.front() != '{' || json.back() != '}') {
        lastError_ = "Invalid JSON structure - must start with { and end with }";
        return false;
    }
    
    // Check for balanced braces
    int braceCount = 0;
    bool inString = false;
    bool escaped = false;
    
    for (size_t i = 0; i < json.size(); ++i) {
        char c = json[i];
        
        if (escaped) {
            escaped = false;
            continue;
        }
        
        if (c == '\\') {
            escaped = true;
            continue;
        }
        
        if (c == '"') {
            inString = !inString;
            continue;
        }
        
        if (!inString) {
            if (c == '{') braceCount++;
            else if (c == '}') braceCount--;
            
            if (braceCount < 0) {
                lastError_ = "Unbalanced braces in JSON";
                return false;
            }
        }
    }
    
    if (braceCount != 0) {
        lastError_ = "Unbalanced braces in JSON";
        return false;
    }
    
    if (inString) {
        lastError_ = "Unterminated string in JSON";
        return false;
    }
    
    // Extract key-value pairs with proper parsing
    std::string content = json.substr(1, json.length() - 2); // Remove { }
    
    // Remove whitespace
    content.erase(std::remove_if(content.begin(), content.end(), ::isspace), content.end());
    
    if (content.empty()) {
        lastError_ = "Empty JSON object";
        return false;
    }
    
    // Parse key-value pairs
    size_t pos = 0;
    while (pos < content.length()) {
        // Find key start
        if (content[pos] != '"') {
            lastError_ = "Expected key starting with quote at position " + std::to_string(pos);
            return false;
        }
        pos++;
        
        // Find key end
        size_t keyEnd = content.find('"', pos);
        if (keyEnd == std::string::npos) {
            lastError_ = "Unterminated key string";
            return false;
        }
        
        std::string key = content.substr(pos, keyEnd - pos);
        pos = keyEnd + 1;
        
        // Skip whitespace and find colon
        while (pos < content.length() && content[pos] == ' ') pos++;
        if (pos >= content.length() || content[pos] != ':') {
            lastError_ = "Expected ':' after key '" + key + "'";
            return false;
        }
        pos++;
        
        // Skip whitespace and find value start
        while (pos < content.length() && content[pos] == ' ') pos++;
        if (pos >= content.length()) {
            lastError_ = "Expected value after ':' for key '" + key + "'";
            return false;
        }
        
        // Parse value (must be quoted string)
        if (content[pos] != '"') {
            lastError_ = "Value must be quoted string for key '" + key + "'";
            return false;
        }
        pos++;
        
        size_t valueEnd = content.find('"', pos);
        if (valueEnd == std::string::npos) {
            lastError_ = "Unterminated value string for key '" + key + "'";
            return false;
        }
        
        std::string value = content.substr(pos, valueEnd - pos);
        pos = valueEnd + 1;
        
        // Store pair
        result[key] = value;
        
        // Skip comma if present
        while (pos < content.length() && content[pos] == ' ') pos++;
        if (pos < content.length() && content[pos] == ',') {
            pos++;
            continue;
        }
        
        // Should be at end now
        break;
    }
    
    if (result.empty()) {
        lastError_ = "No valid key-value pairs found";
        return false;
    }
    
    return true;
}

// Create JSON from map with proper escaping
std::string IpcProtocol::createJson(const std::map<std::string, std::string>& data) {
    // Validate field count
    if (data.size() > MAX_FIELD_COUNT) {
        lastError_ = "Too many fields in JSON object (max " + std::to_string(MAX_FIELD_COUNT) + ")";
        return "{}";
    }
    
    std::ostringstream oss;
    oss << "{";
    
    bool first = true;
    for (const auto& pair : data) {
        if (!first) {
            oss << ",";
        }
        
        // Escape key
        oss << "\"" << escapeJsonString(pair.first) << "\":\"";
        
        // Escape value
        oss << escapeJsonString(pair.second) << "\"";
        
        first = false;
        
        // Check size limit during construction
        if (oss.tellp() > static_cast<std::streampos>(MAX_MESSAGE_SIZE)) {
            lastError_ = "JSON size exceeded during construction";
            return "{}";
        }
    }
    
    oss << "}";
    
    std::string result = oss.str();
    if (result.size() > MAX_MESSAGE_SIZE) {
        lastError_ = "Final JSON size exceeds limit";
        return "{}";
    }
    
    return result;
}

// Escape JSON string - prevent injection
std::string IpcProtocol::escapeJsonString(const std::string& input) {
    std::string escaped;
    escaped.reserve(input.length() * 2); // Reserve space for worst case
    
    for (char c : input) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (c >= 0 && c < 32) {
                    // Control characters
                    escaped += "\\u";
                    char hex[5];
                    snprintf(hex, sizeof(hex), "%04x", static_cast<unsigned char>(c));
                    escaped += hex;
                } else {
                    escaped += c;
                }
                break;
        }
    }
    
    return escaped;
}

// IpcMessage implementations
IpcMessage::IpcMessage(const Command& cmd) 
    : type_(Type::COMMAND), data_(cmd) {
}

IpcMessage::IpcMessage(const Response& resp) 
    : type_(Type::RESPONSE), data_(resp) {
}

IpcMessage::IpcMessage(const Event& evt) 
    : type_(Type::EVENT), data_(evt) {
}

IpcMessage::Type IpcMessage::getType() const {
    return type_;
}

std::string IpcMessage::toJson() const {
    switch (type_) {
        case Type::COMMAND:
            return IpcProtocol::serializeCommand(std::get<Command>(data_));
        case Type::RESPONSE:
            return IpcProtocol::serializeResponse(std::get<Response>(data_));
        case Type::EVENT:
            return IpcProtocol::serializeEvent(std::get<Event>(data_));
        default:
            return "{}";
    }
}

const Command* IpcMessage::asCommand() const {
    return type_ == Type::COMMAND ? &std::get<Command>(data_) : nullptr;
}

const Response* IpcMessage::asResponse() const {
    return type_ == Type::RESPONSE ? &std::get<Response>(data_) : nullptr;
}

const Event* IpcMessage::asEvent() const {
    return type_ == Type::EVENT ? &std::get<Event>(data_) : nullptr;
}

// Type conversion helper implementations
std::string IpcProtocol::commandTypeToString(CommandType type) {
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

CommandType IpcProtocol::stringToCommandType(const std::string& str) {
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
    return CommandType::PING; // Default fallback
}

std::string IpcProtocol::moduleToString(Module module) {
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

Module IpcProtocol::stringToModule(const std::string& str) {
    if (str == "SYSTEM") return Module::SYSTEM;
    if (str == "DEVICE") return Module::DEVICE;
    if (str == "NETWORK") return Module::NETWORK;
    if (str == "PROCESS") return Module::PROCESS;
    if (str == "ANDROID") return Module::ANDROID;
    if (str == "AUTOMATION") return Module::AUTOMATION;
    return Module::SYSTEM; // Default fallback
}

std::string IpcProtocol::commandStatusToString(CommandStatus status) {
    switch (status) {
        case CommandStatus::SUCCESS: return "SUCCESS";
        case CommandStatus::FAILED: return "FAILED";
        case CommandStatus::PENDING: return "PENDING";
        default: return "UNKNOWN";
    }
}

CommandStatus IpcProtocol::stringToCommandStatus(const std::string& str) {
    if (str == "SUCCESS") return CommandStatus::SUCCESS;
    if (str == "FAILED") return CommandStatus::FAILED;
    if (str == "PENDING") return CommandStatus::PENDING;
    return CommandStatus::FAILED; // Default fallback
}

} // namespace SysMon
