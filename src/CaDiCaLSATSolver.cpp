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

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

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

void CaDiCaLSATSolver::set_par( idx_type par , std::string && value )
{
 switch( par ) {
  case( strCaDiCaLConfig ):
   if( ( ! value.empty() ) &&
       ( ! CaDiCaL::Solver::is_valid_configuration( value.c_str() ) ) )
    throw( std::invalid_argument( "CaDiCaLSATSolver::set_par: no "
				  "configuration named " + value ) );
   Config = std::move( value );
   break;
  case( strCaDiCaLOptions ): {
   std::vector< std::pair< std::string , int > > options;
   std::size_t from = 0;
   while( from < value.size() ) {
    auto to = value.find( ',' , from );
    if( to == std::string::npos )
     to = value.size();
    const auto item = value.substr( from , to - from );
    const auto eq = item.find( '=' );
    const auto name = item.substr( 0 , eq );
    std::size_t used = 0;
    int val = 0;
    if( eq != std::string::npos )
     try {
      val = std::stoi( item.substr( eq + 1 ) , & used );
      }
     catch( const std::exception & ) { used = 0; }
    if( ( used == 0 ) || ( used != item.size() - eq - 1 ) ||
	( ! CaDiCaL::Solver::is_valid_option( name.c_str() ) ) )
     throw( std::invalid_argument( "CaDiCaLSATSolver::set_par: " + item +
				   " is not a name=value option" ) );
    options.emplace_back( name , val );
    from = to + 1;
    }
   v_options = std::move( options );
   Options = std::move( value );
   break;
   }
  default:
   SATSolver::set_par( par , std::move( value ) );
  }
 }

/*--------------------------------------------------------------------------*/

const std::string & CaDiCaLSATSolver::get_dflt_str_par( idx_type par )
 const
{
 static const std::string empty;
 if( ( par == strCaDiCaLConfig ) || ( par == strCaDiCaLOptions ) )
  return( empty );
 return( SATSolver::get_dflt_str_par( par ) );
 }

/*--------------------------------------------------------------------------*/

const std::string & CaDiCaLSATSolver::get_str_par( idx_type par ) const
{
 switch( par ) {
  case( strCaDiCaLConfig ):  return( Config );
  case( strCaDiCaLOptions ): return( Options );
  default:                   return( SATSolver::get_str_par( par ) );
  }
 }

/*--------------------------------------------------------------------------*/

Solver::idx_type CaDiCaLSATSolver::str_par_str2idx( const std::string & name )
 const
{
 if( name == "strCaDiCaLConfig" )
  return( strCaDiCaLConfig );
 if( name == "strCaDiCaLOptions" )
  return( strCaDiCaLOptions );
 return( SATSolver::str_par_str2idx( name ) );
 }

/*--------------------------------------------------------------------------*/

const std::string & CaDiCaLSATSolver::str_par_idx2str( idx_type idx ) const
{
 static const std::array< std::string , 2 > names = { "strCaDiCaLConfig" ,
						       "strCaDiCaLOptions" };
 if( ( idx >= strCaDiCaLConfig ) && ( idx < strLastAlgParCaDiCaL ) )
  return( names[ idx - strCaDiCaLConfig ] );
 return( SATSolver::str_par_idx2str( idx ) );
 }

/*--------------------------------------------------------------------------*/

void CaDiCaLSATSolver::sat_new( void )
{
 f_solver = std::make_unique< CaDiCaL::Solver >();
 if( ! Config.empty() )
  f_solver->configure( Config.c_str() );
 for( const auto & [ name , val ] : v_options )
  f_solver->set( name.c_str() , val );
 }

/*--------------------------------------------------------------------------*/

void CaDiCaLSATSolver::sat_clause( const std::vector< int > & clause )
{
 for( auto lit : clause )
  f_solver->add( lit );
 f_solver->add( 0 );
 }

/*--------------------------------------------------------------------------*/

int CaDiCaLSATSolver::sat_solve( const std::vector< int > & assumptions ,
				 long conflicts )
{
 for( auto lit : assumptions )
  f_solver->assume( lit );
 if( conflicts >= 0 )  // for the next solve() only
  f_solver->limit( "conflicts" , int( std::min( conflicts ,
					long( std::numeric_limits< int >::max() ) ) ) );

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
