@echo off
echo Setting up environment for SysMon3 build (without OpenSSL)...
echo.

REM Set up paths for MinGW, Qt 6.10.1, CMake and Ninja
set PATH=C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.10.1\mingw_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%PATH%
set CC=gcc
set CXX=g++
set CMAKE_PREFIX_PATH=C:\Qt\6.10.1\mingw_64
set Qt6_DIR=C:\Qt\6.10.1\mingw_64

echo Testing tools...
echo.
echo Testing GCC:
gcc --version
echo.
echo Testing G++:
g++ --version
echo.
echo Testing CMake:
cmake --version
echo.
echo Testing Ninja:
ninja --version
echo.

REM Set working directory to project root
cd /d "C:\Users\Lenovo\Desktop\sysmon3_gui.cpp"

echo Cleaning build directory...
if exist build (
    echo Removing existing build directory...
    rmdir /s /q build
)
mkdir build
cd build

echo.
echo Running CMake configuration with Ninja generator (without OpenSSL)...
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.10.1\mingw_64 -DSYSMON_NO_OPENSSL=ON

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    echo.
    echo Troubleshooting:
    echo 1. Check Qt6 installation at C:\Qt\6.10.1\mingw_64
    echo 2. Check MinGW installation at C:\Qt\Tools\mingw1310_64
    echo 3. Check CMake installation
    echo 4. Check Ninja installation
    pause
    exit /b 1
)

echo.
echo Building project with Ninja...
ninja

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    echo.
    echo Check the error messages above for compilation errors
    pause
    exit /b 1
)

echo.
echo Creating distribution directory...
if not exist dist mkdir dist
if not exist dist\bin mkdir dist\bin

echo.
echo Copying configuration files...
copy ..\sysmon_agent.conf.example dist\bin\ >nul 2>&1
copy ..\sysmon_gui.conf.example dist\bin\ >nul 2>&1

echo.
echo Copying Qt dependencies...
for %%f in (Qt6Core.dll Qt6Widgets.dll Qt6Gui.dll Qt6Network.dll) do (
    copy "C:\Qt\6.10.1\mingw_64\bin\%%f" "dist\bin\" >nul 2>&1
)

echo.
echo Copying Qt platform plugins...
if not exist dist\bin\platforms mkdir dist\bin\platforms
copy "C:\Qt\6.10.1\mingw_64\plugins\platforms\qwindows.dll" "dist\bin\platforms\" >nul 2>&1
copy "C:\Qt\6.10.1\mingw_64\plugins\platforms\qminimal.dll" "dist\bin\platforms\" >nul 2>&1

echo.
echo Copying Qt image format plugins...
if not exist dist\bin\imageformats mkdir dist\bin\imageformats
copy "C:\Qt\6.10.1\mingw_64\plugins\imageformats\qjpeg.dll" "dist\bin\imageformats\" >nul 2>&1
copy "C:\Qt\6.10.1\mingw_64\plugins\imageformats\qpng.dll" "dist\bin\imageformats\" >nul 2>&1

echo.
echo Creating launcher scripts...
echo @echo off > dist\bin\run_agent.bat
echo echo Starting SysMon3 Agent... >> dist\bin\run_agent.bat
echo set QT_PLUGIN_PATH=%%~dp0platforms >> dist\bin\run_agent.bat
echo set QT_QPA_PLATFORM_PLUGIN_PATH=%%~dp0platforms >> dist\bin\run_agent.bat
echo "%%~dp0sysmon_agent.exe" >> dist\bin\run_agent.bat

echo @echo off > dist\bin\run_gui.bat
echo echo Starting SysMon3 GUI... >> dist\bin\run_gui.bat
echo set QT_PLUGIN_PATH=%%~dp0platforms >> dist\bin\run_gui.bat
echo set QT_QPA_PLATFORM_PLUGIN_PATH=%%~dp0platforms >> dist\bin\run_gui.bat
echo "%%~dp0sysmon_gui.exe" >> dist\bin\run_gui.bat

echo.
echo Copying MinGW runtime dependencies...
for %%f in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
    copy "C:\Qt\Tools\mingw1310_64\bin\%%f" "dist\bin\" >nul 2>&1
)

echo.
echo ========================================
echo BUILD SUCCESSFUL!
echo ========================================
echo.
echo Binaries created in: build\dist\bin\
echo.
echo To run SysMon3:
echo 1. Run dist\bin\run_agent.bat (as Administrator for full functionality)
echo 2. Run dist\bin\run_gui.bat
echo.
echo Or run directly:
echo 1. dist\bin\sysmon_agent.exe (as Administrator)
echo 2. dist\bin\sysmon_gui.exe
echo.
echo Configuration files: dist\bin\sysmon_agent.conf.example and dist\bin\sysmon_gui.conf.example
echo Copy them to sysmon_agent.conf and sysmon_gui.conf and modify as needed.
echo.
echo NOTE: This build was created without OpenSSL support.
echo Security features will be disabled.
echo.
echo Project structure:
echo - Agent: System monitoring service with privileges
echo - GUI: Qt interface for user interaction
echo - IPC: Local TCP socket communication
echo.
pause
exit /b 0
