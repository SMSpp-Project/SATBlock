/*--------------------------------------------------------------------------*/
/*------------------------ File MiniSatSATSolver.h -------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Header file for the *concrete* class MiniSatSATSolver, a SATSolver [see
 * SATSolver.h] whose incremental SAT solver is MiniSat.
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

#ifndef __MiniSatSATSolver
 #define __MiniSatSATSolver
                      /* self-identification: #endif at the end of the file */

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <memory>

#include "SATSolver.h"

namespace Minisat { class Solver; }  // in minisat/core/Solver.h

/*--------------------------------------------------------------------------*/
/*------------------------------ NAMESPACE ---------------------------------*/
/*--------------------------------------------------------------------------*/

/// namespace for the Structured Modeling System++ (SMS++)
namespace SMSpp_di_unipi_it
{
/*--------------------------------------------------------------------------*/
/*-------------------------------- CLASSES ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @defgroup MiniSatSATSolver_CLASSES Classes in MiniSatSATSolver.h
 *  @{ */

/*--------------------------------------------------------------------------*/
/*------------------------- CLASS MiniSatSATSolver -------------------------*/
/*--------------------------------------------------------------------------*/
/// a SATSolver whose incremental SAT solver is MiniSat
/** The MiniSatSATSolver class is a SATSolver [see SATSolver.h] whose
 * incremental SAT solver is MiniSat (http://minisat.se), used through its
 * C++ interface. MiniSat having no callback to be stopped, the time limit
 * is enforced by solving with a budget of conflicts and checking the time
 * between one budget and the next. */

class MiniSatSATSolver : public SATSolver
{
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/

 public:

/*------------------------- CONSTRUCTOR AND DESTRUCTOR ---------------------*/

 MiniSatSATSolver( void );    ///< constructor: no SAT solver until compute()

 ~MiniSatSATSolver() override;  ///< destructor: releases the SAT solver

/*---------------------- METHODS FOR READING RESULTS -----------------------*/
 /// returns the name and version of MiniSat

 [[nodiscard]] std::string signature( void ) const override;

/*---------------------- PROTECTED PART OF THE CLASS -----------------------*/

 protected:

/*------------------ THE PRIMITIVES OF THE DERIVED CLASSES -----------------*/

 void sat_new( void ) override;

 void sat_clause( const std::vector< int > & clause ) override;

 int sat_solve( const std::vector< int > & assumptions ) override;

 [[nodiscard]] bool sat_value( int var ) const override;

 [[nodiscard]] bool sat_failed( int lit ) const override;

/*---------------------------- PROTECTED FIELDS ----------------------------*/

 std::unique_ptr< Minisat::Solver > f_solver;  ///< the SAT solver

/*----------------------- PRIVATE PART OF THE CLASS ------------------------*/

 private:

 SMSpp_insert_in_factory_h;  // insert MiniSatSATSolver in the Solver factory

/*--------------------------------------------------------------------------*/

 };  // end( class( MiniSatSATSolver ) )

/** @} end( group( MiniSatSATSolver_CLASSES ) ) */

/*--------------------------------------------------------------------------*/

 }  // end( namespace SMSpp_di_unipi_it )

/*--------------------------------------------------------------------------*/

#endif  /* MiniSatSATSolver.h included */

/*--------------------------------------------------------------------------*/
/*---------------------- End File MiniSatSATSolver.h -----------------------*/
/*--------------------------------------------------------------------------*/
