#pragma once

#include "../shared/systemtypes.h"
#include <string>
#include <map>
#include <memory>
#include <fstream>
#include <shared_mutex>
#include <mutex>
#include <atomic>

namespace SysMon {

// Config Manager - manages application configuration
class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    
    // Initialization
    bool initialize(const std::string& configFilePath);
    bool load();
    bool save();
    
    // Configuration access
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue = 0) const;
    double getDouble(const std::string& key, double defaultValue = 0.0) const;
    bool getBool(const std::string& key, bool defaultValue = false) const;
    
    void setString(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);
    void setDouble(const std::string& key, double value);
    void setBool(const std::string& key, bool value);
    
    // Configuration sections
    std::map<std::string, std::string> getSection(const std::string& section) const;
    void setSection(const std::string& section, const std::map<std::string, std::string>& values);
    
    // Automation rules persistence
    std::vector<AutomationRule> loadAutomationRules();
    bool saveAutomationRules(const std::vector<AutomationRule>& rules);
    
    // Status
    bool isLoaded() const;
    std::string getFilePath() const;

private:
    // Configuration parsing
    bool parseConfigFile();
    bool writeConfigFile();
    
    // Format helpers
    std::string escapeValue(const std::string& value) const;
    std::string unescapeValue(const std::string& value) const;
    
    // Automation rules serialization
    std::string serializeRule(const AutomationRule& rule) const;
    AutomationRule deserializeRule(const std::string& serializedRule) const;
    
    // File operations
    bool createBackup();
    bool restoreFromBackup();
    
    // Configuration storage
    std::map<std::string, std::string> configData_;
    mutable std::shared_mutex configMutex_;
    
    // File management
    std::string configFilePath_;
    std::string backupFilePath_;
    std::atomic<bool> loaded_;
    
    // Default configuration
    void initializeDefaults();
    void setDefault(const std::string& key, const std::string& value);

private:
    static constexpr const char* BACKUP_SUFFIX = ".backup";
    static constexpr const char* AUTOMATION_SECTION = "automation_rules";
};

} // namespace SysMon
