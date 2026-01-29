#include "gui_config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace SysMon {

GuiConfig& GuiConfig::getInstance() {
    static GuiConfig instance;
    return instance;
}

bool GuiConfig::loadFromFile(const std::string& filename) {
    configFilename_ = filename;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        initializeDefaults();
        return false;
    }
    
    configData_.clear();
    std::string line;
    
    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));
            configData_[key] = value;
        }
    }
    
    // Ensure defaults for missing values
    initializeDefaults();
    return true;
}

void GuiConfig::saveToFile(const std::string& filename) {
    std::ofstream file(filename);
    
    if (!file.is_open()) {
        return;
    }
    
    file << "# SysMon3 GUI Configuration File\n";
    file << "# Connection settings\n";
    
    for (const auto& pair : configData_) {
        file << pair.first << "=" << pair.second << "\n";
    }
}

std::string GuiConfig::getString(const std::string& key, const std::string& defaultValue) const {
    auto it = configData_.find(key);
    return (it != configData_.end()) ? it->second : defaultValue;
}

int GuiConfig::getInt(const std::string& key, int defaultValue) const {
    auto it = configData_.find(key);
    if (it != configData_.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

bool GuiConfig::getBool(const std::string& key, bool defaultValue) const {
    auto it = configData_.find(key);
    if (it != configData_.end()) {
        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return (value == "true" || value == "1" || value == "yes" || value == "on");
    }
    return defaultValue;
}

void GuiConfig::setString(const std::string& key, const std::string& value) {
    configData_[key] = value;
}

void GuiConfig::setInt(const std::string& key, int value) {
    configData_[key] = std::to_string(value);
}

void GuiConfig::setBool(const std::string& key, bool value) {
    configData_[key] = value ? "true" : "false";
}

int GuiConfig::getAgentPort() const {
    return getInt("gui.agent_port", 8081);
}

std::string GuiConfig::getAgentHost() const {
    return getString("gui.agent_host", "localhost");
}

void GuiConfig::initializeDefaults() {
    // Set defaults only if not already set
    if (configData_.find("gui.agent_host") == configData_.end()) {
        configData_["gui.agent_host"] = "localhost";
    }
    if (configData_.find("gui.agent_port") == configData_.end()) {
        configData_["gui.agent_port"] = "8081";
    }
    if (configData_.find("gui.auto_connect") == configData_.end()) {
        configData_["gui.auto_connect"] = "true";
    }
    if (configData_.find("gui.reconnect_interval") == configData_.end()) {
        configData_["gui.reconnect_interval"] = "5000";
    }
}

std::string GuiConfig::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

} // namespace SysMon
