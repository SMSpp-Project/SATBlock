/*--------------------------------------------------------------------------*/
/*------------------------ File CaDiCaLSATSolver.h -------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Header file for the *concrete* class CaDiCaLSATSolver, a SATSolver [see
 * SATSolver.h] whose incremental SAT solver is CaDiCaL.
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

#ifndef __CaDiCaLSATSolver
 #define __CaDiCaLSATSolver
                      /* self-identification: #endif at the end of the file */

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <memory>

#include "SATSolver.h"

namespace CaDiCaL { class Solver; }  // the SAT solver, in cadical.hpp

/*--------------------------------------------------------------------------*/
/*------------------------------ NAMESPACE ---------------------------------*/
/*--------------------------------------------------------------------------*/

/// namespace for the Structured Modeling System++ (SMS++)
namespace SMSpp_di_unipi_it
{
/*--------------------------------------------------------------------------*/
/*-------------------------------- CLASSES ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @defgroup CaDiCaLSATSolver_CLASSES Classes in CaDiCaLSATSolver.h
 *  @{ */

/*--------------------------------------------------------------------------*/
/*------------------------- CLASS CaDiCaLSATSolver -------------------------*/
/*--------------------------------------------------------------------------*/
/// a SATSolver whose incremental SAT solver is CaDiCaL
/** The CaDiCaLSATSolver class is a SATSolver [see SATSolver.h] whose
 * incremental SAT solver is CaDiCaL (https://github.com/arminbiere/cadical),
 * used through its C++ interface. The time limit is enforced by the
 * Terminator that CaDiCaL asks, while solving, whether to stop. */

class CaDiCaLSATSolver : public SATSolver
{
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/

 public:

/*------------------------- CONSTRUCTOR AND DESTRUCTOR ---------------------*/

 CaDiCaLSATSolver( void );    ///< constructor: no SAT solver until compute()

 ~CaDiCaLSATSolver() override;  ///< destructor: releases the SAT solver

/*---------------------- METHODS FOR READING RESULTS -----------------------*/
 /// returns the name and version of CaDiCaL

 [[nodiscard]] std::string signature( void ) const override;

/*---------------------- PROTECTED PART OF THE CLASS -----------------------*/

 protected:

/*------------------ THE PRIMITIVES OF THE DERIVED CLASSES -----------------*/

 void sat_new( void ) override;

 void sat_clause( const std::vector< int > & clause ) override;

 int sat_solve( const std::vector< int > & assumptions ,
		 long conflicts = -1 ) override;

 [[nodiscard]] bool sat_value( int var ) const override;

 [[nodiscard]] bool sat_failed( int lit ) const override;

/*---------------------------- PROTECTED FIELDS ----------------------------*/

 struct Term;  ///< the CaDiCaL::Terminator asking time_is_up()

 std::unique_ptr< CaDiCaL::Solver > f_solver;  ///< the SAT solver

 std::unique_ptr< Term > f_term;  ///< its Terminator

/*----------------------- PRIVATE PART OF THE CLASS ------------------------*/

 private:

 SMSpp_insert_in_factory_h;  // insert CaDiCaLSATSolver in the Solver factory

/*--------------------------------------------------------------------------*/

 };  // end( class( CaDiCaLSATSolver ) )

/** @} end( group( CaDiCaLSATSolver_CLASSES ) ) */

/*--------------------------------------------------------------------------*/

 }  // end( namespace SMSpp_di_unipi_it )

/*--------------------------------------------------------------------------*/

#endif  /* CaDiCaLSATSolver.h included */

/*--------------------------------------------------------------------------*/
/*---------------------- End File CaDiCaLSATSolver.h -----------------------*/
/*--------------------------------------------------------------------------*/
