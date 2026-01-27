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
    
    core_ = core;
    initialized_ = true;
    return true;
}

void AutomationEngine::shutdown() {
    if (!initialized_) {
        return;
    }
    
    running_ = false;
    
    if (evaluationThread_.joinable()) {
        evaluationThread_.join();
    }
    
    initialized_ = false;
}

bool AutomationEngine::start() {
    if (!initialized_) {
        return false;
    }
    
    if (running_) {
        return true;
    }
    
    running_ = true;
    evaluationThread_ = std::thread(&AutomationEngine::evaluationThread, this);
    
    return true;
}

void AutomationEngine::stop() {
    running_ = false;
    
    if (evaluationThread_.joinable()) {
        evaluationThread_.join();
    }
}

void AutomationEngine::evaluationThread() {
    while (running_) {
        try {
            evaluateAllRules();
            
            std::this_thread::sleep_for(evaluationInterval_);
            
        } catch (const std::exception& e) {
            // Log error but continue
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void AutomationEngine::evaluateAllRules() {
    std::shared_lock<std::shared_mutex> lock(rulesMutex_);
    
    for (auto& rule : rules_) {
        if (rule.isEnabled) {
            evaluateRule(rule);
        }
    }
}

bool AutomationEngine::evaluateRule(AutomationRule& rule) {
    // Temporary simplified implementation
    try {
        bool conditionMet = evaluateCondition(rule.condition);
        
        if (conditionMet) {
            // Check duration requirement
            auto now = std::chrono::steady_clock::now();
            auto it = conditionTimers_.find(rule.id);
            
            if (it == conditionTimers_.end()) {
                // Start timer
                conditionTimers_[rule.id] = now;
                return false;
            }
            
            // Check if duration has passed
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second);
            if (elapsed >= rule.duration) {
                // Execute action
                executeAction(rule.action);
                
                // Reset timer
                conditionTimers_[rule.id] = now;
                return true;
            }
        } else {
            // Reset timer if condition is not met
            conditionTimers_.erase(rule.id);
        }
        
    } catch (const std::exception& e) {
        // Log error
    }
    
    return false;
}

bool AutomationEngine::evaluateCondition(const std::string& condition) {
    // Temporary simplified implementation
    return false;
}

void AutomationEngine::executeAction(const std::string& action) {
    // Temporary simplified implementation - just log
    std::cout << "Executing action: " << action << std::endl;
}

std::string AutomationEngine::extractConditionType(const std::string& condition) {
    // Extract the first word before any operators
    std::regex typeRegex(R"(\w+)");
    std::smatch match;
    
    if (std::regex_search(condition, match, typeRegex)) {
        return match.str();
    }
    
    return "";
}

std::string AutomationEngine::extractActionType(const std::string& action) {
    // Extract the first word
    std::regex typeRegex(R"(\w+)");
    std::smatch match;
    
    if (std::regex_search(action, match, typeRegex)) {
        return match.str();
    }
    
    return "";
}

bool AutomationEngine::addRule(const AutomationRule& rule) {
    std::unique_lock<std::shared_mutex> lock(rulesMutex_);
    
    // Check if rule already exists
    for (const auto& existingRule : rules_) {
        if (existingRule.id == rule.id) {
            return false;
        }
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

std::vector<AutomationRule> AutomationEngine::getRules() const {
    std::shared_lock<std::shared_mutex> lock(rulesMutex_);
    return rules_;
}

bool AutomationEngine::isValidCondition(const std::string& condition) {
    // Temporary simplified validation
    return !condition.empty() && condition.length() > 3;
}

bool AutomationEngine::isValidAction(const std::string& action) {
    // Temporary simplified validation
    return !action.empty() && action.length() > 3;
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
    rule2.action = "KILL_PROCESS";
    rule2.isEnabled = false; // Disabled by default for safety
    rule2.duration = std::chrono::seconds(5);
    
    addRule(rule1);
    addRule(rule2);
}

void AutomationEngine::parseAutomationRule(const std::string& line) {
    // Parse rule format: id=rule1;condition=CPU_LOAD > 80%;action=LOGOUT_USER;enabled=true
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
    
    addRule(rule);
}

} // namespace SysMon
