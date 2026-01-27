#pragma once

#include "../shared/systemtypes.h"
#include <memory>
#include <thread>
#include <atomic>
#include <shared_mutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace SysMon {

// Android Manager - manages connected Android devices
class AndroidManager {
public:
    AndroidManager();
    ~AndroidManager();
    
    // Lifecycle
    bool initialize();
    bool start();
    void stop();
    void shutdown();
    
    // Device operations
    std::vector<AndroidDeviceInfo> getConnectedDevices();
    bool isDeviceConnected(const std::string& serialNumber);
    AndroidDeviceInfo getDeviceInfo(const std::string& serialNumber);
    
    // Device control
    bool turnScreenOn(const std::string& serialNumber);
    bool turnScreenOff(const std::string& serialNumber);
    bool lockDevice(const std::string& serialNumber);
    std::string getForegroundApp(const std::string& serialNumber);
    
    // App management
    std::vector<std::string> getInstalledApps(const std::string& serialNumber);
    bool launchApp(const std::string& serialNumber, const std::string& packageName);
    bool stopApp(const std::string& serialNumber, const std::string& packageName);
    
    // System operations
    std::string takeScreenshot(const std::string& serialNumber);
    std::string getScreenOrientation(const std::string& serialNumber);
    std::vector<std::string> getLogcat(const std::string& serialNumber, int lines = 100);
    
    // Status
    bool isRunning() const;

private:
    // Device monitoring
    void deviceMonitoringThread();
    void scanForDevices();
    
    // ADB command execution
    std::string executeAdbCommand(const std::string& command, const std::string& serialNumber = "");
    std::string executeAdbCommandWithTimeout(const std::string& command, 
                                           const std::string& serialNumber, 
                                           std::chrono::milliseconds timeout);
    bool isAdbAvailable();
    
    // Device information parsing
    AndroidDeviceInfo parseDeviceInfo(const std::string& serialNumber);
    std::vector<std::string> parseDeviceList();
    std::vector<std::string> parseInstalledApps(const std::string& serialNumber);
    
    // Platform-specific ADB handling
    std::string getAdbPath();
    bool startAdbServer();
    bool stopAdbServer();
    
    // Thread management
    std::thread monitoringThread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // Device storage
    std::vector<AndroidDeviceInfo> connectedDevices_;
    mutable std::shared_mutex devicesMutex_;
    
    // ADB process management
    std::string adbPath_;
    std::atomic<bool> adbServerRunning_;
    
    // Timing
    std::chrono::steady_clock::time_point lastScan_;
    std::chrono::milliseconds scanInterval_;
    
    // Constants
    static constexpr std::chrono::milliseconds ADB_TIMEOUT{5000};
    static constexpr std::chrono::milliseconds SCAN_INTERVAL{2000};
    static constexpr int MAX_LOGCAT_LINES = 100;
};

} // namespace SysMon
