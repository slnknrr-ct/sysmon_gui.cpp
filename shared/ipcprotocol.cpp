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

// Simple JSON parsing (basic implementation)
bool IpcProtocol::parseJson(const std::string& json, std::map<std::string, std::string>& result) {
    lastError_.clear();
    
    // Very simple JSON parser - just extracts key: value pairs
    // This is a basic implementation for MVP
    std::regex keyValueRegex(R"(\"([^\"]+)\"\s*:\s*\"([^\"]*)\")");
    std::sregex_iterator iter(json.begin(), json.end(), keyValueRegex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        std::smatch match = *iter;
        result[match[1].str()] = match[2].str();
    }
    
    return !result.empty();
}

// Create JSON from map
std::string IpcProtocol::createJson(const std::map<std::string, std::string>& data) {
    std::ostringstream oss;
    oss << "{";
    
    bool first = true;
    for (const auto& pair : data) {
        if (!first) {
            oss << ",";
        }
        oss << "\"" << pair.first << "\":\"" << pair.second << "\"";
        first = false;
    }
    
    oss << "}";
    return oss.str();
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

} // namespace SysMon
