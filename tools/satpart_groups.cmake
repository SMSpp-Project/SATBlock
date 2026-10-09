# --------------------------------------------------------------------------- #
#    satpart_groups.cmake: satpart on an instance of satgen made of 4 groups  #
#    must find 4 groups linked by no more clauses than satgen put there,      #
#    and the same seed must give the same groups                              #
# --------------------------------------------------------------------------- #

execute_process(COMMAND ${SATGEN} -k 4 -n 50 -f 0.02 -g 2 -P -s 3
                        satpart_in.wcnf
                RESULT_VARIABLE res)
if (NOT res EQUAL 0)
    message(FATAL_ERROR "satgen failed")
endif ()
file(READ satpart_in.wcnf inst)
string(REGEX MATCH "then ([0-9]+) linking" _ "${inst}")
set(planted ${CMAKE_MATCH_1})

foreach (run a b)
    execute_process(COMMAND ${SATPART} -k 4 -s 1 satpart_in.wcnf
                            satpart_${run}.nc4
                    OUTPUT_VARIABLE out_${run}
                    RESULT_VARIABLE res)
    if (NOT res EQUAL 0)
        message(FATAL_ERROR "satpart failed")
    endif ()
endforeach ()
if (NOT out_a STREQUAL out_b)
    message(FATAL_ERROR "the same seed gave two different partitions")
endif ()

string(REGEX MATCH "([0-9]+) groups of .* ([0-9]+) linking" _ "${out_a}")
if (NOT CMAKE_MATCH_1 EQUAL 4)
    message(FATAL_ERROR "satpart found ${CMAKE_MATCH_1} groups, not 4")
endif ()
if (CMAKE_MATCH_2 GREATER planted)
    message(FATAL_ERROR "satpart cut ${CMAKE_MATCH_2} clauses, satgen "
                        "linked the groups with ${planted}")
endif ()
