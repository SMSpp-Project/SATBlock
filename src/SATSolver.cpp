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

extern "C" {
#include "ipasir.h"
}

/*--------------------------------------------------------------------------*/
/*------------------------- NAMESPACE AND USING ----------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*----------------------------- STATIC MEMBERS -----------------------------*/
/*--------------------------------------------------------------------------*/

// register SATSolver in the Solver factory

SMSpp_insert_in_factory_cpp_0( SATSolver );

/*--------------------------------------------------------------------------*/
/*--------------------- CONSTRUCTOR AND DESTRUCTOR -------------------------*/
/*--------------------------------------------------------------------------*/

SATSolver::~SATSolver()
{
 if( f_ipasir )
  ipasir_release( f_ipasir );
 }

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
 if( f_ipasir )
  ipasir_release( f_ipasir );
 f_ipasir = ipasir_init();

 const auto & cc = f_sat->get_clause_constraints();
 if( cc.size() == f_sat->get_number_clauses() ) {
  // the abstract representation, leaving out the relaxed ClauseConstraint
  const auto & x = f_sat->get_variables();
  for( const auto & c : cc ) {
   if( c.is_relaxed() )
    continue;
   for( const auto & lit : c.get_literals() ) {
    const int v = int( lit.first - x.data() ) + 1;
    ipasir_add( f_ipasir , lit.second ? - v : v );
    }
   ipasir_add( f_ipasir , 0 );
   }
  }
 else
  // the physical representation, where a tautology is harmless
  for( const auto & clause : f_sat->get_clauses() ) {
   for( auto lit : clause )
    ipasir_add( f_ipasir , lit );
   ipasir_add( f_ipasir , 0 );
   }

 f_reload = false;
 }

/*--------------------------------------------------------------------------*/

int SATSolver::terminate( void * data )
{
 auto solver = static_cast< SATSolver * >( data );
 const std::chrono::duration< double > elapsed =
  std::chrono::steady_clock::now() - solver->f_start;
 return( elapsed.count() >= solver->MaxTime ? 1 : 0 );
 }

/*--------------------------------------------------------------------------*/

int SATSolver::compute( bool changedvars )
{
 if( ! f_sat )
  throw( std::logic_error( "SATSolver::compute: no SATBlock attached" ) );

 lock();

 f_start = std::chrono::steady_clock::now();

 process_outstanding_Modification();
 if( f_reload || ( ! f_ipasir ) )
  load_clauses();

 // the fixed BooleanVariable, if they exist, are assumptions
 const auto & x = std::as_const( *f_sat ).get_variables();
 for( unsigned int i = 0 ; i < x.size() ; ++i )
  if( x[ i ].is_fixed() )
   ipasir_assume( f_ipasir , x[ i ].get_value() ? int( i + 1 )
		                                : - int( i + 1 ) );

 if( MaxTime < Inf< double >() )
  ipasir_set_terminate( f_ipasir , this , & SATSolver::terminate );
 else
  ipasir_set_terminate( f_ipasir , nullptr , nullptr );

 const int res = ipasir_solve( f_ipasir );

 v_failed.assign( f_sat->get_number_variables() , 0 );
 switch( res ) {
  case( 10 ):
   f_status = kOK;
   break;
  case( 20 ):
   f_status = kInfeasible;
   for( unsigned int i = 0 ; i < x.size() ; ++i )
    if( x[ i ].is_fixed() )
     v_failed[ i ] = ipasir_failed( f_ipasir , x[ i ].get_value()
				    ? int( i + 1 ) : - int( i + 1 ) ) ? 1 : 0;
   break;
  default:
   f_status = terminate( this ) ? int( kStopTime ) : int( kError );
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
  x[ i ].set_value( ipasir_val( f_ipasir , int( i + 1 ) ) > 0 );
 }

/*--------------------------------------------------------------------------*/

bool SATSolver::is_failed( unsigned int i ) const
{
 return( ( f_status == kInfeasible ) && ( i < v_failed.size() ) &&
	 v_failed[ i ] );
 }

/*--------------------------------------------------------------------------*/

const char * SATSolver::signature( void )
{
 return( ipasir_signature() );
 }

/*--------------------------------------------------------------------------*/
/*------------------------- End File SATSolver.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
