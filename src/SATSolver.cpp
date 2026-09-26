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

#include <algorithm>
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

void SATSolver::set_par( idx_type par , int value )
{
 if( par == intMaxSAT ) {
  if( ( value != 0 ) && ( value != 1 ) )
   throw( std::invalid_argument( "SATSolver::set_par: intMaxSAT must be 0 "
				 "or 1, not " + std::to_string( value ) ) );
  MaxSATAlg = value;
  }
 else
  Solver::set_par( par , value );
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
 // fixing or unfixing a Variable is taken care of by the assumptions, the
 // clauses added by add_new_clauses(), and so are the weights changed as
 // long as the hard clauses stay the same; any other Modification means the
 // clauses are given again
 while( auto mod = pop() ) {
  if( std::dynamic_pointer_cast< const VariableMod >( mod ) ||
      std::dynamic_pointer_cast< const BlockModAdd< ClauseConstraint > >(
									mod ) )
   continue;
  if( auto smod = std::dynamic_pointer_cast< const SATBlockMod >( mod ) ) {
   if( smod->type() == SATBlockMod::eAddClauses )
    continue;
   if( smod->type() == SATBlockMod::eChgWeight ) {
    // the clauses the SAT solver has (v_hard) against those hard now
    for( unsigned int i = 0 ; i < v_hard.size() ; ++i )
     if( bool( v_hard[ i ] ) != f_sat->is_hard( i ) ) {
      f_reload = true;
      break;
      }
    continue;
    }
   }
  f_reload = true;
  }
 }

/*--------------------------------------------------------------------------*/

void SATSolver::load_clauses( void )
{
 sat_new();
 f_has_sat = true;

 const auto & sat = std::as_const( *f_sat );
 const auto & cc = sat.get_clause_constraints();
 const auto & lc = sat.get_added_clause_constraints();
 v_hard.assign( sat.get_number_clauses() , 0 );
 if( cc.size() + lc.size() == sat.get_number_clauses() ) {
  // the abstract representation, leaving out the relaxed ClauseConstraint
  const auto & x = sat.get_variables();
  std::vector< int > clause;
  unsigned int i = 0;
  auto give = [ & ]( const ClauseConstraint & c ) {
   if( c.is_relaxed() ) {
    ++i;
    return;
    }
   clause.clear();
   for( const auto & lit : c.get_literals() ) {
    const int v = int( lit.first - x.data() ) + 1;
    clause.push_back( lit.second ? - v : v );
    }
   sat_clause( clause );
   v_hard[ i++ ] = 1;
   };
  for( const auto & c : cc )
   give( c );
  for( const auto & c : lc )
   give( c );
  }
 else {
  // the physical representation, where a tautology is harmless
  const auto & clauses = sat.get_clauses();
  for( unsigned int i = 0 ; i < clauses.size() ; ++i )
   if( sat.is_hard( i ) ) {
    sat_clause( clauses[ i ] );
    v_hard[ i ] = 1;
    }
  }

 f_reload = false;
 }

/*--------------------------------------------------------------------------*/

void SATSolver::add_new_clauses( void )
{
 const auto & clauses = f_sat->get_clauses();
 for( auto i = v_hard.size() ; i < clauses.size() ; ++i ) {
  const bool hard = f_sat->is_hard( i );
  if( hard )
   sat_clause( clauses[ i ] );
  v_hard.push_back( hard ? 1 : 0 );
  }
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

 // OLL gives the clauses anew by itself
 process_outstanding_Modification();
 if( MaxSATAlg != 1 ) {
  if( f_reload || ( ! f_has_sat ) )
   load_clauses();
  else
   add_new_clauses();
  }

 // the fixed BooleanVariable, if they exist, are assumptions
 const auto & x = std::as_const( *f_sat ).get_variables();
 std::vector< int > assumptions;
 for( unsigned int i = 0 ; i < x.size() ; ++i )
  if( x[ i ].is_fixed() )
   assumptions.push_back( x[ i ].get_value() ? int( i + 1 )
			                     : - int( i + 1 ) );

 v_failed.assign( f_sat->get_number_variables() , 0 );
 f_ub = Inf< double >();
 v_model.clear();

 int res;
 if( MaxSATAlg == 1 )
  res = oll( assumptions );
 else {
  res = sat_solve( assumptions );
  f_lb = ( res == 10 ? 0 : ( res == 20 ? Inf< double >() :
			     - Inf< double >() ) );
  if( res == 20 )
   for( auto lit : assumptions )
    v_failed[ std::abs( lit ) - 1 ] = sat_failed( lit ) ? 1 : 0;
  }

 switch( res ) {
  case( 10 ): {
   f_status = kOK;
   if( MaxSATAlg == 1 )  // OLL has it already
    break;
   // the weight of the soft clauses the solution violates
   const auto & clauses = f_sat->get_clauses();
   const auto & w = f_sat->get_weights();
   f_ub = 0;
   for( unsigned int i = 0 ; i < clauses.size() ; ++i )
    if( ( ! f_sat->is_hard( i ) ) &&
	std::none_of( clauses[ i ].begin() , clauses[ i ].end() ,
		      [ this ]( int lit ) {
	 return( sat_value( std::abs( lit ) ) == ( lit > 0 ) ); } ) )
     f_ub += w[ i ];
   break;
   }
  case( 20 ):
   f_status = kInfeasible;
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

int SATSolver::tot_build( const std::vector< int > & ins , std::size_t lo ,
			  std::size_t hi )
{
 TotNode node;
 node.size = hi - lo;
 if( node.size == 1 )
  node.out.push_back( ins[ lo ] );
 else {
  const auto mid = lo + node.size / 2;
  node.left = tot_build( ins , lo , mid );
  node.right = tot_build( ins , mid , hi );
  }
 v_tot.push_back( std::move( node ) );
 return( int( v_tot.size() ) - 1 );
 }

/*--------------------------------------------------------------------------*/

void SATSolver::tot_extend( int node , std::size_t k )
{
 k = std::min( k , v_tot[ node ].size );
 const auto old = v_tot[ node ].out.size();
 if( ( v_tot[ node ].left < 0 ) || ( k <= old ) )
  return;

 const int l = v_tot[ node ].left;
 const int r = v_tot[ node ].right;
 tot_extend( l , k );
 tot_extend( r , k );

 for( auto s = old ; s < k ; ++s )
  v_tot[ node ].out.push_back( ++f_next_var );

 // at least i of the left and j of the right imply at least i + j, only for
 // the i + j of the new outputs
 const auto & a = v_tot[ l ].out;
 const auto & b = v_tot[ r ].out;
 const auto & o = v_tot[ node ].out;
 std::vector< int > clause;
 for( std::size_t i = 0 ; i <= a.size() ; ++i )
  for( std::size_t j = 0 ; j <= b.size() ; ++j ) {
   const auto s = i + j;
   if( ( s <= old ) || ( s > k ) )
    continue;
   clause.clear();
   if( i )
    clause.push_back( - a[ i - 1 ] );
   if( j )
    clause.push_back( - b[ j - 1 ] );
   clause.push_back( o[ s - 1 ] );
   sat_clause( clause );
   }
 }

/*--------------------------------------------------------------------------*/

int SATSolver::oll( const std::vector< int > & fixed )
{
 // the SAT solver anew with the hard clauses, the soft ones added below
 load_clauses();
 f_reload = true;  // what OLL adds does not last beyond this compute()
 v_tot.clear();
 f_next_var = int( f_sat->get_number_variables() );

 // an assumption for the soft clauses: its weight, and the totalizer and
 // the bound it says "at most" of, if any
 struct Soft {
  double w;
  int root = -1;
  std::size_t bound = 0;
  };
 std::unordered_map< int , Soft > soft;
 std::vector< int > order;  // the assumptions, in the order they are made
 auto add_soft = [ & ]( int lit , double w , int root , std::size_t bnd ) {
  auto it = soft.find( lit );
  if( it == soft.end() ) {
   soft.emplace( lit , Soft{ w , root , bnd } );
   order.push_back( lit );
   }
  else
   it->second.w += w;
  };

 f_lb = 0;
 const auto & clauses = f_sat->get_clauses();
 const auto & weights = f_sat->get_weights();
 for( unsigned int i = 0 ; i < clauses.size() ; ++i ) {
  if( f_sat->is_hard( i ) || ( weights[ i ] == 0 ) ||
      f_sat->is_tautology( i ) )
   continue;
  if( clauses[ i ].empty() )  // violated whatever
   f_lb += weights[ i ];
  else
   if( clauses[ i ].size() == 1 )
    add_soft( clauses[ i ][ 0 ] , weights[ i ] , -1 , 0 );
   else {
    auto clause = clauses[ i ];
    clause.push_back( ++f_next_var );
    sat_clause( clause );
    add_soft( - f_next_var , weights[ i ] , -1 , 0 );
    }
  }

 // the stratification: the assumptions weighing at least tau are given to
 // the SAT solver; lower_level() lowers tau to take the next weights, until
 // the assumptions are at least 1.25 per distinct weight, returning false
 // if all of them are there already
 double tau = Inf< double >();
 auto lower_level = [ & ]( void ) {
  std::vector< double > ws;
  for( auto lit : order )
   if( soft[ lit ].w > 0 )
    ws.push_back( soft[ lit ].w );
  std::sort( ws.begin() , ws.end() , std::greater< double >() );
  std::size_t i = std::find_if( ws.begin() , ws.end() , [ tau ]( double w ) {
   return( w < tau ); } ) - ws.begin();
  if( i == ws.size() )
   return( false );
  std::size_t distinct = 0;
  for( std::size_t j = 0 ; j < i ; ++j )
   if( ( j == 0 ) || ( ws[ j ] != ws[ j - 1 ] ) )
    ++distinct;
  do {
   tau = ws[ i ];
   ++distinct;
   while( ( i < ws.size() ) && ( ws[ i ] == tau ) )
    ++i;
   }
  while( ( i < ws.size() ) && ( double( i ) < 1.25 * double( distinct ) ) );
  return( true );
  };

 // the weight of the soft clauses the current solution violates
 auto model_cost = [ & ]( void ) {
  double cost = 0;
  for( unsigned int i = 0 ; i < clauses.size() ; ++i )
   if( ( ! f_sat->is_hard( i ) ) &&
       std::none_of( clauses[ i ].begin() , clauses[ i ].end() ,
		     [ this ]( int lit ) {
	return( sat_value( std::abs( lit ) ) == ( lit > 0 ) ); } ) )
    cost += weights[ i ];
  return( cost );
  };

 lower_level();

 std::vector< int > as;
 std::vector< int > core;
 for( ; ; ) {
  as = fixed;
  for( auto lit : order )
   if( ( soft[ lit ].w > 0 ) && ( soft[ lit ].w >= tau ) )
    as.push_back( lit );

  const int res = sat_solve( as );
  if( res == 10 ) {
   // an upper bound, the optimum if all the assumptions are there
   const double cost = model_cost();
   if( cost < f_ub ) {
    f_ub = cost;
    v_model.resize( f_sat->get_number_variables() );
    for( unsigned int i = 0 ; i < v_model.size() ; ++i )
     v_model[ i ] = sat_value( int( i + 1 ) ) ? 1 : 0;
    }
   if( ! lower_level() )  // f_lb == f_ub, up to the rounding of the weights
    return( 10 );
   continue;
   }
  if( res != 20 )
   return( res );

  // the core: the soft assumptions in the reason
  core.clear();
  for( auto it = as.begin() + fixed.size() ; it != as.end() ; ++it )
   if( sat_failed( *it ) )
    core.push_back( *it );

  if( core.empty() ) {
   // the hard clauses and the fixed variables are unsatisfiable: the reason
   // is asked again without the clauses OLL has added, which may be in it
   f_lb = Inf< double >();
   load_clauses();
   const int hres = sat_solve( fixed );
   if( hres == 20 )
    for( auto lit : fixed )
     v_failed[ std::abs( lit ) - 1 ] = sat_failed( lit ) ? 1 : 0;
   return( hres );
   }

  double wmin = Inf< double >();
  for( auto lit : core )
   wmin = std::min( wmin , soft[ lit ].w );
  f_lb += wmin;

  std::vector< int > viol;  // the literals true if an assumption is not
  viol.reserve( core.size() );
  for( auto lit : core ) {
   auto & sl = soft[ lit ];
   sl.w -= wmin;
   viol.push_back( - lit );
   if( sl.root >= 0 ) {
    // "at most bound" is not, "at most bound + 1" takes the weight
    const auto root = sl.root;
    const auto bnd = sl.bound + 1;
    if( bnd < v_tot[ root ].size ) {
     tot_extend( root , bnd + 1 );
     add_soft( - v_tot[ root ].out[ bnd ] , wmin , root , bnd );
     }
    }
   }

  if( core.size() == 1 )  // that assumption never holds
   sat_clause( { - core[ 0 ] } );
  else {
   // "at most one of the core is violated"
   const int root = tot_build( viol , 0 , viol.size() );
   tot_extend( root , 2 );
   add_soft( - v_tot[ root ].out[ 1 ] , wmin , root , 1 );
   }
  }
 }

/*--------------------------------------------------------------------------*/
/*---------------------- METHODS FOR READING RESULTS -----------------------*/
/*--------------------------------------------------------------------------*/

Solver::OFValue SATSolver::get_lb( void )
{
 return( f_status == kUnEval ? - Inf< OFValue >() : OFValue( f_lb ) );
 }

/*--------------------------------------------------------------------------*/

Solver::OFValue SATSolver::get_ub( void )
{
 return( f_status == kOK ? OFValue( f_ub ) : Inf< OFValue >() );
 }

/*--------------------------------------------------------------------------*/

void SATSolver::get_var_solution( Configuration * solc )
{
 if( ! has_var_solution() )
  throw( std::logic_error( "SATSolver::get_var_solution: no solution" ) );

 f_sat->generate_abstract_variables();
 auto & x = f_sat->get_variables();
 for( unsigned int i = 0 ; i < x.size() ; ++i )
  x[ i ].set_value( v_model.empty() ? sat_value( int( i + 1 ) )
		                    : bool( v_model[ i ] ) );
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
