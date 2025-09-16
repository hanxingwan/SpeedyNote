# FindPoppler.cmake - Locate Poppler-Qt6 on MSYS2 MinGW64

# Find Poppler's include directory
find_path(POPPLER_INCLUDE_DIR 
    NAMES poppler/qt5/poppler-qt5.h
    PATHS 
        C:/Qt/tools/mingw810_32/include 
        C:/Qt/tools/mingw810_32/include/poppler
)

# Find Poppler's Qt5 library
find_library(POPPLER_QT5_LIBRARY 
    NAMES poppler-qt5
    PATHS 
        C:/Qt/tools/mingw810_32/lib
)

# Find Poppler's Core library
find_library(POPPLER_CORE_LIBRARY 
    NAMES poppler
    PATHS 
        C:/Qt/tools/mingw810_32/lib
)

include(FindPackageHandleStandardArgs)

# Check if Poppler was found
find_package_handle_standard_args(Poppler DEFAULT_MSG 
    POPPLER_QT5_LIBRARY 
    POPPLER_CORE_LIBRARY 
    POPPLER_INCLUDE_DIR
)

if(Poppler_FOUND)
    set(POPPLER_LIBRARIES ${POPPLER_QT5_LIBRARY} ${POPPLER_CORE_LIBRARY})
    set(POPPLER_INCLUDE_DIRS ${POPPLER_INCLUDE_DIR})
endif()
