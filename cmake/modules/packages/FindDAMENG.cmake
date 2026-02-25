set(DM_HOME $ENV{DM_HOME})

find_path(DAMENG_INCLUDE_DIR NAMES DPI.h PATHS
    "${DM_HOME}/include"
    NO_DEFAULT_PATH
)
find_library(DAMENG_LIBRARY NAMES libdmdpi dmdpi PATHS
    "${DM_HOME}/lib"
    "${DM_HOME}/lib64"
    "${DM_HOME}/drivers/dpi" # for windows
    NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
if(DAMENG_LIBRARY AND DAMENG_INCLUDE_DIR)
    set(DAMENG_FOUND TRUE)
    set(DAMENG_LIBRARIES ${DAMENG_LIBRARY})
    set(DAMENG_INCLUDE_DIRS ${DAMENG_INCLUDE_DIR})
    mark_as_advanced(DAMENG_INCLUDE_DIR DAMENG_LIBRARY)
    message(STATUS "Found Dameng: ${DAMENG_LIBRARY}")
else()
    set(DAMENG_FOUND FALSE)
    set(DAMENG_LIBRARIES "")
    set(DAMENG_INCLUDE_DIRS "")
    message(STATUS "Dameng not found (set DM_HOME to enable Dameng support)")
endif()
