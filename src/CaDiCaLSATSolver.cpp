/*--------------------------------------------------------------------------*/
/*----------------------- File CaDiCaLSATSolver.cpp ------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of the CaDiCaLSATSolver class.
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

#include "cadical.hpp"

#include "CaDiCaLSATSolver.h"

/*--------------------------------------------------------------------------*/
/*------------------------- NAMESPACE AND USING ----------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*----------------------------- STATIC MEMBERS -----------------------------*/
/*--------------------------------------------------------------------------*/

// register CaDiCaLSATSolver in the Solver factory

SMSpp_insert_in_factory_cpp_0( CaDiCaLSATSolver );

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/

/// the Terminator CaDiCaL asks while solving: it stops when the time is up

struct CaDiCaLSATSolver::Term : public CaDiCaL::Terminator {
 explicit Term( const CaDiCaLSATSolver & s ) : f_s( s ) {}
 bool terminate( void ) override { return( f_s.time_is_up() ); }
 const CaDiCaLSATSolver & f_s;
 };

/*--------------------------------------------------------------------------*/
/*--------------------- CONSTRUCTOR AND DESTRUCTOR -------------------------*/
/*--------------------------------------------------------------------------*/

CaDiCaLSATSolver::CaDiCaLSATSolver( void )
 : SATSolver() , f_term( std::make_unique< Term >( *this ) ) {}

CaDiCaLSATSolver::~CaDiCaLSATSolver() = default;

/*--------------------------------------------------------------------------*/
/*--------------------------------- METHODS --------------------------------*/
/*--------------------------------------------------------------------------*/

std::string CaDiCaLSATSolver::signature( void ) const
{
 return( std::string( "cadical-" ) + CaDiCaL::Solver::version() );
 }

/*--------------------------------------------------------------------------*/

void CaDiCaLSATSolver::sat_new( void )
{
 f_solver = std::make_unique< CaDiCaL::Solver >();
 }

/*--------------------------------------------------------------------------*/

void CaDiCaLSATSolver::sat_clause( const std::vector< int > & clause )
{
 for( auto lit : clause )
  f_solver->add( lit );
 f_solver->add( 0 );
 }

/*--------------------------------------------------------------------------*/

int CaDiCaLSATSolver::sat_solve( const std::vector< int > & assumptions )
{
 for( auto lit : assumptions )
  f_solver->assume( lit );

 const bool limited = MaxTime < Inf< double >();
 if( limited )
  f_solver->connect_terminator( f_term.get() );
 const int res = f_solver->solve();
 if( limited )
  f_solver->disconnect_terminator();

 return( res );
 }

/*--------------------------------------------------------------------------*/

bool CaDiCaLSATSolver::sat_value( int var ) const
{
 // a variable in no clause is unknown to CaDiCaL, and it is false
 return( ( var <= f_solver->vars() ) && ( f_solver->val( var ) > 0 ) );
 }

/*--------------------------------------------------------------------------*/

bool CaDiCaLSATSolver::sat_failed( int lit ) const
{
 return( f_solver->failed( lit ) );
 }

/*--------------------------------------------------------------------------*/
/*--------------------- End File CaDiCaLSATSolver.cpp ----------------------*/
/*--------------------------------------------------------------------------*/
