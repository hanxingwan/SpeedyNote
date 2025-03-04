cmake_minimum_required(VERSION 3.10)
project(NoteApp)

set(CMAKE_CXX_STANDARD 17)

# Add the correct CMAKE_PREFIX_PATH
set(CMAKE_PREFIX_PATH "C:/Qt/5.15.2/mingw81_64/lib/cmake")
set(CMAKE_MODULE_PATH "C:/Qt/5.15.2/mingw81_64/lib/cmake/Poppler")

# ✅ Find Qt5 components
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Poppler REQUIRED)

# ✅ Process the .qrc file properly
set(RESOURCES resources.qrc)
QT5_ADD_RESOURCES(QRCC_FILES ${RESOURCES})

# ✅ Include directories for Poppler
include_directories("C:/msys64/mingw64/include/poppler")
include_directories("C:/msys64/mingw64/include/poppler/cpp")
include_directories("C:/msys64/mingw64/include/poppler/qt5")
link_directories("C:/msys64/mingw64/lib")

# ✅ Enable Qt MOC and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ✅ Create the executable with resources
add_executable(NoteApp main.cpp MainWindow.cpp InkCanvas.cpp ${QRCC_FILES})

# ✅ Link necessary Qt5 libraries
add_compile_definitions(QT_NO_DEPRECATED_WARNINGS)
target_link_libraries(NoteApp Qt5::Core Qt5::Gui Qt5::Widgets ${POPPLER_LIBRARIES})
