cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0074 NEW)

project(NoteApp)
set(CMAKE_CXX_STANDARD 17)

# ✅ Add correct CMake module search paths
set(CMAKE_PREFIX_PATH 
    "C:/Qt/5.15.2/mingw81_64/lib/cmake"
    "C:/libs/SDL2/lib/cmake"
)
set(CMAKE_MODULE_PATH "C:/Qt/5.15.2/mingw81_64/lib/cmake/Poppler")
set(SDL2_ROOT "C:/libs/SDL2")

# ✅ Prevent SDL2 from redefining main()
add_compile_definitions(SDL_MAIN_HANDLED)

# ✅ Find all dependencies
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Multimedia Concurrent Qml Quick)
find_package(Poppler REQUIRED)
find_package(SDL2 REQUIRED CONFIG)
unset(SDL2main CACHE)  # Ensure SDL2main isn't picked up

# ✅ Resources
set(RESOURCES resources.qrc)
QT5_ADD_RESOURCES(QRCC_FILES ${RESOURCES})

# ✅ Include + Link paths
include_directories(
    "C:/msys64/mingw64/include/poppler"
    "C:/msys64/mingw64/include/poppler/cpp"
    "C:/msys64/mingw64/include/poppler/qt5"
    "${SDL2_ROOT}/include"
)
link_directories("C:/msys64/mingw64/lib")

# ✅ Qt Meta-Object Compiler support
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ✅ Build target
add_executable(NoteApp
    Main.cpp MainWindow.cpp InkCanvas.cpp ControlPanelDialog.cpp SDLControllerManager.cpp ${QRCC_FILES}
)

add_compile_definitions(QT_NO_DEPRECATED_WARNINGS)

# ✅ Link libraries
target_link_libraries(NoteApp 
    Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Multimedia Qt5::Concurrent Qt5::Qml Qt5::Quick
    SDL2::SDL2
    ${POPPLER_LIBRARIES}
)
