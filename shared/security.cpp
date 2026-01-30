#include "security.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <cctype>

#ifndef SYSMON_NO_OPENSSL
#include <openssl/rand.h>
#include <openssl/sha.h>
#endif

namespace SysMon {
namespace Security {

// RateLimiter implementation
bool RateLimiter::isAllowed() {
    auto now = std::chrono::steady_clock::now();
    
    // Remove old requests outside time window
    requests.erase(
        std::remove_if(requests.begin(), requests.end(),
            [now, this](const auto& time) {
                return (now - time) > timeWindow;
            }),
        requests.end()
    );
    
    if (requests.size() >= maxRequests) {
        return false;
    }
    
    requests.push_back(now);
    return true;
}

void RateLimiter::cleanup() {
    auto now = std::chrono::steady_clock::now();
    requests.erase(
        std::remove_if(requests.begin(), requests.end(),
            [now, this](const auto& time) {
                return (now - time) > timeWindow;
            }),
        requests.end()
    );
}

// SecurityManager implementation
SecurityManager& SecurityManager::getInstance() {
    static SecurityManager instance;
    return instance;
}

std::string SecurityManager::generateClientToken() {
    std::string token = generateSecureToken();
    std::string clientId = "client_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto clientAuth = std::make_unique<ClientAuth>(clientId, token);
    clientAuth->rateLimiter = RateLimiter(maxRequestsPerMinute_, rateLimitWindow_);
    authenticatedClients_[clientId] = std::move(clientAuth);
    
    return token;
}

bool SecurityManager::authenticateClient(const std::string& clientId, const std::string& token) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    // Special case: allow predefined GUI token
    if (token == "gui_client_token") {
        // Create or update client with GUI token
        auto it = authenticatedClients_.find(clientId);
        if (it == authenticatedClients_.end()) {
            auto client = std::make_unique<ClientAuth>(clientId, token);
            client->isAuthenticated = true;
            client->lastActivity = std::chrono::steady_clock::now();
            authenticatedClients_[clientId] = std::move(client);
        } else {
            auto& client = it->second;
            client->isAuthenticated = true;
            client->lastActivity = std::chrono::steady_clock::now();
        }
        return true;
    }
    
    // Normal token authentication
    auto it = authenticatedClients_.find(clientId);
    if (it == authenticatedClients_.end()) {
        return false;
    }
    
    auto& client = it->second;
    if (client->token != token) {
        return false;
    }
    
    client->isAuthenticated = true;
    client->lastActivity = std::chrono::steady_clock::now();
    return true;
}

bool SecurityManager::isClientAuthenticated(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    auto it = authenticatedClients_.find(clientId);
    return it != authenticatedClients_.end() && it->second->isAuthenticated;
}

void SecurityManager::removeClient(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    authenticatedClients_.erase(clientId);
}

bool SecurityManager::isRateLimited(const std::string& clientId) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    auto it = authenticatedClients_.find(clientId);
    if (it == authenticatedClients_.end()) {
        return false; // Allow unknown clients to authenticate
    }
    
    return !it->second->rateLimiter.isAllowed();
}

void SecurityManager::cleanupInactiveClients() {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::minutes(30); // 30 minutes timeout
    
    for (auto it = authenticatedClients_.begin(); it != authenticatedClients_.end();) {
        if ((now - it->second->lastActivity) > timeout) {
            it = authenticatedClients_.erase(it);
        } else {
            it->second->rateLimiter.cleanup();
            ++it;
        }
    }
}

bool SecurityManager::validateCommand(const std::string& command) {
    return Validation::isValidJson(command) && 
           command.size() <= maxMessageSize_ &&
           command.size() >= 2; // At least "{}"
}

bool SecurityManager::validateParameters(const std::string& parameters) {
    return Validation::isValidJson(parameters) &&
           parameters.size() <= maxMessageSize_ / 2; // Parameters should be smaller
}

bool SecurityManager::validateMessageSize(size_t size) {
    return size > 0 && size <= maxMessageSize_;
}

void SecurityManager::setMaxMessageSize(size_t maxSize) {
    maxMessageSize_ = maxSize;
}

void SecurityManager::setRateLimit(size_t maxRequests, std::chrono::seconds window) {
    maxRequestsPerMinute_ = maxRequests;
    rateLimitWindow_ = window;
}

std::string SecurityManager::generateSecureToken() {
    const int TOKEN_LENGTH = 32;
    std::vector<unsigned char> buffer(TOKEN_LENGTH);
    
#ifndef SYSMON_NO_OPENSSL
    // Use OpenSSL for cryptographically secure random bytes
    if (RAND_bytes(buffer.data(), TOKEN_LENGTH) != 1) {
        // Fallback to less secure method if OpenSSL fails
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        
        for (int i = 0; i < TOKEN_LENGTH; ++i) {
            buffer[i] = static_cast<unsigned char>(dis(gen));
        }
    }
#else
    // Fallback to C++11 random device if OpenSSL not available
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (int i = 0; i < TOKEN_LENGTH; ++i) {
        buffer[i] = static_cast<unsigned char>(dis(gen));
    }
#endif
    
    std::stringstream ss;
    for (unsigned char byte : buffer) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    
    return ss.str();
}

bool SecurityManager::isValidTokenFormat(const std::string& token) {
    return token.length() == 64 && 
           std::all_of(token.begin(), token.end(), [](char c) {
               return std::isxdigit(c);
           });
}

// Validation utilities implementation
namespace Validation {

bool isValidJson(const std::string& json) {
    if (json.empty()) return false;
    
    // Basic JSON validation
    if (json.front() != '{' && json.front() != '[') return false;
    if (json.back() != '}' && json.back() != ']') return false;
    
    // Check for balanced braces/brackets
    int braceCount = 0;
    int bracketCount = 0;
    bool inString = false;
    bool escaped = false;
    
    for (size_t i = 0; i < json.length(); ++i) {
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
        
        if (inString) continue;
        
        switch (c) {
            case '{': braceCount++; break;
            case '}': braceCount--; break;
            case '[': bracketCount++; break;
            case ']': bracketCount--; break;
        }
        
        if (braceCount < 0 || bracketCount < 0) return false;
    }
    
    return braceCount == 0 && bracketCount == 0 && !inString;
}

bool sanitizeString(std::string& str) {
    // Remove null bytes and control characters except newlines and tabs
    str.erase(std::remove_if(str.begin(), str.end(), [](char c) {
        return (c < 32 && c != '\n' && c != '\t' && c != '\r') || c == '\0';
    }), str.end());
    
    // Limit string length
    if (str.length() > 1024) {
        str = str.substr(0, 1024);
    }
    
    return !str.empty();
}

bool isValidCommandType(const std::string& type) {
    static const std::vector<std::string> validTypes = {
        "GET_SYSTEM_INFO", "GET_PROCESS_LIST", "GET_USB_DEVICES",
        "ENABLE_USB_DEVICE", "DISABLE_USB_DEVICE", "GET_NETWORK_INTERFACES",
        "ENABLE_NETWORK_INTERFACE", "DISABLE_NETWORK_INTERFACE", "SET_STATIC_IP",
        "SET_DHCP_IP", "TERMINATE_PROCESS", "KILL_PROCESS", "GET_ANDROID_DEVICES",
        "ANDROID_SCREEN_ON", "ANDROID_SCREEN_OFF", "ANDROID_LOCK_DEVICE",
        "ANDROID_GET_FOREGROUND_APP", "ANDROID_LAUNCH_APP", "ANDROID_STOP_APP",
        "ANDROID_TAKE_SCREENSHOT", "ANDROID_GET_ORIENTATION", "ANDROID_GET_LOGCAT",
        "GET_AUTOMATION_RULES", "ADD_AUTOMATION_RULE", "REMOVE_AUTOMATION_RULE",
        "ENABLE_AUTOMATION_RULE", "DISABLE_AUTOMATION_RULE", "PING", "SHUTDOWN"
    };
    
    return std::find(validTypes.begin(), validTypes.end(), type) != validTypes.end();
}

bool isValidModule(const std::string& module) {
    static const std::vector<std::string> validModules = {
        "SYSTEM", "DEVICE", "NETWORK", "PROCESS", "ANDROID", "AUTOMATION"
    };
    
    return std::find(validModules.begin(), validModules.end(), module) != validModules.end();
}

bool isValidParameterKey(const std::string& key) {
    if (key.empty() || key.length() > 64) return false;
    
    // Allow alphanumeric, underscore, and hyphen
    return std::all_of(key.begin(), key.end(), [](char c) {
        return std::isalnum(c) || c == '_' || c == '-';
    });
}

bool isValidParameterValue(const std::string& value) {
    if (value.length() > 512) return false;
    
    // Check for dangerous patterns
    static const std::vector<std::string> dangerousPatterns = {
        "<script", "javascript:", "vbscript:", "onload=", "onerror=",
        "eval(", "exec(", "system(", "shell_exec", "`", "$(", "${"
    };
    
    std::string lowerValue = value;
    std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);
    
    for (const auto& pattern : dangerousPatterns) {
        if (lowerValue.find(pattern) != std::string::npos) {
            return false;
        }
    }
    
    return true;
}

std::string escapeJsonString(const std::string& str) {
    std::string escaped;
    escaped.reserve(str.length() * 2); // Reserve space for potential escapes
    
    for (char c : str) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (c < 0x20) {
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

} // namespace Validation

} // namespace Security
} // namespace SysMon
