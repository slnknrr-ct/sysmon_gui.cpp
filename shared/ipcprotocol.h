#pragma once

#include "systemtypes.h"
#include "commands.h"
#include <string>
#include <memory>
#include <variant>

namespace SysMon {

// IPC Protocol implementation using JSON
class IpcProtocol {
public:
    // Serialize/Deserialize functions
    static std::string serializeCommand(const Command& command);
    static std::string serializeResponse(const Response& response);
    static std::string serializeEvent(const Event& event);
    
    static Command deserializeCommand(const std::string& json);
    static Response deserializeResponse(const std::string& json);
    static Event deserializeEvent(const std::string& json);
    
    // Message type identification
    enum class MessageType {
        COMMAND,
        RESPONSE,
        EVENT,
        UNKNOWN
    };
    
    static MessageType getMessageType(const std::string& json);
    
    // Error handling
    static std::string getLastError();
    static bool hasError();

private:
    // JSON parsing helpers
    static bool parseJson(const std::string& json, std::map<std::string, std::string>& result);
    static std::string createJson(const std::map<std::string, std::string>& data);
    static std::string escapeJsonString(const std::string& input);
    
    // Type conversion helpers
    static std::string commandTypeToString(CommandType type);
    static CommandType stringToCommandType(const std::string& str);
    
    static std::string moduleToString(Module module);
    static Module stringToModule(const std::string& str);
    
    static std::string commandStatusToString(CommandStatus status);
    static CommandStatus stringToCommandStatus(const std::string& str);
    
    // Constants
    static constexpr size_t MAX_MESSAGE_SIZE = 1024 * 1024; // 1MB max message size
    static constexpr size_t MAX_FIELD_COUNT = 100; // Max fields per message
    
    // Error state
    static thread_local std::string lastError_;
};

// IPC Message wrapper
class IpcMessage {
public:
    enum class Type {
        COMMAND,
        RESPONSE,
        EVENT
    };
    
    IpcMessage(const Command& cmd);
    IpcMessage(const Response& resp);
    IpcMessage(const Event& evt);
    
    Type getType() const;
    std::string toJson() const;
    
    // Accessors
    const Command* asCommand() const;
    const Response* asResponse() const;
    const Event* asEvent() const;

private:
    Type type_;
    std::variant<Command, Response, Event> data_;
};

} // namespace SysMon
