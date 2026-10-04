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
 * Terminator that CaDiCaL asks, while solving, whether to stop.
 *
 * Two string parameters set CaDiCaL up each time a SAT solver is made:
 * strCaDiCaLConfig names one of its configurations (e.g., "sat", "unsat",
 * "plain"), and strCaDiCaLOptions lists its options as comma-separated
 * name=value pairs (e.g., "inprocessing=0,chrono=0"), applied after the
 * configuration. An unknown configuration or option throws in set_par(). */

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

/*----------------------- METHODS FOR HANDLING PARAMETERS ------------------*/
 /// the string parameters of CaDiCaLSATSolver, on top of those of SATSolver

 enum str_par_type_CaDiCaL {
  strCaDiCaLConfig = strLastAlgParSATS ,  ///< the configuration of CaDiCaL
                               /**< The name given to configure() of
				* CaDiCaL; empty (the default) leaves its
				* defaults. */
  strCaDiCaLOptions ,          ///< the options of CaDiCaL
                               /**< Comma-separated name=value pairs, each
				* given to set() of CaDiCaL after the
				* configuration; empty by default. */
  strLastAlgParCaDiCaL         ///< first new string parameter of derived
                               ///< classes
  };

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// sets the string parameters, checking the names given to CaDiCaL

 void set_par( idx_type par , std::string && value ) override;

 using SATSolver::set_par;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the number of string parameters

 [[nodiscard]] idx_type get_num_str_par( void ) const override {
  return( idx_type( strLastAlgParCaDiCaL ) );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the default of the string parameters

 [[nodiscard]] const std::string & get_dflt_str_par( idx_type par )
  const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the string parameters

 [[nodiscard]] const std::string & get_str_par( idx_type par )
  const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the index of the string parameter with the given name

 [[nodiscard]] idx_type str_par_str2idx( const std::string & name )
  const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the name of the string parameter with the given index

 [[nodiscard]] const std::string & str_par_idx2str( idx_type idx )
  const override;

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

 std::string Config;           ///< the parameter strCaDiCaLConfig

 std::string Options;          ///< the parameter strCaDiCaLOptions

 /// the options of strCaDiCaLOptions, as names and values
 std::vector< std::pair< std::string , int > > v_options;

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
