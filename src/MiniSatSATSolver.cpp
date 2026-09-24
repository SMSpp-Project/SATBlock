/*--------------------------------------------------------------------------*/
/*----------------------- File MiniSatSATSolver.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of the MiniSatSATSolver class.
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <cstdlib>

#include "minisat/core/Solver.h"

#include "MiniSatSATSolver.h"

/*--------------------------------------------------------------------------*/
/*------------------------- NAMESPACE AND USING ----------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*----------------------------- STATIC MEMBERS -----------------------------*/
/*--------------------------------------------------------------------------*/

// register MiniSatSATSolver in the Solver factory

SMSpp_insert_in_factory_cpp_0( MiniSatSATSolver );

/*--------------------------------------------------------------------------*/
/*-------------------------------- FUNCTIONS -------------------------------*/
/*--------------------------------------------------------------------------*/

namespace {

/// the conflicts MiniSat is given between two checks of the time limit
constexpr int ConflictBudget = 10000;

/// the MiniSat literal of a DIMACS literal, creating its variable if needed

Minisat::Lit to_lit( Minisat::Solver & s , int lit )
{
 const int v = std::abs( lit ) - 1;
 while( s.nVars() <= v )
  s.newVar();
 return( Minisat::mkLit( v , lit < 0 ) );
 }

}  // end( unnamed namespace )

/*--------------------------------------------------------------------------*/
/*--------------------- CONSTRUCTOR AND DESTRUCTOR -------------------------*/
/*--------------------------------------------------------------------------*/

MiniSatSATSolver::MiniSatSATSolver( void ) : SATSolver() {}

MiniSatSATSolver::~MiniSatSATSolver() = default;

/*--------------------------------------------------------------------------*/
/*--------------------------------- METHODS --------------------------------*/
/*--------------------------------------------------------------------------*/

std::string MiniSatSATSolver::signature( void ) const
{
 return( "minisat" );
 }

/*--------------------------------------------------------------------------*/

void MiniSatSATSolver::sat_new( void )
{
 f_solver = std::make_unique< Minisat::Solver >();
 }

/*--------------------------------------------------------------------------*/

void MiniSatSATSolver::sat_clause( const std::vector< int > & clause )
{
 Minisat::vec< Minisat::Lit > ps;
 for( auto lit : clause )
  ps.push( to_lit( *f_solver , lit ) );
 f_solver->addClause( ps );  // false means unsatisfiable, solve() says so
 }

/*--------------------------------------------------------------------------*/

int MiniSatSATSolver::sat_solve( const std::vector< int > & assumptions )
{
 Minisat::vec< Minisat::Lit > as;
 for( auto lit : assumptions )
  as.push( to_lit( *f_solver , lit ) );

 if( ! ( MaxTime < Inf< double >() ) ) {
  f_solver->budgetOff();
  const auto res = f_solver->solveLimited( as );
  return( res == Minisat::l_True ? 10 : ( res == Minisat::l_False ? 20 : 0 ) );
  }

 // solve a budget of conflicts at a time, checking the time in between
 for( ; ; ) {
  f_solver->setConfBudget( ConflictBudget );
  const auto res = f_solver->solveLimited( as );
  if( res == Minisat::l_True )
   return( 10 );
  if( res == Minisat::l_False )
   return( 20 );
  if( time_is_up() )
   return( 0 );
  }
 }

/*--------------------------------------------------------------------------*/

bool MiniSatSATSolver::sat_value( int var ) const
{
 // a variable in no clause is unknown to MiniSat, and it is false
 return( ( var <= f_solver->nVars() ) &&
	 ( f_solver->modelValue( var - 1 ) == Minisat::l_True ) );
 }

/*--------------------------------------------------------------------------*/

bool MiniSatSATSolver::sat_failed( int lit ) const
{
 // the final conflict holds the negation of the failed assumptions
 const Minisat::Lit l = Minisat::mkLit( std::abs( lit ) - 1 , lit < 0 );
 return( ( std::abs( lit ) <= f_solver->nVars() ) &&
	 f_solver->conflict.has( ~l ) );
 }

/*--------------------------------------------------------------------------*/
/*--------------------- End File MiniSatSATSolver.cpp ----------------------*/
/*--------------------------------------------------------------------------*/
