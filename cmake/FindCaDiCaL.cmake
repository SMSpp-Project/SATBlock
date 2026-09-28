# --------------------------------------------------------------------------- #
#    CMake find module for CaDiCaL                                            #
#                                                                             #
#    This module finds CaDiCaL include directories and libraries.             #
#    Use it by invoking find_package() with the form:                         #
#                                                                             #
#        find_package(CaDiCaL [REQUIRED])                                     #
#                                                                             #
#    The results are stored in the following variables:                       #
#                                                                             #
#        CaDiCaL_FOUND         - True if headers and library are found        #
#        CaDiCaL_INCLUDE_DIRS  - Include directories                          #
#        CaDiCaL_LIBRARIES     - Libraries to be linked                       #
#                                                                             #
#    This module reads hints about search locations from variables:           #
#                                                                             #
#        CADICAL_ROOT      - Custom path to CaDiCaL                           #
#                                                                             #
#    The following IMPORTED target is also defined:                           #
#                                                                             #
#        CaDiCaL::CaDiCaL                                                     #
#                                                                             #
#    CaDiCaL installs no CMake configuration file. Both an installed one      #
#    (include/cadical.hpp, lib/libcadical) and a source tree built as its     #
#    own configure script does (src/cadical.hpp, build/libcadical.a) are      #
#    found.                                                                   #
#                                                                             #
#                                Donato Meoli                                 #
#                         Dipartimento di Informatica                         #
#                             Universita' di Pisa                             #
# --------------------------------------------------------------------------- #
include(FindPackageHandleStandardArgs)

find_path(CADICAL_INCLUDE_DIR
          NAMES cadical.hpp
          HINTS ${CADICAL_ROOT} ENV CADICAL_ROOT
          PATH_SUFFIXES include src
          DOC "CaDiCaL include directory.")

find_library(CADICAL_LIBRARY
             NAMES cadical
             HINTS ${CADICAL_ROOT} ENV CADICAL_ROOT
             PATH_SUFFIXES lib build
             DOC "CaDiCaL library.")

find_package_handle_standard_args(CaDiCaL
        REQUIRED_VARS CADICAL_LIBRARY CADICAL_INCLUDE_DIR)

if (CaDiCaL_FOUND)
    set(CaDiCaL_INCLUDE_DIRS ${CADICAL_INCLUDE_DIR})
    set(CaDiCaL_LIBRARIES ${CADICAL_LIBRARY})

    if (NOT TARGET CaDiCaL::CaDiCaL)
        add_library(CaDiCaL::CaDiCaL UNKNOWN IMPORTED)
        set_target_properties(CaDiCaL::CaDiCaL PROPERTIES
                IMPORTED_LOCATION "${CADICAL_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${CADICAL_INCLUDE_DIR}")
    endif ()
endif ()

mark_as_advanced(CADICAL_INCLUDE_DIR CADICAL_LIBRARY)
