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
#include <array>
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
 switch( par ) {
  case( intMaxSAT ):
   if( ( value != 0 ) && ( value != 1 ) )
    throw( std::invalid_argument( "SATSolver::set_par: intMaxSAT must be 0 "
				  "or 1, not " + std::to_string( value ) ) );
   MaxSATAlg = value;
   break;
  case( intMaxSATTrim ):
   CoreTrim = std::max( value , 0 );
   break;
  case( intMaxSATMinBudget ):
   CoreMinBudget = std::max( value , 0 );
   break;
  case( intMaxIter ):
   MaxIter = std::max( value , 0 );
   break;
  default:
   Solver::set_par( par , value );
  }
 }

/*--------------------------------------------------------------------------*/

int SATSolver::get_dflt_int_par( idx_type par ) const
{
 switch( par ) {
  case( intMaxSAT ):          return( 0 );
  case( intMaxSATTrim ):      return( 5 );
  case( intMaxSATMinBudget ): return( 1000 );
  default:                    return( Solver::get_dflt_int_par( par ) );
  }
 }

/*--------------------------------------------------------------------------*/

int SATSolver::get_int_par( idx_type par ) const
{
 switch( par ) {
  case( intMaxSAT ):          return( MaxSATAlg );
  case( intMaxSATTrim ):      return( CoreTrim );
  case( intMaxSATMinBudget ): return( CoreMinBudget );
  case( intMaxIter ):         return( MaxIter );
  default:                    return( Solver::get_int_par( par ) );
  }
 }

/*--------------------------------------------------------------------------*/

Solver::idx_type SATSolver::int_par_str2idx( const std::string & name ) const
{
 if( name == "intMaxSAT" )
  return( intMaxSAT );
 if( name == "intMaxSATTrim" )
  return( intMaxSATTrim );
 if( name == "intMaxSATMinBudget" )
  return( intMaxSATMinBudget );
 return( Solver::int_par_str2idx( name ) );
 }

/*--------------------------------------------------------------------------*/

const std::string & SATSolver::int_par_idx2str( idx_type idx ) const
{
 static const std::array< std::string , 3 > names = { "intMaxSAT" ,
				"intMaxSATTrim" , "intMaxSATMinBudget" };
 if( ( idx >= intMaxSAT ) && ( idx < intLastAlgParSATS ) )
  return( names[ idx - intMaxSAT ] );
 return( Solver::int_par_idx2str( idx ) );
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
 // the Function of the Objective, whose changes are those of the weights
 auto obj = dynamic_cast< const FRealObjective * >( f_sat->get_objective() );
 const Function * objf = obj ? obj->get_function() : nullptr;

 // fixing or unfixing a Variable is taken care of by the assumptions, the
 // clauses added and those turned hard by sync_clauses(), which also finds
 // those turned soft, and the weights and the costs are read anew by each
 // compute(); any other Modification means the clauses are given again
 while( auto mod = pop() ) {
  if( std::dynamic_pointer_cast< const VariableMod >( mod ) ||
      std::dynamic_pointer_cast< const ObjectiveMod >( mod ) ||
      std::dynamic_pointer_cast< const BlockModAdd< ColVariable > >( mod ) ||
      std::dynamic_pointer_cast< const BlockModAdd< FRowConstraint > >(
									mod ) )
   continue;
  if( auto fmod = std::dynamic_pointer_cast< const FunctionMod >( mod ) )
   if( objf && ( fmod->function() == objf ) )
    continue;
  if( auto fmod = std::dynamic_pointer_cast< const FunctionModVars >( mod ) )
   if( objf && ( fmod->function() == objf ) )
    continue;
  if( auto smod = std::dynamic_pointer_cast< const SATBlockMod >( mod ) ) {
   if( ( smod->type() == SATBlockMod::eAddClauses ) ||
       ( smod->type() == SATBlockMod::eChgCost ) ||
       ( smod->type() == SATBlockMod::eChgWeight ) )
    continue;
   }
  f_reload = true;
  }
 }

/*--------------------------------------------------------------------------*/

std::vector< unsigned char > SATSolver::hard_clauses( void ) const
{
 // the hard clauses of the physical representation, but those whose row is
 // relaxed, if the abstract representation is there; a tautology is
 // harmless
 const auto & sat = std::as_const( *f_sat );
 std::vector< unsigned char > hard( sat.get_clauses().size() , 0 );
 for( unsigned int i = 0 ; i < hard.size() ; ++i )
  if( sat.is_hard( i ) )
   hard[ i ] = 1;
 unsigned int i = 0;
 for( const auto & c : sat.get_clause_constraints() )
  if( ( i < hard.size() ) && c.is_relaxed() )
   hard[ i++ ] = 0;
  else
   ++i;
 for( const auto & c : sat.get_added_clause_constraints() )
  if( ( i < hard.size() ) && c.is_relaxed() )
   hard[ i++ ] = 0;
  else
   ++i;
 return( hard );
 }

/*--------------------------------------------------------------------------*/

void SATSolver::load_clauses( void )
{
 sat_new();
 f_has_sat = true;

 v_hard = hard_clauses();
 const auto & clauses = f_sat->get_clauses();
 for( unsigned int i = 0 ; i < clauses.size() ; ++i )
  if( v_hard[ i ] )
   sat_clause( clauses[ i ] );

 // what OLL has made goes with the SAT solver it was made in
 v_soft.clear();
 v_tot.clear();
 v_cores.clear();
 f_next_var = int( f_sat->get_number_variables() );

 f_reload = false;
 }

/*--------------------------------------------------------------------------*/

void SATSolver::sync_clauses( void )
{
 const auto hard = hard_clauses();

 // a clause cannot be taken away from the SAT solver
 if( hard.size() < v_hard.size() ) {
  load_clauses();
  return;
  }
 for( unsigned int i = 0 ; i < v_hard.size() ; ++i )
  if( v_hard[ i ] && ( ! hard[ i ] ) ) {
   load_clauses();
   return;
   }

 const auto & clauses = f_sat->get_clauses();
 for( unsigned int i = 0 ; i < hard.size() ; ++i )
  if( hard[ i ] && ( ( i >= v_hard.size() ) || ( ! v_hard[ i ] ) ) )
   sat_clause( clauses[ i ] );
 v_hard = hard;
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
 else
  sync_clauses();

 // the fixed ColVariable x, if they exist, are assumptions
 const auto & x = std::as_const( *f_sat ).get_variables();
 std::vector< int > assumptions;
 for( unsigned int i = 0 ; i < x.size() ; ++i )
  if( x[ i ].is_fixed() )
   assumptions.push_back( ( x[ i ].get_value() > 0.5 ) ? int( i + 1 )
			                     : - int( i + 1 ) );

 v_failed.assign( f_sat->get_number_variables() , 0 );
 f_ub = Inf< double >();
 v_model.clear();
 f_iter_stop = false;

 int res;
 if( MaxSATAlg == 1 )
  res = oll( assumptions );
 else {
  res = sat_solve( assumptions );
  // the weights are not negative, the costs may be
  double lb = 0;
  for( auto c : f_sat->get_costs() )
   lb += std::min( c , 0.0 );
  f_lb = ( res == 10 ? lb : ( res == 20 ? Inf< double >() :
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
   // the weight of the soft clauses the solution violates, plus the costs
   // of its true variables
   const auto & clauses = f_sat->get_clauses();
   const auto & w = f_sat->get_weights();
   const auto & c = f_sat->get_costs();
   f_ub = 0;
   for( unsigned int i = 0 ; i < clauses.size() ; ++i )
    if( ( ! f_sat->is_hard( i ) ) &&
	std::none_of( clauses[ i ].begin() , clauses[ i ].end() ,
		      [ this ]( int lit ) {
	 return( sat_value( std::abs( lit ) ) == ( lit > 0 ) ); } ) )
     f_ub += w[ i ];
   for( unsigned int i = 0 ; i < c.size() ; ++i )
    if( ( c[ i ] != 0 ) && sat_value( int( i + 1 ) ) )
     f_ub += c[ i ];
   break;
   }
  case( 20 ):
   f_status = kInfeasible;
   break;
  default:
   // the budget of intMaxIter ends the relaxation of the cores found so far
   // [see the comments to the class]
   f_status = f_iter_stop ? int( kOK ) :
	      ( time_is_up() ? int( kStopTime ) : int( kError ) );
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

void SATSolver::reduce_core( const std::vector< int > & fixed ,
			     std::vector< int > & core ,
			     std::vector< int > & cond )
{
 std::vector< int > as;
 std::vector< int > smaller;
 // the fixed variables in the reason of the last answer
 auto add_cond = [ & ]( void ) {
  for( auto lit : fixed )
   if( sat_failed( lit ) )
    cond.push_back( lit );
  };

 // trimming: the core alone, until it stops shrinking
 for( int t = 0 ; ( t < CoreTrim ) && ( core.size() > 1 ) ; ++t ) {
  as = fixed;
  as.insert( as.end() , core.begin() , core.end() );
  if( sat_solve( as ) != 20 )  // the time is up
   return;
  add_cond();
  smaller.clear();
  for( auto lit : core )
   if( sat_failed( lit ) )
    smaller.push_back( lit );
  if( smaller.empty() || ( smaller.size() == core.size() ) )
   break;
  core.swap( smaller );
  }

 // minimization: each assumption out in turn, within the budget
 if( CoreMinBudget <= 0 )
  return;
 for( std::size_t i = 0 ; ( i < core.size() ) && ( core.size() > 1 ) ; ) {
  if( time_is_up() )
   return;
  as = fixed;
  for( std::size_t j = 0 ; j < core.size() ; ++j )
   if( j != i )
    as.push_back( core[ j ] );
  if( sat_solve( as , CoreMinBudget ) == 20 ) {
   add_cond();
   core.erase( core.begin() + i );  // the rest is a core
   }
  else
   ++i;
  }
 }

/*--------------------------------------------------------------------------*/

int SATSolver::oll( const std::vector< int > & fixed )
{
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

 // the soft clauses with the weights of now, the literal of each being made
 // once for all the compute() of the same SAT solver
 f_lb = 0;
 const auto & clauses = f_sat->get_clauses();
 const auto & weights = f_sat->get_weights();
 if( v_soft.size() < clauses.size() )
  v_soft.resize( clauses.size() , 0 );
 for( unsigned int i = 0 ; i < clauses.size() ; ++i ) {
  if( f_sat->is_hard( i ) || ( weights[ i ] == 0 ) ||
      f_sat->is_tautology( i ) )
   continue;
  if( clauses[ i ].empty() ) {  // violated whatever
   f_lb += weights[ i ];
   continue;
   }
  if( ! v_soft[ i ] ) {
   if( clauses[ i ].size() == 1 )
    v_soft[ i ] = clauses[ i ][ 0 ];
   else {
    auto clause = clauses[ i ];
    clause.push_back( ++f_next_var );
    sat_clause( clause );
    v_soft[ i ] = - f_next_var;
    }
   }
  add_soft( v_soft[ i ] , weights[ i ] , -1 , 0 );
  }

 // the costs of the variables: c_i > 0 is the unit soft clause "not x_i" of
 // weight c_i, c_i < 0 the unit soft clause "x_i" of weight - c_i with c_i
 // paid anyway
 const auto & costs = f_sat->get_costs();
 for( unsigned int i = 0 ; i < costs.size() ; ++i )
  if( costs[ i ] > 0 )
   add_soft( - int( i + 1 ) , costs[ i ] , -1 , 0 );
  else
   if( costs[ i ] < 0 ) {
    add_soft( int( i + 1 ) , - costs[ i ] , -1 , 0 );
    f_lb += costs[ i ];
    }

 // a core: its assumptions have their weight lowered by the smallest one
 // among them, which goes to the lower bound and to the assumption "at most
 // one of them is violated" of the totalizer root, if any; an assumption
 // "at most k" among them makes "at most k + 1" with that weight
 auto relax = [ & ]( const std::vector< int > & core , int root ) {
  double wmin = Inf< double >();
  for( auto lit : core ) {
   const auto it = soft.find( lit );
   wmin = std::min( wmin , it == soft.end() ? 0.0 : it->second.w );
   }
  if( ! ( wmin > 0 ) )
   return;
  f_lb += wmin;
  for( auto lit : core ) {
   auto & sl = soft[ lit ];
   sl.w -= wmin;
   if( sl.root >= 0 ) {
    const auto troot = sl.root;
    const auto bnd = sl.bound + 1;
    if( bnd < v_tot[ troot ].size ) {
     tot_extend( troot , bnd + 1 );
     add_soft( - v_tot[ troot ].out[ bnd ] , wmin , troot , bnd );
     }
    }
   }
  if( root >= 0 ) {
   tot_extend( root , 2 );
   add_soft( - v_tot[ root ].out[ 1 ] , wmin , root , 1 );
   }
  };

 // the cores of the previous compute() with the same SAT solver: a core
 // depends on the hard clauses, which have only grown since, and on the
 // fixed variables in its reason, but not on the weights, so each of those
 // whose fixed variables are still fixed so is relaxed again, in the same
 // order, with the weights of now
 {
  std::vector< int > sfixed( fixed );
  std::sort( sfixed.begin() , sfixed.end() );
  for( const auto & core : v_cores )
   if( std::includes( sfixed.begin() , sfixed.end() ,
		      core.cond.begin() , core.cond.end() ) )
    relax( core.lits , core.root );
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

 // the weight of the soft clauses the current solution violates, plus the
 // costs of its true variables
 auto model_cost = [ & ]( void ) {
  double cost = 0;
  for( unsigned int i = 0 ; i < clauses.size() ; ++i )
   if( ( ! f_sat->is_hard( i ) ) &&
       std::none_of( clauses[ i ].begin() , clauses[ i ].end() ,
		     [ this ]( int lit ) {
	return( sat_value( std::abs( lit ) ) == ( lit > 0 ) ); } ) )
    cost += weights[ i ];
  for( unsigned int i = 0 ; i < costs.size() ; ++i )
   if( ( costs[ i ] != 0 ) && sat_value( int( i + 1 ) ) )
    cost += costs[ i ];
  return( cost );
  };

 lower_level();

 std::vector< int > as;
 std::vector< int > core;
 std::vector< int > cond;
 // the budget of intMaxIter, but for a node whose x are all fixed, which
 // has nothing left to branch on
 const int budget = ( fixed.size() >= f_sat->get_number_variables() ) ?
		    Inf< int >() : MaxIter;
 for( int iter = 0 ; ; ++iter ) {
  if( iter >= budget ) {
   f_iter_stop = true;
   return( 0 );
   }
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

  // the core: the soft assumptions in the reason, and the fixed variables
  // in it, which it holds under
  core.clear();
  for( auto it = as.begin() + fixed.size() ; it != as.end() ; ++it )
   if( sat_failed( *it ) )
    core.push_back( *it );
  cond.clear();
  for( auto lit : fixed )
   if( sat_failed( lit ) )
    cond.push_back( lit );

  if( core.empty() ) {
   // the hard clauses and the fixed variables are unsatisfiable, the
   // clauses OLL adds being implied by the hard ones or only defining new
   // variables
   f_lb = Inf< double >();
   for( auto lit : fixed )
    v_failed[ std::abs( lit ) - 1 ] = sat_failed( lit ) ? 1 : 0;
   return( 20 );
   }

  reduce_core( fixed , core , cond );
  std::sort( cond.begin() , cond.end() );
  cond.erase( std::unique( cond.begin() , cond.end() ) , cond.end() );

  // "at most one of the core is violated", or the only assumption of the
  // core never holds, which the hard clauses imply if no fixed variable is
  // in the reason
  int root = -1;
  if( core.size() > 1 ) {
   std::vector< int > viol;  // the literals true if an assumption is not
   viol.reserve( core.size() );
   for( auto lit : core )
    viol.push_back( - lit );
   root = tot_build( viol , 0 , viol.size() );
   }
  else
   if( cond.empty() )
    sat_clause( { - core[ 0 ] } );

  v_cores.push_back( { core , cond , root } );
  relax( core , root );
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
 return( ( ( f_status == kOK ) || ( f_status == kStopTime ) ) ?
	 OFValue( f_ub ) : Inf< OFValue >() );
 }

/*--------------------------------------------------------------------------*/

void SATSolver::get_var_solution( Configuration * solc )
{
 if( ! has_var_solution() )
  throw( std::logic_error( "SATSolver::get_var_solution: no solution" ) );

 f_sat->generate_abstract_variables();
 auto & x = f_sat->get_variables();
 for( unsigned int i = 0 ; i < x.size() ; ++i )
  x[ i ].set_value( ( v_model.empty() ? sat_value( int( i + 1 ) )
		                      : bool( v_model[ i ] ) ) ? 1 : 0 );

 // r is 1 for the soft clauses the solution violates, 0 for all the others
 const auto & clauses = f_sat->get_clauses();
 for( unsigned int i = 0 ; i < clauses.size() ; ++i ) {
  const bool violated = ( ! f_sat->is_hard( i ) ) &&
   std::none_of( clauses[ i ].begin() , clauses[ i ].end() ,
		 [ & x ]( int lit ) {
    return( ( x[ std::abs( lit ) - 1 ].get_value() > 0.5 ) == ( lit > 0 ) );
    } );
  f_sat->get_violation( i ).set_value( violated ? 1 : 0 );
  }
 }

/*--------------------------------------------------------------------------*/

bool SATSolver::is_failed( unsigned int i ) const
{
 return( ( f_status == kInfeasible ) && ( i < v_failed.size() ) &&
	 v_failed[ i ] );
 }

/*--------------------------------------------------------------------------*/
/*------------------- METHODS OF THE RelaxationSolver ----------------------*/
/*--------------------------------------------------------------------------*/

std::vector< Change * > SATSolver::branch( void )
{
 f_sat->generate_abstract_variables();
 const auto & x = std::as_const( *f_sat ).get_variables();
 const auto n = x.size();
 const auto & clauses = f_sat->get_clauses();

 // the clause of each relaxation variable
 std::unordered_map< int , unsigned int > relax_clause;
 for( unsigned int i = 0 ; i < v_soft.size() ; ++i )
  if( std::abs( v_soft[ i ] ) > int( n ) )
   relax_clause[ std::abs( v_soft[ i ] ) ] = i;

 // the score of each x: the soft assumptions of the cores it is in, a
 // relaxation variable shared among the variables of its clause; the
 // outputs of the totalizers count for nothing
 std::vector< double > score( n , 0 );
 for( const auto & core : v_cores )
  for( auto lit : core.lits ) {
   const auto v = std::abs( lit );
   if( v <= int( n ) )
    score[ v - 1 ] += 1;
   else {
    const auto it = relax_clause.find( v );
    if( it != relax_clause.end() ) {
     const auto & cl = clauses[ it->second ];
     for( auto l : cl )
      score[ std::abs( l ) - 1 ] += 1.0 / double( cl.size() );
     }
    }
   }

 unsigned int best = n;
 for( unsigned int i = 0 ; i < n ; ++i )
  if( ( ! x[ i ].is_fixed() ) &&
      ( ( best == n ) || ( score[ i ] > score[ best ] ) ) )
   best = i;
 if( best == n )
  throw( std::logic_error( "SATSolver::branch: all the variables are "
			   "fixed" ) );

 // the value of the best solution first
 const double first = ( ( best < v_model.size() ) && v_model[ best ] ) ? 1 : 0;
 return( std::vector< Change * >{
	   new SATBlockChange( SATBlockChange::eFixX , Block::Subset{ best } ,
			       std::vector< double >{ first } ) ,
	   new SATBlockChange( SATBlockChange::eFixX , Block::Subset{ best } ,
			       std::vector< double >{ 1 - first } ) } );
 }

/*--------------------------------------------------------------------------*/

int SATSolver::classify( const sp_Mod & mod )
{
 if( auto smod = std::dynamic_pointer_cast< const SATBlockMod >( mod ) )
  if( smod->type() == SATBlockMod::eChgCost )
   return( eModObjective );
 return( eModEverything );
 }

/*--------------------------------------------------------------------------*/
/*------------------------- End File SATSolver.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
