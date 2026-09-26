# --------------------------------------------------------------------------- #
#    satgen_seed.cmake: satgen twice with the same seed, then with another    #
#    one; the first two instances must be the same, the third a different one #
# --------------------------------------------------------------------------- #

foreach (run a b)
    execute_process(COMMAND ${SATGEN} -k 3 -n 20 -f 0.1 -g 3 -P -s 7
                            satgen_${run}.wcnf
                    RESULT_VARIABLE res)
    if (NOT res EQUAL 0)
        message(FATAL_ERROR "satgen failed")
    endif ()
endforeach ()
execute_process(COMMAND ${SATGEN} -k 3 -n 20 -f 0.1 -g 3 -P -s 8
                        satgen_c.wcnf
                RESULT_VARIABLE res)
if (NOT res EQUAL 0)
    message(FATAL_ERROR "satgen failed")
endif ()

file(READ satgen_a.wcnf a)
file(READ satgen_b.wcnf b)
file(READ satgen_c.wcnf c)
if (NOT a STREQUAL b)
    message(FATAL_ERROR "the same seed gave two different instances")
endif ()
if (a STREQUAL c)
    message(FATAL_ERROR "two seeds gave the same instance")
endif ()
