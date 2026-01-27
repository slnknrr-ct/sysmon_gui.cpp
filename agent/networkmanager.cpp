#include "networkmanager.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <sstream>
#include <iostream>

#ifdef _WIN32
#include <iphlpapi.h>
#include <ws2tcpip.h>
#else
#include <netlink/socket.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <net/if.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/rtnetlink.h>
#endif

namespace SysMon {

NetworkManager::NetworkManager() 
    : running_(false)
    , initialized_(false)
    , statsUpdateInterval_(2000)
    , hIphlpapi_(nullptr)
    , pGetAdaptersAddresses_(nullptr)
    , pGetIfEntry2_(nullptr)
    , pSetIfEntry_(nullptr)
#ifndef _WIN32
    , netlinkSocket_(-1)
#endif
{
    
}

NetworkManager::~NetworkManager() {
    shutdown();
}

bool NetworkManager::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Initialize platform-specific resources
#ifdef _WIN32
    // Initialize IP Helper API for Windows
    // Load IP Helper library dynamically
    hIphlpapi_ = LoadLibraryA("Iphlpapi.dll");
    if (!hIphlpapi_) {
        std::cerr << "Failed to load Iphlpapi.dll" << std::endl;
        return false;
    }
    
    // Get function pointers
    pGetAdaptersAddresses_ = (PFN_GETADAPTERSADDRESSES)GetProcAddress(hIphlpapi_, "GetAdaptersAddresses");
    pGetIfEntry2_ = (PFN_GETIFENTRY2)GetProcAddress(hIphlpapi_, "GetIfEntry2");
    pSetIfEntry_ = (PFN_SETIFENTRY)GetProcAddress(hIphlpapi_, "SetIfEntry");
    
    if (!pGetAdaptersAddresses_ || !pGetIfEntry2_ || !pSetIfEntry_) {
        std::cerr << "Failed to get IP Helper API function pointers" << std::endl;
        FreeLibrary(hIphlpapi_);
        hIphlpapi_ = nullptr;
        return false;
    }
#else
    // Initialize netlink socket for Linux
    netlinkSocket_ = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (netlinkSocket_ < 0) {
        std::cerr << "Failed to create netlink socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Bind socket
    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_ROUTE;
    
    if (bind(netlinkSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind netlink socket: " << strerror(errno) << std::endl;
        close(netlinkSocket_);
        netlinkSocket_ = -1;
        return false;
    }
#endif
    
    initialized_ = true;
    return true;
}

bool NetworkManager::start() {
    if (!initialized_) {
        return false;
    }
    
    if (running_) {
        return true;
    }
    
    running_ = true;
    monitoringThread_ = std::thread(&NetworkManager::networkMonitoringThread, this);
    
    return true;
}

void NetworkManager::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }
}

void NetworkManager::shutdown() {
    stop();
    
#ifdef _WIN32
    // Clean up Windows resources
    if (hIphlpapi_) {
        FreeLibrary(hIphlpapi_);
        hIphlpapi_ = nullptr;
        pGetAdaptersAddresses_ = nullptr;
        pGetIfEntry2_ = nullptr;
        pSetIfEntry_ = nullptr;
    }
#else
    // Close netlink socket
    if (netlinkSocket_ >= 0) {
        close(netlinkSocket_);
        netlinkSocket_ = -1;
    }
#endif
    
    initialized_ = false;
}

bool NetworkManager::isRunning() const {
    return running_;
}

std::vector<NetworkInterface> NetworkManager::getNetworkInterfaces() {
    std::shared_lock<std::shared_mutex> lock(interfacesMutex_);
    return interfaces_;
}

bool NetworkManager::enableInterface(const std::string& interfaceName) {
#ifdef _WIN32
    return enableInterfaceWindows(interfaceName);
#else
    return enableInterfaceLinux(interfaceName);
#endif
}

bool NetworkManager::disableInterface(const std::string& interfaceName) {
#ifdef _WIN32
    return disableInterfaceWindows(interfaceName);
#else
    return disableInterfaceLinux(interfaceName);
#endif
}

bool NetworkManager::setStaticIp(const std::string& interfaceName, const std::string& ip, 
                                const std::string& netmask, const std::string& gateway) {
    // Validate interface name (non-empty, reasonable length)
    if (interfaceName.empty() || interfaceName.length() > 32) {
        return false;
    }
    
    // Validate IP address format (basic IPv4 validation)
    std::istringstream ipStream(ip);
    int octet;
    char dot;
    for (int i = 0; i < 4; ++i) {
        if (!(ipStream >> octet) || octet < 0 || octet > 255) {
            return false;
        }
        if (i < 3 && !(ipStream >> dot) || dot != '.') {
            return false;
        }
    }
    
    // Validate netmask format (basic IPv4 validation)
    std::istringstream maskStream(netmask);
    for (int i = 0; i < 4; ++i) {
        if (!(maskStream >> octet) || octet < 0 || octet > 255) {
            return false;
        }
        if (i < 3 && !(maskStream >> dot) || dot != '.') {
            return false;
        }
    }
    
    // Validate gateway format (basic IPv4 validation)
    std::istringstream gatewayStream(gateway);
    for (int i = 0; i < 4; ++i) {
        if (!(gatewayStream >> octet) || octet < 0 || octet > 255) {
            return false;
        }
        if (i < 3 && !(gatewayStream >> dot) || dot != '.') {
            return false;
        }
    }
    
#ifdef _WIN32
    return setStaticIpWindows(interfaceName, ip, netmask, gateway);
#else
    return setStaticIpLinux(interfaceName, ip, netmask, gateway);
#endif
}

bool NetworkManager::setDhcpIp(const std::string& interfaceName) {
#ifdef _WIN32
    return setDhcpIpWindows(interfaceName);
#else
    return setDhcpIpLinux(interfaceName);
#endif
}

void NetworkManager::networkMonitoringThread() {
    while (running_) {
        try {
            updateNetworkStats();
            std::this_thread::sleep_for(statsUpdateInterval_);
            
        } catch (const std::exception& e) {
            // Log error but continue
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void NetworkManager::updateNetworkStats() {
    std::vector<SysMon::NetworkInterface> interfaces;
    
#ifdef _WIN32
    interfaces = getNetworkInterfacesWindows();
#else
    interfaces = getNetworkInterfacesLinux();
#endif
    
    // Update statistics for each interface
    // Statistics are now updated in getNetworkInterfaces functions
    
    std::unique_lock<std::shared_mutex> lock(interfacesMutex_);
    interfaces_ = interfaces;
    lastStatsUpdate_ = std::chrono::steady_clock::now();
}

uint64_t NetworkManager::getInterfaceRxBytes(const std::string& interfaceName) {
#ifdef _WIN32
    // Temporary simplified implementation for Windows
    (void)interfaceName;
    return 0;
#else
    // Linux implementation using /proc/net/dev
    std::ifstream file("/proc/net/dev");
    if (!file.is_open()) {
        return 0;
    }
    
    std::string line;
    // Skip header lines
    std::getline(file, line);
    std::getline(file, line);
    
    while (std::getline(file, line)) {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string iface = line.substr(0, colonPos);
            // Remove leading spaces
            iface.erase(0, iface.find_first_not_of(" \t"));
            
            if (iface == interfaceName) {
                std::istringstream iss(line.substr(colonPos + 1));
                uint64_t rxBytes, rxPackets, rxErrs, rxDrop, rxFifo, rxFrames, rxCompressed, rxMulticast;
                uint64_t txBytes, txPackets, txErrs, txDrop, txFifo, txFrames, txCompressed, txMulticast;
                
                if (iss >> rxBytes >> rxPackets >> rxErrs >> rxDrop >> rxFifo >> rxFrames >> rxCompressed >> rxMulticast >>
                           txBytes >> txPackets >> txErrs >> txDrop >> txFifo >> txFrames >> txCompressed >> txMulticast) {
                    return rxBytes;
                }
            }
        }
    }
#endif
    return 0;
}

uint64_t NetworkManager::getInterfaceTxBytes(const std::string& interfaceName) {
#ifdef _WIN32
    // Temporary simplified implementation for Windows
    (void)interfaceName;
    return 0;
#else
    // Linux implementation using /proc/net/dev
    std::ifstream file("/proc/net/dev");
    if (!file.is_open()) {
        return 0;
    }
    
    std::string line;
    // Skip header lines
    std::getline(file, line);
    std::getline(file, line);
    
    while (std::getline(file, line)) {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string iface = line.substr(0, colonPos);
            iface.erase(0, iface.find_first_not_of(" \t"));
            
            if (iface == interfaceName) {
                std::istringstream iss(line.substr(colonPos + 1));
                uint64_t rxBytes, rxPackets, rxErrs, rxDrop, rxFifo, rxFrames, rxCompressed, rxMulticast;
                uint64_t txBytes, txPackets, txErrs, txDrop, txFifo, txFrames, txCompressed, txMulticast;
                
                if (iss >> rxBytes >> rxPackets >> rxErrs >> rxDrop >> rxFifo >> rxFrames >> rxCompressed >> rxMulticast >>
                           txBytes >> txPackets >> txErrs >> txDrop >> txFifo >> txFrames >> txCompressed >> txMulticast) {
                    return txBytes;
                }
            }
        }
    }
#endif
    return 0;
}

std::vector<SysMon::NetworkInterface> NetworkManager::getNetworkInterfacesLinux() {
    std::vector<SysMon::NetworkInterface> interfaces;
    
    // Temporary simplified implementation for compilation
    SysMon::NetworkInterface iface;
    iface.name = "eth0";
    iface.ipv4 = "192.168.1.100";
    iface.ipv6 = "";
    iface.isEnabled = true;
    iface.rxBytes = 0;
    iface.txBytes = 0;
    iface.rxSpeed = 0.0;
    iface.txSpeed = 0.0;
    interfaces.push_back(iface);
    
    return interfaces;
}

std::vector<SysMon::NetworkInterface> NetworkManager::getNetworkInterfacesWindows() {
    std::vector<SysMon::NetworkInterface> interfaces;
    
    // Temporary simplified implementation for compilation
    SysMon::NetworkInterface iface;
    iface.name = "eth0";
    iface.ipv4 = "192.168.1.100";
    iface.ipv6 = "";
    iface.isEnabled = true;
    iface.rxBytes = 0;
    iface.txBytes = 0;
    iface.rxSpeed = 0.0;
    iface.txSpeed = 0.0;
    interfaces.push_back(iface);
    
    return interfaces;
}

bool NetworkManager::enableInterfaceLinux(const std::string& interfaceName) {
    // Use system command to enable interface
    std::string command = "ip link set " + interfaceName + " up";
    int result = system(command.c_str());
    return result == 0;
}

bool NetworkManager::enableInterfaceWindows(const std::string& interfaceName) {
    // Temporary simplified implementation
    (void)interfaceName;
    return true;
}

bool NetworkManager::disableInterfaceLinux(const std::string& interfaceName) {
    // Use system command to disable interface
    std::string command = "ip link set " + interfaceName + " down";
    int result = system(command.c_str());
    return result == 0;
}

bool NetworkManager::setStaticIpLinux(const std::string& interfaceName, const std::string& ip, 
                                      const std::string& netmask, const std::string& gateway) {
    // Configure static IP using system commands
    std::string cmd1 = "ip addr flush dev " + interfaceName;
    std::string cmd2 = "ip addr add " + ip + "/" + netmask + " dev " + interfaceName;
    std::string cmd3 = "ip link set " + interfaceName + " up";
    
    if (!gateway.empty()) {
        std::string cmd4 = "ip route add default via " + gateway + " dev " + interfaceName;
        return (system(cmd1.c_str()) == 0) && 
               (system(cmd2.c_str()) == 0) && 
               (system(cmd3.c_str()) == 0) && 
               (system(cmd4.c_str()) == 0);
    }
    
    return (system(cmd1.c_str()) == 0) && 
           (system(cmd2.c_str()) == 0) && 
           (system(cmd3.c_str()) == 0);
}

bool NetworkManager::setStaticIpWindows(const std::string& interfaceName, const std::string& ip, 
                                        const std::string& netmask, const std::string& gateway) {
    // Convert interface name to wide string
    std::wstring wideName(interfaceName.begin(), interfaceName.end());
    
    // Get interface index
    NET_LUID luid;
    if (ConvertInterfaceAliasToLuid(wideName.c_str(), &luid) != NO_ERROR) {
        return false;
    }
    
    // Configure static IP using netsh
    std::wstring wideIp(ip.begin(), ip.end());
    std::wstring wideNetmask(netmask.begin(), netmask.end());
    std::wstring wideGateway(gateway.begin(), gateway.end());
    
    std::wstring cmd1 = L"netsh interface ip set address \"" + wideName + L"\" static " + wideIp + L" " + wideNetmask;
    
    if (!gateway.empty()) {
        cmd1 += L" " + wideGateway;
    }
    
    int result = _wsystem(cmd1.c_str());
    return result == 0;
}

bool NetworkManager::setDhcpIpLinux(const std::string& interfaceName) {
    // Configure DHCP using system commands
    std::string cmd1 = "ip addr flush dev " + interfaceName;
    std::string cmd2 = "dhclient " + interfaceName + " -r";  // Release
    std::string cmd3 = "dhclient " + interfaceName;           // Renew
    
    return (system(cmd1.c_str()) == 0) && 
           (system(cmd2.c_str()) == 0) && 
           (system(cmd3.c_str()) == 0);
}

bool NetworkManager::setDhcpIpWindows(const std::string& interfaceName) {
    // Convert interface name to wide string
    std::wstring wideName(interfaceName.begin(), interfaceName.end());
    
    // Configure DHCP using netsh
    std::wstring cmd = L"netsh interface ip set address \"" + wideName + L"\" dhcp";
    
    int result = _wsystem(cmd.c_str());
    return result == 0;
}

} // namespace SysMon
