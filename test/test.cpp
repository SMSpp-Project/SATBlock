/*--------------------------------------------------------------------------*/
/*------------------------------ File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Tests for SATBlock: the factory, the DIMACS reader on hand-made cases and
 * on an instance of the SATLIB collection, the abstract representation,
 * the feasibility check, the Solution, and the round trips through netCDF
 * and through the DIMACS writer.
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
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include "BooleanVariableSolution.h"
#include "SATBlock.h"
#if defined( SATBLOCK_HAS_CADICAL ) || defined( SATBLOCK_HAS_MINISAT )
 #define SATBLOCK_HAS_SATSOLVER
 #include "SATSolver.h"
 #include <filesystem>
#endif

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

/// loads a SATBlock out of a string in the DIMACS CNF format

static void load_string( SATBlock & b , const std::string & text )
{
 std::istringstream in( text );
 b.load( in );
 }

/*--------------------------------------------------------------------------*/
/// true if loading the given text throws std::invalid_argument

static bool load_throws( const std::string & text )
{
 SATBlock b;
 try {
  load_string( b , text );
  }
 catch( std::invalid_argument & ) {
  return( true );
  }
 return( false );
 }

/*--------------------------------------------------------------------------*/
/// sets the BooleanVariable of b to the bits of mask

static void set_values( SATBlock & b , unsigned long mask )
{
 auto & x = b.get_variables();
 for( unsigned int i = 0 ; i < x.size() ; ++i )
  x[ i ].set_value( ( mask >> i ) & 1 );
 }

/*--------------------------------------------------------------------------*/

static void test_factory( void )
{
 auto block = Block::new_Block( "SATBlock" );
 assert( block && dynamic_cast< SATBlock * >( block ) );
 delete block;
 }

/*--------------------------------------------------------------------------*/

static void test_load( void )
{
 // comments anywhere, a clause over two lines, a repeated literal, a
 // tautology and an empty clause
 SATBlock b;
 load_string( b , "c a comment\n"
		  "p cnf 3 5\n"
		  "1 -2 0\n"
		  "c another comment\n"
		  "2 3\n"
		  "  -1 0 3 3 -2 0\n"
		  "1 -1 2 0\n"
		  "0\n" );
 assert( b.get_number_variables() == 3 );
 assert( b.get_number_clauses() == 5 );
 const auto & c = b.get_clauses();
 assert( ( c[ 0 ] == SATBlock::Clause{ 1 , -2 } ) &&
	 ( c[ 1 ] == SATBlock::Clause{ 2 , 3 , -1 } ) &&
	 ( c[ 2 ] == SATBlock::Clause{ 3 , -2 } ) &&
	 ( c[ 3 ] == SATBlock::Clause{ 1 , -1 , 2 } ) && c[ 4 ].empty() );
 assert( b.is_tautology( 3 ) && ! b.is_tautology( 0 ) );

 // reading stops at the m-th clause and at a line beginning with '%'
 SATBlock s;
 load_string( s , "p cnf 2 1\n1 2 0\n-1 -2 0\n" );
 assert( s.get_number_clauses() == 1 );
 assert( load_throws( "p cnf 2 2\n1 2 0\n%\n-1 0\n" ) );

 // the errors
 assert( load_throws( "1 2 0\n" ) );                   // no header
 assert( load_throws( "p dnf 2 1\n1 2 0\n" ) );        // not cnf
 assert( load_throws( "p cnf 2 1\n1 3 0\n" ) );        // out of range
 assert( load_throws( "p cnf 2 2\n1 2 0\n" ) );        // too few clauses
 assert( load_throws( "p cnf 2 1\n1 x 0\n" ) );        // not a literal

 // loading again replaces everything
 load_string( s , "p cnf 4 2\n1 2 3 4 0\n-4 0\n" );
 assert( ( s.get_number_variables() == 4 ) &&
	 ( s.get_number_clauses() == 2 ) );
 }

/*--------------------------------------------------------------------------*/

static void test_abstract( void )
{
 SATBlock b;
 load_string( b , "p cnf 3 4\n1 -2 0\n2 3 0\n1 -1 0\n-3 0\n" );
 b.generate_abstract_constraints();

 const auto & x = b.get_variables();
 const auto & c = b.get_clause_constraints();
 assert( ( x.size() == 3 ) && ( c.size() == 4 ) );
 assert( b.get_static_variable_groups().size() == 1 );
 assert( b.get_static_constraint_groups().size() == 1 );

 // clause 0 is x0 or not x1
 assert( c[ 0 ].get_num_active_var() == 2 );
 assert( ( c[ 0 ].get_literals()[ 0 ] ==
	   ClauseConstraint::Literal( const_cast< BooleanVariable * >(
					       & x[ 0 ] ) , false ) ) &&
	 ( c[ 0 ].get_literals()[ 1 ] ==
	   ClauseConstraint::Literal( const_cast< BooleanVariable * >(
					       & x[ 1 ] ) , true ) ) );
 assert( x[ 1 ].get_num_active() == 2 );

 // the tautology is relaxed and has no literals
 assert( c[ 2 ].is_relaxed() && ( c[ 2 ].get_num_active_var() == 0 ) );

 // the ClauseConstraint agree with is_feasible() on every assignment
 for( unsigned long mask = 0 ; mask < 8 ; ++mask ) {
  set_values( b , mask );
  bool all = true;
  for( auto & cc : b.get_clause_constraints() ) {
   cc.compute();
   all = all && cc.feasible();
   }
  assert( b.is_feasible() == all );
  }
 // x0 = true, x1 = true, x2 = false is the only assignment satisfying all
 for( unsigned long mask = 0 ; mask < 8 ; ++mask ) {
  set_values( b , mask );
  assert( b.is_feasible() == ( mask == 3 ) );
  }
 }

/*--------------------------------------------------------------------------*/

static void test_solution( void )
{
 SATBlock b;
 load_string( b , "p cnf 3 1\n1 2 3 0\n" );
 b.generate_abstract_variables();
 set_values( b , 5 );

 auto sol = b.get_Solution( nullptr , false );
 assert( dynamic_cast< BooleanVariableSolution * >( sol ) );
 set_values( b , 0 );
 sol->write( & b );
 assert( b.get_variables()[ 0 ].get_value() &&
	 ( ! b.get_variables()[ 1 ].get_value() ) &&
	 b.get_variables()[ 2 ].get_value() );
 delete sol;
 }

/*--------------------------------------------------------------------------*/

static void test_round_trips( void )
{
 SATBlock b;
 load_string( b , "p cnf 4 4\n1 -2 0\n2 3 -4 0\n4 0\n-1 -3 0\n" );

 // netCDF
 const char * file = "SATBlock_test.nc4";
 {
  netCDF::NcFile f( file , netCDF::NcFile::replace );
  auto g = f.addGroup( "Block" );
  b.serialize( g );
  }
 {
  netCDF::NcFile f( file , netCDF::NcFile::read );
  auto d = Block::new_Block( f.getGroup( "Block" ) );
  auto s = dynamic_cast< SATBlock * >( d );
  assert( s && ( s->get_number_variables() == 4 ) &&
	  ( s->get_clauses() == b.get_clauses() ) );
  delete d;
  }
 std::remove( file );

 // the DIMACS writer and the reader
 SATBlock p;
 std::ostringstream full;
 b.print( full , 'C' );
 load_string( p , full.str() );
 assert( ( p.get_number_variables() == 4 ) &&
	 ( p.get_clauses() == b.get_clauses() ) );
 }

/*--------------------------------------------------------------------------*/
/// an instance of SATLIB: satisfiable, found by enumeration

static void test_satlib( void )
{
 std::ifstream in( "../data/cnf/uf20-91/uf20-07.cnf" );
 assert( in );
 SATBlock b;
 b.load( in );
 assert( ( b.get_number_variables() == 20 ) &&
	 ( b.get_number_clauses() == 91 ) );
 b.generate_abstract_variables();

 // the uf instances are satisfiable: look for a solution among the 2^20
 // assignments, on the clauses as bit masks
 std::vector< std::pair< unsigned long , unsigned long > > cl;
 for( const auto & clause : b.get_clauses() ) {
  unsigned long pos = 0 , neg = 0;
  for( auto lit : clause )
   ( lit > 0 ? pos : neg ) |= 1ul << ( std::abs( lit ) - 1 );
  cl.emplace_back( pos , neg );
  }
 unsigned long found = 1ul << 20;
 for( unsigned long mask = 0 ; mask < ( 1ul << 20 ) ; ++mask )
  if( std::all_of( cl.begin() , cl.end() , [ mask ]( const auto & c ) {
       return( ( mask & c.first ) || ( ~mask & c.second ) ); } ) ) {
   found = mask;
   break;
   }
 assert( found < ( 1ul << 20 ) );

 set_values( b , found );
 assert( b.is_feasible() );
 // flipping each variable of the solution is checked the same way
 for( unsigned int i = 0 ; i < 20 ; ++i ) {
  const unsigned long flip = found ^ ( 1ul << i );
  set_values( b , flip );
  const bool sat = std::all_of( cl.begin() , cl.end() ,
				[ flip ]( const auto & c ) {
    return( ( flip & c.first ) || ( ~flip & c.second ) ); } );
  assert( b.is_feasible() == sat );
  }
 }

/*--------------------------------------------------------------------------*/

#ifdef SATBLOCK_HAS_SATSOLVER

/// the name of the SATSolver the tests below are run with

static std::string solver_name;

/// solves b with a SATSolver registered to it, returning the status

static int solve( SATBlock & b , SATSolver * & s )
{
 if( ! s ) {
  s = dynamic_cast< SATSolver * >( Solver::new_Solver( solver_name ) );
  assert( s );
  b.register_Solver( s );
  }
 return( s->compute() );
 }

/*--------------------------------------------------------------------------*/

static void test_solver( void )
{
 // satisfiable: the only solution is x0 = x1 = true, x2 = false
 {
  SATBlock b;
  load_string( b , "p cnf 3 4\n1 -2 0\n2 3 0\n1 -1 0\n-3 0\n" );
  SATSolver * s = nullptr;
  assert( solve( b , s ) == Solver::kOK );
  assert( ( s->get_lb() == 0 ) && ( s->get_ub() == 0 ) );
  s->get_var_solution();
  const auto & x = std::as_const( b ).get_variables();
  assert( x[ 0 ].get_value() && x[ 1 ].get_value() && ! x[ 2 ].get_value() );
  assert( b.is_feasible() );
  b.unregister_Solvers( true );
  }

 // unsatisfiable
 {
  SATBlock b;
  load_string( b , "p cnf 1 2\n1 0\n-1 0\n" );
  SATSolver * s = nullptr;
  assert( solve( b , s ) == Solver::kInfeasible );
  assert( ( s->get_lb() == Inf< Solver::OFValue >() ) &&
	  ( s->get_ub() == Inf< Solver::OFValue >() ) );
  assert( ! s->has_var_solution() );

  // relaxing the clause "not x0" makes it satisfiable: the ConstraintMod
  // makes the SATSolver give the clauses again
  b.generate_abstract_constraints();
  b.get_clause_constraints()[ 1 ].relax( true );
  assert( s->compute() == Solver::kOK );
  s->get_var_solution();
  assert( b.get_variables()[ 0 ].get_value() );
  b.unregister_Solvers( true );
  }

 // assumptions: x0 or x1, with both fixed to false
 {
  SATBlock b;
  load_string( b , "p cnf 3 1\n1 2 0\n" );
  b.generate_abstract_variables();
  auto & x = b.get_variables();
  SATSolver * s = nullptr;
  x[ 0 ].set_value( false ); x[ 0 ].is_fixed( true );
  x[ 1 ].set_value( false ); x[ 1 ].is_fixed( true );
  x[ 2 ].set_value( true );  x[ 2 ].is_fixed( true );
  assert( solve( b , s ) == Solver::kInfeasible );
  assert( s->is_failed( 0 ) && s->is_failed( 1 ) && ! s->is_failed( 2 ) );
  // unfixing x1 is enough, and the assumptions hold for one compute() only
  x[ 1 ].is_fixed( false );
  assert( s->compute() == Solver::kOK );
  s->get_var_solution();
  assert( ( ! x[ 0 ].get_value() ) && x[ 1 ].get_value() &&
	  x[ 2 ].get_value() );
  b.unregister_Solvers( true );
  }
 }

/*--------------------------------------------------------------------------*/
/// the SATLIB families whose satisfiability is known by their name

static void test_solver_satlib( void )
{
 namespace fs = std::filesystem;
 struct Family { const char * dir; unsigned n; };
 const Family families[] = { { "uf50-218" , 50 } , { "uuf50-218" , 50 } ,
			     { "uf100-430" , 20 } , { "uuf100-430" , 20 } ,
			     { "uf250-1065" , 2 } , { "uuf250-1065" , 2 } ,
			     { "aim" , 72 } };
 unsigned solved = 0;
 for( const auto & fam : families ) {
  std::vector< fs::path > files;
  for( const auto & e : fs::directory_iterator( std::string(
					 "../data/cnf/" ) + fam.dir ) )
   files.push_back( e.path() );
  std::sort( files.begin() , files.end() );
  if( files.size() > fam.n )
   files.resize( fam.n );

  for( const auto & f : files ) {
   const std::string name = f.filename().string();
   const bool expect_sat = ( name.rfind( "uuf" , 0 ) != 0 ) &&
			   ( name.find( "-no-" ) == std::string::npos );
   std::ifstream in( f );
   SATBlock b;
   b.load( in );
   SATSolver * s = nullptr;
   const int status = solve( b , s );
   if( expect_sat ) {
    assert( status == Solver::kOK );
    s->get_var_solution();
    assert( b.is_feasible() );
    }
   else
    assert( status == Solver::kInfeasible );
   b.unregister_Solvers( true );
   ++solved;
   }
  }
 std::cout << solver_name << ": " << solved
	   << " SATLIB instances as expected" << std::endl;
 }

#endif

/*--------------------------------------------------------------------------*/
/*-------------------------------- main() ----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 test_factory();
 test_load();
 test_abstract();
 test_solution();
 test_round_trips();
 test_satlib();
#ifdef SATBLOCK_HAS_CADICAL
 solver_name = "CaDiCaLSATSolver";
 test_solver();
 test_solver_satlib();
#endif
#ifdef SATBLOCK_HAS_MINISAT
 solver_name = "MiniSatSATSolver";
 test_solver();
 test_solver_satlib();
#endif

 std::cout << "SATBlock: all tests passed" << std::endl;

 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------------- End File test.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
