/*--------------------------------------------------------------------------*/
/*---------------------------- File SATSolver.h ----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Header file for the *concrete* class SATSolver, which implements the
 * Solver concept [see Solver.h] for a SATBlock [see SATBlock.h] by means of
 * any SAT solver offering the IPASIR interface, the incremental interface of
 * the SAT competitions (CaDiCaL, MiniSat, Lingeling, CryptoMiniSat, ...).
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*----------------------------- DEFINITIONS --------------------------------*/
/*--------------------------------------------------------------------------*/

#ifndef __SATSolver
 #define __SATSolver
                      /* self-identification: #endif at the end of the file */

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <chrono>
#include <vector>

#include "SATBlock.h"
#include "Solver.h"

/*--------------------------------------------------------------------------*/
/*------------------------------ NAMESPACE ---------------------------------*/
/*--------------------------------------------------------------------------*/

/// namespace for the Structured Modeling System++ (SMS++)
namespace SMSpp_di_unipi_it
{
/*--------------------------------------------------------------------------*/
/*-------------------------------- CLASSES ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @defgroup SATSolver_CLASSES Classes in SATSolver.h
 *  @{ */

/*--------------------------------------------------------------------------*/
/*---------------------------- CLASS SATSolver -----------------------------*/
/*--------------------------------------------------------------------------*/
/// a Solver for a SATBlock through the IPASIR interface
/** The SATSolver class implements the Solver concept [see Solver.h] for a
 * SATBlock [see SATBlock.h] by means of a SAT solver offering IPASIR, the
 * incremental interface of the SAT competitions: which SAT solver it is is
 * decided when the module is linked, CaDiCaL by default, since IPASIR is a
 * C interface of which a program holds one implementation only.
 *
 * compute() gives the SAT solver the clauses of the SATBlock: those of its
 * abstract representation, if it has been generated, leaving out the
 * relaxed ClauseConstraint (a tautology among them), and those of its
 * physical representation otherwise. A BooleanVariable that is fixed [see
 * Variable::is_fixed()] is fixed at its current value by an *assumption*,
 * i.e., a literal that holds for this compute() only: when the SATBlock is
 * unsatisfiable under the assumptions, is_failed() tells which of them are
 * in the reason of it, which is what a core-guided MaxSAT algorithm needs.
 *
 * The clauses are given again to the SAT solver from scratch at the first
 * compute() after any Modification but that of a Variable being fixed or
 * unfixed, which the assumptions take care of.
 *
 * The status returned by compute() is kOK if the clauses are satisfiable,
 * with a solution that get_var_solution() writes into the BooleanVariable,
 * kInfeasible if they are not, kStopTime if the time limit dblMaxTime is
 * reached first, and kError if the SAT solver gives up for any other
 * reason. The SATBlock having no Objective, get_lb() and get_ub() are both 0
 * after kOK, both +INF after kInfeasible, and -INF and +INF otherwise. */

class SATSolver : public Solver
{
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/

 public:

/*------------------------------ CONSTRUCTOR -------------------------------*/
 /// constructor: it does nothing, the SAT solver is created by compute()

 SATSolver( void ) : Solver() {}

/*------------------------------- DESTRUCTOR -------------------------------*/
 /// destructor: releases the SAT solver

 ~SATSolver() override;

/*------------------------- OTHER INITIALIZATIONS --------------------------*/
 /// sets the SATBlock to solve; throws if the Block is not a SATBlock

 void set_Block( Block * block ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// sets the double parameters; dblMaxTime is the time limit of compute()

 void set_par( idx_type par , double value ) override;

 using Solver::set_par;  // keep the other set_par() visible

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the double parameters, dblMaxTime included

 [[nodiscard]] double get_dbl_par( idx_type par ) const override {
  return( par == dblMaxTime ? MaxTime : Solver::get_dbl_par( par ) );
  }

/*--------------------- METHODS FOR SOLVING THE MODEL ----------------------*/
 /// solves the SATBlock
 /** Solves the SATBlock, giving the SAT solver the clauses again if a
  * Modification requires it and the fixed BooleanVariable as assumptions;
  * the returned status is described in the comments to the class. */

 int compute( bool changedvars = true ) override;

/*---------------------- METHODS FOR READING RESULTS -----------------------*/
 /// 0 after kOK, +INF after kInfeasible, -INF otherwise

 [[nodiscard]] OFValue get_lb( void ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// 0 after kOK, +INF after kInfeasible and otherwise

 [[nodiscard]] OFValue get_ub( void ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// true if the last compute() has found a solution

 [[nodiscard]] bool has_var_solution( void ) override {
  return( f_status == kOK );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// writes the solution found into the BooleanVariable of the SATBlock
 /** Writes the solution found by the last compute() into the
  * BooleanVariable of the SATBlock, generating them if needed; an
  * exception is thrown if there is no solution [see has_var_solution()]. */

 void get_var_solution( Configuration * solc = nullptr ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// true if the assumption on the i-th variable made it unsatisfiable
 /** After a compute() that returned kInfeasible, returns true if the i-th
  * variable was fixed, i.e., it was an assumption, and that assumption is
  * in the reason the SAT solver has found for the clauses being
  * unsatisfiable; false in any other case, in particular if the clauses are
  * unsatisfiable whatever the fixed variables are. */

 [[nodiscard]] bool is_failed( unsigned int i ) const;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the name and version of the SAT solver behind IPASIR

 static const char * signature( void );

/*---------------------- PROTECTED PART OF THE CLASS -----------------------*/

 protected:

/*--------------------------- PROTECTED METHODS ----------------------------*/
 /// reads the Modification, deciding if the clauses have to be given again

 void process_outstanding_Modification( void );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// creates the SAT solver anew and gives it the clauses of the SATBlock

 void load_clauses( void );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// the terminate callback of IPASIR: nonzero when the time is up

 static int terminate( void * data );

/*---------------------------- PROTECTED FIELDS ----------------------------*/

 SATBlock * f_sat = nullptr;   ///< the SATBlock being solved

 void * f_ipasir = nullptr;    ///< the IPASIR solver, nullptr if none

 bool f_reload = true;         ///< if the clauses have to be given again

 int f_status = kUnEval;       ///< status of the last compute()

 double MaxTime = Inf< double >();  ///< the time limit of compute() (s)

 std::chrono::steady_clock::time_point f_start;  ///< start of compute()

 std::vector< unsigned char > v_failed;  ///< failed assumptions

/*----------------------- PRIVATE PART OF THE CLASS ------------------------*/

 private:

 SMSpp_insert_in_factory_h;  // insert SATSolver in the Solver factory

/*--------------------------------------------------------------------------*/

 };  // end( class( SATSolver ) )

/** @} end( group( SATSolver_CLASSES ) ) */

/*--------------------------------------------------------------------------*/

 }  // end( namespace SMSpp_di_unipi_it )

/*--------------------------------------------------------------------------*/

#endif  /* SATSolver.h included */

/*--------------------------------------------------------------------------*/
/*-------------------------- End File SATSolver.h --------------------------*/
/*--------------------------------------------------------------------------*/
