cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0074 NEW)

project(NoteApp)
set(CMAKE_CXX_STANDARD 17)

# ✅ Platform-specific configuration
if(WIN32)
    # Windows-specific paths and settings
set(CMAKE_PREFIX_PATH 
    "C:/Qt/6.8.2/mingw_64/lib/cmake"
    "C:/libs/SDL2/lib/cmake"
)
set(CMAKE_MODULE_PATH "C:/Qt/6.8.2/mingw_64/lib/cmake/Poppler")
set(SDL2_ROOT "C:/libs/SDL2")

    # Include paths for Windows
    include_directories(
        "C:/msys64/mingw64/include/poppler"
        "C:/msys64/mingw64/include/poppler/cpp"
        "C:/msys64/mingw64/include/poppler/qt6"
        "${SDL2_ROOT}/include"
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    link_directories("C:/msys64/mingw64/lib")
    
    # Prevent SDL2 from redefining main()
    add_compile_definitions(SDL_MAIN_HANDLED)
    
    # Windows resource file
    enable_language(RC)
    set(WIN_RESOURCES app_icon.rc)
    
    # Find dependencies for Windows
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia Concurrent Xml)
    find_package(Poppler REQUIRED)
    find_package(SDL2 REQUIRED CONFIG)
    unset(SDL2main CACHE)
    
elseif(UNIX)
    # Linux-specific configuration
    # Use system paths for Linux
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia Concurrent Xml)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(POPPLER REQUIRED IMPORTED_TARGET poppler-qt6)
    
    # For SDL2, we use sdl2-compat which provides SDL2 compatibility
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    
    # Include current binary directory for generated files
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    
    # No Windows-specific resources
    set(WIN_RESOURCES "")
endif()

# Translation files
set(TS_FILES
    ${CMAKE_SOURCE_DIR}/resources/translations/app_zh.ts
    ${CMAKE_SOURCE_DIR}/resources/translations/app_es.ts
    ${CMAKE_SOURCE_DIR}/resources/translations/app_fr.ts
)

# ✅ Enable Qt automatic features
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ✅ QMarkdownTextEdit sources
set(QMARKDOWNTEXTEDIT_SOURCES
    markdown/qmarkdowntextedit.cpp
    markdown/markdownhighlighter.cpp
    markdown/qownlanguagedata.cpp
    markdown/qplaintexteditsearchwidget.cpp
)

set(QMARKDOWNTEXTEDIT_HEADERS
    markdown/qmarkdowntextedit.h
    markdown/markdownhighlighter.h
    markdown/qownlanguagedata.h
    markdown/qplaintexteditsearchwidget.h
    markdown/linenumberarea.h
)

# Process headers that contain Q_OBJECT for MOC
qt6_wrap_cpp(QMARKDOWNTEXTEDIT_MOC_SOURCES markdown/linenumberarea.h)

set(QMARKDOWNTEXTEDIT_UI
    markdown/qplaintexteditsearchwidget.ui
)

set(QMARKDOWNTEXTEDIT_RESOURCES
    markdown/media.qrc
)

# Process UI files to generate headers
qt6_wrap_ui(QMARKDOWNTEXTEDIT_UI_HEADERS ${QMARKDOWNTEXTEDIT_UI})

# ✅ Resources
set(RESOURCES resources.qrc ${QMARKDOWNTEXTEDIT_RESOURCES})
QT6_ADD_RESOURCES(QRCC_FILES ${RESOURCES})

# ✅ Build target
add_executable(NoteApp
    Main.cpp MainWindow.cpp InkCanvas.cpp ControlPanelDialog.cpp SDLControllerManager.cpp ButtonMappingTypes.cpp ${QRCC_FILES}
    RecentNotebooksManager.cpp
    RecentNotebooksDialog.cpp
    KeyCaptureDialog.cpp
    ControllerMappingDialog.cpp
    MarkdownWindow.cpp
    MarkdownWindowManager.cpp
    ${QMARKDOWNTEXTEDIT_SOURCES}
    ${QMARKDOWNTEXTEDIT_UI_HEADERS}
    ${QMARKDOWNTEXTEDIT_MOC_SOURCES}
    ${WIN_RESOURCES}
)

# ✅ Compile .ts → .qm using Qt's lrelease
find_program(LRELEASE_EXECUTABLE lrelease)
if(WIN32)
find_program(LRELEASE_EXECUTABLE lrelease PATHS "C:/Qt/6.8.2/mingw_64/bin")
endif()

if(LRELEASE_EXECUTABLE)
foreach(_ts ${TS_FILES})
    get_filename_component(_qm ${_ts} NAME_WE)
    set(_qm_file ${CMAKE_CURRENT_BINARY_DIR}/${_qm}.qm)
    add_custom_command(
        OUTPUT ${_qm_file}
        COMMAND ${LRELEASE_EXECUTABLE} ${_ts} -qm ${_qm_file}
        DEPENDS ${_ts}
        COMMENT "Generating ${_qm_file}"
        VERBATIM
    )
    list(APPEND QM_FILES ${_qm_file})
endforeach()

    # Add a target to build translations
    add_custom_target(translations ALL DEPENDS ${QM_FILES})
endif()

# Disable Qt deprecated warnings
add_compile_definitions(QT_NO_DEPRECATED_WARNINGS)

# ✅ Link libraries - platform specific
if(WIN32)
target_link_libraries(NoteApp 
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia Qt6::Concurrent Qt6::Xml
    SDL2::SDL2
    ${POPPLER_LIBRARIES}
)
elseif(UNIX)
    target_link_libraries(NoteApp
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia Qt6::Concurrent Qt6::Xml
        PkgConfig::SDL2
        PkgConfig::POPPLER
    )
endif()

# ✅ Set output name for Linux
if(UNIX)
    set_target_properties(NoteApp PROPERTIES OUTPUT_NAME speedynote)
endif()

# ✅ Install targets for Linux/Flatpak
if(UNIX)
    # Install the main executable
    install(TARGETS NoteApp DESTINATION bin)
    
    # Install translation files if they exist
    if(QM_FILES)
        install(FILES ${QM_FILES} DESTINATION share/speedynote/translations)
    endif()
    
    # Install icon with app ID name for proper Flatpak desktop integration
    install(FILES resources/icons/mainicon.png 
            DESTINATION share/icons/hicolor/256x256/apps 
            RENAME com.github.alpha_liu_01.SpeedyNote.png)
    
    # Install desktop file and metainfo (handled by Flatpak post-install)
endif()
