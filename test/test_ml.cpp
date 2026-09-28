/*--------------------------------------------------------------------------*/
/*--------------------------- File test_ml.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Unit test of SATBlockML: the GQSATBranchRule, with a policy written here
 * as a TorchScript module whose choice is known, and the branch and bound
 * of test.cpp with that rule against the enumeration. If the environment
 * variable SATBLOCK_GQSAT_MODEL names the file of a policy learned by
 * Graph-Q-SAT, the branch and bound is run with that policy too.
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
#include <iostream>
#include <sstream>

#include "GQSATBranchRule.h"
#include "SATBlock.h"

// the checks hold in every build type, Release included
#undef NDEBUG
#include <cassert>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/
/// a policy whose choice is known: the Q-values of vertex i are i and
/// i - 1/2, so that the last variable is chosen, and "true" first

static torch::jit::Module known_policy( void )
{
 torch::jit::Module m( "KnownPolicy" );
 m.define( R"(
def forward(self, x, edge_index, edge_attr, u):
    i = torch.cumsum(torch.ones_like(x[:, 0:1]), dim=0) - 1.0
    return torch.cat([i, i - 0.5], dim=1)
)" );
 return( m );
 }

/*--------------------------------------------------------------------------*/
/// a depth-first branch and bound on the SATSolver as a RelaxationSolver,
/// as in test.cpp

static double dive( SATSolver * s , double incumbent , unsigned & nodes )
{
 ++nodes;
 const int status = s->compute();
 if( status == Solver::kInfeasible )
  return( incumbent );
 assert( status == Solver::kOK );
 if( s->has_true_var_solution() )
  incumbent = std::min( incumbent , double( s->get_true_ub() ) );
 if( s->get_lb() >= incumbent )
  return( incumbent );
 for( auto chg : s->branch() ) {
  auto undo = s->apply( chg , true );
  incumbent = dive( s , incumbent , nodes );
  s->apply( undo );
  delete undo;
  delete chg;
  }
 return( incumbent );
 }

/*--------------------------------------------------------------------------*/
/// the name of a SATSolver of this build

static std::string solver_name( void )
{
 for( const char * name : { "CaDiCaLSATSolver" , "MiniSATSolver" } )
  try {
   delete Solver::new_Solver( name );
   return( name );
   }
  catch( ... ) {}
 return( "" );
 }

/*--------------------------------------------------------------------------*/
/// the branch and bound with the rule of the given name on random
/// instances, against the enumeration; returns the nodes

static unsigned branch_and_bound( const std::string & rule ,
				  const std::string & file , unsigned seed )
{
 const auto inf = Inf< double >();
 const std::string name = solver_name();
 std::srand( seed );
 unsigned nodes = 0;
 for( unsigned t = 0 ; t < 60 ; ++t ) {
  const unsigned n = 4 + std::rand() % 7;          // 4 to 10 variables
  const unsigned m = n + std::rand() % ( 3 * n );
  SATBlock::v_Clause clauses( m );
  SATBlock::v_Weight weights( m );
  for( unsigned c = 0 ; c < m ; ++c ) {
   const unsigned len = 1 + std::rand() % 3;
   for( unsigned l = 0 ; l < len ; ++l )
    clauses[ c ].push_back( int( 1 + std::rand() % n ) *
			    ( std::rand() % 2 ? 1 : -1 ) );
   weights[ c ] = ( std::rand() % 4 == 0 ) ? inf : 1 + std::rand() % 20;
   }
  SATBlock b;
  b.load( n , std::move( clauses ) , std::move( weights ) );
  b.generate_abstract_variables();
  auto & x = b.get_variables();

  double best = inf;
  for( unsigned long mask = 0 ; mask < ( 1ul << n ) ; ++mask ) {
   for( unsigned i = 0 ; i < n ; ++i )
    x[ i ].set_value( ( mask >> i ) & 1 );
   if( b.is_feasible() )
    best = std::min( best , b.get_objective_value() );
   }

  auto s = dynamic_cast< SATSolver * >( Solver::new_Solver( name ) );
  s->set_par( SATSolver::intMaxSAT , 1 );
  s->set_par( Solver::intMaxIter , 2 );
  if( ! file.empty() )
   s->set_par( SATSolver::strBranchRuleFile , file );
  s->set_par( SATSolver::strBranchRule , rule );
  b.register_Solver( s );
  const double found = dive( s , inf , nodes );
  assert( found == best );
  b.unregister_Solvers( true );
  }
 return( nodes );
 }

/*--------------------------------------------------------------------------*/
/*-------------------------------- main() ----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( void )
{
 if( solver_name().empty() ) {
  std::cout << "SATBlockML: no SATSolver in this build" << std::endl;
  return( 77 );
  }

 // the known policy, as a rule of its own name
 SATBranchRule::add( "GQSAT-known" , []( void ) -> SATBranchRule * {
  auto rule = new GQSATBranchRule();
  rule->set_policy( known_policy() );
  return( rule );
  } );

 // the choice: the last unfixed variable of a clause not satisfied, true
 {
  SATBlock b;
  std::istringstream in( "p cnf 4 2\n1 2 0\n-3 1 0\n" );
  b.load( in );
  b.generate_abstract_variables();
  auto & x = b.get_variables();
  std::unique_ptr< SATBranchRule > rule(
				     SATBranchRule::make( "GQSAT-known" ) );
  unsigned int var = 99;
  double first = -1;
  assert( rule->choose( b , var , first ) && ( var == 3 ) && ( first == 1 ) );
  // x3 is in no clause, but it is an unfixed variable all the same; fixing
  // x0 to true satisfies both clauses, and nothing is left to say
  x[ 3 ].set_value( 0 ); x[ 3 ].is_fixed( true );
  assert( rule->choose( b , var , first ) && ( var == 2 ) );
  x[ 0 ].set_value( 1 ); x[ 0 ].is_fixed( true );
  assert( ! rule->choose( b , var , first ) );
  // no policy read: nothing to say
  GQSATBranchRule empty;
  assert( ! empty.choose( b , var , first ) );
  }

 // an unknown rule
 {
  auto s = dynamic_cast< SATSolver * >( Solver::new_Solver( solver_name() ) );
  bool threw = false;
  try {
   s->set_par( SATSolver::strBranchRule , std::string( "NoSuchRule" ) );
   }
  catch( std::invalid_argument & ) { threw = true; }
  assert( threw );
  delete s;
  }

 const unsigned cores = branch_and_bound( "" , "" , 31 );
 std::cout << "SATBlockML: branch and bound optimal on 60 random instances "
	   << "with the rule of the cores, " << cores << " nodes" << std::endl;
 const unsigned known = branch_and_bound( "GQSAT-known" , "" , 31 );
 std::cout << "SATBlockML: branch and bound optimal on 60 random instances "
	   << "with the known policy, " << known << " nodes" << std::endl;

 if( const char * model = std::getenv( "SATBLOCK_GQSAT_MODEL" ) ) {
  const unsigned learned = branch_and_bound( "GQSAT" , model , 31 );
  std::cout << "SATBlockML: branch and bound optimal on 60 random instances "
	    << "with the policy of " << model << ", " << learned << " nodes"
	    << std::endl;
  }

 std::cout << "SATBlockML: all tests passed" << std::endl;
 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*--------------------------- End File test_ml.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
