# --------------------------------------------------------------------------- #
#    CMake find module for MiniSat                                            #
#                                                                             #
#    This module finds MiniSat include directories and libraries.             #
#    Use it by invoking find_package() with the form:                         #
#                                                                             #
#        find_package(MiniSat [REQUIRED])                                     #
#                                                                             #
#    The results are stored in the following variables:                       #
#                                                                             #
#        MiniSat_FOUND         - True if headers and library are found        #
#        MiniSat_INCLUDE_DIRS  - Include directories                          #
#        MiniSat_LIBRARIES     - Libraries to be linked                       #
#                                                                             #
#    This module reads hints about search locations from variables:           #
#                                                                             #
#        MINISAT_ROOT      - Custom path to MiniSat                           #
#                                                                             #
#    The following IMPORTED target is also defined:                           #
#                                                                             #
#        MiniSat::MiniSat                                                     #
#                                                                             #
#    The CMake configuration file that MiniSat installs, when it does,        #
#    defines no target: this module is used instead.                          #
#                                                                             #
#                                Donato Meoli                                 #
#                         Dipartimento di Informatica                         #
#                             Universita' di Pisa                             #
# --------------------------------------------------------------------------- #
include(FindPackageHandleStandardArgs)

find_path(MINISAT_INCLUDE_DIR
          NAMES minisat/core/Solver.h
          HINTS ${MINISAT_ROOT} ENV MINISAT_ROOT
          PATH_SUFFIXES include
          DOC "MiniSat include directory.")

# also where vcpkg puts it, with a debug one of its own: by HINTS, as the
# toolchain of vcpkg puts its debug directory first among the default ones
# under a generator of several configurations
set(_minisat_dirs ${MINISAT_ROOT} $ENV{MINISAT_ROOT})
set(_minisat_debug "")
if (VCPKG_INSTALLED_DIR)
    list(APPEND _minisat_dirs ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET})
    list(APPEND _minisat_debug
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug)
endif ()
find_library(MINISAT_LIBRARY
             NAMES minisat
             HINTS ${_minisat_dirs}
             PATH_SUFFIXES lib/${CMAKE_LIBRARY_ARCHITECTURE} lib
             DOC "MiniSat library.")
find_library(MINISAT_LIBRARY_DEBUG
             NAMES minisat
             PATHS ${_minisat_debug}
             PATH_SUFFIXES lib
             NO_DEFAULT_PATH
             DOC "MiniSat debug library.")
if (NOT MINISAT_LIBRARY_DEBUG)
    set(MINISAT_LIBRARY_DEBUG ${MINISAT_LIBRARY}
            CACHE FILEPATH "MiniSat debug library." FORCE)
endif ()
unset(_minisat_dirs)
unset(_minisat_debug)

find_package_handle_standard_args(MiniSat
        REQUIRED_VARS MINISAT_LIBRARY MINISAT_INCLUDE_DIR)

if (MiniSat_FOUND)
    set(MiniSat_INCLUDE_DIRS ${MINISAT_INCLUDE_DIR})
    set(MiniSat_LIBRARIES ${MINISAT_LIBRARY})

    if (NOT TARGET MiniSat::MiniSat)
        add_library(MiniSat::MiniSat UNKNOWN IMPORTED)
        set_target_properties(MiniSat::MiniSat PROPERTIES
                IMPORTED_LOCATION "${MINISAT_LIBRARY}"
                IMPORTED_LOCATION_DEBUG "${MINISAT_LIBRARY_DEBUG}"
                INTERFACE_INCLUDE_DIRECTORIES "${MINISAT_INCLUDE_DIR}")
    endif ()
endif ()

mark_as_advanced(MINISAT_INCLUDE_DIR MINISAT_LIBRARY MINISAT_LIBRARY_DEBUG)
