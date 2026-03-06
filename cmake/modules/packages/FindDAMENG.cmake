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
find_package_handle_standard_args(DAMENG
    REQUIRED_VARS DAMENG_LIBRARY DAMENG_INCLUDE_DIR
)

if(DAMENG_FOUND AND NOT TARGET Dameng::Dameng)
    add_library(Dameng::Dameng UNKNOWN IMPORTED)
    set_target_properties(Dameng::Dameng PROPERTIES
        IMPORTED_LOCATION "${DAMENG_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${DAMENG_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(DAMENG_INCLUDE_DIR DAMENG_LIBRARY)
