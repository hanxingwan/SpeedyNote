# FindPoppler.cmake - Locate Poppler-Qt6 on MSYS2 clangarm64

# Find Poppler's include directory
find_path(POPPLER_INCLUDE_DIR 
    NAMES poppler/qt6/poppler-qt6.h
    PATHS 
        C:/msys64/clangarm64/include 
        C:/msys64/clangarm64/include/poppler
)

# Find Poppler's Qt6 library
find_library(POPPLER_QT6_LIBRARY 
    NAMES poppler-qt6
    PATHS 
        C:/msys64/clangarm64/lib
)

# Find Poppler's Core library
find_library(POPPLER_CORE_LIBRARY 
    NAMES poppler
    PATHS 
        C:/msys64/clangarm64/lib
)

include(FindPackageHandleStandardArgs)

# Check if Poppler was found
find_package_handle_standard_args(Poppler DEFAULT_MSG 
    POPPLER_QT6_LIBRARY 
    POPPLER_CORE_LIBRARY 
    POPPLER_INCLUDE_DIR
)

if(Poppler_FOUND)
    set(POPPLER_LIBRARIES ${POPPLER_QT6_LIBRARY} ${POPPLER_CORE_LIBRARY})
    set(POPPLER_INCLUDE_DIRS ${POPPLER_INCLUDE_DIR})
endif()
