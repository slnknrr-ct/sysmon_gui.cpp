# SysMon3 GUI - Qt-based user interface

set(GUI_SOURCES
    main.cpp
    mainwindow.cpp
    ipcclient.cpp
    systemmonitortab.cpp
    devicemanagertab.cpp
    networkmanagertab.cpp
    processmanagertab.cpp
    androidtab.cpp
    automationtab.cpp
    logger.cpp
)

set(GUI_HEADERS
    mainwindow.h
    ipcclient.h
    systemmonitortab.h
    devicemanagertab.h
    networkmanagertab.h
    processmanagertab.h
    androidtab.h
    automationtab.h
    logger.h
)

set(GUI_UI_FILES
    # Add UI files if using Qt Designer
    # mainwindow.ui
)

# Enable Qt MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Create GUI executable
add_executable(sysmon_gui
    ${GUI_SOURCES}
    ${GUI_HEADERS}
    ${GUI_UI_FILES}
)

# Link with Qt6 and shared library
target_link_libraries(sysmon_gui
    PRIVATE
    Qt6::Core
    Qt6::Widgets
    sysmon_shared
)

# Link with system libraries
if(WIN32)
    target_link_libraries(sysmon_gui
        PRIVATE
        ws2_32
    )
else()
    target_link_libraries(sysmon_gui
        PRIVATE
        pthread
    )
endif()

# Set target properties
set_target_properties(sysmon_gui PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME "sysmon_gui"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add compile definitions
target_compile_definitions(sysmon_gui
    PRIVATE
    SYSMON_GUI_EXPORTS
)

# Qt-specific settings
set_target_properties(sysmon_gui PROPERTIES
    QT_MOC_SKIP ${GUI_HEADERS}
)

# Installation
install(TARGETS sysmon_gui
    RUNTIME DESTINATION bin
    COMPONENT gui
    BUNDLE DESTINATION Applications
)

# Install desktop file for Linux
if(UNIX AND NOT APPLE)
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../sysmon.desktop
        DESTINATION share/applications
        COMPONENT gui
        OPTIONAL
    )
    
    # Install icon
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../sysmon.png
        DESTINATION share/icons/hicolor/256x256/apps
        COMPONENT gui
        OPTIONAL
    )
endif()

# Windows-specific settings
if(WIN32)
    # Set subsystem to Windows
    set_target_properties(sysmon_gui PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
    
    # Add Windows manifest
    if(MSVC)
        set(MANIFEST_FILE ${CMAKE_CURRENT_SOURCE_DIR}/sysmon_gui.manifest)
        if(EXISTS ${MANIFEST_FILE})
            target_sources(sysmon_gui PRIVATE ${MANIFEST_FILE})
        endif()
    endif()
endif()
