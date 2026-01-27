#pragma once

#include "../shared/systemtypes.h"
#include "../shared/commands.h"
#include <memory>
#include <thread>
#include <atomic>
#include <shared_mutex>
#include <condition_variable>
#include <mutex>
#include <condition_variable>

namespace SysMon {

// Forward declarations
class AgentCore;

// Automation Engine - manages and executes automation rules
class AutomationEngine {
public:
    AutomationEngine();
    ~AutomationEngine();
    
    // Lifecycle
    bool initialize(AgentCore* core);
    bool start();
    void stop();
    void shutdown();
    
    // Rule management
    std::vector<AutomationRule> getRules() const;
    bool addRule(const AutomationRule& rule);
    bool removeRule(const std::string& ruleId);
    bool enableRule(const std::string& ruleId);
    bool disableRule(const std::string& ruleId);
    bool isRuleEnabled(const std::string& ruleId) const;
    
    // Status
    bool isRunning() const;
    size_t getActiveRulesCount() const;

private:
    // Rule evaluation
    void automationThread();
    void evaluateRules();
    bool evaluateCondition(const std::string& condition);
    void executeAction(const std::string& action);
    
    // Condition evaluation helpers
    bool evaluateCpuCondition(const std::string& condition);
    bool evaluateMemoryCondition(const std::string& condition);
    bool evaluateProcessCondition(const std::string& condition);
    bool evaluateDeviceCondition(const std::string& condition);
    bool evaluateNetworkCondition(const std::string& condition);
    bool evaluateAndroidCondition(const std::string& condition);
    
    // Action execution helpers
    void executeSystemAction(const std::string& action);
    void executeDeviceAction(const std::string& action);
    void executeNetworkAction(const std::string& action);
    void executeProcessAction(const std::string& action);
    void executeAndroidAction(const std::string& action);
    
    // Rule parsing
    std::string extractConditionType(const std::string& condition);
    std::string extractConditionValue(const std::string& condition);
    std::string extractActionType(const std::string& action);
    std::string extractActionValue(const std::string& action);
    
    // Validation helpers
    bool isValidCondition(const std::string& condition) const;
    bool isValidAction(const std::string& action) const;
    
    // Time-based conditions
    bool checkDurationCondition(const std::string& condition);
    void startConditionTimer(const std::string& condition);
    void stopConditionTimer(const std::string& condition);
    
    // Configuration and system integration
    void loadRulesFromConfiguration();
    double getCpuUsageFromSystem();
    void createDefaultRules();
    void parseAutomationRule(const std::string& line);
    
    // Thread management
    std::thread automationThread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // Core reference
    AgentCore* core_;
    
    // Rule storage
    std::vector<AutomationRule> rules_;
    mutable std::shared_mutex rulesMutex_;
    
    // Condition timing
    struct ConditionTimer {
        std::string condition;
        std::chrono::steady_clock::time_point startTime;
        bool active;
    };
    std::vector<ConditionTimer> conditionTimers_;
    std::mutex timersMutex_;
    
    // Timing
    std::chrono::steady_clock::time_point lastEvaluation_;
    std::chrono::milliseconds evaluationInterval_;
    
    // Constants
    static constexpr std::chrono::milliseconds EVALUATION_INTERVAL{1000};
    static constexpr size_t MAX_RULES = 100;
};

} // namespace SysMon
