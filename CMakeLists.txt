cmake_minimum_required(VERSION 3.16)
project(SpeedyNote)
set(CMAKE_CXX_STANDARD 17)

# === Qt6 detection ===
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets VirtualKeyboard Multimedia Concurrent)

# === Poppler detection ===
find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER REQUIRED IMPORTED_TARGET poppler-qt6)
pkg_check_modules(SDL2 REQUIRED sdl2)

# === Enable Qt build features ===
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# === Resources ===
qt_add_resources(QRCS resources.qrc)

# === Source files ===
add_executable(SpeedyNote
    Main.cpp
    MainWindow.cpp
    MainWindow.h
    InkCanvas.cpp
    InkCanvas.h
    ControlPanelDialog.cpp
    ControlPanelDialog.h
    SDLControllerManager.cpp
    SDLControllerManager.h
    ToolType.h
    ${QRCS}
)

# === Include & Link ===
if(APPLE)
    # Tell CMake where to find poppler-qt6 headers and libraries
    include_directories(/opt/poppler-qt6/include/poppler/qt6)
    include_directories(/opt/poppler-qt6/include/poppler/cpp)
    link_directories(/opt/poppler-qt6/lib)

    include_directories(${SDL2_INCLUDE_DIRS})
    link_directories(${SDL2_LIBRARY_DIRS})
    add_definitions(${SDL2_CFLAGS_OTHER})

    # Help CMake find the correct poppler-qt6 using pkg-config
    set(CMAKE_PREFIX_PATH "/opt/poppler-qt6")
    set(ENV{PKG_CONFIG_PATH} "/opt/poppler-qt6/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")

    set(CMAKE_MACOSX_RPATH ON)
    set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")
endif()

target_include_directories(SpeedyNote PRIVATE ${Qt6_INCLUDE_DIRS})
add_compile_definitions(QT_NO_DEPRECATED_WARNINGS)

target_link_libraries(SpeedyNote
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::VirtualKeyboard
    Qt6::Multimedia
    Qt6::Concurrent
    PkgConfig::POPPLER
    ${SDL2_LIBRARIES}
)

# set_target_properties(SpeedyNote PROPERTIES
    # MACOSX_BUNDLE TRUE
    # MACOSX_BUNDLE_GUI_IDENTIFIER com.speedynote.app
    # MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/macos/Info.plist
    # RESOURCE "${CMAKE_SOURCE_DIR}/resources/icons/mainicon.icns"
# )

