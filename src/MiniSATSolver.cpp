/*--------------------------------------------------------------------------*/
/*------------------------- File MiniSATSolver.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of the MiniSATSolver class.
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

#include "MiniSATSolver.h"

/*--------------------------------------------------------------------------*/
/*------------------------- NAMESPACE AND USING ----------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*----------------------------- STATIC MEMBERS -----------------------------*/
/*--------------------------------------------------------------------------*/

// register MiniSATSolver in the Solver factory

SMSpp_insert_in_factory_cpp_0( MiniSATSolver );

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

MiniSATSolver::MiniSATSolver( void ) : SATSolver() {}

MiniSATSolver::~MiniSATSolver() = default;

/*--------------------------------------------------------------------------*/
/*--------------------------------- METHODS --------------------------------*/
/*--------------------------------------------------------------------------*/

std::string MiniSATSolver::signature( void ) const
{
 return( "minisat" );
 }

/*--------------------------------------------------------------------------*/

void MiniSATSolver::sat_new( void )
{
 f_solver = std::make_unique< Minisat::Solver >();
 }

/*--------------------------------------------------------------------------*/

void MiniSATSolver::sat_clause( const std::vector< int > & clause )
{
 Minisat::vec< Minisat::Lit > ps;
 for( auto lit : clause )
  ps.push( to_lit( *f_solver , lit ) );
 f_solver->addClause( ps );  // false means unsatisfiable, solve() says so
 }

/*--------------------------------------------------------------------------*/

int MiniSATSolver::sat_solve( const std::vector< int > & assumptions ,
				 long conflicts )
{
 Minisat::vec< Minisat::Lit > as;
 for( auto lit : assumptions )
  as.push( to_lit( *f_solver , lit ) );

 if( conflicts >= 0 ) {
  // a budget of its own, the time being looked at only after it
  f_solver->setConfBudget( conflicts );
  const auto res = f_solver->solveLimited( as );
  f_solver->budgetOff();
  return( res == Minisat::l_True ? 10 : ( res == Minisat::l_False ? 20 : 0 ) );
  }

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

bool MiniSATSolver::sat_value( int var ) const
{
 // a variable in no clause is unknown to MiniSat, and it is false
 return( ( var <= f_solver->nVars() ) &&
	 ( f_solver->modelValue( var - 1 ) == Minisat::l_True ) );
 }

/*--------------------------------------------------------------------------*/

bool MiniSATSolver::sat_failed( int lit ) const
{
 // the final conflict holds the negation of the failed assumptions
 const Minisat::Lit l = Minisat::mkLit( std::abs( lit ) - 1 , lit < 0 );
 return( ( std::abs( lit ) <= f_solver->nVars() ) &&
	 f_solver->conflict.has( ~l ) );
 }

/*--------------------------------------------------------------------------*/
/*----------------------- End File MiniSATSolver.cpp -----------------------*/
/*--------------------------------------------------------------------------*/
