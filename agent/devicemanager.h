#pragma once

#include "../shared/systemtypes.h"
#include <memory>
#include <thread>
#include <atomic>
#include <shared_mutex>

#ifndef _WIN32
#include <libudev.h>
#endif

namespace SysMon {

// Device Manager - manages USB and other devices
class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();
    
    // Lifecycle
    bool initialize();
    bool start();
    void stop();
    void shutdown();
    
    // USB device operations
    std::vector<UsbDevice> getUsbDevices();
    bool enableUsbDevice(const std::string& vid, const std::string& pid);
    bool disableUsbDevice(const std::string& vid, const std::string& pid);
    bool preventAutoConnect(const std::string& vid, const std::string& pid, bool prevent);
    
    // Status
    bool isRunning() const;

private:
    // Device monitoring
    void deviceMonitoringThread();
    void scanUsbDevices();
    
    // Platform-specific implementations
    std::vector<UsbDevice> scanUsbDevicesLinux();
    std::vector<UsbDevice> scanUsbDevicesWindows();
    bool enableUsbDeviceLinux(const std::string& vid, const std::string& pid);
    bool enableUsbDeviceWindows(const std::string& vid, const std::string& pid);
    bool disableUsbDeviceLinux(const std::string& vid, const std::string& pid);
    bool disableUsbDeviceWindows(const std::string& vid, const std::string& pid);
    
    // Device event handling
    void handleDeviceConnect(const UsbDevice& device);
    void handleDeviceDisconnect(const UsbDevice& device);
    
    // Thread management
    std::thread monitoringThread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // Device storage
    std::vector<UsbDevice> usbDevices_;
    std::vector<std::pair<std::string, std::string>> preventedDevices_; // VID,PID pairs
    mutable std::shared_mutex devicesMutex_;
    
    // Platform-specific data
#ifdef _WIN32
    // Windows-specific handles and data
#else
    // Linux-specific udev monitoring
    struct udev* udev_;
    struct udev_monitor* udevMonitor_;
    int monitorFd_;
#endif
};

} // namespace SysMon
