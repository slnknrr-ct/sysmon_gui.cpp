#include "androidmanager.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <regex>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <tlhelp32.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

namespace SysMon {

AndroidManager::AndroidManager() 
    : running_(false)
    , initialized_(false)
    , adbServerRunning_(false)
    , scanInterval_(2000) {
}

AndroidManager::~AndroidManager() {
    shutdown();
}

bool AndroidManager::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Find ADB path
    adbPath_ = getAdbPath();
    if (adbPath_.empty()) {
        return false;
    }
    
    // Check if ADB is available
    if (!isAdbAvailable()) {
        std::cerr << "ADB not found at: " << adbPath_ << std::endl;
        std::cerr << "Please ensure Android SDK Platform-Tools are installed and ADB is in PATH" << std::endl;
        std::cerr << "Common ADB locations:" << std::endl;
        std::cerr << "  - C:\\Users\\%USERNAME%\\AppData\\Local\\Android\\Sdk\\platform-tools\\adb.exe" << std::endl;
        std::cerr << "  - C:\\Android\\platform-tools\\adb.exe" << std::endl;
        std::cerr << "  - /usr/bin/adb" << std::endl;
        std::cerr << "  - /usr/local/bin/adb" << std::endl;
        return false;
    }
    
    initialized_ = true;
    return true;
}

bool AndroidManager::start() {
    if (!initialized_) {
        return false;
    }
    
    if (running_) {
        return true;
    }
    
    // Start ADB server
    if (!startAdbServer()) {
        return false;
    }
    
    running_ = true;
    monitoringThread_ = std::thread(&AndroidManager::deviceMonitoringThread, this);
    
    return true;
}

void AndroidManager::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
    
    // Stop ADB server
    stopAdbServer();
}

void AndroidManager::shutdown() {
    stop();
    initialized_ = false;
}

bool AndroidManager::isRunning() const {
    return running_;
}

std::vector<AndroidDeviceInfo> AndroidManager::getConnectedDevices() {
    std::shared_lock<std::shared_mutex> lock(devicesMutex_);
    return connectedDevices_;
}

bool AndroidManager::isDeviceConnected(const std::string& serialNumber) {
    std::shared_lock<std::shared_mutex> lock(devicesMutex_);
    for (const auto& device : connectedDevices_) {
        if (device.serialNumber == serialNumber) {
            return true;
        }
    }
    return false;
}

AndroidDeviceInfo AndroidManager::getDeviceInfo(const std::string& serialNumber) {
    std::shared_lock<std::shared_mutex> lock(devicesMutex_);
    for (const auto& device : connectedDevices_) {
        if (device.serialNumber == serialNumber) {
            return device;
        }
    }
    return AndroidDeviceInfo{};
}

bool AndroidManager::turnScreenOn(const std::string& serialNumber) {
    std::string command = "shell input keyevent KEYCODE_POWER";
    std::string result = executeAdbCommand(command, serialNumber);
    return !result.empty();
}

bool AndroidManager::turnScreenOff(const std::string& serialNumber) {
    std::string command = "shell input keyevent KEYCODE_POWER";
    std::string result = executeAdbCommand(command, serialNumber);
    return !result.empty();
}

bool AndroidManager::lockDevice(const std::string& serialNumber) {
    std::string command = "shell input keyevent KEYCODE_POWER";
    std::string result = executeAdbCommand(command, serialNumber);
    return !result.empty();
}

std::string AndroidManager::getForegroundApp(const std::string& serialNumber) {
    std::string command = "shell dumpsys window windows | grep -E 'mCurrentFocus|mFocusedApp'";
    std::string result = executeAdbCommand(command, serialNumber);
    
    // Parse foreground app from dumpsys output
    std::regex focusRegex(R"(mCurrentFocus=Window\{[^}]+\s+([^\s}]+)\})");
    std::smatch match;
    
    if (std::regex_search(result, match, focusRegex)) {
        std::string windowToken = match[1].str();
        
        // Get package name from window token
        std::string pkgCommand = "shell dumpsys window windows | grep '" + windowToken + "' | grep -o 'package=[^']*' | cut -d= -f2";
        std::string pkgResult = executeAdbCommand(pkgCommand, serialNumber);
        
        if (!pkgResult.empty()) {
            // Remove whitespace
            pkgResult.erase(0, pkgResult.find_first_not_of(" \t\n\r"));
            pkgResult.erase(pkgResult.find_last_not_of(" \t\n\r") + 1);
            return pkgResult;
        }
    }
    
    // Fallback: try to get from top activity
    std::string topCommand = "shell dumpsys activity top | grep 'ACTIVITY' | head -1";
    std::string topResult = executeAdbCommand(topCommand, serialNumber);
    
    std::regex activityRegex(R"(ACTIVITY\s+([^\s]+)/[^\s]+\s+[^\s]+\s+pid=\d+)");
    if (std::regex_search(topResult, match, activityRegex)) {
        return match[1].str();
    }
    
    return "Unknown";
}

std::vector<std::string> AndroidManager::getInstalledApps(const std::string& serialNumber) {
    std::string command = "shell pm list packages";
    std::string result = executeAdbCommand(command, serialNumber);
    
    std::vector<std::string> apps;
    std::istringstream iss(result);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Remove 'package:' prefix
        if (line.find("package:") == 0) {
            std::string packageName = line.substr(8);
            // Remove whitespace
            packageName.erase(0, packageName.find_first_not_of(" \t\n\r"));
            packageName.erase(packageName.find_last_not_of(" \t\n\r") + 1);
            
            if (!packageName.empty()) {
                apps.push_back(packageName);
            }
        }
    }
    
    return apps;
}

bool AndroidManager::launchApp(const std::string& serialNumber, const std::string& packageName) {
    std::string command = "shell monkey -p " + packageName + " -c android.intent.category.LAUNCHER 1";
    std::string result = executeAdbCommand(command, serialNumber);
    return !result.empty();
}

bool AndroidManager::stopApp(const std::string& serialNumber, const std::string& packageName) {
    std::string command = "shell am force-stop " + packageName;
    std::string result = executeAdbCommand(command, serialNumber);
    return !result.empty();
}

std::string AndroidManager::takeScreenshot(const std::string& serialNumber) {
    std::string command = "shell screencap -p /sdcard/screenshot.png";
    std::string result = executeAdbCommand(command, serialNumber);
    
    if (!result.empty()) {
        // Pull screenshot to local
        command = "pull /sdcard/screenshot.png screenshot.png";
        result = executeAdbCommand(command, serialNumber);
        
        // Clean up remote file
        executeAdbCommand("shell rm /sdcard/screenshot.png", serialNumber);
        
        return "screenshot.png";
    }
    
    return "";
}

std::string AndroidManager::getScreenOrientation(const std::string& serialNumber) {
    std::string command = "shell dumpsys input | grep 'SurfaceOrientation'";
    std::string result = executeAdbCommand(command, serialNumber);
    
    // Parse orientation value
    std::regex orientationRegex(R"(SurfaceOrientation=\s*(\d+))");
    std::smatch match;
    
    if (std::regex_search(result, match, orientationRegex)) {
        int orientation;
        
        try {
            orientation = std::stoi(match[1].str());
        } catch (const std::exception& e) {
            orientation = 1; // Default to portrait
        }
        
        switch (orientation) {
            case 0: return "portrait";
            case 1: return "landscape";
            case 2: return "reverse portrait";
            case 3: return "reverse landscape";
            default: return "unknown";
        }
    }
    
    // Fallback: try alternative method
    std::string altCommand = "shell dumpsys window | grep 'mRotation'";
    std::string altResult = executeAdbCommand(altCommand, serialNumber);
    
    std::regex rotationRegex(R"(mRotation=\s*\{([^}]+)\})");
    if (std::regex_search(altResult, match, rotationRegex)) {
        std::string rotation = match[1].str();
        if (rotation.find("0.0, 0.0, 0.0, 1.0") != std::string::npos) {
            return "portrait";
        } else if (rotation.find("0.0, 0.0, -1.0, 0.0") != std::string::npos) {
            return "landscape";
        }
    }
    
    return "unknown";
}

std::vector<std::string> AndroidManager::getLogcat(const std::string& serialNumber, int lines) {
    std::string command = "shell logcat -d -t " + std::to_string(lines);
    std::string result = executeAdbCommand(command, serialNumber);
    
    std::vector<std::string> logcat;
    std::istringstream iss(result);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Remove whitespace and skip empty lines
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        if (!line.empty()) {
            logcat.push_back(line);
        }
    }
    
    return logcat;
}

void AndroidManager::deviceMonitoringThread() {
    while (running_) {
        try {
            scanForDevices();
            lastScan_ = std::chrono::steady_clock::now();
            
            std::this_thread::sleep_for(scanInterval_);
            
        } catch (const std::exception& e) {
            // Log error but continue
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void AndroidManager::scanForDevices() {
    std::vector<AndroidDeviceInfo> devices;
    std::vector<std::string> deviceSerials = parseDeviceList();
    
    for (const auto& serial : deviceSerials) {
        AndroidDeviceInfo device = parseDeviceInfo(serial);
        devices.push_back(device);
    }
    
    std::unique_lock<std::shared_mutex> lock(devicesMutex_);
    connectedDevices_ = devices;
}

std::string AndroidManager::executeAdbCommand(const std::string& command, const std::string& serialNumber) {
    return executeAdbCommandWithTimeout(command, serialNumber, ADB_TIMEOUT);
}

std::string AndroidManager::executeAdbCommandWithTimeout(const std::string& command, 
                                                        const std::string& serialNumber, 
                                                        std::chrono::milliseconds timeout) {
    std::string fullCommand = adbPath_ + " ";
    if (!serialNumber.empty()) {
        fullCommand += "-s " + serialNumber + " ";
    }
    fullCommand += command;
    
    // Execute command with timeout support
    std::string result;
    
#ifdef _WIN32
    // Windows implementation with timeout
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return "";
    }
    
    // Set up process startup info
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES;
    
    ZeroMemory(&pi, sizeof(pi));
    
    // Create the process
    if (CreateProcessA(nullptr, const_cast<char*>(fullCommand.c_str()), nullptr, nullptr, TRUE, 
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        
        // Wait for process with timeout
        DWORD waitResult = WaitForSingleObject(pi.hProcess, timeout.count());
        
        if (waitResult == WAIT_OBJECT_0) {
            // Process completed, read output
            char buffer[128];
            DWORD bytesRead;
            while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                result += buffer;
            }
        } else if (waitResult == WAIT_TIMEOUT) {
            // Timeout, terminate process
            TerminateProcess(pi.hProcess, 1);
        }
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    CloseHandle(hReadPipe);
    CloseHandle(hWritePipe);
    
#else
    // Linux/macOS implementation with timeout
    FILE* pipe = popen(fullCommand.c_str(), "r");
    if (!pipe) {
        return "";
    }
    
    // Simple timeout implementation using select
    int pipeFd = fileno(pipe);
    fd_set readFds;
    struct timeval tv;
    
    FD_ZERO(&readFds);
    FD_SET(pipeFd, &readFds);
    tv.tv_sec = timeout.count() / 1000;
    tv.tv_usec = (timeout.count() % 1000) * 1000;
    
    char buffer[128];
    while (true) {
        int selectResult = select(pipeFd + 1, &readFds, nullptr, nullptr, &tv);
        
        if (selectResult > 0 && FD_ISSET(pipeFd, &readFds)) {
            if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            } else {
                break; // EOF
            }
        } else {
            // Timeout or error
            pclose(pipe);
            return "";
        }
    }
    
    pclose(pipe);
#endif
    
    return result;
}

bool AndroidManager::isAdbAvailable() {
    std::string command = adbPath_ + " version";
    std::string result = executeAdbCommand(command);
    return !result.empty();
}

AndroidDeviceInfo AndroidManager::parseDeviceInfo(const std::string& serialNumber) {
    AndroidDeviceInfo device;
    device.serialNumber = serialNumber;
    
    // Get device model
    std::string modelCommand = "shell getprop ro.product.model";
    std::string modelResult = executeAdbCommand(modelCommand, serialNumber);
    device.model = modelResult.empty() ? "Unknown" : modelResult;
    device.model.erase(0, device.model.find_first_not_of(" \t\n\r"));
    device.model.erase(device.model.find_last_not_of(" \t\n\r") + 1);
    
    // Get Android version
    std::string versionCommand = "shell getprop ro.build.version.release";
    std::string versionResult = executeAdbCommand(versionCommand, serialNumber);
    device.androidVersion = versionResult.empty() ? "Unknown" : versionResult;
    device.androidVersion.erase(0, device.androidVersion.find_first_not_of(" \t\n\r"));
    device.androidVersion.erase(device.androidVersion.find_last_not_of(" \t\n\r") + 1);
    
    // Get battery level
    std::string batteryCommand = "shell dumpsys battery | grep 'level:'";
    std::string batteryResult = executeAdbCommand(batteryCommand, serialNumber);
    std::regex batteryRegex(R"(level:\s*(\d+))");
    std::smatch batteryMatch;
    if (std::regex_search(batteryResult, batteryMatch, batteryRegex)) {
        try {
            device.batteryLevel = std::stoi(batteryMatch[1].str());
        } catch (const std::exception& e) {
            device.batteryLevel = 0; // Default battery level
        }
    } else {
        device.batteryLevel = 0;
    }
    
    // Check if screen is on
    std::string screenCommand = "shell dumpsys power | grep 'mScreenOn='";
    std::string screenResult = executeAdbCommand(screenCommand, serialNumber);
    device.isScreenOn = screenResult.find("mScreenOn=true") != std::string::npos;
    
    // Check if device is locked
    std::string lockCommand = "shell dumpsys window | grep 'mShowingLockscreen='";
    std::string lockResult = executeAdbCommand(lockCommand, serialNumber);
    device.isLocked = lockResult.find("mShowingLockscreen=true") != std::string::npos;
    
    // Get foreground app
    device.foregroundApp = getForegroundApp(serialNumber);
    
    return device;
}

std::vector<std::string> AndroidManager::parseDeviceList() {
    std::string command = "devices";
    std::string result = executeAdbCommand(command);
    
    std::vector<std::string> devices;
    std::istringstream iss(result);
    std::string line;
    
    // Skip header line
    std::getline(iss, line);
    
    while (std::getline(iss, line)) {
        // Parse device serial numbers
        std::istringstream lineStream(line);
        std::string serial, status;
        
        if (lineStream >> serial >> status) {
            // Only include devices that are online
            if (status == "device" || status == "recovery") {
                devices.push_back(serial);
            }
        }
    }
    
    return devices;
}

std::vector<std::string> AndroidManager::parseInstalledApps(const std::string& serialNumber) {
    std::vector<std::string> apps;
    
    // Get list of installed packages
    std::string command = "shell pm list packages -3"; // Third-party apps only
    std::string result = executeAdbCommand(command, serialNumber);
    
    if (result.empty()) {
        return apps;
    }
    
    // Parse the output
    std::istringstream iss(result);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Remove 'package:' prefix
        if (line.find("package:") == 0) {
            std::string packageName = line.substr(8); // Remove "package:"
            
            // Trim whitespace
            packageName.erase(0, packageName.find_first_not_of(" \t\n\r"));
            packageName.erase(packageName.find_last_not_of(" \t\n\r") + 1);
            
            if (!packageName.empty()) {
                apps.push_back(packageName);
            }
        }
    }
    
    // If no third-party apps found, get all apps
    if (apps.empty()) {
        command = "shell pm list packages";
        result = executeAdbCommand(command, serialNumber);
        
        std::istringstream iss2(result);
        while (std::getline(iss2, line)) {
            if (line.find("package:") == 0) {
                std::string packageName = line.substr(8);
                
                // Skip system apps (usually start with android., com.android., etc.)
                if (packageName.find("android.") != 0 && 
                    packageName.find("com.android.") != 0 &&
                    packageName.find("com.google.") != 0) {
                    
                    packageName.erase(0, packageName.find_first_not_of(" \t\n\r"));
                    packageName.erase(packageName.find_last_not_of(" \t\n\r") + 1);
                    
                    if (!packageName.empty()) {
                        apps.push_back(packageName);
                    }
                }
            }
        }
    }
    
    return apps;
}

std::string AndroidManager::getAdbPath() {
    // Try to find ADB in common locations
    std::vector<std::string> possiblePaths;
    
#ifdef _WIN32
    // Windows paths
    const char* localAppData = std::getenv("LOCALAPPDATA");
    const char* programFiles = std::getenv("ProgramFiles");
    const char* programFilesX86 = std::getenv("ProgramFiles(x86)");
    
    if (localAppData) {
        possiblePaths.push_back(std::string(localAppData) + "\\Android\\Sdk\\platform-tools\\adb.exe");
    }
    if (programFiles) {
        possiblePaths.push_back(std::string(programFiles) + "\\Android\\Sdk\\platform-tools\\adb.exe");
    }
    if (programFilesX86) {
        possiblePaths.push_back(std::string(programFilesX86) + "\\Android\\Sdk\\platform-tools\\adb.exe");
    }
    possiblePaths.push_back("adb.exe");
    possiblePaths.push_back("platform-tools\\adb.exe");
#else
    // Linux/macOS paths
    const char* home = std::getenv("HOME");
    if (home) {
        possiblePaths.push_back(std::string(home) + "/Android/Sdk/platform-tools/adb");
        possiblePaths.push_back(std::string(home) + "/.local/share/Android/Sdk/platform-tools/adb");
    }
    possiblePaths.push_back("/usr/local/bin/adb");
    possiblePaths.push_back("/usr/bin/adb");
    possiblePaths.push_back("adb");
    possiblePaths.push_back("./platform-tools/adb");
#endif
    
    // Test each path
    for (const auto& path : possiblePaths) {
        std::string testCommand = path + " version";
        std::string result = executeAdbCommand(testCommand);
        if (!result.empty()) {
            return path;
        }
    }
    
    return ""; // Not found
}

bool AndroidManager::startAdbServer() {
    std::string command = "start-server";
    std::string result = executeAdbCommand(command);
    adbServerRunning_ = !result.empty();
    return adbServerRunning_;
}

bool AndroidManager::stopAdbServer() {
    std::string command = "kill-server";
    std::string result = executeAdbCommand(command);
    adbServerRunning_ = false;
    return true;
}

} // namespace SysMon
