##############################################################################
################################ makefile ####################################
##############################################################################
#                                                                            #
#   makefile of test (for SATBlock)                                          #
#                                                                            #
#                                Donato Meoli                                #
#                         Dipartimento di Informatica                        #
#                             Universita' di Pisa                            #
#                                                                            #
##############################################################################

# module name
NAME = SATBlock_unit_test

# basic directory
DIR = .

# compiler
CC = clang++

# common flags
COMMON_SW = -std=c++20 -ferror-limit=1 -Wno-enum-compare

# debug switches
SW_DEBUG = -g3 -glldb -fno-inline $(COMMON_SW)

# production switches
SW_RELEASE = -O3 $(COMMON_SW) -DNDEBUG

# default target - build in release mode
default: SW = $(SW_RELEASE)
default: $(DIR)/$(NAME)

# debug target - build in debug mode
debug: SW = $(SW_DEBUG)
debug: $(DIR)/$(NAME)

# define & include the necessary modules- - - - - - - - - - - - - - - - - - -

# SMS++
SMS++SDR = ../../SMS++
include $(SMS++SDR)/lib/makefile-inc

# SATBlock
SATBkSDR = ..
include $(SATBkSDR)/makefile-s

# the tests of a SAT solver are compiled only if it is there
ifneq ($(SATBkCADICAL),)
TDEF += -DSATBLOCK_HAS_CADICAL
endif
ifneq ($(SATBkMINISAT),)
TDEF += -DSATBLOCK_HAS_MINISAT
endif

# main module (linking phase) - - - - - - - - - - - - - - - - - - - - - - - -

# object files
MOBJ =  $(SATBkOBJ) $(SMS++OBJ)

# libraries
MLIB =  $(SATBkLIB) $(SMS++LIB)

$(DIR)/$(NAME): $(MOBJ) $(DIR)/test.o
	$(CC) -o $(DIR)/$(NAME) $(DIR)/test.o $(MOBJ) $(MLIB) $(SW)

# dependencies: every .o from its .cpp + every recursively included .h- - - -

# include directives
MINC =  $(SMS++INC) $(SATBkINC)

# includes
MH =    $(SMS++H) $(SATBkH)

# compile command
$(DIR)/test.o: $(DIR)/test.cpp $(MH)
	$(CC) -c $(DIR)/test.cpp -o $@ $(MINC) $(TDEF) $(SW)

# clean target
clean::
	rm -f $(DIR)/*.o $(DIR)/*~
	rm -f $(DIR)/$(NAME)

# phony targets
.PHONY: default debug clean

############################ End of makefile #################################
