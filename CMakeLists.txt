# --------------------------------------------------------------------------- #
#    Main CMake file for SMS++ SATBlock                                       #
#                                                                             #
#    This file allows one to build the library using CMake.                   #
#    To do so, you can use the following commands:                            #
#                                                                             #
#        $ cmake -S <source-path> -B <build-path>                             #
#        $ cmake --build <build-path>                                         #
#                                                                             #
#    The following command also installs the library in the system:           #
#                                                                             #
#        $ cmake --build <build-path> --target install                        #
#                                                                             #
#                                Donato Meoli                                 #
#                         Dipartimento di Informatica                         #
#                             Universita' di Pisa                             #
# --------------------------------------------------------------------------- #
cmake_minimum_required(VERSION 3.21)
cmake_policy(VERSION 3.15)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeriveVersion.cmake)
smspp_derive_version(SMSPP_MODULE_VERSION)

project(SATBlock
        VERSION ${SMSPP_MODULE_VERSION}
        DESCRIPTION "SMS++ Block for the satisfiability problems, SAT and weighted MaxSAT"
        HOMEPAGE_URL https://gitlab.com/smspp/satblock
        LANGUAGES C CXX)

# These variables make the code harder to read but easier to change.
set(modName ${PROJECT_NAME})

# C-safe form of modName for the preprocessor macros in <modName>Config.h
string(MAKE_C_IDENTIFIER "${modName}" modMacro)
set(modNamespace "SMS++")

# Find out if it's being called by the umbrella.
get_directory_property(hasParent PARENT_DIRECTORY)

# This adds the cmake directory to the module search paths,
# allowing us to use our modules.
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# ----- Settings ------------------------------------------------------------ #
# Sets the default build type (if none was specified).
# See: https://cmake.org/cmake/help/latest/variable/CMAKE_BUILD_TYPE.html
include(BuildType)

# An option is an ON/OFF user-settable cache variable
option(SATBlock_USE_CADICAL "Whether SATBlock will use CaDiCaL or not." ON)
option(SATBlock_USE_MINISAT "Whether SATBlock will use MiniSat or not." ON)

# ----- Requirements -------------------------------------------------------- #
# If it's not being called by the umbrella, we need to
# look for the system-installed libraries.
if (NOT hasParent)
    find_package(SMS++ REQUIRED)
endif ()

# ----- Configuration header ------------------------------------------------ #
# This will generate a *Config.h header in the build directory.
configure_file(cmake/${modName}Config.h.in ${modName}Config.h)

# ----- RPATH --------------------------------------------------------------- #
# The installed libraries and executables look for the libraries of the same
# installation relative to their own location, so that the installed tree can
# be moved as a whole; the directories of the external libraries are kept.
include(GNUInstallDirs)
if (NOT CMAKE_INSTALL_RPATH)
    if (APPLE)
        set(rpathOrigin "@loader_path")
    else ()
        set(rpathOrigin "$ORIGIN")
    endif ()
    file(RELATIVE_PATH binToLib ${CMAKE_INSTALL_FULL_BINDIR}
            ${CMAKE_INSTALL_FULL_LIBDIR})
    set(CMAKE_INSTALL_RPATH "${rpathOrigin}" "${rpathOrigin}/${binToLib}")
endif ()
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# ----- Library ------------------------------------------------------------- #
# With the BUILD_SHARED_LIBS variable we can specify if the library will
# be STATIC or SHARED, so no reason to do it now.
add_library(${modName})
target_compile_features(${modName} PUBLIC cxx_std_20)

# The name of the shared library carries the version of its binary interface,
# which any minor release may change while the major version is 0, and then
# any major release.
if (PROJECT_VERSION_MAJOR EQUAL 0)
    set(soVersion ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
else ()
    set(soVersion ${PROJECT_VERSION_MAJOR})
endif ()
set_target_properties(${modName} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${soVersion})

# The classes of the module register themselves in the factory from a static
# initialiser, and a linker that drops what looks unused takes the
# registration away with it: the target tells whoever links it to keep the
# symbol that forces the module in, or the library as a whole where naming
# the symbol is not enough.
if (BUILD_SHARED_LIBS)
    if (MSVC)
        target_link_options(${modName} INTERFACE
                "/INCLUDE:SMSpp_force_load_${modName}")
    elseif (UNIX AND (NOT APPLE))
        # the ELF linker decides by the symbols a regular object asks for, so
        # naming the symbol is not enough and the library has to be kept
        target_link_options(${modName} INTERFACE "LINKER:--no-as-needed")
    endif ()
endif ()

# When adding source files with target_sources(), PRIVATE means that the files
# should only be added to this library, whereas PUBLIC means they should be
# added to this library and to any target that links to it.
# INTERFACE can be used for sources that should not be added to this library
# but should be added to anything that links to it.
# Note: do not GLOB files here.
target_sources(${modName} PRIVATE
        src/SATBlock.cpp)

# ----- SAT solvers --------------------------------------------------------- #
# SATSolver, the base of the Solver of a SATBlock, needs nothing; each of the
# classes derived from it wraps a SAT solver, and is built if that SAT solver
# is found [see cmake/FindCaDiCaL.cmake and cmake/FindMiniSat.cmake]:
# CaDiCaL (CADICAL_ROOT) for CaDiCaLSATSolver, and MiniSat (MINISAT_ROOT)
# for MiniSATSolver. Their headers are included by the sources only, hence
# the SAT solvers are linked PRIVATE.
target_sources(${modName} PRIVATE src/SATSolver.cpp)

if (SATBlock_USE_CADICAL)
    find_package(CaDiCaL)
    if (NOT CaDiCaL_FOUND)
        set(SATBlock_USE_CADICAL OFF)
    endif ()
endif ()
if (SATBlock_USE_CADICAL)
    message(STATUS "CaDiCaLSATSolver: CaDiCaL from ${CADICAL_LIBRARY}")
    target_sources(${modName} PRIVATE src/CaDiCaLSATSolver.cpp)
    target_link_libraries(${modName} PRIVATE CaDiCaL::CaDiCaL)
else ()
    message(STATUS "CaDiCaLSATSolver: CaDiCaL not found, not built")
endif ()

if (SATBlock_USE_MINISAT)
    find_package(MiniSat)
    if (NOT MiniSat_FOUND)
        set(SATBlock_USE_MINISAT OFF)
    endif ()
endif ()
if (SATBlock_USE_MINISAT)
    message(STATUS "MiniSATSolver: MiniSat from ${MINISAT_LIBRARY}")
    target_sources(${modName} PRIVATE src/MiniSATSolver.cpp)

    # ----- Compatibility shim for an unpatched MiniSat --------------------- #
    # The upstream MiniSat (2.2, and its last commit) declares in
    # minisat/core/SolverTypes.h
    #     friend Lit mkLit(Var var, bool sign = false);
    # a friend declaration with a default argument that is not a definition,
    # which the recent compilers reject (g++ 13 does); the rewrite moves the
    # default argument to the definition of mkLit(), which changes nothing
    # for whoever calls it. It is applied, at configure time, to a private
    # copy of the header, shadowing the system one through an include
    # directory searched first: MiniSat includes it as
    # "minisat/core/SolverTypes.h", a path that does not exist next to the
    # headers including it, so the include directories are searched in
    # their order. Both rewrites are no-ops on an already-patched MiniSat.
    set(MINISAT_SHIM_DIR ${CMAKE_CURRENT_BINARY_DIR}/minisat-shim)
    file(READ ${MINISAT_INCLUDE_DIR}/minisat/core/SolverTypes.h
         _minisat_types)
    string(REGEX REPLACE
            "friend[ \t]+Lit[ \t]+mkLit[ \t]*\\([ \t]*Var[ \t]+var[ \t]*,[ \t]*bool[ \t]+sign[ \t]*=[ \t]*false[ \t]*\\)[ \t]*;"
            "friend Lit mkLit(Var var, bool sign);"
            _minisat_types "${_minisat_types}")
    string(REGEX REPLACE
            "(inline[ \t]+Lit[ \t]+mkLit[ \t]*\\([ \t]*Var[ \t]+var[ \t]*,[ \t]*bool[ \t]+sign)([ \t]*\\)[ \t]*{)"
            "\\1 = false\\2"
            _minisat_types "${_minisat_types}")
    file(WRITE ${MINISAT_SHIM_DIR}/minisat/core/SolverTypes.h
         "${_minisat_types}")
    target_include_directories(${modName} SYSTEM BEFORE PRIVATE
            ${MINISAT_SHIM_DIR})

    target_link_libraries(${modName} PRIVATE MiniSat::MiniSat)
else ()
    message(STATUS "MiniSATSolver: MiniSat not found, not built")
endif ()

# When using target_include_directories(), PUBLIC means that any targets
# that link to this target also need that include directory.
# Other options are PRIVATE (only affect the current target, not dependencies),
# and INTERFACE (only needed for dependencies).

# Different INSTALL_INTERFACE and BUILD_INTERFACE paths are used when
# generating the target import file (***Targets.cmake).
# This means that if a target finds this library in its build directory
# will look into the BUILD_INTERFACE path for its headers, if it finds it
# installed in the system will look into the INSTALL_INTERFACE path.
target_include_directories(
        ${modName} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:include/${modNamespace}>)

# When linking other targets to the library with target_link_libraries(),
# PRIVATE means that the libraries will be linked only to this library,
# PUBLIC means they will be linked also to the targets that depend on this
# library, INTERFACE means they will be linked only to the targets that depend
# on this library.
target_link_libraries(${modName} PUBLIC ${modNamespace}::SMS++)

# This alias is defined so that executables in this same project can use
# the library with this notation.
add_library(${modNamespace}::${modName} ALIAS ${modName})

# ----- SATBlockML ---------------------------------------------------------- #
# The learned SATBranchRule (GQSATBranchRule) is a library of its own, built
# only if Torch is found: Torch is some hundreds of megabytes of shared
# objects, which a program that does not use that rule should not load.
# Whoever wants it links SMS++::SATBlockML, which brings this one along.
find_package(Torch QUIET HINTS ${Torch_ROOT})
if (Torch_FOUND)
    message(STATUS "${modName}: Torch found, configuring ${modName}ML")

    add_library(${modName}ML)
    target_compile_features(${modName}ML PUBLIC cxx_std_20)
    set_target_properties(${modName}ML PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION ${soVersion})

    # the rule registers itself from a static initialiser, as the classes
    # of the module do [see above]
    if (BUILD_SHARED_LIBS)
        if (MSVC)
            target_link_options(${modName}ML INTERFACE
                    "/INCLUDE:SMSpp_force_load_${modName}ML")
        elseif (UNIX AND (NOT APPLE))
            target_link_options(${modName}ML INTERFACE "LINKER:--no-as-needed")
        endif ()
    endif ()

    target_sources(${modName}ML PRIVATE src/GQSATBranchRule.cpp)

    target_include_directories(
            ${modName}ML PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
            $<INSTALL_INTERFACE:include/${modNamespace}>)

    separate_arguments(TORCH_CXX_FLAGS_LIST UNIX_COMMAND "${TORCH_CXX_FLAGS}")
    target_compile_options(${modName}ML PRIVATE ${TORCH_CXX_FLAGS_LIST})
    target_link_libraries(${modName}ML PUBLIC ${modName} ${TORCH_LIBRARIES})

    add_library(${modNamespace}::${modName}ML ALIAS ${modName}ML)
else ()
    message(STATUS "${modName}ML: Torch not found, not built")
endif ()

# ----- Data ---------------------------------------------------------------- #
# The instances the tests read, in two archives of the GitLab Package
# Registry: cnf.tgz, a selection of the SATLIB collection, and wcnf.tgz, the
# smallest instance of each family of the exact weighted track of the MaxSAT
# Evaluation 2024 with the optimum RC2 finds for it [see data/wcnf/README].
# Each is downloaded and extracted into data/ once per version.

# The version of the archives in the Package Registry. A published version is
# never replaced: new data go out under a new version, written here first [see
# data/upload], and the archives and the markers of their extraction carry it
# in their name, so that a tree holding the data of an older version (a cache
# of the CI, or a clone that has been extracted before) downloads and extracts
# again instead of taking them for the current ones.
set(DATA_VERSION 2026-09-26)

set(DATA_MARKERS)
foreach (fmt cnf wcnf)
    set(url "https://gitlab.com/api/v4/projects/86936161/packages/generic/${fmt}/${DATA_VERSION}/${fmt}.tgz")
    set(archive ${CMAKE_CURRENT_SOURCE_DIR}/data/${fmt}-${DATA_VERSION}.tgz)
    set(marker ${CMAKE_CURRENT_SOURCE_DIR}/data/.extracted-${fmt}-${DATA_VERSION})
    set(script ${CMAKE_CURRENT_BINARY_DIR}/download_sat_${fmt}.cmake)

    file(WRITE ${script} "
file(DOWNLOAD \"${url}\" \"${archive}\" SHOW_PROGRESS STATUS _status)
list(GET _status 0 _code)
list(GET _status 1 _message)
if(NOT _code EQUAL 0)
  # what is on disk is whatever the server answered with, and leaving it there
  # would let the build take it for the archive: the next attempt has to
  # download again rather than fail while extracting it
  file(REMOVE \"${archive}\")
  message(FATAL_ERROR \"cannot download ${url}: \${_message}\")
endif()
file(SIZE \"${archive}\" _size)
if(_size LESS 1024)
  file(REMOVE \"${archive}\")
  message(FATAL_ERROR \"${archive} came down with \${_size} bytes, so the \
server answered but not with the archive: try again later\")
endif()
")

    add_custom_command(
            OUTPUT ${archive}
            COMMAND ${CMAKE_COMMAND} -P ${script}
            COMMENT "Downloading ${fmt}.tgz from the GitLab Package Registry"
            VERBATIM)

    add_custom_command(
            OUTPUT ${marker}
            COMMAND ${CMAKE_COMMAND} -E tar xzf ${archive}
            COMMAND ${CMAKE_COMMAND} -E touch ${marker}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data
            COMMENT "Extracting ${fmt}.tgz"
            VERBATIM
            DEPENDS ${archive})

    list(APPEND DATA_MARKERS ${marker})
endforeach ()

# the target the test depends on: both archives downloaded and extracted
add_custom_target(fetch_sat_data DEPENDS ${DATA_MARKERS})

# ----- Tests --------------------------------------------------------------- #
if (BUILD_TESTING)
    # enable testing in this directory's scope: the umbrella calls
    # enable_testing() only after this subdirectory has been processed, so the
    # test would otherwise not be registered with CTest
    enable_testing()
    add_subdirectory(test)
endif ()

# ----- Subdirectories ------------------------------------------------------ #
# after the tests, so that the test of the tools is registered with CTest
add_subdirectory(tools)

# ----- Install instructions ------------------------------------------------ #
# The following commands are used when installing the library
# and its CMake configuration files on the system.
# They are not required for local builds (see below).

include(GNUInstallDirs)

# Install the library; SATBlockML goes in the same export set, so that a
# consumer finding this module gets SMS++::SATBlockML too when it was built
if (Torch_FOUND)
    install(TARGETS ${modName}ML
            EXPORT ${modName}Targets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} # .dll
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} # .so .dylib
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}) # .lib
endif ()

install(TARGETS ${modName}
        EXPORT ${modName}Targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} # .dll
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} # .so .dylib
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}) # .lib

# Install the headers, that of SATBlockML only if it is built
if (Torch_FOUND)
    install(DIRECTORY include/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${modNamespace})
else ()
    install(DIRECTORY include/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${modNamespace}
            PATTERN "GQSATBranchRule.h" EXCLUDE)
endif ()

# Install the auto-generated configuration header (see above).
install(FILES ${PROJECT_BINARY_DIR}/${modName}Config.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${modNamespace})

# (Generate and) install the target import file, that allows other
# CMake projects to import the target.
install(EXPORT ${modName}Targets
        NAMESPACE ${modNamespace}::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${modName})

# Generate the package version file, that allows other
# CMake projects to know the version.
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/${modName}ConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion)

# Generate the package configuration file, that allows other
# CMake projects to find the library with find_package().
configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/cmake/${modName}Config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/${modName}Config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${modName})

# Install the package version and configuration files.
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${modName}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${modName}ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${modName})

# The find modules of the SAT solvers, which install no CMake configuration
# file of their own, go with the configuration file that uses them.
install(FILES
        ${CMAKE_CURRENT_LIST_DIR}/cmake/FindCaDiCaL.cmake
        ${CMAKE_CURRENT_LIST_DIR}/cmake/FindMiniSat.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${modName})

# Install the README and LICENSE files.
install(FILES
        ${CMAKE_CURRENT_LIST_DIR}/README.md
        ${CMAKE_CURRENT_LIST_DIR}/LICENSE
        DESTINATION ${CMAKE_INSTALL_DATADIR}/${modName})

# ----- Add the build tree to RPATH------------------------------------------ #
# Add the binary directory to RPATH, so executables that use
# dynamic loading will look into it.
if (hasParent)
    set(SMSPP_BUILD_RPATH
            "${SMSPP_BUILD_RPATH};${CMAKE_CURRENT_BINARY_DIR}"
            PARENT_SCOPE)
endif ()

# ----- Packaging support --------------------------------------------------- #
# The following commands allow one to build a proper package using CPack.
# See: https://cmake.org/cmake/help/latest/manual/cpack.1.html

# Using this with the umbrella project is too messy.
if (NOT hasParent)
    set(CPACK_PACKAGE_VENDOR "SMS++ Team")
    set(CPACK_PACKAGE_CONTACT "Donato Meoli")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_LIST_DIR}/LICENSE")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_LIST_DIR}/README.md")
    set(CPACK_SOURCE_IGNORE_FILES
            \\\\.git/
            \\\\.gitignore$
            \\\\.gitattributes$)
    include(CPack)
endif ()

# --------------------------------------------------------------------------- #
# Remove from the search paths the cmake directory we added at the beginning.
list(REMOVE_AT CMAKE_MODULE_PATH -1)

# --------------------------------------------------------------------------- #
