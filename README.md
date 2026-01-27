# SysMon3 - Cross-platform System Monitoring and Management

## 🎯 Project Overview

SysMon3 is a full-featured desktop application in C++ + Qt for system monitoring and management. The project consists of two components: a privileged background agent and a GUI interface that communicate via a local TCP socket.

## 🚀 MVP Capabilities

### 📊 System Monitoring
- **CPU**: total load + per-core usage in real-time
- **Memory**: total/used/free + cache/buffers
- **Processes**: process and thread count, context switches
- **System Uptime**: time since system boot
- **Updates**: all data refreshes every 1 second

### 🖥️ Process Management
- **Display**: complete process list with PID, name, CPU/memory usage
- **Termination**: SIGTERM/SIGKILL (Linux) and TerminateProcess (Windows) support
- **Protection**: prevention of critical system process termination

### 🔌 Device Management
- **USB Devices**: detection and display with VID/PID
- **Control**: enable/disable, prevent auto-connect
- **Cross-platform**: Linux (udev/sysfs), Windows (SetupAPI)

### 🌐 Network Management
- **Interfaces**: list of network interfaces with IPv4/IPv6, status
- **Speed**: real-time Rx/Tx traffic monitoring
- **Settings**: enable/disable, static IP, DHCP
- **Platforms**: Linux (netlink), Windows (IP Helper API)

### 📱 Android Integration
- **Auto-discovery**: automatic detection of Android device connect/disconnect
- **Information**: model, Android version, serial number, battery level
- **Management**: screen on/off, lock, foreground app
- **Applications**: launch and stop applications
- **System Functions**: screenshot, screen orientation, logcat
- **Implementation**: via ADB (management only from agent)

### ⚡ Automation and Rules
- **Rule Format**: IF <condition> THEN <action>
- **Examples**: IF CPU_LOAD > 85% FOR 10s THEN DISABLE_USB <VID:PID>
- **Asynchronous Execution**: rules run in background
- **Storage**: in configuration file

### 🎨 Modern GUI
- **6 Tabs**: one for each functional module
- **Real-time**: instant data updates from agent
- **Qt6**: modern interface based on Qt Widgets
- **Intuitive Design**: easy control of all functions

## 🏗️ Architecture

### Two-Component System
```
GUI (Qt) 
   ↓ IPC (Local TCP Socket, JSON)
System Agent (privileged)
   ↓
OS APIs / Devices / Android
```

- **Agent**: background service with privileges for system API access
- **GUI**: user interface without direct system access
- **IPC**: local TCP socket with JSON protocol

## 📚 Additional Documentation

### 📖 Main Guides
- **[MVP_GUIDE.md](MVP_GUIDE.md)** - Detailed MVP guide
- **[ARCHITECTURE_GUIDE.md](ARCHITECTURE_GUIDE.md)** - Comprehensive project architecture
- **[COMPONENT_GUIDE.md](COMPONENT_GUIDE.md)** - Detailed component description
- **[ARCHITECTURE_ESSENTIALS.md](ARCHITECTURE_ESSENTIALS.md)** - Architecture concepts and patterns

### 🔧 Technology Stacks
- **[CPP17_ESSENTIALS.md](CPP17_ESSENTIALS.md)** - C++17 features in the project
- **[QT6_ESSENTIALS.md](QT6_ESSENTIALS.md)** - Qt 6.10.1 essential capabilities

### 🌐 Russian Documentation
- **[README_RU.md](README_RU.md)** - Russian version
- **[MVP_GUIDE_RU.md](MVP_GUIDE_RU.md)** - MVP guide (Russian)
- **[ARCHITECTURE_GUIDE_RU.md](ARCHITECTURE_GUIDE_RU.md)** - Architecture guide (Russian)
- **[COMPONENT_GUIDE_RU.md](COMPONENT_GUIDE_RU.md)** - Component guide (Russian)

## ⚙️ Technical Requirements

### Technology Stack
- **Language**: C++17
- **GUI**: Qt 6.10.1, Qt Widgets
- **Build**: CMake
- **IPC**: Local TCP Socket + JSON
- **Platforms**: Linux, Windows

### Build Requirements
- **CMake**: version 3.16 or higher
- **C++**: compiler with C++17 support
- **Qt6**: version 6.10.1 with Core and Widgets modules
- **Platforms**: Linux (gcc/clang), Windows (MSVC/MinGW)

## 🚀 Quick Start

### Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Run
```bash
# Start agent (with privileges)
./sysmon_agent

# Start GUI
./sysmon_gui
```

### Configuration
Copy `sysmon_agent.conf.example` to `sysmon_agent.conf` and configure:
- IPC port (default: 12345)
- Update intervals
- Log paths

## 🔐 Security

- **Privilege Separation**: Agent runs with elevated rights, GUI with user rights
- **Local IPC**: communication only via local TCP socket
- **Process Protection**: prevention of critical system process termination
- **Logging**: all privileged operations are logged

## 📊 Performance

- **Agent**: ~50MB RAM, minimal CPU load
- **GUI**: ~100MB RAM with Qt6
- **IPC**: <1MB traffic during active work
- **Asynchronous**: all operations execute without interface blocking

## 📦 Installation

### Linux
```bash
# DEB package
sudo dpkg -i sysmon3_1.0.0_amd64.deb

# Or from source
cmake --build . --target install
```

### Windows
Run `SysMon3-Setup.exe` or use the ZIP archive.

## 📄 License

See [LICENSE](LICENSE) file for details.

## 🤝 Contributing

The project follows architectural principles described in:
- [ARCHITECTURE_GUIDE.md](ARCHITECTURE_GUIDE.md)
- [COMPONENT_GUIDE.md](COMPONENT_GUIDE.md)

All changes must comply with MVP requirements from [MVP_GUIDE.md](MVP_GUIDE.md).

---

**SysMon3** - Powerful monitoring system ready for real-world use!
