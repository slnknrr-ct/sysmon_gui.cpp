#pragma once

#include <string>
#include <map>
#include <memory>

namespace SysMon {

class GuiConfig {
public:
    static GuiConfig& getInstance();
    
    bool loadFromFile(const std::string& filename);
    void saveToFile(const std::string& filename);
    
    // Getters
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue = 0) const;
    bool getBool(const std::string& key, bool defaultValue = false) const;
    
    // Setters
    void setString(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);
    void setBool(const std::string& key, bool value);
    
    // Convenience methods
    int getAgentPort() const;
    std::string getAgentHost() const;
    
private:
    GuiConfig() = default;
    ~GuiConfig() = default;
    GuiConfig(const GuiConfig&) = delete;
    GuiConfig& operator=(const GuiConfig&) = delete;
    
    std::map<std::string, std::string> configData_;
    std::string configFilename_;
    
    void initializeDefaults();
    std::string trim(const std::string& str) const;
};

} // namespace SysMon
