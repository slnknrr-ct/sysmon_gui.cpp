#pragma once

#include "../shared/systemtypes.h"
#include <memory>
#include <thread>
#include <atomic>
#include <shared_mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
// Function pointer types for dynamic loading
typedef ULONG (WINAPI *PFN_GETADAPTERSADDRESSES)(
    ULONG Family, ULONG Flags, PVOID Reserved, PVOID pAdapterAddresses,
    PULONG pOutBufLen, PULONG pReserved);
typedef ULONG (WINAPI *PFN_GETIFENTRY2)(PVOID Row);
typedef DWORD (WINAPI *PFN_SETIFENTRY)(PMIB_IFROW Row);
#else
#include <ifaddrs.h>
#include <net/if.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#endif

namespace SysMon {

// Network Manager - manages network interfaces
class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    
    // Lifecycle
    bool initialize();
    bool start();
    void stop();
    void shutdown();
    
    // Network interface operations
    std::vector<NetworkInterface> getNetworkInterfaces();
    bool enableInterface(const std::string& interfaceName);
    bool disableInterface(const std::string& interfaceName);
    bool setStaticIp(const std::string& interfaceName, const std::string& ip, 
                     const std::string& netmask, const std::string& gateway);
    bool setDhcpIp(const std::string& interfaceName);
    
    // Status
    bool isRunning() const;

private:
    // Device monitoring
    void networkMonitoringThread();
    void updateNetworkStats();
    
    // Platform-specific implementations
    std::vector<NetworkInterface> getNetworkInterfacesLinux();
    std::vector<NetworkInterface> getNetworkInterfacesWindows();
    bool enableInterfaceLinux(const std::string& interfaceName);
    bool enableInterfaceWindows(const std::string& interfaceName);
    bool disableInterfaceLinux(const std::string& interfaceName);
    bool disableInterfaceWindows(const std::string& interfaceName);
    bool setStaticIpLinux(const std::string& interfaceName, const std::string& ip, 
                         const std::string& netmask, const std::string& gateway);
    bool setStaticIpWindows(const std::string& interfaceName, const std::string& ip, 
                           const std::string& netmask, const std::string& gateway);
    bool setDhcpIpLinux(const std::string& interfaceName);
    bool setDhcpIpWindows(const std::string& interfaceName);
    
    // Network statistics
    uint64_t getInterfaceRxBytes(const std::string& interfaceName);
    uint64_t getInterfaceTxBytes(const std::string& interfaceName);
    
    // Thread management
    std::thread monitoringThread_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // Data storage
    std::vector<NetworkInterface> interfaces_;
    mutable std::shared_mutex interfacesMutex_;
    
    // Timing
    std::chrono::steady_clock::time_point lastStatsUpdate_;
    std::chrono::milliseconds statsUpdateInterval_;
    
    // Platform-specific data
#ifdef _WIN32
    // Windows IP Helper API
    HMODULE hIphlpapi_;
    PFN_GETADAPTERSADDRESSES pGetAdaptersAddresses_;
    PFN_GETIFENTRY2 pGetIfEntry2_;
    PFN_SETIFENTRY pSetIfEntry_;
#else
    // Linux-specific netlink socket
    int netlinkSocket_;
#endif
};

} // namespace SysMon
