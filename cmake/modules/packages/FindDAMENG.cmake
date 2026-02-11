set(DM_HOME $ENV{DM_HOME})
if(NOT DM_HOME)
    message(STATUS "needs set DM_HOME for ogr_DAMENG enable")
endif()

find_path(DAMENG_INCLUDE_DIRS NAMES DPI.h PATHS
    "${DM_HOME}/include/include"
    NO_DEFAULT_PATH
)
find_library(DAMENG_LIBRARY NAMES libdmdpi dmdpi PATHS
    "${DM_HOME}/lib"
    "${DM_HOME}/lib64"
    "${DM_HOME}/drivers/dpi" # for windows
    NO_DEFAULT_PATH
)
