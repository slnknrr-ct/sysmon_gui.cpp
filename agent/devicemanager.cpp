#include "devicemanager.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <mutex>

#ifndef _WIN32
#include <libudev.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>
#else
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <usbioctl.h>
#endif

namespace SysMon {

DeviceManager::DeviceManager() 
    : running_(false)
    , initialized_(false) {
    
#ifndef _WIN32
    udev_ = nullptr;
    udevMonitor_ = nullptr;
    monitorFd_ = -1;
#endif
}

DeviceManager::~DeviceManager() {
    shutdown();
}

bool DeviceManager::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Initialize platform-specific resources
#ifndef _WIN32
    // Initialize udev for Linux
    udev_ = udev_new();
    if (udev_) {
        udevMonitor_ = udev_monitor_new_from_netlink(udev_, "udev");
        if (udevMonitor_) {
            udev_monitor_filter_add_match_subsystem_devtype(udevMonitor_, "usb", nullptr);
            if (udev_monitor_enable_receiving(udevMonitor_) == 0) {
                monitorFd_ = udev_monitor_get_fd(udevMonitor_);
            }
        }
    }
#else
    // Initialize Windows device management
    // Windows-specific initialization will be done in device scanning
#endif
    
    initialized_ = true;
    return true;
}

bool DeviceManager::start() {
    if (!initialized_) {
        return false;
    }
    
    if (running_) {
        return true;
    }
    
    running_ = true;
    monitoringThread_ = std::thread(&DeviceManager::deviceMonitoringThread, this);
    
    return true;
}

void DeviceManager::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
}

void DeviceManager::shutdown() {
    stop();
    
#ifndef _WIN32
    // Clean up udev resources
    if (udevMonitor_) {
        udev_monitor_unref(udevMonitor_);
        udevMonitor_ = nullptr;
    }
    if (udev_) {
        udev_unref(udev_);
        udev_ = nullptr;
    }
    monitorFd_ = -1;
#endif
    
    initialized_ = false;
}

bool DeviceManager::isRunning() const {
    return running_;
}

std::vector<UsbDevice> DeviceManager::getUsbDevices() {
    std::shared_lock<std::shared_mutex> lock(devicesMutex_);
    return usbDevices_;
}

bool DeviceManager::enableUsbDevice(const std::string& vid, const std::string& pid) {
    // Validate VID/PID format (hexadecimal, 4 characters each)
    if (vid.length() != 4 || pid.length() != 4) {
        return false;
    }
    
    // Check if VID/PID contains only hexadecimal characters
    for (char c : vid + pid) {
        if (!std::isxdigit(c)) {
            return false;
        }
    }
    
#ifdef _WIN32
    return enableUsbDeviceWindows(vid, pid);
#else
    return enableUsbDeviceLinux(vid, pid);
#endif
}

bool DeviceManager::disableUsbDevice(const std::string& vid, const std::string& pid) {
    // Validate VID/PID format (hexadecimal, 4 characters each)
    if (vid.length() != 4 || pid.length() != 4) {
        return false;
    }
    
    // Check if VID/PID contains only hexadecimal characters
    for (char c : vid + pid) {
        if (!std::isxdigit(c)) {
            return false;
        }
    }
    
#ifdef _WIN32
    return disableUsbDeviceWindows(vid, pid);
#else
    return disableUsbDeviceLinux(vid, pid);
#endif
}

bool DeviceManager::preventAutoConnect(const std::string& vid, const std::string& pid, bool prevent) {
    std::unique_lock<std::shared_mutex> lock(devicesMutex_);
    
    if (prevent) {
        preventedDevices_.push_back({vid, pid});
    } else {
        preventedDevices_.erase(
            std::remove_if(preventedDevices_.begin(), preventedDevices_.end(),
                [&vid, &pid](const auto& pair) {
                    return pair.first == vid && pair.second == pid;
                }),
            preventedDevices_.end());
    }
    
    return true;
}

void DeviceManager::deviceMonitoringThread() {
    while (running_) {
        try {
            scanUsbDevices();
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
        } catch (const std::exception& e) {
            // Log error but continue
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void DeviceManager::scanUsbDevices() {
    std::vector<UsbDevice> devices;
    
#ifdef _WIN32
    devices = scanUsbDevicesWindows();
#else
    devices = scanUsbDevicesLinux();
#endif
    
    std::unique_lock<std::shared_mutex> lock(devicesMutex_);
    usbDevices_ = devices;
}

std::vector<UsbDevice> DeviceManager::scanUsbDevicesLinux() {
    std::vector<UsbDevice> devices;
    
    // Temporary simplified implementation for Windows compilation
    UsbDevice device;
    device.vid = "1234";
    device.pid = "5678";
    device.name = "Sample USB Device";
    device.serialNumber = "ABC123";
    device.isConnected = true;
    device.isEnabled = true;
    devices.push_back(device);
    
    return devices;
}

std::vector<UsbDevice> DeviceManager::scanUsbDevicesWindows() {
    std::vector<UsbDevice> devices;
    
    // Get device information set for all USB devices
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(
        &GUID_DEVINTERFACE_USB_DEVICE,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return devices;
    }
    
    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    
    // Enumerate all USB devices
    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
        char buffer[1024];
        DWORD bufferSize = sizeof(buffer);
        
        UsbDevice usbDevice;
        
        // Get hardware ID which contains VID and PID
        if (SetupDiGetDeviceRegistryPropertyA(
                deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID,
                nullptr, (PBYTE)buffer, bufferSize, &bufferSize)) {
            
            std::string hardwareId(buffer);
            
            // Parse VID and PID from hardware ID
            size_t vidPos = hardwareId.find("VID_");
            size_t pidPos = hardwareId.find("PID_");
            
            if (vidPos != std::string::npos && pidPos != std::string::npos) {
                usbDevice.vid = hardwareId.substr(vidPos + 4, 4);
                usbDevice.pid = hardwareId.substr(pidPos + 4, 4);
                
                // Get device description
                if (SetupDiGetDeviceRegistryPropertyA(
                        deviceInfoSet, &deviceInfoData, SPDRP_DEVICEDESC,
                        nullptr, (PBYTE)buffer, bufferSize, &bufferSize)) {
                    usbDevice.name = buffer;
                } else {
                    usbDevice.name = "Unknown USB Device";
                }
                
                // Get serial number
                if (SetupDiGetDeviceRegistryPropertyA(
                        deviceInfoSet, &deviceInfoData, SPDRP_MFG,
                        nullptr, (PBYTE)buffer, bufferSize, &bufferSize)) {
                    usbDevice.serialNumber = buffer;
                } else {
                    usbDevice.serialNumber = "Unknown";
                }
                
                // Check if device is enabled
                DWORD configFlags = 0;
                if (SetupDiGetDeviceRegistryPropertyA(
                        deviceInfoSet, &deviceInfoData, SPDRP_CONFIGFLAGS,
                        nullptr, (PBYTE)&configFlags, sizeof(configFlags), nullptr)) {
                    usbDevice.isConnected = !(configFlags & 0x00000001); // CONFIGFLAG_DISABLED
                    usbDevice.isEnabled = usbDevice.isConnected;
                } else {
                    usbDevice.isConnected = true;
                    usbDevice.isEnabled = true;
                }
                
                devices.push_back(usbDevice);
            }
        }
    }
    
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return devices;
}

bool DeviceManager::enableUsbDeviceLinux(const std::string& vid, const std::string& pid) {
    (void)vid;
    (void)pid;
    // Temporary simplified implementation
    return true;
}

bool DeviceManager::enableUsbDeviceWindows(const std::string& vid, const std::string& pid) {
    // Use Windows SetupAPI to enable the device
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(
        &GUID_DEVINTERFACE_USB_DEVICE,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    
    bool found = false;
    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
        char buffer[1024];
        DWORD bufferSize = sizeof(buffer);
        
        if (SetupDiGetDeviceRegistryPropertyA(
                deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID,
                nullptr, (PBYTE)buffer, bufferSize, &bufferSize)) {
            
            std::string hardwareId(buffer);
            size_t vidPos = hardwareId.find("VID_" + vid);
            size_t pidPos = hardwareId.find("PID_" + pid);
            
            if (vidPos != std::string::npos && pidPos != std::string::npos) {
                // Enable the device
                SP_PROPCHANGE_PARAMS params;
                params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
                params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
                params.StateChange = DICS_ENABLE;
                params.Scope = DICS_FLAG_CONFIGSPECIFIC;
                params.HwProfile = 0;
                
                SetupDiSetClassInstallParamsA(deviceInfoSet, &deviceInfoData, 
                    (SP_CLASSINSTALL_HEADER*)&params, sizeof(params));
                SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, deviceInfoSet, &deviceInfoData);
                
                found = true;
                break;
            }
        }
    }
    
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return found;
}

bool DeviceManager::disableUsbDeviceLinux(const std::string& vid, const std::string& pid) {
    (void)vid;
    (void)pid;
    // Temporary simplified implementation
    return true;
}

bool DeviceManager::disableUsbDeviceWindows(const std::string& vid, const std::string& pid) {
    // Use Windows SetupAPI to disable the device
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(
        &GUID_DEVINTERFACE_USB_DEVICE,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    
    bool found = false;
    for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
        char buffer[1024];
        DWORD bufferSize = sizeof(buffer);
        
        if (SetupDiGetDeviceRegistryPropertyA(
                deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID,
                nullptr, (PBYTE)buffer, bufferSize, &bufferSize)) {
            
            std::string hardwareId(buffer);
            size_t vidPos = hardwareId.find("VID_" + vid);
            size_t pidPos = hardwareId.find("PID_" + pid);
            
            if (vidPos != std::string::npos && pidPos != std::string::npos) {
                // Disable the device
                SP_PROPCHANGE_PARAMS params;
                params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
                params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
                params.StateChange = DICS_DISABLE;
                params.Scope = DICS_FLAG_CONFIGSPECIFIC;
                params.HwProfile = 0;
                
                SetupDiSetClassInstallParamsA(deviceInfoSet, &deviceInfoData, 
                    (SP_CLASSINSTALL_HEADER*)&params, sizeof(params));
                SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, deviceInfoSet, &deviceInfoData);
                
                found = true;
                break;
            }
        }
    }
    
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return found;
}

void DeviceManager::handleDeviceConnect(const UsbDevice& device) {
    // Check if this device is in the prevented list
    std::shared_lock<std::shared_mutex> lock(devicesMutex_);
    
    for (const auto& prevented : preventedDevices_) {
        if (prevented.first == device.vid && prevented.second == device.pid) {
            // Device is prevented, disable it
            lock.unlock();
            disableUsbDevice(device.vid, device.pid);
            return;
        }
    }
    
    // Device is allowed, add to the list
    // The scanUsbDevices() function will handle the actual addition
}

void DeviceManager::handleDeviceDisconnect(const UsbDevice& device) {
    // Remove device from the list
    std::unique_lock<std::shared_mutex> lock(devicesMutex_);
    
    usbDevices_.erase(
        std::remove_if(usbDevices_.begin(), usbDevices_.end(),
            [&device](const UsbDevice& dev) {
                return dev.vid == device.vid && dev.pid == device.pid;
            }),
        usbDevices_.end());
}

} // namespace SysMon
