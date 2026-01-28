#pragma once

#include <string>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <memory>

namespace SysMon {
namespace Security {

// Rate limiting structure
struct RateLimiter {
    std::vector<std::chrono::steady_clock::time_point> requests;
    size_t maxRequests;
    std::chrono::seconds timeWindow;
    
    RateLimiter(size_t maxReq = 100, std::chrono::seconds window = std::chrono::seconds(60))
        : maxRequests(maxReq), timeWindow(window) {}
    
    bool isAllowed();
    void cleanup();
};

// Client authentication
struct ClientAuth {
    std::string clientId;
    std::string token;
    std::chrono::steady_clock::time_point lastActivity;
    RateLimiter rateLimiter;
    bool isAuthenticated;
    
    ClientAuth(const std::string& id, const std::string& tk) 
        : clientId(id), token(tk), isAuthenticated(false) {}
};

// Security manager
class SecurityManager {
public:
    static SecurityManager& getInstance();
    
    // Authentication
    std::string generateClientToken();
    bool authenticateClient(const std::string& clientId, const std::string& token);
    bool isClientAuthenticated(const std::string& clientId);
    void removeClient(const std::string& clientId);
    
    // Rate limiting
    bool isRateLimited(const std::string& clientId);
    void cleanupInactiveClients();
    
    // Input validation
    bool validateCommand(const std::string& command);
    bool validateParameters(const std::string& parameters);
    bool validateMessageSize(size_t size);
    
    // Security settings
    void setMaxMessageSize(size_t maxSize);
    void setRateLimit(size_t maxRequests, std::chrono::seconds window);
    
private:
    SecurityManager() = default;
    ~SecurityManager() = default;
    SecurityManager(const SecurityManager&) = delete;
    SecurityManager& operator=(const SecurityManager&) = delete;
    
    mutable std::mutex clientsMutex_;
    std::unordered_map<std::string, std::unique_ptr<ClientAuth>> authenticatedClients_;
    
    size_t maxMessageSize_ = 1024 * 1024; // 1MB default
    size_t maxRequestsPerMinute_ = 100;
    std::chrono::seconds rateLimitWindow_{60};
    
    // Token generation
    std::string generateSecureToken();
    bool isValidTokenFormat(const std::string& token);
};

// Input validation utilities
namespace Validation {
    bool isValidJson(const std::string& json);
    bool sanitizeString(std::string& str);
    bool isValidCommandType(const std::string& type);
    bool isValidModule(const std::string& module);
    bool isValidParameterKey(const std::string& key);
    bool isValidParameterValue(const std::string& value);
    std::string escapeJsonString(const std::string& str);
}

} // namespace Security
} // namespace SysMon
