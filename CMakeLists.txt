cmake_minimum_required(VERSION 3.10)
project(NoteApp)

set(CMAKE_CXX_STANDARD 17)

# Add the correct CMAKE_PREFIX_PATH
set(CMAKE_PREFIX_PATH "C:/Qt/6.8.2/mingw_64/lib/cmake")
set(CMAKE_MODULE_PATH "C:/Qt/6.8.2/mingw_64/lib/cmake/Poppler")


# Adjust find_package for Qt6 Widgets

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Poppler REQUIRED)


include_directories("C:/msys64/mingw64/include/poppler")
include_directories("C:/msys64/mingw64/include/poppler/cpp")
include_directories("C:/msys64/mingw64/include/poppler/qt6")
link_directories("C:/msys64/mingw64/lib")

# Enable Qt MOC and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(NoteApp main.cpp MainWindow.cpp InkCanvas.cpp)
target_link_libraries(NoteApp Qt6::Core Qt6::Gui Qt6::Widgets ${POPPLER_LIBRARIES})