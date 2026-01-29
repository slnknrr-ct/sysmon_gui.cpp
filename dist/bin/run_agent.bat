@echo off
echo Starting SysMon3 Agent...
echo.
REM Set Qt plugin paths
set QT_PLUGIN_PATH=%~dp0platforms
set QT_QPA_PLATFORM_PLUGIN_PATH=%~dp0platforms

REM Start agent
"%~dp0sysmon_agent.exe"

if %ERRORLEVEL% neq 0 (
    echo Agent failed to start with error code %ERRORLEVEL%
    pause
)
