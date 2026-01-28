#pragma once

#include <chrono>

namespace SysMon {
namespace Constants {

// Timing constants
constexpr std::chrono::milliseconds DEFAULT_UPDATE_INTERVAL{2000};
constexpr std::chrono::milliseconds SYSTEM_UPDATE_INTERVAL{2000};
constexpr std::chrono::milliseconds PROCESS_UPDATE_INTERVAL{3000};
constexpr std::chrono::milliseconds NETWORK_UPDATE_INTERVAL{2000};
constexpr std::chrono::milliseconds AUTOMATION_EVALUATION_INTERVAL{1000};
constexpr std::chrono::milliseconds ANDROID_UPDATE_INTERVAL{2000};
constexpr std::chrono::milliseconds DEVICE_UPDATE_INTERVAL{5000};

// Timeout constants
constexpr std::chrono::milliseconds DEFAULT_COMMAND_TIMEOUT{5000};
constexpr std::chrono::milliseconds ADB_COMMAND_TIMEOUT{10000};
constexpr std::chrono::milliseconds ERROR_DISPLAY_DURATION{5000};

// Memory constants (bytes)
constexpr uint64_t BYTES_PER_KB = 1024;
constexpr uint64_t BYTES_PER_MB = 1024 * 1024;
constexpr uint64_t BYTES_PER_GB = 1024 * 1024 * 1024;
constexpr uint64_t DEFAULT_MEMORY_TOTAL = 8ULL * BYTES_PER_GB;
constexpr uint64_t DEFAULT_MEMORY_USED = 4ULL * BYTES_PER_GB;

// Process constants
constexpr uint32_t CRITICAL_PID_RANGE_MAX = 200;
constexpr uint32_t DEFAULT_PROCESS_COUNT = 150;
constexpr uint32_t DEFAULT_THREAD_COUNT = 320;

// Network constants
constexpr uint16_t DEFAULT_IPC_PORT = 12345;
constexpr int DEFAULT_NETLINK_BUFFER_SIZE = 4096;
constexpr int MAX_NETWORK_INTERFACES = 32;

// Security constants
constexpr size_t MAX_MESSAGE_SIZE = 1024 * 1024; // 1MB
constexpr size_t MAX_CLIENTS = 10;
constexpr std::chrono::seconds CLIENT_TIMEOUT{300}; // 5 minutes
constexpr std::chrono::seconds AUTH_TOKEN_EXPIRY{3600}; // 1 hour
constexpr size_t MAX_REQUESTS_PER_MINUTE = 100;
constexpr std::chrono::seconds RATE_LIMIT_WINDOW{60}; // 1 minute
constexpr int MAX_LOGIN_ATTEMPTS = 3;
constexpr std::chrono::seconds LOCKOUT_DURATION{300}; // 5 minutes

// Automation constants
constexpr size_t MAX_AUTOMATION_RULES = 100;
constexpr int DEFAULT_CONDITION_DURATION = 5;
constexpr int DEFAULT_CPU_THRESHOLD = 80;
constexpr int DEFAULT_MEMORY_THRESHOLD = 90;

// System constants
constexpr int DEFAULT_CPU_CORES = 4;
constexpr uint64_t DEFAULT_CONTEXT_SWITCHES = 1000000;
constexpr std::chrono::seconds DEFAULT_UPTIME{3600};

// File and buffer sizes
constexpr int MAX_PATH_LENGTH = 260;
constexpr int MAX_BUFFER_SIZE = 128;
constexpr int MAX_COMMAND_LENGTH = 1024;
constexpr int MAX_FIELD_COUNT = 10;

// Error codes
enum class ErrorCode {
    SUCCESS = 0,
    INVALID_PARAMETER = 1,
    RESOURCE_NOT_FOUND = 2,
    PERMISSION_DENIED = 3,
    TIMEOUT = 4,
    CONNECTION_FAILED = 5,
    INVALID_DATA = 6,
    SYSTEM_ERROR = 7,
    UNKNOWN_ERROR = 99
};

// Error messages
namespace ErrorMessages {
    constexpr const char* INVALID_PARAMETER = "Invalid parameter provided";
    constexpr const char* RESOURCE_NOT_FOUND = "Resource not found";
    constexpr const char* PERMISSION_DENIED = "Permission denied";
    constexpr const char* TIMEOUT = "Operation timed out";
    constexpr const char* CONNECTION_FAILED = "Connection failed";
    constexpr const char* INVALID_DATA = "Invalid data format";
    constexpr const char* SYSTEM_ERROR = "System error occurred";
    constexpr const char* UNKNOWN_ERROR = "Unknown error occurred";
    constexpr const char* FAILED_TO_PARSE = "Failed to parse response";
    constexpr const char* COMMAND_FAILED = "Command execution failed";
}

} // namespace Constants
} // namespace SysMon
