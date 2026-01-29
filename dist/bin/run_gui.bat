@echo off
echo Starting SysMon3 GUI...
echo.
REM Set Qt plugin paths
set QT_PLUGIN_PATH=%~dp0platforms
set QT_QPA_PLATFORM_PLUGIN_PATH=%~dp0platforms

REM Start GUI
"%~dp0sysmon_gui.exe"

if %ERRORLEVEL% neq 0 (
    echo GUI failed to start with error code %ERRORLEVEL%
    pause
)
