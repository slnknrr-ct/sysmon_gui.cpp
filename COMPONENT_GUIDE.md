# Agent Core
Main orchestrator component that coordinates all system monitoring and management operations. Contains core initialization, command processing, and component lifecycle management. Used as the central hub for all system monitoring functionality.

## (/agent/agentcore.h) class AgentCore
Main agent class that manages all system monitoring components. Found throughout the agent subsystem. Role - central coordination and lifecycle management. Parameters: none in constructor. Behavior: initializes, starts, stops, and coordinates all monitoring components. Avalanche effect: failure of AgentCore causes complete system failure, all monitoring becomes unavailable.
Files:
- /agent/agentcore.h
- /agent/agentcore.cpp
- /agent/main.cpp

## (/agent/agentcore.cpp) function AgentCore::initialize()
Initializes the agent core and all its components. Found in application startup sequence. Role - prepare all subsystems for operation. Parameters: none. Behavior: creates logger, config manager, IPC server, and all monitoring components. Avalanche effect: initialization failure prevents entire system from starting, GUI cannot connect.
Files:
- /agent/agentcore.cpp
- /agent/agentcore.h
- /agent/main.cpp

## (/agent/agentcore.cpp) function AgentCore::start()
Starts all monitoring components and worker threads. Found after successful initialization. Role - activate monitoring functionality. Parameters: none. Behavior: starts IPC server, all managers, and worker thread. Avalanche effect: start failure leaves system initialized but non-functional, GUI cannot receive data.
Files:
- /agent/agentcore.cpp
- /agent/agentcore.h

## (/agent/agentcore.cpp) function AgentCore::handleCommand()
Processes incoming commands from GUI via IPC. Found in main command processing loop. Role - route commands to appropriate handlers. Parameters: const Command& command. Behavior: switches on command module and calls specific handler. Avalanche effect: command handling errors can crash the agent or leave commands unprocessed.
Files:
- /agent/agentcore.cpp
- /agent/agentcore.h
- /shared/commands.h

## (/agent/agentcore.cpp) function AgentCore::workerThread()
Main worker thread for background processing. Found running continuously after start. Role - background task processing. Parameters: none. Behavior: sleeps and processes pending tasks in loop. Avalanche effect: worker thread failure stops background processing, system becomes unresponsive.
Files:
- /agent/agentcore.cpp
- /agent/agentcore.h

# Android Manager
Manages Android device connections and operations through ADB. Contains device discovery, control operations, and app management functionality. Used for Android device monitoring and automation.

## (/agent/androidmanager.h) class AndroidManager
Manages connected Android devices and ADB operations. Found in Android-related functionality. Role - Android device lifecycle management. Parameters: none in constructor. Behavior: initializes ADB, scans for devices, executes commands. Avalanche effect: Android manager failure disables all Android functionality, device automation becomes unavailable.
Files:
- /agent/androidmanager.h
- /agent/androidmanager.cpp
- /agent/agentcore.cpp

## (/agent/androidmanager.cpp) function AndroidManager::getConnectedDevices()
Retrieves list of connected Android devices. Found in device enumeration operations. Role - provide current device inventory. Parameters: none. Behavior: scans ADB devices, returns device info vector. Avalanche effect: device enumeration failures prevent GUI from showing available devices.
Files:
- /agent/androidmanager.cpp
- /agent/androidmanager.h
- /gui/androidtab.cpp

## (/agent/androidmanager.cpp) function AndroidManager::executeAdbCommand()
Executes ADB commands with optional device targeting. Found in all Android operations. Role - low-level ADB communication. Parameters: const string& command, const string& serialNumber. Behavior: runs ADB process, returns output. Avalanche effect: ADB command failures break all Android functionality.
Files:
- /agent/androidmanager.cpp
- /agent/androidmanager.h

## (/agent/androidmanager.cpp) function AndroidManager::deviceMonitoringThread()
Background thread for device monitoring. Found running continuously after start. Role - continuous device presence monitoring. Parameters: none. Behavior: periodically scans for device changes. Avalanche effect: monitoring thread failure stops device detection, new devices won't appear.
Files:
- /agent/androidmanager.cpp
- /agent/androidmanager.h

# GUI Main Window
Main application window providing tabbed interface for all monitoring functions. Contains menu system, status bar, and tab management. Used as primary user interface for system monitoring.

## (/gui/mainwindow.h) class MainWindow
Main application window with tabbed interface. Found as the primary GUI entry point. Role - user interface coordination and display. Parameters: QWidget* parent. Behavior: creates tabs, manages menus, handles events. Avalanche effect: main window failure prevents entire GUI from functioning.
Files:
- /gui/mainwindow.h
- /gui/mainwindow.cpp
- /gui/main.cpp

## (/gui/mainwindow.cpp) function MainWindow::setupUI()
Initializes the main window interface components. Found during window construction. Role - create and arrange UI elements. Parameters: none. Behavior: creates menu bar, status bar, central widget, tabs. Avalanche effect: UI setup failure prevents window from displaying properly.
Files:
- /gui/mainwindow.cpp
- /gui/mainwindow.h

## (/gui/mainwindow.cpp) function MainWindow::createSystemMonitorTab()
Creates system monitoring tab instance. Found during tab initialization. Role - instantiate system monitor interface. Parameters: none. Behavior: creates SystemMonitorTab object and adds to tab widget. Avalanche effect: tab creation failure removes system monitoring from GUI.
Files:
- /gui/mainwindow.cpp
- /gui/mainwindow.h
- /gui/systemmonitortab.cpp

## (/gui/mainwindow.cpp) function MainWindow::connectToAgent()
Establishes IPC connection to agent. Found when user initiates connection. Role - establish communication with backend. Parameters: none. Behavior: creates IPC client, attempts connection. Avalanche effect: connection failure prevents GUI from receiving any data.
Files:
- /gui/mainwindow.cpp
- /gui/mainwindow.h
- /gui/ipcclient.cpp

# Shared Commands
Contains command and response structures for IPC communication. Provides unified interface for all GUI-agent communication. Used throughout the system for message passing.

## (/shared/commands.h) enum class CommandType
Enumeration of all supported command types. Found in all IPC communication. Role - command type identification and routing. Parameters: enum values. Behavior: used in switch statements for command handling. Avalanche effect: missing command types prevent new functionality from being added.
Files:
- /shared/commands.h
- /shared/commands.cpp
- /agent/agentcore.cpp
- /gui/ipcclient.cpp

## (/shared/commands.h) struct Command
Command structure for IPC communication. Found in all command operations. Role - standardized command data container. Parameters: type, module, id, parameters, timestamp. Behavior: passed between GUI and agent. Avalanche effect: command structure corruption breaks all communication.
Files:
- /shared/commands.h
- /shared/commands.cpp
- /agent/agentcore.cpp
- /gui/ipcclient.cpp

## (/shared/commands.h) struct Response
Response structure for IPC communication. Found in all command responses. Role - standardized response data container. Parameters: commandId, status, message, data, timestamp. Behavior: returned from agent to GUI. Avalanche effect: response structure errors prevent command results from reaching GUI.
Files:
- /shared/commands.h
- /shared/commands.cpp
- /agent/agentcore.cpp
- /gui/ipcclient.cpp

## (/shared/commands.h) function createCommand()
Factory function for creating command objects. Found throughout GUI code. Role - standardized command creation with timestamps. Parameters: CommandType type, Module module, params. Behavior: creates Command with unique ID and timestamp. Avalanche effect: command creation failures prevent GUI from sending requests.
Files:
- /shared/commands.h
- /shared/commands.cpp
- /gui/ipcclient.cpp

# Shared System Types
Contains data structures for system information representation. Provides unified data models for all monitoring data. Used throughout the system for data consistency.

## (/shared/systemtypes.h) struct SystemInfo
System information data structure. Found in system monitoring operations. Role - container for system metrics. Parameters: cpu usage, memory, process count, uptime. Behavior: populated by system monitor, sent to GUI. Avalanche effect: system info structure errors prevent system monitoring data display.
Files:
- /shared/systemtypes.h
- /shared/systemtypes.cpp
- /agent/systemmonitor.cpp
- /gui/systemmonitortab.cpp

## (/shared/systemtypes.h) struct ProcessInfo
Process information data structure. Found in process monitoring operations. Role - container for process metrics. Parameters: pid, name, cpu, memory, status. Behavior: populated by process manager, displayed in GUI. Avalanche effect: process info errors break process list display.
Files:
- /shared/systemtypes.h
- /shared/systemtypes.cpp
- /agent/processmanager.cpp
- /gui/processmanagertab.cpp

## (/shared/systemtypes.h) struct AndroidDeviceInfo
Android device information structure. Found in Android device operations. Role - container for device metadata. Parameters: model, version, serial, battery, screen state. Behavior: populated by Android manager, shown in GUI. Avalanche effect: device info errors prevent Android device details from displaying.
Files:
- /shared/systemtypes.h
- /shared/systemtypes.cpp
- /agent/androidmanager.cpp
- /gui/androidtab.cpp

## (/shared/systemtypes.h) enum class CommandStatus
Command status enumeration for responses. Found in all command responses. Role - indicate command execution result. Parameters: SUCCESS, FAILED, PENDING. Behavior: used in Response structures. Avalanche effect: status enumeration changes affect all command handling logic.
Files:
- /shared/systemtypes.h
- /shared/commands.h
- /agent/agentcore.cpp
- /gui/ipcclient.cpp
