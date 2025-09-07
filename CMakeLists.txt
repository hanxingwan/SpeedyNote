cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0074 NEW)

project(NoteApp)
set(CMAKE_CXX_STANDARD 17)

# Using variable
set(QT_PATH "C:/Qt/6.8.2/mingw_64" CACHE PATH "Path to Qt installation")
set(SDL2_ROOT "C:/libs/SDL2" CACHE PATH "Path to SDL2")
set(POPPLER_PATH "C:/msys64/mingw64" CACHE PATH "Path to Poppler")
# This is where the "libpoppler-qt6-dev" equivalent in Windows is located. 
# Go to the MSYS2 terminal to run this commands:
# pacman -S mingw-w64-x86_64-poppler-qt6

# Also, there isn't such thing called PopplerConfig.cmake. 
#That's why I need to make a FindPoppler.cmake file

# ✅ Platform-specific configuration
if (WIN32)
    # ✅ Platform-specific configuration
    
    set(CMAKE_PREFIX_PATH
            "${QT_PATH}/lib/cmake"
            "${SDL2_ROOT}/lib/cmake"
    )
    set(CMAKE_MODULE_PATH "${QT_PATH}/lib/cmake/Poppler")
    # In my case, this path is C:\Qt\6.8.2\mingw_64\lib\cmake\Poppler. 
    # This path includes the FindPoppler.cmake file. 
    # You need to manually put FindPoppler.cmake into this folder.

    include_directories(
            # "${POPPLER_PATH}/include/poppler"
            # "${POPPLER_PATH}/include/poppler/cpp" In fact, these 2 paths are poppler and poppler-cpp, which are different from poppler-qt6.
            "${POPPLER_PATH}/include/poppler/qt6"
            "${SDL2_ROOT}/include"
            ${CMAKE_CURRENT_BINARY_DIR}
    )
    link_directories("${POPPLER_PATH}/lib")

    # Prevent SDL2 from redefining main()
    add_compile_definitions(SDL_MAIN_HANDLED)

    # Windows resource file
    enable_language(RC)
    set(WIN_RESOURCES app_icon.rc)

    # Find dependencies for Windows
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia Concurrent Xml Network)
    find_package(Poppler REQUIRED)
    find_package(SDL2 REQUIRED CONFIG)
    unset(SDL2main CACHE)
elseif (UNIX)
    # Linux-specific configuration
    # Use system paths for Linux
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia Concurrent Xml Network)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(POPPLER REQUIRED IMPORTED_TARGET poppler-qt6)

    # For SDL2, we use sdl2-compat which provides SDL2 compatibility
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

    # Include current binary directory for generated files
    include_directories(${CMAKE_CURRENT_BINARY_DIR})

    # No Windows-specific resources
    set(WIN_RESOURCES "")
endif ()

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
add_executable(${PROJECT_NAME}
        source/Main.cpp
        source/MainWindow.cpp
        source/InkCanvas.cpp
        source/ControlPanelDialog.cpp
        source/SDLControllerManager.cpp
        source/ButtonMappingTypes.cpp
        source/RecentNotebooksManager.cpp
        source/KeyCaptureDialog.cpp
        source/ControllerMappingDialog.cpp
        source/MarkdownWindow.cpp
        source/MarkdownWindowManager.cpp
	  source/PictureWindow.cpp
    	  source/PictureWindowManager.cpp
        source/PdfOpenDialog.cpp
	  source/PdfRelinkDialog.cpp
    	  source/SpnPackageManager.cpp
    	  source/LauncherWindow.cpp
        ${QRCC_FILES}
        ${QMARKDOWNTEXTEDIT_SOURCES}
        ${QMARKDOWNTEXTEDIT_UI_HEADERS}
        ${QMARKDOWNTEXTEDIT_MOC_SOURCES}
        ${WIN_RESOURCES}
)

# ✅ Compile .ts → .qm using Qt's lrelease
find_program(LRELEASE_EXECUTABLE
        lrelease
        PATHS "${QT_PATH}/bin"
        NO_DEFAULT_PATH
)
if (WIN32)
    find_program(LRELEASE_EXECUTABLE lrelease PATHS "C:/Qt/6.8.2/mingw_64/bin")
endif ()

if (LRELEASE_EXECUTABLE)
    foreach (_ts ${TS_FILES})
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
    endforeach ()

    # Add a target to build translations
    add_custom_target(translations ALL DEPENDS ${QM_FILES})
endif ()

# Disable Qt deprecated warnings
add_compile_definitions(QT_NO_DEPRECATED_WARNINGS)

# ✅ Link libraries - platform specific
if (WIN32)
    target_link_libraries(NoteApp
            Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia Qt6::Concurrent Qt6::Xml Qt6::Network
            SDL2::SDL2
            ${POPPLER_LIBRARIES}
    )
elseif (UNIX)
    target_link_libraries(NoteApp
            Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia Qt6::Concurrent Qt6::Xml Qt6::Network
            PkgConfig::SDL2
            PkgConfig::POPPLER
    )
endif ()

# ✅ Set output name for Linux
if (UNIX)
    set_target_properties(NoteApp PROPERTIES OUTPUT_NAME NoteApp)
endif ()

# ✅ Install targets for Linux/Flatpak
if (UNIX)
    # Install the main executable
    install(TARGETS NoteApp DESTINATION bin)

    # Install translation files if they exist
    if (QM_FILES)
        install(FILES ${QM_FILES} DESTINATION share/speedynote/translations)
    endif ()

    # Install icon with app ID name for proper Flatpak desktop integration
    install(FILES resources/icons/mainicon.png
            DESTINATION share/icons/hicolor/256x256/apps
            RENAME com.github.alpha_liu_01.SpeedyNote.png)

    # Install desktop file and metainfo (handled by Flatpak post-install)
endif ()
