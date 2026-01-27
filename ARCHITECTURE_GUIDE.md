# SysMon3 Architecture Guide

This document provides a comprehensive overview of the SysMon3 project architecture, including component relationships, data flow, and system design.

## Project Overview

SysMon3 is a cross-platform system monitoring and management tool consisting of two main components:
- **Agent** - Background service that monitors system resources
- **GUI** - Qt-based graphical user interface for visualization and control

## High-Level Architecture

```
┌─────────────────┐    TCP/IP     ┌─────────────────┐
│   GUI Client    │ ◄──────────► │   Agent Server  │
│                 │               │                 │
│ - Qt Interface  │               │ - System Monitor│
│ - IPC Client    │               │ - Device Manager│
│ - Tab Views     │               │ - Network Manager│
└─────────────────┘               │ - Process Manager│
                                 │ - Android Manager│
                                 │ - Automation Engine│
                                 └─────────────────┘
```

## Directory Structure

```
sysmon3_gui.cpp/
├── CMakeLists.txt          # Main build configuration
├── .build.bat             # Windows build script
├── sysmon.desktop         # Linux desktop entry
├── sysmon_agent.conf.example # Agent configuration template
├── agent/                 # Agent server component
│   ├── CMakeLists.txt
│   ├── main.cpp           # Agent entry point
│   ├── agentcore.h/cpp    # Core agent orchestration
│   ├── ipcserver.h/cpp    # IPC server implementation
│   ├── systemmonitor.h/cpp # System monitoring
│   ├── devicemanager.h/cpp # Device management
│   ├── networkmanager.h/cpp # Network monitoring
│   ├── processmanager.h/cpp # Process management
│   ├── androidmanager.h/cpp # Android device management
│   ├── automationengine.h/cpp # Automation rules engine
│   ├── logger.h/cpp       # Logging system
│   └── configmanager.h/cpp # Configuration management
├── gui/                   # GUI client component
│   ├── CMakeLists.txt
│   ├── main.cpp           # GUI entry point
│   ├── mainwindow.h/cpp   # Main application window
│   ├── ipcclient.h/cpp    # IPC client implementation
│   ├── systemmonitortab.h/cpp # System monitor tab
│   ├── devicemanagertab.h/cpp # Device manager tab
│   ├── networkmanagertab.h/cpp # Network manager tab
│   ├── processmanagertab.h/cpp # Process manager tab
│   ├── androidtab.h/cpp   # Android device tab
│   ├── automationtab.h/cpp # Automation rules tab
│   └── logger.h/cpp       # GUI logging
├── shared/                # Shared components
│   ├── CMakeLists.txt
│   ├── systemtypes.h/cpp  # Common data structures
│   ├── commands.h/cpp     # Command/response definitions
│   ├── ipcprotocol.h/cpp  # IPC protocol implementation
│   └── constants.h        # System-wide constants
└── build/                 # Build output directory
```

## Component Architecture

### 1. Shared Library

#### Purpose
Provides common data structures and IPC protocol used by both agent and GUI.

#### Key Components
- **SystemTypes** - Data structures for system information
- **Commands** - Command/response message definitions
- **IpcProtocol** - JSON-based serialization/deserialization

#### Data Structures
```cpp
struct SystemInfo {
    double cpuUsageTotal;
    std::vector<double> cpuCoresUsage;
    uint64_t memoryTotal, memoryUsed, memoryFree;
    uint32_t processCount, threadCount;
    std::chrono::seconds uptime;
};

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

### 2. Agent Component

#### Architecture Pattern
The agent follows a **component-based architecture** with a central orchestrator.

#### Core Components

##### AgentCore (Orchestrator)
- **Purpose**: Central coordination of all agent components
- **Responsibilities**:
  - Component lifecycle management
  - Command processing and routing
  - Event broadcasting to clients
  - Worker thread management

##### IPC Server
- **Purpose**: Handles communication with GUI clients
- **Features**:
  - Multi-client support (up to 10 concurrent clients)
  - TCP socket-based communication
  - JSON message protocol
  - Client connection management

##### System Monitor
- **Purpose**: Real-time system resource monitoring
- **Capabilities**:
  - CPU usage monitoring (total and per-core)
  - Memory usage tracking
  - Process and thread counting
  - System uptime tracking

##### Device Manager
- **Purpose**: USB and hardware device management
- **Features**:
  - USB device detection and enumeration
  - Device connection status monitoring
  - Device information retrieval

##### Network Manager
- **Purpose**: Network interface monitoring
- **Capabilities**:
  - Network interface enumeration
  - IP address tracking (IPv4/IPv6)
  - Network traffic monitoring
  - Interface status monitoring

##### Process Manager
- **Purpose**: Process monitoring and management
- **Features**:
  - Process enumeration
  - Process resource usage tracking
  - Process lifecycle management

##### Android Manager
- **Purpose**: Android device integration
- **Capabilities**:
  - ADB device detection
  - Device information retrieval
  - Battery level monitoring
  - Screen state tracking

##### Automation Engine
- **Purpose**: Rule-based automation system
- **Features**:
  - Rule definition and management
  - Condition evaluation
  - Action execution
  - Rule scheduling

#### Agent Data Flow
```
GUI Client --[Command]--> IPC Server --[Command]--> AgentCore
AgentCore --[Command]--> Specific Manager --> [Response] --> AgentCore
AgentCore --[Response]--> IPC Server --[Response]--> GUI Client

System Events --> Managers --> AgentCore --[Event]--> IPC Server --> All GUI Clients
```

### 3. GUI Component

#### Architecture Pattern
The GUI follows a **Model-View-Controller (MVC)** pattern with Qt's signal/slot mechanism.

#### Core Components

##### Main Window
- **Purpose**: Main application container and coordinator
- **Features**:
  - Menu bar and status bar
  - Tab-based interface
  - Connection management
  - Status updates

##### IPC Client
- **Purpose**: Communication with agent server
- **Features**:
  - Asynchronous command sending
  - Response handling through callbacks
  - Automatic reconnection
  - Connection status monitoring

##### Tab Widgets
Each system management area has its own tab:
- **System Monitor Tab** - Real-time system metrics
- **Device Manager Tab** - USB device management
- **Network Manager Tab** - Network interface monitoring
- **Process Manager Tab** - Process management interface
- **Android Tab** - Android device management
- **Automation Tab** - Rule configuration and management

#### GUI Data Flow
```
User Action --> Tab Widget --> IPC Client --> Agent
Agent Response --> IPC Client --> Tab Widget --> UI Update
Agent Events --> IPC Client --> Tab Widgets --> Real-time Updates
```

## Communication Protocol

### IPC Protocol Design

#### Message Types
1. **Commands** - Request messages from GUI to Agent
2. **Responses** - Reply messages from Agent to GUI
3. **Events** - Broadcast messages from Agent to all GUI clients

#### Message Format
All messages use JSON format for language-agnostic compatibility:
```json
{
  "type": "command|response|event",
  "id": "unique_message_id",
  "timestamp": "2023-01-01T12:00:00Z",
  "module": "system|device|network|process|android|automation",
  "command": "get_system_info|list_processes|...",
  "status": "success|failed|pending",
  "data": { ... }
}
```

#### Command Examples

##### Get System Information
```json
{
  "type": "command",
  "id": "cmd_001",
  "module": "system",
  "command": "get_system_info"
}
```

##### Response
```json
{
  "type": "response",
  "id": "cmd_001",
  "status": "success",
  "data": {
    "cpuUsageTotal": 45.2,
    "memoryUsed": 8589934592,
    "processCount": 156
  }
}
```

##### System Event
```json
{
  "type": "event",
  "id": "evt_001",
  "module": "device",
  "event": "device_connected",
  "data": {
    "deviceId": "usb_001",
    "deviceName": "USB Flash Drive"
  }
}
```

## Threading Model

### Agent Threading

#### Thread Structure
1. **Main Thread** - Application lifecycle and signal handling
2. **Worker Thread** - Command processing and orchestration
3. **Server Thread** - Client connection acceptance
4. **Client Threads** - Individual client communication (one per client)

#### Thread Safety
- **Shared Mutex** - Protects shared data structures
- **Atomic Flags** - Thread-safe status management
- **Thread-Local Storage** - Error state management

### GUI Threading

#### Qt Thread Model
- **Main GUI Thread** - UI rendering and user interaction
- **IPC Client Thread** - Network communication (handled by Qt)
- **Timer Threads** - Periodic updates (handled by Qt)

#### Signal/Slot Connections
- **Queued Connections** - Cross-thread communication
- **Direct Connections** - Same-thread communication
- **Auto Connections** - Qt determines connection type

## Configuration Management

### Agent Configuration

#### Configuration File Location
- **Linux**: `/etc/sysmon/sysmon_agent.conf`
- **Windows**: `%PROGRAMDATA%\SysMon\sysmon_agent.conf`

#### Configuration Sections
```ini
[server]
port=12345
max_clients=10
timeout=300

[logging]
level=INFO
file=/var/log/sysmon/agent.log
max_size=10MB

[system]
update_interval=1000
enable_automation=true

[android]
adb_path=/usr/bin/adb
timeout=30
```

### GUI Configuration

#### Settings Storage
- **Qt Settings** - Platform-specific settings storage
- **Windows**: Registry
- **Linux**: Configuration files
- **macOS**: Property lists

#### Configuration Categories
- Connection settings
- UI preferences
- Tab configurations
- Display options

## Error Handling

### Error Handling Strategy

#### Multi-Level Error Handling
1. **Function Level** - Local error recovery
2. **Component Level** - Component error state management
3. **System Level** - Application-wide error reporting

#### Error Propagation
- **Return Codes** - Traditional error codes for simple cases
- **Exceptions** - Exception-based error handling for complex cases
- **Callbacks** - Asynchronous error notification
- **Events** - Error event broadcasting to clients

#### Error Recovery
- **Automatic Reconnection** - Network connection recovery
- **Graceful Degradation** - Reduced functionality on errors
- **Component Restart** - Automatic component restart on failure

## Security Considerations

### Security Measures

#### Network Security
- **Localhost Only** - Default bind to localhost only
- **Optional Authentication** - Configurable authentication
- **Connection Limits** - Maximum client connection limits
- **Timeout Protection** - Connection timeout management

#### Data Protection
- **Input Validation** - Validate all incoming data
- **Resource Limits** - Prevent resource exhaustion attacks
- **Secure Defaults** - Secure configuration by default

#### Access Control
- **Permission System** - Role-based access control
- **Command Authorization** - Command-level permission checking
- **Resource Access** - Controlled resource access

## Performance Optimization

### Agent Performance

#### Efficient Data Collection
- **Batch Operations** - Group multiple operations
- **Caching** - Cache frequently accessed data
- **Lazy Loading** - Load data on demand
- **Memory Pooling** - Custom allocators for frequent allocations

#### Threading Optimization
- **Lock-Free Operations** - Atomic operations where possible
- **Reader-Writer Locks** - Multiple readers, single writer
- **Thread Pools** - Reuse threads for operations

### GUI Performance

#### UI Responsiveness
- **Asynchronous Operations** - Non-blocking UI operations
- **Progressive Loading** - Load data progressively
- **Virtual Scrolling** - Efficient large dataset display
- **Background Processing** - Move work off UI thread

#### Memory Management
- **Smart Pointers** - Automatic memory management
- **Object Pooling** - Reuse UI objects
- **Resource Cleanup** - Prompt resource release

## Deployment Architecture

### Build System

#### CMake Configuration
- **Modern CMake** - Target-based configuration
- **Cross-Platform** - Windows, Linux, macOS support
- **Component-Based** - Separate build for each component
- **Dependency Management** - Automatic dependency resolution

#### Build Targets
- **sysmon_shared** - Shared library
- **sysmon_agent** - Agent executable
- **sysmon_gui** - GUI executable

### Installation

#### Package Structure
```
/usr/local/bin/
├── sysmon_agent          # Agent executable
└── sysmon_gui            # GUI executable

/usr/local/lib/
└── libsysmon_shared.a    # Shared library

/usr/local/include/sysmon/
├── systemtypes.h
├── commands.h
└── ipcprotocol.h

/etc/sysmon/
└── sysmon_agent.conf     # Agent configuration

/usr/share/applications/
└── sysmon.desktop        # Linux desktop entry
```

#### Service Integration
- **systemd Service** - Linux service integration
- **Windows Service** - Windows service integration
- **Launch Agent** - macOS service integration

## Monitoring and Debugging

### Logging System

#### Log Levels
- **INFO** - General information
- **WARNING** - Warning messages
- **ERROR** - Error messages

#### Log Destinations
- **Console Output** - Development logging
- **Log Files** - Persistent logging
- **System Log** - Integration with system logging

### Debugging Features

#### Agent Debugging
- **Command Tracing** - Log all incoming commands
- **Performance Metrics** - Component performance tracking
- **Connection Monitoring** - Client connection status

#### GUI Debugging
- **Network Debugging** - IPC communication logging
- **UI Debugging** - Widget state tracking
- **Performance Monitoring** - UI responsiveness metrics

## Future Extensibility

### Plugin Architecture

#### Plugin Interface
- **Standardized API** - Common plugin interface
- **Dynamic Loading** - Runtime plugin loading
- **Configuration** - Plugin configuration management

#### Extension Points
- **Custom Managers** - New system component managers
- **Custom Tabs** - New GUI tabs
- **Custom Commands** - New command types
- **Custom Events** - New event types

### Protocol Evolution

#### Versioning Strategy
- **Backward Compatibility** - Support older clients
- **Feature Detection** - Capability negotiation
- **Graceful Degradation** - Fallback for unsupported features

#### Protocol Extensions
- **Binary Protocol** - Optional binary protocol for performance
- **Compression** - Optional data compression
- **Encryption** - Optional data encryption

## Best Practices

### Code Organization

#### Naming Conventions
- **PascalCase** - Class names (AgentCore, MainWindow)
- **camelCase** - Function names (initialize(), sendCommand())
- **snake_case** - Variable names (system_info_, client_socket_)
- **UPPER_CASE** - Constants (MAX_CLIENTS, BUFFER_SIZE)

#### File Organization
- **Header Guards** - `#pragma once` for header files
- **Forward Declarations** - Reduce compilation dependencies
- **Implementation Separation** - Clear interface/implementation separation

### Memory Management

#### RAII Principles
- **Smart Pointers** - Automatic memory management
- **Resource Cleanup** - Destructor-based cleanup
- **Exception Safety** - Strong exception safety guarantees

#### Ownership Semantics
- **Clear Ownership** - Explicit ownership semantics
- **Avoid Cycles** - Prevent circular references
- **Resource Limits** - Prevent resource leaks

### Error Handling

#### Consistent Error Handling
- **Standardized Errors** - Consistent error reporting
- **Error Context** - Provide error context information
- **Recovery Strategies** - Define error recovery approaches

#### Logging Best Practices
- **Structured Logging** - Consistent log format
- **Appropriate Levels** - Use appropriate log levels
- **Performance Considerations** - Avoid logging in hot paths

This architecture guide provides a comprehensive understanding of the SysMon3 project structure, component relationships, and design decisions. It serves as a reference for developers working on extending, maintaining, or understanding the system.
