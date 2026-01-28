# SysMon3 API Reference

## 📚 Overview

This document provides comprehensive API reference for SysMon3, including IPC protocol, command specifications, data structures, and security interfaces.

## 🚀 IPC Protocol

### Connection Establishment

#### Server Connection
```cpp
// Connect to SysMon3 agent
bool connectToAgent(const std::string& host = "localhost", int port = 12345);
```

#### Authentication
```json
{
  "type": "command",
  "id": "auth_001",
  "module": "generic",
  "command": "PING",
  "parameters": {
    "auth_token": "64_character_hex_token"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

### Message Format

All IPC messages follow this structure:

```json
{
  "type": "command|response|event",
  "id": "unique_identifier",
  "module": "system|device|network|process|android|automation|generic",
  "command": "command_type",
  "parameters": {
    "key": "value"
  },
  "timestamp": "ISO_8601_timestamp",
  "auth_token": "authentication_token"
}
```

### Response Format

```json
{
  "type": "response",
  "commandId": "original_command_id",
  "status": "SUCCESS|FAILED",
  "message": "human_readable_message",
  "data": {
    "key": "response_data"
  },
  "timestamp": "ISO_8601_timestamp"
}
```

## 📊 System Monitor API

### Commands

#### GET_SYSTEM_INFO
Get current system information.

**Request:**
```json
{
  "type": "command",
  "id": "sys_001",
  "module": "system",
  "command": "GET_SYSTEM_INFO",
  "parameters": {},
  "timestamp": "2024-01-01T12:00:00Z"
}
```

**Response:**
```json
{
  "type": "response",
  "commandId": "sys_001",
  "status": "SUCCESS",
  "message": "System info retrieved",
  "data": {
    "data": "{\"cpu_total\":25.5,\"memory_total\":8589934592,\"memory_used\":4294967296,\"memory_free\":2147483648,\"memory_cache\":536870912,\"memory_buffers\":268435456,\"process_count\":156,\"thread_count\":412,\"context_switches\":1234567,\"uptime_seconds\":86400,\"cpu_cores\":[20.1,15.3,30.2,12.4]}"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### GET_PROCESS_LIST
Get list of running processes.

**Request:**
```json
{
  "type": "command",
  "id": "sys_002",
  "module": "system",
  "command": "GET_PROCESS_LIST",
  "parameters": {
    "limit": 100,
    "sort_by": "cpu",
    "order": "desc"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

**Response:**
```json
{
  "type": "response",
  "commandId": "sys_002",
  "status": "SUCCESS",
  "message": "Process list retrieved",
  "data": {
    "data": "{\"process_count\":100,\"processes\":[{\"pid\":1234,\"name\":\"chrome\",\"cpu_usage\":15.5,\"memory_usage\":536870912,\"status\":\"Running\",\"parent_pid\":1,\"user\":\"user\"}]}"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

## 🔌 Device Manager API

### Commands

#### GET_USB_DEVICES
Get list of USB devices.

**Request:**
```json
{
  "type": "command",
  "id": "dev_001",
  "module": "device",
  "command": "GET_USB_DEVICES",
  "parameters": {},
  "timestamp": "2024-01-01T12:00:00Z"
}
```

**Response:**
```json
{
  "type": "response",
  "commandId": "dev_001",
  "status": "SUCCESS",
  "message": "USB devices retrieved",
  "data": {
    "data": "{\"device_count\":2,\"devices\":[{\"vid\":\"0x1234\",\"pid\":\"0x5678\",\"name\":\"USB Flash Drive\",\"serial_number\":\"ABC123\",\"connected\":true,\"enabled\":true}]}"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### ENABLE_USB_DEVICE
Enable a USB device.

**Request:**
```json
{
  "type": "command",
  "id": "dev_002",
  "module": "device",
  "command": "ENABLE_USB_DEVICE",
  "parameters": {
    "vid": "0x1234",
    "pid": "0x5678"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### DISABLE_USB_DEVICE
Disable a USB device.

**Request:**
```json
{
  "type": "command",
  "id": "dev_003",
  "module": "device",
  "command": "DISABLE_USB_DEVICE",
  "parameters": {
    "vid": "0x1234",
    "pid": "0x5678"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

## 🌐 Network Manager API

### Commands

#### GET_NETWORK_INTERFACES
Get list of network interfaces.

**Request:**
```json
{
  "type": "command",
  "id": "net_001",
  "module": "network",
  "command": "GET_NETWORK_INTERFACES",
  "parameters": {},
  "timestamp": "2024-01-01T12:00:00Z"
}
```

**Response:**
```json
{
  "type": "response",
  "commandId": "net_001",
  "status": "SUCCESS",
  "message": "Network interfaces retrieved",
  "data": {
    "data": "{\"interface_count\":2,\"interfaces\":[{\"name\":\"eth0\",\"ipv4\":\"192.168.1.100\",\"ipv6\":\"fe80::1\",\"enabled\":true,\"rx_bytes\":1024000,\"tx_bytes\":512000,\"rx_speed\":1024,\"tx_speed\":512}]}"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### ENABLE_NETWORK_INTERFACE
Enable a network interface.

**Request:**
```json
{
  "type": "command",
  "id": "net_002",
  "module": "network",
  "command": "ENABLE_NETWORK_INTERFACE",
  "parameters": {
    "interface": "eth0"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### DISABLE_NETWORK_INTERFACE
Disable a network interface.

**Request:**
```json
{
  "type": "command",
  "id": "net_003",
  "module": "network",
  "command": "DISABLE_NETWORK_INTERFACE",
  "parameters": {
    "interface": "eth0"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### SET_STATIC_IP
Configure static IP for network interface.

**Request:**
```json
{
  "type": "command",
  "id": "net_004",
  "module": "network",
  "command": "SET_STATIC_IP",
  "parameters": {
    "interface": "eth0",
    "ip": "192.168.1.100",
    "netmask": "255.255.255.0",
    "gateway": "192.168.1.1"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### SET_DHCP_IP
Configure DHCP for network interface.

**Request:**
```json
{
  "type": "command",
  "id": "net_005",
  "module": "network",
  "command": "SET_DHCP_IP",
  "parameters": {
    "interface": "eth0"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

## 🔄 Process Manager API

### Commands

#### TERMINATE_PROCESS
Terminate a process gracefully.

**Request:**
```json
{
  "type": "command",
  "id": "proc_001",
  "module": "process",
  "command": "TERMINATE_PROCESS",
  "parameters": {
    "pid": 1234
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### KILL_PROCESS
Force kill a process.

**Request:**
```json
{
  "type": "command",
  "id": "proc_002",
  "module": "process",
  "command": "KILL_PROCESS",
  "parameters": {
    "pid": 1234
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

## 📱 Android Manager API

### Commands

#### GET_ANDROID_DEVICES
Get list of connected Android devices.

**Request:**
```json
{
  "type": "command",
  "id": "android_001",
  "module": "android",
  "command": "GET_ANDROID_DEVICES",
  "parameters": {},
  "timestamp": "2024-01-01T12:00:00Z"
}
```

**Response:**
```json
{
  "type": "response",
  "commandId": "android_001",
  "status": "SUCCESS",
  "message": "Android devices retrieved",
  "data": {
    "data": "{\"device_count\":1,\"devices\":[{\"model\":\"Pixel 6\",\"androidVersion\":\"13\",\"serialNumber\":\"ABC123\",\"batteryLevel\":85,\"isScreenOn\":true,\"isLocked\":false,\"foregroundApp\":\"com.android.launcher\"}]}"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### ANDROID_SCREEN_ON
Turn Android device screen on.

**Request:**
```json
{
  "type": "command",
  "id": "android_002",
  "module": "android",
  "command": "ANDROID_SCREEN_ON",
  "parameters": {
    "serial": "ABC123"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### ANDROID_SCREEN_OFF
Turn Android device screen off.

**Request:**
```json
{
  "type": "command",
  "id": "android_003",
  "module": "android",
  "command": "ANDROID_SCREEN_OFF",
  "parameters": {
    "serial": "ABC123"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### ANDROID_LOCK_DEVICE
Lock Android device.

**Request:**
```json
{
  "type": "command",
  "id": "android_004",
  "module": "android",
  "command": "ANDROID_LOCK_DEVICE",
  "parameters": {
    "serial": "ABC123"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### ANDROID_GET_FOREGROUND_APP
Get foreground application.

**Request:**
```json
{
  "type": "command",
  "id": "android_005",
  "module": "android",
  "command": "ANDROID_GET_FOREGROUND_APP",
  "parameters": {
    "serial": "ABC123"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### ANDROID_LAUNCH_APP
Launch Android application.

**Request:**
```json
{
  "type": "command",
  "id": "android_006",
  "module": "android",
  "command": "ANDROID_LAUNCH_APP",
  "parameters": {
    "serial": "ABC123",
    "package": "com.example.app"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### ANDROID_STOP_APP
Stop Android application.

**Request:**
```json
{
  "type": "command",
  "id": "android_007",
  "module": "android",
  "command": "ANDROID_STOP_APP",
  "parameters": {
    "serial": "ABC123",
    "package": "com.example.app"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### ANDROID_TAKE_SCREENSHOT
Take device screenshot.

**Request:**
```json
{
  "type": "command",
  "id": "android_008",
  "module": "android",
  "command": "ANDROID_TAKE_SCREENSHOT",
  "parameters": {
    "serial": "ABC123"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### ANDROID_GET_ORIENTATION
Get device orientation.

**Request:**
```json
{
  "type": "command",
  "id": "android_009",
  "module": "android",
  "command": "ANDROID_GET_ORIENTATION",
  "parameters": {
    "serial": "ABC123"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### ANDROID_GET_LOGCAT
Get device logcat.

**Request:**
```json
{
  "type": "command",
  "id": "android_010",
  "module": "android",
  "command": "ANDROID_GET_LOGCAT",
  "parameters": {
    "serial": "ABC123",
    "lines": 100
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

## ⚡ Automation Engine API

### Commands

#### GET_AUTOMATION_RULES
Get automation rules.

**Request:**
```json
{
  "type": "command",
  "id": "auto_001",
  "module": "automation",
  "command": "GET_AUTOMATION_RULES",
  "parameters": {},
  "timestamp": "2024-01-01T12:00:00Z"
}
```

**Response:**
```json
{
  "type": "response",
  "commandId": "auto_001",
  "status": "SUCCESS",
  "message": "Automation rules retrieved",
  "data": {
    "data": "{\"rule_count\":1,\"rules\":[{\"id\":\"rule_001\",\"condition\":\"cpu_usage > 80\",\"action\":\"disable_usb\",\"enabled\":true,\"duration\":10}]}"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### ADD_AUTOMATION_RULE
Add new automation rule.

**Request:**
```json
{
  "type": "command",
  "id": "auto_002",
  "module": "automation",
  "command": "ADD_AUTOMATION_RULE",
  "parameters": {
    "condition": "cpu_usage > 80",
    "action": "disable_usb",
    "duration": 10,
    "enabled": true
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### REMOVE_AUTOMATION_RULE
Remove automation rule.

**Request:**
```json
{
  "type": "command",
  "id": "auto_003",
  "module": "automation",
  "command": "REMOVE_AUTOMATION_RULE",
  "parameters": {
    "id": "rule_001"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### ENABLE_AUTOMATION_RULE
Enable automation rule.

**Request:**
```json
{
  "type": "command",
  "id": "auto_004",
  "module": "automation",
  "command": "ENABLE_AUTOMATION_RULE",
  "parameters": {
    "id": "rule_001"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### DISABLE_AUTOMATION_RULE
Disable automation rule.

**Request:**
```json
{
  "type": "command",
  "id": "auto_005",
  "module": "automation",
  "command": "DISABLE_AUTOMATION_RULE",
  "parameters": {
    "id": "rule_001"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

## 🔧 Generic API

### Commands

#### PING
Ping the agent.

**Request:**
```json
{
  "type": "command",
  "id": "ping_001",
  "module": "generic",
  "command": "PING",
  "parameters": {},
  "timestamp": "2024-01-01T12:00:00Z"
}
```

**Response:**
```json
{
  "type": "response",
  "commandId": "ping_001",
  "status": "SUCCESS",
  "message": "PONG",
  "data": {},
  "timestamp": "2024-01-01T12:00:00Z"
}
```

#### SHUTDOWN
Shutdown the agent.

**Request:**
```json
{
  "type": "command",
  "id": "shutdown_001",
  "module": "generic",
  "command": "SHUTDOWN",
  "parameters": {},
  "timestamp": "2024-01-01T12:00:00Z"
}
```

## 📊 Data Structures

### SystemInfo
```cpp
struct SystemInfo {
    double cpuUsageTotal;
    std::vector<double> cpuCoresUsage;
    uint64_t memoryTotal;
    uint64_t memoryUsed;
    uint64_t memoryFree;
    uint64_t memoryCache;
    uint64_t memoryBuffers;
    uint32_t processCount;
    uint32_t threadCount;
    uint64_t contextSwitches;
    std::chrono::seconds uptime;
};
```

### ProcessInfo
```cpp
struct ProcessInfo {
    uint32_t pid;
    std::string name;
    double cpuUsage;
    uint64_t memoryUsage;
    std::string status;
    uint32_t parentPid;
    std::string user;
};
```

### UsbDevice
```cpp
struct UsbDevice {
    std::string vid;
    std::string pid;
    std::string name;
    std::string serialNumber;
    bool isConnected;
    bool isEnabled;
};
```

### NetworkInterface
```cpp
struct NetworkInterface {
    std::string name;
    std::string ipv4;
    std::string ipv6;
    bool isEnabled;
    uint64_t rxBytes;
    uint64_t txBytes;
    double rxSpeed;
    double txSpeed;
};
```

### AndroidDeviceInfo
```cpp
struct AndroidDeviceInfo {
    std::string model;
    std::string androidVersion;
    std::string serialNumber;
    uint32_t batteryLevel;
    bool isScreenOn;
    bool isLocked;
    std::string foregroundApp;
};
```

### AutomationRule
```cpp
struct AutomationRule {
    std::string id;
    std::string condition;
    std::string action;
    bool isEnabled;
    std::chrono::seconds duration;
};
```

## 🔒 Security API

### Authentication

#### Generate Token
```cpp
std::string generateClientToken();
```

#### Authenticate Client
```cpp
bool authenticateClient(const std::string& clientId, const std::string& token);
```

#### Check Authentication
```cpp
bool isClientAuthenticated(const std::string& clientId);
```

### Rate Limiting

#### Check Rate Limit
```cpp
bool isRateLimited(const std::string& clientId);
```

#### Set Rate Limit
```cpp
void setRateLimit(size_t maxRequests, std::chrono::seconds window);
```

### Input Validation

#### Validate Command
```cpp
bool validateCommand(const std::string& command);
```

#### Validate Parameters
```cpp
bool validateParameters(const std::string& parameters);
```

#### Validate Message Size
```cpp
bool validateMessageSize(size_t size);
```

## 📝 Events

### System Events
```json
{
  "type": "event",
  "id": "event_001",
  "module": "system",
  "event_type": "cpu_threshold_exceeded",
  "data": {
    "cpu_usage": 85.5,
    "threshold": 80.0
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

### Device Events
```json
{
  "type": "event",
  "id": "event_002",
  "module": "device",
  "event_type": "usb_device_connected",
  "data": {
    "vid": "0x1234",
    "pid": "0x5678",
    "name": "USB Flash Drive"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

### Android Events
```json
{
  "type": "event",
  "id": "event_003",
  "module": "android",
  "event_type": "device_connected",
  "data": {
    "serial": "ABC123",
    "model": "Pixel 6"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

## 🚨 Error Codes

### Status Codes
- `SUCCESS`: Command executed successfully
- `FAILED`: Command failed
- `INVALID_PARAMETERS`: Invalid parameters provided
- `UNAUTHORIZED`: Authentication required
- `RATE_LIMITED`: Rate limit exceeded
- `DEVICE_NOT_FOUND`: Device not found
- `PERMISSION_DENIED**: Insufficient permissions
- `TIMEOUT`: Operation timed out

### Error Response Format
```json
{
  "type": "response",
  "commandId": "cmd_001",
  "status": "FAILED",
  "message": "Device not found",
  "data": {
    "error_code": "DEVICE_NOT_FOUND",
    "details": "USB device with VID:0x1234 PID:0x5678 not found"
  },
  "timestamp": "2024-01-01T12:00:00Z"
}
```

## 🧪 Testing API

### Unit Test Examples
```cpp
// Test authentication
TEST(SecurityTest, TokenAuthentication) {
    std::string token = securityManager.generateClientToken();
    EXPECT_TRUE(securityManager.authenticateClient("client1", token));
    EXPECT_TRUE(securityManager.isClientAuthenticated("client1"));
}

// Test rate limiting
TEST(SecurityTest, RateLimiting) {
    std::string clientId = "client1";
    
    // Should allow first 100 requests
    for (int i = 0; i < 100; ++i) {
        EXPECT_FALSE(securityManager.isRateLimited(clientId));
    }
    
    // Should limit 101st request
    EXPECT_TRUE(securityManager.isRateLimited(clientId));
}
```

### Integration Test Examples
```bash
# Test IPC communication
./test_ipc --host localhost --port 12345

# Test Android integration
./test_android --serial ABC123

# Test automation rules
./test_automation --rule-file test_rules.json
```

## 📚 Client Libraries

### C++ Client
```cpp
#include "ipcclient.h"

int main() {
    SysMon::GUI::IpcClient client;
    
    // Connect to agent
    if (!client.connectToAgent("localhost", 12345)) {
        std::cerr << "Failed to connect" << std::endl;
        return 1;
    }
    
    // Authenticate
    client.authenticate();
    
    // Send command
    Command cmd = createCommand(CommandType::GET_SYSTEM_INFO);
    client.sendCommand(cmd, [](const Response& response) {
        std::cout << "Response: " << response.message << std::endl;
    });
    
    return 0;
}
```

### Python Client (Future)
```python
import sysmon3_client

client = sysmon3_client.IpcClient()
client.connect("localhost", 12345)
client.authenticate()

response = client.get_system_info()
print(response.data)
```

## 🔧 Configuration API

### Configuration Structure
```ini
[server]
port = 12345
max_clients = 10
bind_address = 127.0.0.1

[security]
require_authentication = true
token_expiry = 3600
max_message_size = 1048576
rate_limit = 100
rate_window = 60

[logging]
level = INFO
file = sysmon_agent.log
max_size = 10485760
max_files = 5
```

### Configuration API
```cpp
class ConfigManager {
public:
    bool loadConfig(const std::string& filename);
    bool saveConfig(const std::string& filename);
    
    int getPort() const;
    void setPort(int port);
    
    int getMaxClients() const;
    void setMaxClients(int maxClients);
    
    // Security settings
    int getTokenExpiry() const;
    void setTokenExpiry(int seconds);
    
    size_t getMaxMessageSize() const;
    void setMaxMessageSize(size_t size);
    
    // Logging settings
    std::string getLogLevel() const;
    void setLogLevel(const std::string& level);
};
```

---

## 📋 API Summary

SysMon3 provides a comprehensive API with:

- **🔒 Secure Authentication**: Token-based with rate limiting
- **📊 Full System Monitoring**: CPU, memory, processes, devices
- **🌐 Network Management**: Interface control and monitoring
- **📱 Android Integration**: Complete device management
- **⚡ Automation Engine**: Rule-based automation
- **📝 Comprehensive Logging**: Full audit trail
- **🧪 Testing Support**: Unit and integration test APIs
- **📚 Client Libraries**: Easy integration support

This API reference provides complete documentation for building clients and extending SysMon3 functionality.
