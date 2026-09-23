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
#   Output: $(SATBkOBJ)  = the final object(s) / library                     #
#           $(SATBkH)    = the .h files to include                           #
#           $(SATBkINC)  = the -I$( source directory )                       #
#                                                                            #
#                             Antonio Frangioni                              #
#                         Dipartimento di Informatica                        #
#                             Universita' di Pisa                            #
#                                                                            #
##############################################################################

# macros to be exported - - - - - - - - - - - - - - - - - - - - - - - - - - -

SATBkOBJ = $(SATBkSDR)/obj/SATBlock.o

SATBkINC = -I$(SATBkSDR)/include

SATBkH   = $(SATBkSDR)/include/SATBlock.h

# clean - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

clean::
	rm -f $(SATBkOBJ) $(SATBkSDR)/*~

# dependencies: every .o from its .cpp + every recursively included .h- - - -

$(SATBkSDR)/obj/SATBlock.o: $(SATBkSDR)/src/SATBlock.cpp \
	$(SATBkSDR)/include/SATBlock.h $(SMS++H) $(SMS++OBJ)
	$(CC) -c $(SATBkSDR)/src/SATBlock.cpp -o $@ \
	$(SATBkINC) $(SMS++INC) $(SW)

########################## End of makefile ###################################
