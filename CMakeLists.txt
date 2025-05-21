cmake_minimum_required(VERSION 3.10)
project(NoteApp)
set(CMAKE_CXX_STANDARD 17)

# ✅ Enable Qt automatic features
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Use system paths for Linux
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia Concurrent)
find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER REQUIRED IMPORTED_TARGET poppler-qt6)
find_package(SDL2 REQUIRED)

# Resources
set(RESOURCES resources.qrc)
qt6_add_resources(QRCC_FILES ${RESOURCES})

# Translations
set(TS_FILES
    ${CMAKE_SOURCE_DIR}/resources/translations/app_zh.ts
    ${CMAKE_SOURCE_DIR}/resources/translations/app_es.ts
    ${CMAKE_SOURCE_DIR}/resources/translations/app_fr.ts
)

find_program(LRELEASE_EXECUTABLE lrelease)

foreach(_ts ${TS_FILES})
    get_filename_component(_qm ${_ts} NAME_WE)
    set(_qm_file ${CMAKE_CURRENT_BINARY_DIR}/${_qm}.qm)
    add_custom_command(
        OUTPUT ${_qm_file}
        COMMAND ${LRELEASE_EXECUTABLE} ${_ts} -qm ${_qm_file}
        DEPENDS ${_ts}
        COMMENT "Generating ${_qm_file}"
    )
    list(APPEND QM_FILES ${_qm_file})
endforeach()

add_custom_target(translations ALL DEPENDS ${QM_FILES})

# Main executable
add_executable(NoteApp
    Main.cpp MainWindow.cpp InkCanvas.cpp ControlPanelDialog.cpp SDLControllerManager.cpp ToolType.h ${QRCC_FILES}
)

# Link libraries
target_link_libraries(NoteApp
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Multimedia Qt6::Concurrent
    SDL2::SDL2
    PkgConfig::POPPLER
)
