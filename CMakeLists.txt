cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0074 NEW)

project(SpeedyNote VERSION 0.10.5)
project(NoteApp)
set(CMAKE_CXX_STANDARD 17)

# Using variable
set(QT_PATH "C:/Qt/6.9.2/mingw_64" CACHE PATH "Path to Qt installation")
set(SDL2_ROOT "C:/msys64/mingw64" CACHE PATH "Path to SDL2")
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
    # In my case, this path is C:\Qt\6.9.2\mingw_64\lib\cmake\Poppler. 
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

    # CPU architecture optimization target (can be set via -DCPU_ARCH=old)
    if(NOT DEFINED CPU_ARCH)
        set(CPU_ARCH "modern")
    endif()

    if(CPU_ARCH STREQUAL "old")
        # Optimizations for older CPUs (SSE3, compatible with Core 2 Duo, Atom Z, ARM64 emulation)
        message(STATUS "Building for older CPUs (SSE3/SSSE3 compatible - Core 2 Duo era)")
        add_compile_options(
            -march=core2            # Enable SSE3, SSSE3 (2006+ CPUs: Core 2 Duo/Conroe)
            -mtune=generic          # Optimize for broad CPU compatibility
            -msse3                  # Explicitly enable SSE3
            -mssse3                 # Explicitly enable SSSE3 (Supplemental SSE3)
            -O3                     # Maximum optimization level
            -DNDEBUG                # Disable debug assertions for performance
        )
    else()
        # Optimizations for modern CPUs (AVX2, Haswell and newer)
        message(STATUS "Building for modern CPUs (AVX2 compatible)")
        add_compile_options(
            -march=haswell          # Enable AVX2, FMA, BMI1, BMI2 instructions
            -mtune=haswell          # Optimize for Haswell+ architectures
            -mavx2                  # Explicitly enable AVX2
            -mfma                   # Enable FMA (Fused Multiply-Add)
            -mbmi                   # Enable BMI1 (Bit Manipulation Instructions)
            -mbmi2                  # Enable BMI2
            -O3                     # Maximum optimization level
            -DNDEBUG                # Disable debug assertions for performance
        )
    endif()

    # Windows resource file
    enable_language(RC)
    set(WIN_RESOURCES app_icon.rc)

    # Find dependencies for Windows
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent Xml Network)
    find_package(Poppler REQUIRED)
    find_package(SDL2 REQUIRED CONFIG)
    unset(SDL2main CACHE)
elseif (APPLE)
    # macOS-specific configuration
    # Use Homebrew paths for macOS
    message(STATUS "🍎 Configuring for macOS with Homebrew dependencies")
    
    # Set Homebrew prefix based on architecture
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(HOMEBREW_PREFIX "/opt/homebrew")
        message(STATUS "Detected Apple Silicon (ARM64) - using ${HOMEBREW_PREFIX}")
    else()
        set(HOMEBREW_PREFIX "/usr/local")
        message(STATUS "Detected Intel Mac (x86-64) - using ${HOMEBREW_PREFIX}")
    endif()
    
    # Set paths for Homebrew-installed Qt6
    set(CMAKE_PREFIX_PATH
        "${HOMEBREW_PREFIX}/opt/qt@6"
        "${HOMEBREW_PREFIX}/opt/sdl2"
    )
    
    # Find Qt6 from Homebrew
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent Xml Network)
    
    # Find SDL2 from Homebrew via pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    
    # Poppler-Qt6 on macOS doesn't provide a .pc file, so we configure it manually
    set(POPPLER_PREFIX "/opt/poppler-qt6")
    message(STATUS "Using Poppler from: ${POPPLER_PREFIX}")
    
    # Include directories
    include_directories(
        ${CMAKE_CURRENT_BINARY_DIR}
        "${POPPLER_PREFIX}/include/poppler/qt6"
        "${HOMEBREW_PREFIX}/include"
    )
    
    # Link directories for Poppler
    link_directories("${POPPLER_PREFIX}/lib")
    
    # Set Poppler libraries manually
    set(POPPLER_LIBRARIES "poppler-qt6")
    
    # No Windows-specific resources
    set(WIN_RESOURCES "")
    
    # macOS CPU architecture optimization
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        # Apple Silicon (M1/M2/M3/M4) optimization
        message(STATUS "🚀 Optimizing for Apple Silicon (ARM64)")
        add_compile_options(
            -mcpu=apple-m1           # Target Apple M1 (compatible with M2/M3/M4)
            -O3                      # Maximum optimization level
            -ffast-math              # Aggressive floating-point optimizations
            -funroll-loops           # Unroll loops for better performance
            -fomit-frame-pointer     # Omit frame pointer for more registers
            -DNDEBUG                 # Disable debug assertions
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        # Intel Mac optimization - optimize for Nehalem (Core i series)
        message(STATUS "🚀 Optimizing for Intel Mac (x86-64, Nehalem+)")
        add_compile_options(
            -march=nehalem           # Target 1st gen Intel Core i
            -mtune=nehalem           # Tune for Nehalem
            -msse4.2                 # Enable SSE4.2
            -mpopcnt                 # Enable POPCNT instruction
            -O3                      # Maximum optimization level
            -ffast-math              # Aggressive floating-point optimizations
            -funroll-loops           # Unroll loops for better performance
            -fomit-frame-pointer     # Omit frame pointer for more registers
            -DNDEBUG                 # Disable debug assertions
        )
    else()
        # Generic fallback
        message(STATUS "Building for generic macOS (${CMAKE_SYSTEM_PROCESSOR})")
        add_compile_options(
            -O3
            -DNDEBUG
        )
    endif()
    
    # macOS-specific frameworks
    find_library(COREAUDIO_FRAMEWORK CoreAudio)
    find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
    find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
    
elseif (UNIX)
    # Linux-specific configuration
    # Use system paths for Linux
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent Xml Network)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(POPPLER REQUIRED IMPORTED_TARGET poppler-qt6)

    # For SDL2, we use sdl2-compat which provides SDL2 compatibility
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

    # Include current binary directory for generated files
    include_directories(${CMAKE_CURRENT_BINARY_DIR})

    # No Windows-specific resources
    set(WIN_RESOURCES "")
    
    # Linux CPU architecture optimization
    # Detect architecture
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        # x86-64 Linux build - optimize for 1st gen Intel Core i (Nehalem) with SSE4.2
        message(STATUS "🚀 Optimizing for x86-64 Linux (1st gen Intel Core i / Nehalem with SSE4.2)")
        add_compile_options(
            -march=nehalem           # Target 1st gen Intel Core i (Nehalem microarchitecture)
            -mtune=nehalem           # Tune for Nehalem specifically
            -msse4.2                 # Explicitly enable SSE4.2
            -mpopcnt                 # Enable POPCNT instruction (part of SSE4.2)
            -O3                      # Maximum optimization level
            -ffast-math              # Aggressive floating-point optimizations
            -funroll-loops           # Unroll loops for better performance
            -fomit-frame-pointer     # Omit frame pointer for more registers
            -DNDEBUG                 # Disable debug assertions
        )
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        # ARM64 Linux build - optimize for older ARM cores (Cortex-A72/A53 big.LITTLE)
        message(STATUS "🚀 Optimizing for ARM64 Linux (Cortex-A72/A53 compatible)")
        add_compile_options(
            -march=armv8-a+crc+simd  # ARMv8-A baseline with CRC32 and NEON SIMD
            -mtune=cortex-a72        # Tune for Cortex-A72 (big cores in big.LITTLE)
            -O3                      # Maximum optimization level
            -ffast-math              # Aggressive floating-point optimizations
            -funroll-loops           # Unroll loops for better performance
            -fomit-frame-pointer     # Omit frame pointer for more registers
            -DNDEBUG                 # Disable debug assertions
        )
    else()
        # Generic fallback for other architectures
        message(STATUS "Building for generic Linux (${CMAKE_SYSTEM_PROCESSOR})")
        add_compile_options(
            -O3
            -DNDEBUG
        )
    endif()
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
        source/SimpleAudio.cpp
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
    find_program(LRELEASE_EXECUTABLE lrelease PATHS "C:/Qt/6.9.2/mingw_64/bin")
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
# add_compile_definitions(QT_NO_DEPRECATED_WARNINGS)

# ✅ Link libraries - platform specific
if (WIN32)
    target_link_libraries(NoteApp
            Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent Qt6::Xml Qt6::Network
            SDL2::SDL2
            ${POPPLER_LIBRARIES}
            dsound  # For DirectSound API
            dxguid  # For DirectSound GUIDs
    )
elseif (APPLE)
    target_link_libraries(NoteApp
            Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent Qt6::Xml Qt6::Network
            PkgConfig::SDL2
            ${POPPLER_LIBRARIES}
            ${COREAUDIO_FRAMEWORK}
            ${AUDIOTOOLBOX_FRAMEWORK}
            ${AUDIOUNIT_FRAMEWORK}
    )
elseif (UNIX)
    target_link_libraries(NoteApp
            Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent Qt6::Xml Qt6::Network
            PkgConfig::SDL2
            PkgConfig::POPPLER
            asound  # For ALSA audio
    )
endif ()

# ✅ Set output name for macOS and Linux
if (APPLE OR UNIX)
    set_target_properties(NoteApp PROPERTIES OUTPUT_NAME NoteApp)
endif ()

# ✅ Install targets for macOS
if (APPLE)
    # Install the main executable
    install(TARGETS NoteApp DESTINATION bin)

    # Install translation files if they exist
    if (QM_FILES)
        install(FILES ${QM_FILES} DESTINATION share/speedynote/translations)
    endif ()
    
    # Also install resource translation files
    file(GLOB RESOURCE_QM_FILES "resources/translations/*.qm")
    if (RESOURCE_QM_FILES)
        install(FILES ${RESOURCE_QM_FILES} DESTINATION share/speedynote/translations)
    endif ()

    # Install icon for macOS
    install(FILES resources/icons/mainicon.png
            DESTINATION share/speedynote/icons)
endif ()

# ✅ Install targets for Linux/Flatpak
if (UNIX AND NOT APPLE)
    # Install the main executable
    install(TARGETS NoteApp DESTINATION bin)

    # Install translation files if they exist
    if (QM_FILES)
        install(FILES ${QM_FILES} DESTINATION share/speedynote/translations)
    endif ()
    
    # Also install resource translation files
    file(GLOB RESOURCE_QM_FILES "resources/translations/*.qm")
    if (RESOURCE_QM_FILES)
        install(FILES ${RESOURCE_QM_FILES} DESTINATION share/speedynote/translations)
    endif ()

    # Install icon with app ID name for proper Flatpak desktop integration
    install(FILES resources/icons/mainicon.png
            DESTINATION share/icons/hicolor/256x256/apps
            RENAME com.github.alpha_liu_01.SpeedyNote.png)

    # Install desktop file and metainfo (handled by Flatpak post-install)
endif ()
