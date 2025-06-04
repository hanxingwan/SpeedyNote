cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0074 NEW)

project(NoteApp)
set(CMAKE_CXX_STANDARD 17)

# ✅ Add correct CMake module search paths
set(CMAKE_PREFIX_PATH 
    "C:/Qt/6.8.2/mingw_64/lib/cmake"
    "C:/libs/SDL2/lib/cmake"
)
set(CMAKE_MODULE_PATH "C:/Qt/6.8.2/mingw_64/lib/cmake/Poppler")
set(SDL2_ROOT "C:/libs/SDL2")

set(TS_FILES
    ${CMAKE_SOURCE_DIR}/resources/translations/app_zh.ts
    ${CMAKE_SOURCE_DIR}/resources/translations/app_es.ts
    ${CMAKE_SOURCE_DIR}/resources/translations/app_fr.ts
)

# ✅ Prevent SDL2 from redefining main()
add_compile_definitions(SDL_MAIN_HANDLED)

# ✅ Find all dependencies
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets VirtualKeyboard Multimedia Concurrent)
find_package(Poppler REQUIRED)
find_package(SDL2 REQUIRED CONFIG)
unset(SDL2main CACHE)  # Ensure SDL2main isn't picked up

# ✅ Resources
set(RESOURCES resources.qrc)
QT6_ADD_RESOURCES(QRCC_FILES ${RESOURCES})

# ✅ Include + Link paths
include_directories(
    "C:/msys64/mingw64/include/poppler"
    "C:/msys64/mingw64/include/poppler/cpp"
    "C:/msys64/mingw64/include/poppler/qt6"
    "${SDL2_ROOT}/include"
)
link_directories("C:/msys64/mingw64/lib")

# ✅ Qt Meta-Object Compiler support
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ✅ Windows-specific resource file for executable icon
if(WIN32)
    enable_language(RC)
    set(WIN_RESOURCES app_icon.rc)
endif()

# ✅ Build target
add_executable(NoteApp
    Main.cpp MainWindow.cpp InkCanvas.cpp ControlPanelDialog.cpp SDLControllerManager.cpp ButtonMappingTypes.cpp ${QRCC_FILES}
    RecentNotebooksManager.cpp
    RecentNotebooksDialog.cpp
    KeyCaptureDialog.cpp
    ${WIN_RESOURCES}  # Windows resource file for executable icon (only on Windows)
)

# ✅ Compile .ts → .qm using Qt's lrelease
find_program(LRELEASE_EXECUTABLE lrelease PATHS "C:/Qt/6.8.2/mingw_64/bin")

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

# ✅ Add a target to build translations
# add_custom_target(translations ALL DEPENDS ${QM_FILES})

#
add_compile_definitions(QT_NO_DEPRECATED_WARNINGS)


# ✅ Link libraries
target_link_libraries(NoteApp 
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::VirtualKeyboard Qt6::Multimedia Qt6::Concurrent
    SDL2::SDL2
    ${POPPLER_LIBRARIES}
)
