/*--------------------------------------------------------------------------*/
/*--------------------------- File SATSolver.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of the SATSolver class.
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*----------------------------- IMPLEMENTATION -----------------------------*/
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <cstdlib>
#include <utility>

#include "SATSolver.h"

/*--------------------------------------------------------------------------*/
/*------------------------- NAMESPACE AND USING ----------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------- OTHER INITIALIZATIONS --------------------------*/
/*--------------------------------------------------------------------------*/

void SATSolver::set_Block( Block * block )
{
 if( block == f_Block )
  return;

 SATBlock * sat = nullptr;
 if( block && ! ( sat = dynamic_cast< SATBlock * >( block ) ) )
  throw( std::invalid_argument( "SATSolver::set_Block: the Block is not a "
				"SATBlock" ) );

 Solver::set_Block( block );
 f_sat = sat;
 f_reload = true;
 f_status = kUnEval;
 }

/*--------------------------------------------------------------------------*/

void SATSolver::set_par( idx_type par , double value )
{
 if( par == dblMaxTime )
  MaxTime = value;
 else
  Solver::set_par( par , value );
 }

/*--------------------------------------------------------------------------*/
/*--------------------- METHODS FOR SOLVING THE MODEL ----------------------*/
/*--------------------------------------------------------------------------*/

void SATSolver::process_outstanding_Modification( void )
{
 // fixing or unfixing a Variable is taken care of by the assumptions; any
 // other Modification means the clauses are given again
 while( auto mod = pop() )
  if( ! std::dynamic_pointer_cast< const VariableMod >( mod ) )
   f_reload = true;
 }

/*--------------------------------------------------------------------------*/

void SATSolver::load_clauses( void )
{
 sat_new();
 f_has_sat = true;

 const auto & cc = std::as_const( *f_sat ).get_clause_constraints();
 if( cc.size() == f_sat->get_number_clauses() ) {
  // the abstract representation, leaving out the relaxed ClauseConstraint
  const auto & x = std::as_const( *f_sat ).get_variables();
  std::vector< int > clause;
  for( const auto & c : cc ) {
   if( c.is_relaxed() )
    continue;
   clause.clear();
   for( const auto & lit : c.get_literals() ) {
    const int v = int( lit.first - x.data() ) + 1;
    clause.push_back( lit.second ? - v : v );
    }
   sat_clause( clause );
   }
  }
 else
  // the physical representation, where a tautology is harmless
  for( const auto & clause : f_sat->get_clauses() )
   sat_clause( clause );

 f_reload = false;
 }

/*--------------------------------------------------------------------------*/

bool SATSolver::time_is_up( void ) const
{
 const std::chrono::duration< double > elapsed =
  std::chrono::steady_clock::now() - f_start;
 return( elapsed.count() >= MaxTime );
 }

/*--------------------------------------------------------------------------*/

int SATSolver::compute( bool changedvars )
{
 if( ! f_sat )
  throw( std::logic_error( "SATSolver::compute: no SATBlock attached" ) );

 lock();

 f_start = std::chrono::steady_clock::now();

 process_outstanding_Modification();
 if( f_reload || ( ! f_has_sat ) )
  load_clauses();

 // the fixed BooleanVariable, if they exist, are assumptions
 const auto & x = std::as_const( *f_sat ).get_variables();
 std::vector< int > assumptions;
 for( unsigned int i = 0 ; i < x.size() ; ++i )
  if( x[ i ].is_fixed() )
   assumptions.push_back( x[ i ].get_value() ? int( i + 1 )
			                     : - int( i + 1 ) );

 const int res = sat_solve( assumptions );

 v_failed.assign( f_sat->get_number_variables() , 0 );
 switch( res ) {
  case( 10 ):
   f_status = kOK;
   break;
  case( 20 ):
   f_status = kInfeasible;
   for( auto lit : assumptions )
    v_failed[ std::abs( lit ) - 1 ] = sat_failed( lit ) ? 1 : 0;
   break;
  default:
   f_status = time_is_up() ? int( kStopTime ) : int( kError );
  }

 if( f_log )
  *f_log << "SATSolver [" << signature() << "]: " << ( res == 10 ? "SAT" :
	    ( res == 20 ? "UNSAT" : "unknown" ) ) << std::endl;

 unlock();

 return( f_status );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- METHODS FOR READING RESULTS -----------------------*/
/*--------------------------------------------------------------------------*/

Solver::OFValue SATSolver::get_lb( void )
{
 if( f_status == kOK )
  return( 0 );
 if( f_status == kInfeasible )
  return( Inf< OFValue >() );
 return( - Inf< OFValue >() );
 }

/*--------------------------------------------------------------------------*/

Solver::OFValue SATSolver::get_ub( void )
{
 return( f_status == kOK ? 0 : Inf< OFValue >() );
 }

/*--------------------------------------------------------------------------*/

void SATSolver::get_var_solution( Configuration * solc )
{
 if( f_status != kOK )
  throw( std::logic_error( "SATSolver::get_var_solution: no solution" ) );

 f_sat->generate_abstract_variables();
 auto & x = f_sat->get_variables();
 for( unsigned int i = 0 ; i < x.size() ; ++i )
  x[ i ].set_value( sat_value( int( i + 1 ) ) );
 }

/*--------------------------------------------------------------------------*/

bool SATSolver::is_failed( unsigned int i ) const
{
 return( ( f_status == kInfeasible ) && ( i < v_failed.size() ) &&
	 v_failed[ i ] );
 }

/*--------------------------------------------------------------------------*/
/*------------------------- End File SATSolver.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
