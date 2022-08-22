# Finder for Yaramod library.
#
# Imported targets:
# Yaramod::Yaramod
# Yaramod::YaramodRuntime
#
# Input variables:
# YARAMOD_ROOT_DIR - Directory where Yaramod is installed. Will be detected automatically if not set.
#
# Result variables:
# YARAMOD_FOUND - System has found Yaramod library.
# YARAMOD_INCLUDE_DIR -  include directory.
# YARAMOD_LIBRARIES - Yaramod library files.
# YARAMOD_RUNTIME_LIBRARIES - Yaramod runtime library files.
# YARAMOD_VERSION - Version of Yaramod library.

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PKG_YARAMOD QUIET libhs)
endif()

find_path(
    YARAMOD_INCLUDE_DIR
    NAMES yaramod
    HINTS
        ${PKG_YARAMOD_INCLUDEDIR}
        ${YARAMOD_ROOT_DIR}/include
)

find_library(
    YARAMOD_LIBRARIES
    NAMES yaramod
    HINTS
        ${PKG_YARAMOD_LIBDIR}
        ${YARAMOD_ROOT_DIR}/lib
        ${YARAMOD_ROOT_DIR}/lib64
    PATHS
        /usr/lib
        /usr/lib64
)

if(YARAMOD_LIBRARIES AND YARAMOD_INCLUDE_DIR)
    set(YARAMOD_FOUND 1)

    file(STRINGS "${YARAMOD_INCLUDE_DIR}/yaramod/yaramod.h" YARAMOD_VERSION_MAJOR_DEFINE REGEX "^#[ \t]*define[ \t]+YARAMOD_VERSION_MAJOR[ \t]+[0-9]+")
    file(STRINGS "${YARAMOD_INCLUDE_DIR}/yaramod/yaramod.h" YARAMOD_VERSION_MINOR_DEFINE REGEX "^#[ \t]*define[ \t]+YARAMOD_VERSION_MINOR[ \t]+[0-9]+")
    file(STRINGS "${YARAMOD_INCLUDE_DIR}/yaramod/yaramod.h" YARAMOD_VERSION_PATCH_DEFINE REGEX "^#[ \t]*define[ \t]+YARAMOD_VERSION_PATCH[ \t]+[0-9]+")
    string(REGEX REPLACE ".* ([0-9]+).*" "\\1" YARAMOD_VERSION_MAJOR "${YARAMOD_VERSION_MAJOR_DEFINE}")
    string(REGEX REPLACE ".* ([0-9]+).*" "\\1" YARAMOD_VERSION_MINOR "${YARAMOD_VERSION_MINOR_DEFINE}")
    string(REGEX REPLACE ".* ([0-9]+).*" "\\1" YARAMOD_VERSION_PATCH "${YARAMOD_VERSION_PATCH_DEFINE}")

    set(YARAMOD_VERSION "${YARAMOD_VERSION_MAJOR}.${YARAMOD_VERSION_MINOR}.${YARAMOD_VERSION_PATCH}")

    if(NOT TARGET Yaramod::Yaramod)
        add_library(Yaramod::Yaramod UNKNOWN IMPORTED)
        set_target_properties(Yaramod::Yaramod PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${YARAMOD_INCLUDE_DIR};${YARAMOD_INCLUDE_DIR}/pog")
        set_target_properties(Yaramod::Yaramod PROPERTIES IMPORTED_LOCATION "${YARAMOD_LIBRARIES}")
        get_filename_component(YARAMOD_LIBRARY_DIR "${YARAMOD_LIBRARIES}" DIRECTORY)
        target_link_libraries(Yaramod::Yaramod INTERFACE "${YARAMOD_LIBRARY_DIR}/libpog_fmt.a" "${YARAMOD_LIBRARY_DIR}/libpog_re2.a")
    endif()
else()
    set(YARAMOD_FOUND 0)
endif()

find_package_handle_standard_args(
    Yaramod
    REQUIRED_VARS
        YARAMOD_LIBRARIES
        YARAMOD_INCLUDE_DIR
    VERSION_VAR
        YARAMOD_VERSION
)
