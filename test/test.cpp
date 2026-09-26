/*--------------------------------------------------------------------------*/
/*------------------------------ File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Tests for SATBlock: the factory, the DIMACS CNF and WCNF readers on
 * hand-made cases and on an instance of the SATLIB collection, the abstract
 * representation, the feasibility check and the violated weight, the
 * Solution, the round trips through netCDF and through the writer, the
 * changes of the weights and the added clauses, and the SATSolver.
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

/// loads a SATBlock out of a string in the given format

static void load_string( SATBlock & b , const std::string & text ,
			 char frmt = 0 )
{
 std::istringstream in( text );
 b.load( in , frmt );
 }

/*--------------------------------------------------------------------------*/
/// true if loading the given text throws std::invalid_argument

static bool load_throws( const std::string & text , char frmt = 0 )
{
 SATBlock b;
 try {
  load_string( b , text , frmt );
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
 assert( load_throws( "1 2 0\n" , 'D' ) );              // no header
 assert( load_throws( "p dnf 2 1\n1 2 0\n" ) );        // not cnf
 assert( load_throws( "p cnf 2 1\n1 3 0\n" ) );        // out of range
 assert( load_throws( "p cnf 2 2\n1 2 0\n" ) );        // too few clauses
 assert( load_throws( "p cnf 2 1\n1 x 0\n" ) );        // not a literal

 // loading again replaces everything
 load_string( s , "p cnf 4 2\n1 2 3 4 0\n-4 0\n" );
 assert( ( s.get_number_variables() == 4 ) &&
	 ( s.get_number_clauses() == 2 ) );
 assert( s.all_hard() );
 }

/*--------------------------------------------------------------------------*/

static void test_load_wcnf( void )
{
 const auto inf = Inf< double >();

 // up to 2021, with top: weight 10 and more is hard
 SATBlock o;
 load_string( o , "c old format\n"
		  "p wcnf 3 4 10\n"
		  "10 1 -2 0\n"
		  "3 2 0\n"
		  "c a comment\n"
		  "7 -1 3 3 0\n"
		  "12 -3 0\n" );
 assert( ( o.get_number_variables() == 3 ) &&
	 ( o.get_number_clauses() == 4 ) );
 assert( ( o.get_weights() == SATBlock::v_Weight{ inf , 3 , 7 , inf } ) );
 assert( ( o.get_clauses()[ 2 ] == SATBlock::Clause{ -1 , 3 } ) );
 assert( o.is_hard( 0 ) && ! o.is_hard( 1 ) && ! o.all_hard() );

 // up to 2021, without top: all soft
 SATBlock t;
 load_string( t , "p wcnf 2 2\n5 1 0\n1000000 -1 2 0\n" , 'W' );
 assert( ( t.get_weights() == SATBlock::v_Weight{ 5 , 1000000 } ) );

 // from 2022 on: "h" or the weight, the variables are the largest one
 SATBlock n;
 load_string( n , "c new format\n"
		  "h 1 -2 0\n"
		  "4 2 0\n"
		  "\n"
		  "2.5 -5 0\n"
		  "h 0\n" );
 assert( ( n.get_number_variables() == 5 ) &&
	 ( n.get_number_clauses() == 4 ) );
 assert( ( n.get_weights() == SATBlock::v_Weight{ inf , 4 , 2.5 , inf } ) );
 assert( n.get_clauses()[ 3 ].empty() );

 // the errors
 assert( load_throws( "p wcnf 2 1\n1 2 0\n" , 'D' ) );    // not cnf
 assert( load_throws( "p cnf 2 1\n1 2 0\n" , 'W' ) );     // not wcnf
 assert( load_throws( "p wcnf 2 1\n-1 2 0\n" ) );         // negative
 assert( load_throws( "p wcnf 2 1\n1 2\n" ) );            // no 0
 assert( load_throws( "h 1 2\n" ) );                      // no 0
 assert( load_throws( "x 1 0\n" ) );                      // not a weight
 assert( load_throws( "c only comments\n" ) );            // nothing
 SATBlock w;
 bool thrown = false;
 try {
  w.load( 2 , { { 1 } , { 2 } } , { 1 } );                // one weight short
  }
 catch( std::invalid_argument & ) {
  thrown = true;
  }
 assert( thrown );
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

static void test_soft( void )
{
 // hard: x0 or not x1; soft: x1 (weight 3), x2 (weight 2), and a soft
 // tautology
 SATBlock b;
 load_string( b , "h 1 -2 0\n3 2 0\n2 3 0\n1 1 -1 0\n" );
 b.generate_abstract_constraints();

 // the soft clauses are relaxed, with their literals, the tautology without
 const auto & c = b.get_clause_constraints();
 assert( ! c[ 0 ].is_relaxed() );
 assert( c[ 1 ].is_relaxed() && ( c[ 1 ].get_num_active_var() == 1 ) );
 assert( c[ 3 ].is_relaxed() && ( c[ 3 ].get_num_active_var() == 0 ) );

 // feasibility looks at the hard clause only, the weight at the soft ones
 for( unsigned long mask = 0 ; mask < 8 ; ++mask ) {
  set_values( b , mask );
  const bool x0 = mask & 1 , x1 = mask & 2 , x2 = mask & 4;
  assert( b.is_feasible() == ( x0 || ! x1 ) );
  assert( b.get_violated_weight() == ( x1 ? 0 : 3 ) + ( x2 ? 0 : 2 ) );
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
	 ( p.get_clauses() == b.get_clauses() ) && p.all_hard() );

 // the same with weights, one that is not an integer among them
 SATBlock w;
 load_string( w , "h 1 -2 0\n0.1 2 3 -4 0\n7 4 0\nh -1 -3 0\n" );
 {
  netCDF::NcFile f( file , netCDF::NcFile::replace );
  auto g = f.addGroup( "Block" );
  w.serialize( g );
  }
 {
  netCDF::NcFile f( file , netCDF::NcFile::read );
  auto d = Block::new_Block( f.getGroup( "Block" ) );
  auto s = dynamic_cast< SATBlock * >( d );
  assert( s && ( s->get_clauses() == w.get_clauses() ) &&
	  ( s->get_weights() == w.get_weights() ) );
  delete d;
  }
 std::remove( file );

 SATBlock q;
 std::ostringstream wfull;
 w.print( wfull , 'C' );
 load_string( q , wfull.str() , 'W' );
 assert( ( q.get_number_variables() == 4 ) &&
	 ( q.get_clauses() == w.get_clauses() ) &&
	 ( q.get_weights() == w.get_weights() ) );
 }

/*--------------------------------------------------------------------------*/

static void test_modifications( void )
{
 const auto inf = Inf< double >();

 SATBlock b;
 load_string( b , "h 1 2 0\n5 -1 0\n3 -2 0\n" );

 // the physical representation only
 std::vector< double > nw = { 1 , inf };
 b.chg_weights( nw , Block::Range( 1 , 3 ) );
 assert( ( b.get_weights() == SATBlock::v_Weight{ inf , 1 , inf } ) );
 b.add_clauses( { { 2 , -2 } , { -1 , -2 } } , { 4 , inf } );
 assert( ( b.get_number_clauses() == 5 ) &&
	 ( b.get_weights()[ 3 ] == 4 ) && b.is_hard( 4 ) );

 // with the abstract representation: the weights relax and enforce, the
 // clauses added join the dynamic group, in order
 b.generate_abstract_constraints();
 assert( ( b.get_clause_constraints().size() == 5 ) &&
	 b.get_added_clause_constraints().empty() );
 assert( b.get_clause_constraints()[ 1 ].is_relaxed() &&
	 ! b.get_clause_constraints()[ 2 ].is_relaxed() );

 std::vector< double > sw = { 2 , inf };
 b.chg_weights( sw , Block::Subset{ 2 , 1 } );  // not ordered
 assert( ( b.get_weights() == SATBlock::v_Weight{ inf , inf , 2 , 4 , inf } ) );
 assert( ! b.get_clause_constraints()[ 1 ].is_relaxed() &&
	 b.get_clause_constraints()[ 2 ].is_relaxed() );
 // a tautology stays relaxed
 std::vector< double > tw = { inf };
 b.chg_weights( tw , Block::Range( 3 , 4 ) );
 assert( b.get_clause_constraints()[ 3 ].is_relaxed() );

 b.add_clauses( { { 2 } , { 1 , 1 , -2 } } , { 6 , inf } );
 const auto & lc = b.get_added_clause_constraints();
 assert( ( b.get_number_clauses() == 7 ) && ( lc.size() == 2 ) );
 assert( lc.front().is_relaxed() && ( lc.front().get_num_active_var() == 1 ) );
 assert( ( ! lc.back().is_relaxed() ) &&
	 ( lc.back().get_num_active_var() == 2 ) );
 assert( b.get_dynamic_constraint_groups().size() == 1 );

 // wrong input leaves the SATBlock as it was
 bool thrown = false;
 try {
  b.add_clauses( { { 1 } , { 9 } } );             // variable out of range
  }
 catch( std::invalid_argument & ) {
  thrown = true;
  }
 assert( thrown && ( b.get_number_clauses() == 7 ) && ( lc.size() == 2 ) );
 thrown = false;
 std::vector< double > bad = { -1 };
 try {
  b.chg_weights( bad , Block::Range( 0 , 1 ) );
  }
 catch( std::invalid_argument & ) {
  thrown = true;
  }
 assert( thrown && b.is_hard( 0 ) );
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

 // soft clauses are not given to the SAT solver, their weight is the ub;
 // turning one hard gives the clauses again
 {
  SATBlock b;
  load_string( b , "h 1 0\n5 -1 0\n2 2 0\n" );
  SATSolver * s = nullptr;
  assert( solve( b , s ) == Solver::kOK );
  assert( s->get_lb() == 0 );
  s->get_var_solution();
  const auto & x = std::as_const( b ).get_variables();
  assert( x[ 0 ].get_value() );
  assert( s->get_ub() == b.get_violated_weight() );
  assert( ( s->get_ub() == 5 ) || ( s->get_ub() == 7 ) );

  std::vector< double > w = { Inf< double >() };
  b.chg_weights( w , Block::Range( 1 , 2 ) );
  assert( s->compute() == Solver::kInfeasible );
  // and back to soft, now with the abstract representation
  b.generate_abstract_constraints();
  std::vector< double > w1 = { 1 };
  b.chg_weights( w1 , Block::Range( 1 , 2 ) );
  assert( s->compute() == Solver::kOK );
  b.unregister_Solvers( true );
  }

 // clauses added are given on top of those the SAT solver has
 for( bool abstract : { false , true } ) {
  SATBlock b;
  load_string( b , "p cnf 2 1\n1 2 0\n" );
  if( abstract )
   b.generate_abstract_constraints();
  SATSolver * s = nullptr;
  assert( solve( b , s ) == Solver::kOK );
  b.add_clauses( { { -1 } } );
  assert( s->compute() == Solver::kOK );
  s->get_var_solution();
  assert( ( ! b.get_variables()[ 0 ].get_value() ) &&
	  b.get_variables()[ 1 ].get_value() );
  b.add_clauses( { { -2 } , { 1 } } , { 3 , 4 } );  // soft: still SAT
  assert( s->compute() == Solver::kOK );
  assert( s->get_ub() == 7 );
  b.add_clauses( { { -2 } } );
  assert( s->compute() == Solver::kInfeasible );
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
/// OLL against the enumeration of all the assignments, on random instances

static void test_oll( void )
{
 const auto inf = Inf< double >();

 // small cases by hand: an empty soft clause, a unit core, a soft
 // tautology, and infeasible hard clauses
 {
  SATBlock b;
  load_string( b , "h 1 2 0\n3 -1 0\n4 -2 0\n2 0\n5 1 -1 0\n" );
  SATSolver * s = nullptr;
  s = dynamic_cast< SATSolver * >( Solver::new_Solver( solver_name ) );
  s->set_par( SATSolver::intMaxSAT , 1 );
  b.register_Solver( s );
  assert( s->compute() == Solver::kOK );
  assert( ( s->get_lb() == 5 ) && ( s->get_ub() == 5 ) );
  s->get_var_solution();
  assert( b.get_variables()[ 0 ].get_value() &&
	  ! b.get_variables()[ 1 ].get_value() );

  // fixing x0 to false makes x1 true, which costs 4
  auto & x = b.get_variables();
  x[ 0 ].set_value( false ); x[ 0 ].is_fixed( true );
  assert( s->compute() == Solver::kOK );
  assert( ( s->get_lb() == 6 ) && ( s->get_ub() == 6 ) );
  // and fixing x1 to false too is infeasible, the two being the reason
  x[ 1 ].set_value( false ); x[ 1 ].is_fixed( true );
  assert( s->compute() == Solver::kInfeasible );
  assert( s->is_failed( 0 ) && s->is_failed( 1 ) );
  x[ 0 ].is_fixed( false ); x[ 1 ].is_fixed( false );

  // the same SATSolver goes back to the hard clauses only
  s->set_par( SATSolver::intMaxSAT , 0 );
  assert( s->compute() == Solver::kOK );
  assert( s->get_lb() == 0 );
  b.unregister_Solvers( true );
  }

 // random instances: n variables, hard and soft clauses of 1 to 3 literals
 std::srand( 12345 );
 unsigned checked = 0 , infeasible = 0;
 for( unsigned t = 0 ; t < 300 ; ++t ) {
  const unsigned n = 4 + std::rand() % 9;          // 4 to 12 variables
  const unsigned m = n + std::rand() % ( 3 * n );
  SATBlock::v_Clause clauses( m );
  SATBlock::v_Weight weights( m );
  for( unsigned c = 0 ; c < m ; ++c ) {
   const unsigned len = 1 + std::rand() % 3;
   for( unsigned l = 0 ; l < len ; ++l )
    clauses[ c ].push_back( ( 1 + std::rand() % n ) *
			    ( std::rand() % 2 ? 1 : -1 ) );
   // a third hard, the others with weights 1 to 20, some of them equal
   weights[ c ] = ( std::rand() % 3 == 0 ) ? inf : 1 + std::rand() % 20;
   }
  SATBlock b;
  b.load( n , std::move( clauses ) , std::move( weights ) );
  b.generate_abstract_variables();

  // the optimum by enumeration
  double best = inf;
  for( unsigned long mask = 0 ; mask < ( 1ul << n ) ; ++mask ) {
   set_values( b , mask );
   if( b.is_feasible() )
    best = std::min( best , b.get_violated_weight() );
   }

  SATSolver * s = dynamic_cast< SATSolver * >(
				       Solver::new_Solver( solver_name ) );
  s->set_par( SATSolver::intMaxSAT , 1 );
  b.register_Solver( s );
  const int status = s->compute();
  if( best == inf ) {
   assert( status == Solver::kInfeasible );
   ++infeasible;
   }
  else {
   assert( status == Solver::kOK );
   assert( ( s->get_lb() == best ) && ( s->get_ub() == best ) );
   s->get_var_solution();
   assert( b.is_feasible() && ( b.get_violated_weight() == best ) );
   }
  b.unregister_Solvers( true );
  ++checked;
  }
 std::cout << solver_name << ": OLL optimal on " << checked
	   << " random instances (" << infeasible << " infeasible)"
	   << std::endl;
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
 test_load_wcnf();
 test_abstract();
 test_soft();
 test_solution();
 test_round_trips();
 test_modifications();
 test_satlib();
#ifdef SATBLOCK_HAS_CADICAL
 solver_name = "CaDiCaLSATSolver";
 test_solver();
 test_oll();
 test_solver_satlib();
#endif
#ifdef SATBLOCK_HAS_MINISAT
 solver_name = "MiniSatSATSolver";
 test_solver();
 test_oll();
 test_solver_satlib();
#endif

 std::cout << "SATBlock: all tests passed" << std::endl;

 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------------- End File test.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
