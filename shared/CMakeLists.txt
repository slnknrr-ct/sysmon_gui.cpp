# Shared library for IPC protocol and common types

set(SHARED_SOURCES
    systemtypes.cpp
    commands.cpp
    ipcprotocol.cpp
)

set(SHARED_HEADERS
    systemtypes.h
    commands.h
    ipcprotocol.h
)

# Create shared library
add_library(sysmon_shared STATIC ${SHARED_SOURCES} ${SHARED_HEADERS})

# Set target properties
target_include_directories(sysmon_shared
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Link with system libraries
if(WIN32)
    target_link_libraries(sysmon_shared ws2_32)
else()
    target_link_libraries(sysmon_shared pthread)
endif()

# Set compile definitions
target_compile_definitions(sysmon_shared
    PRIVATE
    SYSMON_SHARED_EXPORTS
)

# Export library for other components
set_target_properties(sysmon_shared PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${SHARED_HEADERS}"
)

# Installation
install(TARGETS sysmon_shared
    EXPORT SysMonTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/sysmon
)
