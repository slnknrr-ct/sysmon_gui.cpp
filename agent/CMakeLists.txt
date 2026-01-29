# SysMon3 Agent - System monitoring and management service

set(AGENT_SOURCES
    main.cpp
    agentcore.cpp
    ipcserver.cpp
    systemmonitor.cpp
    devicemanager.cpp
    networkmanager.cpp
    processmanager.cpp
    androidmanager.cpp
    automationengine.cpp
    logger.cpp
    configmanager.cpp
)

set(AGENT_HEADERS
    agentcore.h
    ipcserver.h
    systemmonitor.h
    devicemanager.h
    networkmanager.h
    processmanager.h
    androidmanager.h
    automationengine.h
    logger.h
    configmanager.h
)

# Create agent executable
add_executable(sysmon_agent ${AGENT_SOURCES} ${AGENT_HEADERS})

# Link with shared library
target_link_libraries(sysmon_agent
    PRIVATE
    sysmon_shared
)

# Link with system libraries
if(WIN32)
    target_link_libraries(sysmon_agent
        PRIVATE
        ws2_32
        psapi
        pdh
        setupapi
        iphlpapi
    )
else()
    target_link_libraries(sysmon_agent
        PRIVATE
        pthread
    )
    
    # Find udev for Linux device management
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(UDEV libudev)
        if(UDEV_FOUND)
            target_link_libraries(sysmon_agent PRIVATE ${UDEV_LIBRARIES})
            target_include_directories(sysmon_agent PRIVATE ${UDEV_INCLUDE_DIRS})
            target_compile_options(sysmon_agent PRIVATE ${UDEV_CFLAGS_OTHER})
        endif()
    endif()
endif()

# Set target properties
set_target_properties(sysmon_agent PROPERTIES
    OUTPUT_NAME "sysmon_agent"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/dist/bin
)

# Add compile definitions
target_compile_definitions(sysmon_agent
    PRIVATE
    SYSMON_AGENT_EXPORTS
)

# Add SYSMON_NO_OPENSSL definition if needed
if(SYSMON_NO_OPENSSL)
    target_compile_definitions(sysmon_agent PRIVATE SYSMON_NO_OPENSSL)
endif()

# Installation
install(TARGETS sysmon_agent
    RUNTIME DESTINATION bin
    COMPONENT agent
)

# Install configuration files
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../sysmon_agent.conf
    DESTINATION etc/sysmon
    COMPONENT agent
    OPTIONAL
)

# Create default configuration file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../sysmon_agent.conf.example
    ${CMAKE_BINARY_DIR}/sysmon_agent.conf
    @ONLY
)

install(FILES
    ${CMAKE_BINARY_DIR}/sysmon_agent.conf
    DESTINATION etc/sysmon
    COMPONENT agent
    RENAME sysmon_agent.conf.example
)
