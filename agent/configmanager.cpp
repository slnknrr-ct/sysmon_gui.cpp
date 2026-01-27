#include "configmanager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <shared_mutex>

namespace SysMon {

ConfigManager::ConfigManager() 
    : loaded_(false) {
}

ConfigManager::~ConfigManager() {
}

bool ConfigManager::initialize(const std::string& configFilePath) {
    configFilePath_ = configFilePath;
    backupFilePath_ = configFilePath + BACKUP_SUFFIX;
    
    initializeDefaults();
    return true;
}

bool ConfigManager::load() {
    std::ifstream file(configFilePath_);
    if (!file.is_open()) {
        return false;
    }
    
    if (!parseConfigFile()) {
        return false;
    }
    
    loaded_ = true;
    return true;
}

bool ConfigManager::save() {
    if (!createBackup()) {
        return false;
    }
    
    if (!writeConfigFile()) {
        restoreFromBackup();
        return false;
    }
    
    return true;
}

std::string ConfigManager::getString(const std::string& key, const std::string& defaultValue) const {
    std::shared_lock<std::shared_mutex> lock(configMutex_);
    
    auto it = configData_.find(key);
    if (it != configData_.end()) {
        return it->second;
    }
    
    return defaultValue;
}

int ConfigManager::getInt(const std::string& key, int defaultValue) const {
    std::string value = getString(key);
    if (!value.empty()) {
        try {
            return std::stoi(value);
        } catch (const std::exception&) {
            // Invalid integer format
        }
    }
    return defaultValue;
}

double ConfigManager::getDouble(const std::string& key, double defaultValue) const {
    std::string value = getString(key);
    if (!value.empty()) {
        try {
            return std::stod(value);
        } catch (const std::exception&) {
            // Invalid double format
        }
    }
    return defaultValue;
}

bool ConfigManager::getBool(const std::string& key, bool defaultValue) const {
    std::string value = getString(key);
    if (!value.empty()) {
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return (value == "true" || value == "1" || value == "yes" || value == "on");
    }
    return defaultValue;
}

void ConfigManager::setString(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(configMutex_);
    configData_[key] = value;
}

void ConfigManager::setInt(const std::string& key, int value) {
    setString(key, std::to_string(value));
}

void ConfigManager::setDouble(const std::string& key, double value) {
    setString(key, std::to_string(value));
}

void ConfigManager::setBool(const std::string& key, bool value) {
    setString(key, value ? "true" : "false");
}

std::map<std::string, std::string> ConfigManager::getSection(const std::string& section) const {
    std::shared_lock<std::shared_mutex> lock(configMutex_);
    
    std::map<std::string, std::string> sectionData;
    std::string sectionPrefix = section + ".";
    
    for (const auto& pair : configData_) {
        if (pair.first.substr(0, sectionPrefix.length()) == sectionPrefix) {
            std::string key = pair.first.substr(sectionPrefix.length());
            sectionData[key] = pair.second;
        }
    }
    
    return sectionData;
}

void ConfigManager::setSection(const std::string& section, const std::map<std::string, std::string>& values) {
    std::unique_lock<std::shared_mutex> lock(configMutex_);
    
    std::string sectionPrefix = section + ".";
    
    // Remove existing section keys
    for (auto it = configData_.begin(); it != configData_.end();) {
        if (it->first.substr(0, sectionPrefix.length()) == sectionPrefix) {
            it = configData_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Add new section keys
    for (const auto& pair : values) {
        configData_[sectionPrefix + pair.first] = pair.second;
    }
}

std::vector<AutomationRule> ConfigManager::loadAutomationRules() {
    std::vector<AutomationRule> rules;
    
    std::string rulesSection = getString(AUTOMATION_SECTION, "");
    if (rulesSection.empty()) {
        return rules;
    }
    
    // TODO: Parse automation rules from config
    // This is a basic placeholder implementation
    
    AutomationRule rule;
    rule.id = "rule1";
    rule.condition = "CPU_LOAD > 80%";
    rule.action = "DISABLE_USB 1234:5678";
    rule.isEnabled = true;
    rule.duration = std::chrono::seconds(10);
    rules.push_back(rule);
    
    return rules;
}

bool ConfigManager::saveAutomationRules(const std::vector<AutomationRule>& rules) {
    std::string rulesData;
    
    for (const auto& rule : rules) {
        rulesData += serializeRule(rule) + "\n";
    }
    
    setString(AUTOMATION_SECTION, rulesData);
    return save();
}

bool ConfigManager::isLoaded() const {
    return loaded_;
}

std::string ConfigManager::getFilePath() const {
    return configFilePath_;
}

bool ConfigManager::parseConfigFile() {
    std::ifstream file(configFilePath_);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Parse key=value pairs
        size_t equalPos = line.find('=');
        if (equalPos != std::string::npos) {
            std::string key = line.substr(0, equalPos);
            std::string value = line.substr(equalPos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            configData_[key] = unescapeValue(value);
        }
    }
    
    return true;
}

bool ConfigManager::writeConfigFile() {
    std::ofstream file(configFilePath_);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# SysMon3 Agent Configuration\n";
    file << "# Generated automatically - do not edit manually\n\n";
    
    for (const auto& pair : configData_) {
        file << pair.first << "=" << escapeValue(pair.second) << "\n";
    }
    
    return true;
}

std::string ConfigManager::escapeValue(const std::string& value) const {
    std::string escaped = value;
    
    // Escape special characters
    size_t pos = 0;
    while ((pos = escaped.find('\n', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\n");
        pos += 2;
    }
    
    return escaped;
}

std::string ConfigManager::unescapeValue(const std::string& value) const {
    std::string unescaped = value;
    
    // Unescape special characters
    size_t pos = 0;
    while ((pos = unescaped.find("\\n", pos)) != std::string::npos) {
        unescaped.replace(pos, 2, "\n");
        pos += 1;
    }
    
    return unescaped;
}

std::string ConfigManager::serializeRule(const AutomationRule& rule) const {
    std::ostringstream oss;
    oss << rule.id << "|" << rule.condition << "|" << rule.action << "|"
        << (rule.isEnabled ? "1" : "0") << "|" << rule.duration.count();
    return oss.str();
}

AutomationRule ConfigManager::deserializeRule(const std::string& serializedRule) const {
    AutomationRule rule;
    
    std::istringstream iss(serializedRule);
    std::string token;
    
    if (std::getline(iss, token, '|')) rule.id = token;
    if (std::getline(iss, token, '|')) rule.condition = token;
    if (std::getline(iss, token, '|')) rule.action = token;
    if (std::getline(iss, token, '|')) rule.isEnabled = (token == "1");
    if (std::getline(iss, token, '|')) {
        try {
            rule.duration = std::chrono::seconds(std::stol(token));
        } catch (const std::exception& e) {
            rule.duration = std::chrono::seconds(0); // Default duration
        }
    }
    
    return rule;
}

bool ConfigManager::createBackup() {
    if (std::filesystem::exists(configFilePath_)) {
        return std::filesystem::copy_file(configFilePath_, backupFilePath_, 
                                         std::filesystem::copy_options::overwrite_existing);
    }
    return true;
}

bool ConfigManager::restoreFromBackup() {
    if (std::filesystem::exists(backupFilePath_)) {
        return std::filesystem::copy_file(backupFilePath_, configFilePath_,
                                         std::filesystem::copy_options::overwrite_existing);
    }
    return false;
}

void ConfigManager::initializeDefaults() {
    // Agent settings
    setString("agent.ipc_port", "12345");
    setString("agent.log_level", "INFO");
    setString("agent.log_file", "sysmon_agent.log");
    
    // System monitor settings
    setString("system.update_interval", "1000");
    
    // Device manager settings
    setString("devices.scan_interval", "5000");
    
    // Network manager settings
    setString("network.update_interval", "2000");
    
    // Process manager settings
    setString("processes.update_interval", "2000");
    setString("processes.max_display", "200");
    
    // Android manager settings
    setString("android.scan_interval", "2000");
    setString("android.adb_timeout", "5000");
    
    // Automation settings
    setString("automation.enabled", "true");
    setString("automation.evaluation_interval", "1000");
    setString("automation.max_rules", "100");
}

void ConfigManager::setDefault(const std::string& key, const std::string& value) {
    // Only set if key doesn't exist
    if (configData_.find(key) == configData_.end()) {
        configData_[key] = value;
    }
}

} // namespace SysMon
