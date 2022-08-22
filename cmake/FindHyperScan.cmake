# Finder for HyperScan library.
#
# Imported targets:
# HyperScan::HyperScan
# HyperScan::HyperScanRuntime
#
# Input variables:
# HYPERSCAN_ROOT_DIR - Directory where HyperScan is installed. Will be detected automatically if not set.
#
# Result variables:
# HYPERSCAN_FOUND - System has found HyperScan library.
# HYPERSCAN_INCLUDE_DIR -  include directory.
# HYPERSCAN_LIBRARIES - HyperScan library files.
# HYPERSCAN_RUNTIME_LIBRARIES - HyperScan runtime library files.
# HYPERSCAN_VERSION - Version of HyperScan library.

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PKG_HYPERSCAN QUIET libhs)
endif()

find_path(
    HYPERSCAN_INCLUDE_DIR
    NAMES hs
    HINTS
        ${PKG_HYPERSCAN_INCLUDEDIR}
        ${HYPERSCAN_ROOT_DIR}/include
)

find_library(
    HYPERSCAN_LIBRARIES
    NAMES hs
    HINTS
        ${PKG_HYPERSCAN_LIBDIR}
        ${HYPERSCAN_ROOT_DIR}/lib
        ${HYPERSCAN_ROOT_DIR}/lib64
    PATHS
        /usr/lib
        /usr/lib64
)

find_library(
    HYPERSCAN_RUNTIME_LIBRARIES
    NAMES hs_runtime
    HINTS
        ${PKG_HYPERSCAN_LIBDIR}
        ${HYPERSCAN_ROOT_DIR}/lib
        ${HYPERSCAN_ROOT_DIR}/lib64
    PATHS
        /usr/lib
        /usr/lib64
)

if(HYPERSCAN_LIBRARIES AND HYPERSCAN_RUNTIME_LIBRARIES AND HYPERSCAN_INCLUDE_DIR)
    set(HYPERSCAN_FOUND 1)

    file(STRINGS "${HYPERSCAN_INCLUDE_DIR}/hs/hs.h" HYPERSCAN_VERSION_MAJOR_DEFINE REGEX "^#[ \t]*define[ \t]+HS_MAJOR[ \t]+[0-9]+")
    file(STRINGS "${HYPERSCAN_INCLUDE_DIR}/hs/hs.h" HYPERSCAN_VERSION_MINOR_DEFINE REGEX "^#[ \t]*define[ \t]+HS_MINOR[ \t]+[0-9]+")
    file(STRINGS "${HYPERSCAN_INCLUDE_DIR}/hs/hs.h" HYPERSCAN_VERSION_PATCH_DEFINE REGEX "^#[ \t]*define[ \t]+HS_PATCH[ \t]+[0-9]+")
    string(REGEX REPLACE ".*([0-9]+).*" "\\1" HYPERSCAN_VERSION_MAJOR "${HYPERSCAN_VERSION_MAJOR_DEFINE}")
    string(REGEX REPLACE ".*([0-9]+).*" "\\1" HYPERSCAN_VERSION_MINOR "${HYPERSCAN_VERSION_MINOR_DEFINE}")
    string(REGEX REPLACE ".*([0-9]+).*" "\\1" HYPERSCAN_VERSION_PATCH "${HYPERSCAN_VERSION_PATCH_DEFINE}")

    set(HYPERSCAN_VERSION "${HYPERSCAN_VERSION_MAJOR}.${HYPERSCAN_VERSION_MINOR}.${HYPERSCAN_VERSION_PATCH}")

    if(NOT TARGET HyperScan::HyperScan)
        add_library(HyperScan::HyperScan UNKNOWN IMPORTED)
        set_target_properties(HyperScan::HyperScan PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${HYPERSCAN_INCLUDE_DIR}")
        set_target_properties(HyperScan::HyperScan PROPERTIES IMPORTED_LOCATION "${HYPERSCAN_LIBRARIES}")
    endif()

    if(NOT TARGET HyperScan::HyperScanRuntime)
        add_library(HyperScan::HyperScanRuntime UNKNOWN IMPORTED)
        set_target_properties(HyperScan::HyperScanRuntime PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${HYPERSCAN_INCLUDE_DIR}")
        set_target_properties(HyperScan::HyperScanRuntime PROPERTIES IMPORTED_LOCATION "${HYPERSCAN_RUNTIME_LIBRARIES}")
    endif()
else()
    set(HYPERSCAN_FOUND 0)
endif()

find_package_handle_standard_args(
    HyperScan
    REQUIRED_VARS
        HYPERSCAN_LIBRARIES
        HYPERSCAN_RUNTIME_LIBRARIES
        HYPERSCAN_INCLUDE_DIR
    VERSION_VAR
        HYPERSCAN_VERSION
)
