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

find_library(MINISAT_LIBRARY
             NAMES minisat
             HINTS ${MINISAT_ROOT} ENV MINISAT_ROOT
             PATH_SUFFIXES lib
             DOC "MiniSat library.")

find_package_handle_standard_args(MiniSat
        REQUIRED_VARS MINISAT_LIBRARY MINISAT_INCLUDE_DIR)

if (MiniSat_FOUND)
    set(MiniSat_INCLUDE_DIRS ${MINISAT_INCLUDE_DIR})
    set(MiniSat_LIBRARIES ${MINISAT_LIBRARY})

    if (NOT TARGET MiniSat::MiniSat)
        add_library(MiniSat::MiniSat UNKNOWN IMPORTED)
        set_target_properties(MiniSat::MiniSat PROPERTIES
                IMPORTED_LOCATION "${MINISAT_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${MINISAT_INCLUDE_DIR}")
    endif ()
endif ()

mark_as_advanced(MINISAT_INCLUDE_DIR MINISAT_LIBRARY)
