# --------------------------------------------------------------------------- #
#    CMake file for SATBlock/tools                                            #
# --------------------------------------------------------------------------- #

# This is needed for setting the runtime path when installing.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# The classes of the modules register themselves in the factories from
# static objects, which nothing here names: a shared library is kept by
# what its module asks of the linker [see the CMakeLists.txt of the
# module], while an archive has to be taken whole.
if (NOT BUILD_SHARED_LIBS)
    if (MSVC)
        add_link_options("/WHOLEARCHIVE" "/FORCE:MULTIPLE")
    else () # Unix
        if (APPLE)
            add_link_options("-Wl,-all_load")
        else ()
            add_link_options("-Wl,--whole-archive,--allow-multiple-definition")
        endif ()
    endif ()
endif ()

# ----- satgen -------------------------------------------------------------- #
# random weighted partial MaxSAT instances made of groups of clauses bound by
# a tunable fraction of linking clauses, written as WCNF or as a netCDF
# SATBlock
add_executable(satgen satgen.cpp)
set_target_properties(satgen PROPERTIES OUTPUT_NAME smspp_satgen)
target_link_libraries(satgen PRIVATE SMS++::SATBlock)

# ----- satpart ------------------------------------------------------------- #
# the variables of an instance dealt out to groups, the communities of its
# variable incidence graph, for the structures of a SATBlock
add_executable(satpart satpart.cpp)
set_target_properties(satpart PROPERTIES OUTPUT_NAME smspp_satpart)
target_link_libraries(satpart PRIVATE SMS++::SATBlock)

# ----- Install instructions ------------------------------------------------ #
include(GNUInstallDirs)
install(TARGETS satgen satpart
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ----- Tests --------------------------------------------------------------- #
# the same seed gives the same instance
if (BUILD_TESTING)
    add_test(NAME satgen_seed
             COMMAND ${CMAKE_COMMAND}
                     -DSATGEN=$<TARGET_FILE:satgen>
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/satgen_seed.cmake
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(satgen_seed PROPERTIES LABELS SATBlock)
    # the groups of satgen found again
    add_test(NAME satpart_groups
             COMMAND ${CMAKE_COMMAND}
                     -DSATGEN=$<TARGET_FILE:satgen>
                     -DSATPART=$<TARGET_FILE:satpart>
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/satpart_groups.cmake
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(satpart_groups PROPERTIES LABELS SATBlock)
endif ()
