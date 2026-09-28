##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of SATBlock                                                     #
#                                                                            #
#   Note that $(SMS++INC) is assumed to include any -I directive             #
#   corresponding to external libraries needed by SMS++, at least to the     #
#   extent in which they are needed by the parts of SMS++ used by            #
#   SATBlock.                                                                #
#                                                                            #
#   Input:  $(CC)        = compiler command                                  #
#           $(SW)        = compiler options                                  #
#           $(SMS++INC)  = the -I$( core SMS++ directory )                   #
#           $(SMS++OBJ)  = the libSMS++ library itself                       #
#           $(SATBkSDR)  = the directory where the source is                 #
#                                                                            #
#   The SAT solvers are found through $(CADICAL_ROOT) and $(MINISAT_ROOT),   #
#   set in extlib/makefile-default-paths-* (overridable via                  #
#   extlib/makefile-paths): CaDiCaLSATSolver is built if CaDiCaL is there,   #
#   MiniSATSolver if MiniSat is, and SATSolver, their base, always.          #
#                                                                            #
#   Output: $(SATBkOBJ)  = the final object(s) / library                     #
#           $(SATBkH)    = the .h files to include                           #
#           $(SATBkINC)  = the -I$( source directory )                       #
#           $(SATBkLIB)  = the SAT solvers + -L< libdirs >                   #
#                                                                            #
#                             Antonio Frangioni                              #
#                         Dipartimento di Informatica                        #
#                             Universita' di Pisa                            #
#                                                                            #
##############################################################################

# the SAT solvers - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

include $(SATBkSDR)/../extlib/makefile-libCaDiCaL
include $(SATBkSDR)/../extlib/makefile-libMiniSat

# non-empty if and only if the SAT solver is there
SATBkCADICAL := $(libCaDiCaLINCDIR)
SATBkMINISAT := $(wildcard $(libMiniSatBSCDIR)/include/minisat/core/Solver.h)

# Compatibility shim for an unpatched MiniSat: the upstream one (2.2, and its
# last commit) declares in minisat/core/SolverTypes.h
#     friend Lit mkLit(Var var, bool sign = false);
# a friend declaration with a default argument that is not a definition,
# which the recent compilers reject (g++ 13 does); the rewrite moves the
# default argument to the definition of mkLit(), which changes nothing for
# whoever calls it. It is applied to a private copy of the header, put on
# the include path before the one of MiniSat, as the CMake build does; both
# rewrites are no-ops on an already-patched MiniSat.
SATBkSHIM = $(SATBkSDR)/minisat-shim

# macros to be exported - - - - - - - - - - - - - - - - - - - - - - - - - - -

SATBkOBJ = $(SATBkSDR)/obj/SATBlock.o $(SATBkSDR)/obj/SATSolver.o

SATBkINC = -I$(SATBkSDR)/include

SATBkH   = $(SATBkSDR)/include/SATBlock.h $(SATBkSDR)/include/SATSolver.h

SATBkLIB =

ifneq ($(SATBkCADICAL),)
SATBkOBJ += $(SATBkSDR)/obj/CaDiCaLSATSolver.o
SATBkH += $(SATBkSDR)/include/CaDiCaLSATSolver.h
SATBkLIB += $(libCaDiCaLLIB)
endif

ifneq ($(SATBkMINISAT),)
SATBkOBJ += $(SATBkSDR)/obj/MiniSATSolver.o
SATBkH += $(SATBkSDR)/include/MiniSATSolver.h
SATBkLIB += $(libMiniSatLIB)
endif

# clean - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

clean::
	rm -f $(SATBkOBJ) $(SATBkSDR)/*~
	rm -rf $(SATBkSHIM)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

$(SATBkSDR)/obj/SATBlock.o: $(SATBkSDR)/src/SATBlock.cpp \
	$(SATBkSDR)/include/SATBlock.h $(SMS++H) $(SMS++OBJ)
	$(CC) -c $(SATBkSDR)/src/SATBlock.cpp -o $@ \
	$(SATBkINC) $(SMS++INC) $(SW)

$(SATBkSDR)/obj/SATSolver.o: $(SATBkSDR)/src/SATSolver.cpp \
	$(SATBkSDR)/include/SATSolver.h $(SATBkSDR)/include/SATBlock.h \
	$(SMS++H) $(SMS++OBJ)
	$(CC) -c $(SATBkSDR)/src/SATSolver.cpp -o $@ \
	$(SATBkINC) $(SMS++INC) $(SW)

$(SATBkSDR)/obj/CaDiCaLSATSolver.o: $(SATBkSDR)/src/CaDiCaLSATSolver.cpp \
	$(SATBkSDR)/include/CaDiCaLSATSolver.h \
	$(SATBkSDR)/include/SATSolver.h $(SATBkSDR)/include/SATBlock.h \
	$(SMS++H) $(SMS++OBJ)
	$(CC) -c $(SATBkSDR)/src/CaDiCaLSATSolver.cpp -o $@ \
	$(SATBkINC) $(libCaDiCaLINC) $(SMS++INC) $(SW)

$(SATBkSHIM)/minisat/core/SolverTypes.h: \
	$(libMiniSatBSCDIR)/include/minisat/core/SolverTypes.h
	mkdir -p $(dir $@)
	sed -E \
	  -e 's/friend[ \t]+Lit[ \t]+mkLit[ \t]*\([ \t]*Var[ \t]+var[ \t]*,[ \t]*bool[ \t]+sign[ \t]*=[ \t]*false[ \t]*\)[ \t]*;/friend Lit mkLit(Var var, bool sign);/' \
	  -e 's/(inline[ \t]+Lit[ \t]+mkLit[ \t]*\([ \t]*Var[ \t]+var[ \t]*,[ \t]*bool[ \t]+sign)([ \t]*\)[ \t]*\{)/\1 = false\2/' \
	  $< > $@

$(SATBkSDR)/obj/MiniSATSolver.o: $(SATBkSDR)/src/MiniSATSolver.cpp \
	$(SATBkSDR)/include/MiniSATSolver.h \
	$(SATBkSDR)/include/SATSolver.h $(SATBkSDR)/include/SATBlock.h \
	$(SATBkSHIM)/minisat/core/SolverTypes.h $(SMS++H) $(SMS++OBJ)
	$(CC) -c $(SATBkSDR)/src/MiniSATSolver.cpp -o $@ \
	$(SATBkINC) -I$(SATBkSHIM) $(libMiniSatINC) $(SMS++INC) $(SW)

########################## End of makefile ###################################
