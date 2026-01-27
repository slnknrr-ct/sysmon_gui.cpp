cmake_minimum_required(VERSION 3.16)
project(SysMon3 VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4 /WX)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Set Qt6 options
set(Qt6_DIR "${CMAKE_PREFIX_PATH}")
cmake_policy(SET CMP0074 NEW)

# Enable Qt MOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Add subdirectories
add_subdirectory(shared)
add_subdirectory(agent)
add_subdirectory(gui)

# Installation
install(TARGETS sysmon_agent sysmon_gui
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# CPack configuration
include(CPack)
set(CPACK_PACKAGE_NAME "SysMon3")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform system monitoring and management tool")
set(CPACK_PACKAGE_VENDOR "SysMon")
set(CPACK_PACKAGE_CONTACT "support@sysmon.local")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "SysMon3")
    set(CPACK_NSIS_PACKAGE_NAME "SysMon3")
    set(CPACK_NSIS_CONTACT "support@sysmon.local")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://sysmon.local")
else()
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "SysMon Team")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6widgets6")
endif()
