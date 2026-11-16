# --------------------------------------------------------------------------- #
#    CMake file for SATBlock/test                                             #
#                                                                             #
#    In this file we define the unit tests for this library.                  #
#    Tests that should be supported by ctest executable must be added         #
#    with the add_test() command.                                             #
#                                                                             #
#                                Donato Meoli                                 #
#                         Dipartimento di Informatica                         #
#                             Universita' di Pisa                             #
# --------------------------------------------------------------------------- #

# The classes of the modules register themselves in the factories from
# static objects, which nothing here names: a shared library is kept by
# what its module asks of the linker, while an archive has to be taken
# whole [see SMSppModule.cmake of SMS++].
smspp_keep_modules()

# ----- SATBlock_unit_test ---------------------------------------------      #

add_executable(${modName}_unit_test test.cpp)

target_link_libraries(${modName}_unit_test PRIVATE ${modNamespace}::${modName})
if (SATBlock_USE_CADICAL)
    target_compile_definitions(${modName}_unit_test PRIVATE
                               SATBLOCK_HAS_CADICAL)
endif ()
if (SATBlock_USE_MINISAT)
    target_compile_definitions(${modName}_unit_test PRIVATE
                               SATBLOCK_HAS_MINISAT)
endif ()

smspp_copy_runtime(${modName}_unit_test)

add_test(NAME ${modName}_unit_test
         COMMAND ${modName}_unit_test
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# The label lets the module CI select exactly the tests exercising this
# module with `ctest -L <module>` (see tests/cmake/TestLabels.cmake in the
# umbrella for the system-tests counterpart).
set_tests_properties(${modName}_unit_test PROPERTIES LABELS "${modName}")

# the instances of the module, downloaded and extracted when the tests that
# read them are run, not when they are built [see the CMakeLists.txt of the
# module]; the lock is shared by the tests that build a target of the data of
# the module, which under ctest -j would write the same files at once
foreach (fmt cnf wcnf)
    add_test(NAME ${modName}_extract_sat_${fmt}
             COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                     --target extract_sat_${fmt})
    set_tests_properties(${modName}_extract_sat_${fmt} PROPERTIES
                         FIXTURES_SETUP sat_${fmt}_extracted
                         RESOURCE_LOCK sat_data
                         LABELS "${modName}")
endforeach ()
set_tests_properties(${modName}_unit_test PROPERTIES FIXTURES_REQUIRED
                     "sat_cnf_extracted;sat_wcnf_extracted")

# SATBlockML, if it is built: the learned SATBranchRule
if (TARGET ${modNamespace}::${modName}ML)
    add_executable(${modName}ML_unit_test test_ml.cpp)
    target_link_libraries(${modName}ML_unit_test PRIVATE
                          ${modNamespace}::${modName}ML)
    add_test(NAME ${modName}ML_unit_test
             COMMAND ${modName}ML_unit_test
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    set_tests_properties(${modName}ML_unit_test PROPERTIES
                         LABELS "${modName}" SKIP_RETURN_CODE 77)
endif ()

# --------------------------------------------------------------------------- #
