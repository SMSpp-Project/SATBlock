/*--------------------------------------------------------------------------*/
/*------------------------------ File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Tests for SATBlock: the factory, the DIMACS CNF and WCNF readers on
 * hand-made cases and on an instance of the SATLIB collection, the abstract
 * representation, the feasibility check and the violated weight, the
 * Solution, the round trips through netCDF and through the writer, the
 * changes of the weights and the added clauses, and the SATSolver, OLL
 * included, on random instances and on instances of the MaxSAT Evaluation.
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

#include "ColVariableSolution.h"
#include "LinearFunction.h"
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
/// sets the ColVariable x of b to the bits of mask, and each r to 1 if its
/// clause is violated

static void set_values( SATBlock & b , unsigned long mask )
{
 auto & x = b.get_variables();
 for( unsigned int i = 0 ; i < x.size() ; ++i )
  x[ i ].set_value( ( mask >> i ) & 1 );
 for( unsigned int c = 0 ; c < b.get_number_clauses() ; ++c ) {
  const auto & cl = b.get_clauses()[ c ];
  const bool sat = std::any_of( cl.begin() , cl.end() , [ mask ]( int l ) {
   return( bool( ( mask >> ( std::abs( l ) - 1 ) ) & 1 ) == ( l > 0 ) ); } );
  if( ! b.get_violation( c ).is_fixed() )
   b.get_violation( c ).set_value( sat ? 0 : 1 );
  }
 }

/*--------------------------------------------------------------------------*/
/// true if all the rows of the clauses of b are satisfied by the values of
/// its ColVariable

static bool rows_feasible( SATBlock & b )
{
 bool all = true;
 for( unsigned int c = 0 ; c < b.get_number_clauses() ; ++c ) {
  auto & row = b.get_clause_constraint( c );
  row.compute();
  all = all && row.feasible();
  }
 return( all );
 }

/*--------------------------------------------------------------------------*/
/// the value of the Objective of b at the values of its ColVariable

static double objective_value( SATBlock & b )
{
 auto obj = static_cast< FRealObjective * >( b.get_objective() );
 obj->compute();
 return( obj->value() );
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
 b.generate_objective();

 const auto & x = b.get_variables();
 const auto & c = b.get_clause_constraints();
 assert( ( x.size() == 3 ) && ( c.size() == 4 ) );
 assert( b.get_static_variable_groups().size() == 2 );
 assert( b.get_static_constraint_groups().size() == 1 );
 assert( x[ 0 ].get_type() == ColVariable::kBinary );

 // clause 0 is x0 - x1 + r0 >= 0, r0 being fixed to 0 since it is hard
 auto lf = static_cast< const LinearFunction * >( c[ 0 ].get_function() );
 assert( lf->get_num_active_var() == 3 );
 assert( ( lf->get_coefficient( 0 ) == 1 ) &&
	 ( lf->get_coefficient( 1 ) == -1 ) &&
	 ( lf->get_coefficient( 2 ) == 1 ) );
 assert( ( c[ 0 ].get_lhs() == 0 ) &&
	 ( c[ 0 ].get_rhs() == Inf< double >() ) );
 assert( b.get_violation( 0 ).is_fixed() &&
	 ( b.get_violation( 0 ).get_value() == 0 ) );

 // the tautology is the row r2 >= -INF
 assert( ( c[ 2 ].get_lhs() == - Inf< double >() ) &&
	 ( static_cast< const LinearFunction * >( c[ 2 ].get_function()
					      )->get_num_active_var() == 1 ) );

 // the rows agree with is_feasible() on every assignment, and the Objective
 // is 0, all the clauses being hard
 for( unsigned long mask = 0 ; mask < 8 ; ++mask ) {
  set_values( b , mask );
  assert( b.is_feasible() == rows_feasible( b ) );
  assert( objective_value( b ) == 0 );
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
 // hard: x0 or not x1; soft: x1 (weight 3), x2 (weight 2), a soft
 // tautology and a soft clause with a variable twice
 SATBlock b;
 load_string( b , "h 1 -2 0\n3 2 0\n2 3 0\n1 1 -1 0\n4 -1 -1 3 0\n" );
 b.generate_objective();
 b.generate_abstract_constraints();

 // the r of the soft clauses are free, that of the hard one is fixed
 assert( b.get_violation( 0 ).is_fixed() &&
	 ( ! b.get_violation( 1 ).is_fixed() ) );

 // feasibility looks at the hard clause only, the weight at the soft ones,
 // and the MILP formulation agrees with both
 for( unsigned long mask = 0 ; mask < 8 ; ++mask ) {
  set_values( b , mask );
  const bool x0 = mask & 1 , x1 = mask & 2 , x2 = mask & 4;
  assert( b.is_feasible() == ( x0 || ! x1 ) );
  const double w = ( x1 ? 0 : 3 ) + ( x2 ? 0 : 2 ) +
                   ( ( ( ! x0 ) || x2 ) ? 0 : 4 );
  assert( b.get_violated_weight() == w );
  assert( rows_feasible( b ) == b.is_feasible() );
  assert( objective_value( b ) == w );
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
 assert( dynamic_cast< ColVariableSolution * >( sol ) );
 set_values( b , 0 );
 sol->write( & b );
 assert( ( b.get_variables()[ 0 ].get_value() == 1 ) &&
	 ( b.get_variables()[ 1 ].get_value() == 0 ) &&
	 ( b.get_variables()[ 2 ].get_value() == 1 ) );
 delete sol;
 }

/*--------------------------------------------------------------------------*/

static void test_round_trips( void )
{
 SATBlock b;
 load_string( b , "p cnf 4 4\n1 -2 0\n2 3 -4 0\n4 0\n-1 -3 0\n" );

 // netCDF
 const char * file = "SATBlock_unit_test.nc4";
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

static void test_costs( void )
{
 // x0 or x1 hard, the soft clause x2 of weight 3, and the costs 2, -1, 0
 SATBlock b;
 load_string( b , "h 1 2 0\n3 3 0\n" );
 std::vector< double > c = { 2 , -1 };
 b.chg_costs( c , Block::Range( 0 , 2 ) );
 assert( ( b.get_costs() == SATBlock::v_Weight{ 2 , -1 , 0 } ) &&
	 b.has_costs() );
 b.generate_abstract_constraints();
 b.generate_objective();

 // the value of every assignment, with the MILP formulation agreeing
 for( unsigned long mask = 0 ; mask < 8 ; ++mask ) {
  set_values( b , mask );
  const bool x0 = mask & 1 , x1 = mask & 2 , x2 = mask & 4;
  const double v = ( x0 ? 2 : 0 ) + ( x1 ? -1 : 0 ) + ( x2 ? 0 : 3 );
  assert( b.get_objective_value() == v );
  assert( objective_value( b ) == v );
  }

 // a change of the Objective, as a LagBFunction does it, is a change of the
 // costs and of the weights of the soft clauses
 auto lf = static_cast< LinearFunction * >(
	     static_cast< FRealObjective * >( b.get_objective() )->get_function() );
 lf->modify_coefficient( lf->is_active( & b.get_variables()[ 2 ] ) , 5 );
 lf->modify_coefficient( lf->is_active( & b.get_violation( 1 ) ) , 7 );
 assert( ( b.get_costs() == SATBlock::v_Weight{ 2 , -1 , 5 } ) &&
	 ( b.get_weights()[ 1 ] == 7 ) );
 // and a change of the costs is one of the Objective
 std::vector< double > sc = { 4 };
 b.chg_costs( sc , Block::Subset{ 1 } );
 assert( lf->get_coefficient( lf->is_active( & b.get_variables()[ 1 ] ) ) ==
	 4 );

 // the round trips: netCDF keeps the costs, WCNF writes them as unit soft
 // clauses, the value being the same up to the constant of the negative ones
 const char * file = "SATBlock_costs.nc4";
 {
  netCDF::NcFile f( file , netCDF::NcFile::replace );
  auto g = f.addGroup( "Block" );
  b.serialize( g );
  }
 {
  netCDF::NcFile f( file , netCDF::NcFile::read );
  auto d = Block::new_Block( f.getGroup( "Block" ) );
  auto s = dynamic_cast< SATBlock * >( d );
  assert( s && ( s->get_costs() == b.get_costs() ) &&
	  ( s->get_weights() == b.get_weights() ) );
  delete d;
  }
 std::remove( file );

 std::vector< double > neg = { -3 };
 b.chg_costs( neg , Block::Range( 0 , 1 ) );
 std::ostringstream out;
 b.print( out , 'C' );
 SATBlock w;
 load_string( w , out.str() , 'W' );
 w.generate_abstract_variables();
 for( unsigned long mask = 0 ; mask < 8 ; ++mask ) {
  set_values( b , mask );
  set_values( w , mask );
  assert( b.get_objective_value() == w.get_objective_value() - 3 );
  }

 // wrong costs
 bool thrown = false;
 std::vector< double > bad = { Inf< double >() };
 try {
  b.chg_costs( bad , Block::Range( 0 , 1 ) );
  }
 catch( std::invalid_argument & ) {
  thrown = true;
  }
 assert( thrown );
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

 // with the abstract representation: the weights fix and unfix the r and
 // change the Objective, the clauses added join the dynamic groups, in
 // order, and the Objective
 b.generate_abstract_constraints();
 b.generate_objective();
 assert( ( b.get_clause_constraints().size() == 5 ) &&
	 b.get_added_clause_constraints().empty() );
 assert( ( ! b.get_violation( 1 ).is_fixed() ) &&
	 b.get_violation( 2 ).is_fixed() );
 auto obj = static_cast< const LinearFunction * >(
	     static_cast< FRealObjective * >( b.get_objective() )->get_function() );
 assert( ( obj->get_coefficient( 1 ) == 1 ) &&
	 ( obj->get_coefficient( 2 ) == 0 ) );

 std::vector< double > sw = { 2 , inf };
 b.chg_weights( sw , Block::Subset{ 2 , 1 } );  // not ordered
 assert( ( b.get_weights() ==
	   SATBlock::v_Weight{ inf , inf , 2 , 4 , inf } ) );
 assert( b.get_violation( 1 ).is_fixed() &&
	 ( ! b.get_violation( 2 ).is_fixed() ) );
 assert( ( obj->get_coefficient( 1 ) == 0 ) &&
	 ( obj->get_coefficient( 2 ) == 2 ) );

 b.add_clauses( { { 2 } , { 1 , 1 , -2 } } , { 6 , inf } );
 const auto & lc = b.get_added_clause_constraints();
 assert( ( b.get_number_clauses() == 7 ) && ( lc.size() == 2 ) );
 assert( b.get_dynamic_constraint_groups().size() == 1 );
 assert( b.get_dynamic_variable_groups().size() == 1 );
 assert( ( ! b.get_violation( 5 ).is_fixed() ) &&
	 b.get_violation( 6 ).is_fixed() );
 // the 7 r and the 2 x, the terms found by their variable
 assert( ( obj->get_num_active_var() == 9 ) &&
	 ( obj->get_coefficient( obj->is_active( & b.get_violation( 5 ) ) )
	   == 6 ) &&
	 ( obj->get_coefficient( obj->is_active( & b.get_violation( 6 ) ) )
	   == 0 ) );
 assert( static_cast< const LinearFunction * >( lc.back().get_function()
					     )->get_num_active_var() == 3 );

 // the MILP formulation still agrees with the physical representation
 for( unsigned long mask = 0 ; mask < 4 ; ++mask ) {
  set_values( b , mask );
  assert( rows_feasible( b ) == b.is_feasible() );
  assert( objective_value( b ) == b.get_violated_weight() );
  }

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
  b.get_clause_constraint( 1 ).relax( true );
  assert( s->compute() == Solver::kOK );
  s->get_var_solution();
  assert( b.get_variables()[ 0 ].get_value() == 1 );
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

  // a time limit of 0 stops OLL as soon as the SAT solver looks at the
  // time, which MiniSat does after a budget of conflicts: if it stops before
  // any solution, there is none to write
  s->set_par( Solver::dblMaxTime , 0.0 );
  if( s->compute() == Solver::kStopTime )
   assert( ( s->get_ub() == Inf< Solver::OFValue >() ) ==
	   ! s->has_var_solution() );
  s->set_par( Solver::dblMaxTime , Inf< double >() );

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
  // half of the instances have costs of either sign on their variables
  if( t % 2 ) {
   std::vector< double > costs( n );
   for( auto & c : costs )
    c = int( std::rand() % 21 ) - 10;
   b.chg_costs( costs , Block::Range( 0 , n ) );
   }

  // the optimum by enumeration
  double best = inf;
  for( unsigned long mask = 0 ; mask < ( 1ul << n ) ; ++mask ) {
   set_values( b , mask );
   if( b.is_feasible() )
    best = std::min( best , b.get_objective_value() );
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
   assert( b.is_feasible() && ( b.get_objective_value() == best ) );
   }
  b.unregister_Solvers( true );
  ++checked;
  }
 std::cout << solver_name << ": OLL optimal on " << checked
	   << " random instances (" << infeasible << " infeasible)"
	   << std::endl;
 }

/*--------------------------------------------------------------------------*/
/// the same OLL through a sequence of Modification, against the enumeration

static void test_oll_incremental( void )
{
 const auto inf = Inf< double >();

 std::srand( 54321 );
 unsigned steps = 0 , infeasible = 0;
 for( unsigned t = 0 ; t < 60 ; ++t ) {
  const unsigned n = 4 + std::rand() % 7;          // 4 to 10 variables
  auto rnd_clause = [ n ]( void ) {
   SATBlock::Clause cl;
   const unsigned len = 1 + std::rand() % 3;
   for( unsigned l = 0 ; l < len ; ++l )
    cl.push_back( int( 1 + std::rand() % n ) * ( std::rand() % 2 ? 1 : -1 ) );
   return( cl );
   };
  auto rnd_weight = [ inf ]( void ) {
   return( ( std::rand() % 4 == 0 ) ? inf : double( 1 + std::rand() % 20 ) );
   };

  const unsigned m = n + std::rand() % ( 2 * n );
  SATBlock::v_Clause clauses( m );
  SATBlock::v_Weight weights( m );
  for( unsigned c = 0 ; c < m ; ++c ) {
   clauses[ c ] = rnd_clause();
   weights[ c ] = rnd_weight();
   }
  SATBlock b;
  b.load( n , std::move( clauses ) , std::move( weights ) );
  b.generate_abstract_variables();
  auto & x = b.get_variables();

  auto s = dynamic_cast< SATSolver * >( Solver::new_Solver( solver_name ) );
  s->set_par( SATSolver::intMaxSAT , 1 );
  b.register_Solver( s );

  std::vector< int > fixed( n , -1 );  // the value of a fixed x, -1 if not
  for( unsigned step = 0 ; step < 15 ; ++step ) {
   // the first compute() on the instance as it is, then a Modification
   // before each of the others: mostly new costs, as the Lagrangian term of
   // a decomposition gives, then weights changed (a clause possibly turning
   // hard or soft), clauses added, a variable fixed or all of them unfixed
   if( step > 0 )
    switch( std::rand() % 8 ) {
     case( 0 ): case( 1 ): case( 2 ): {
      std::vector< double > c( n );
      for( auto & ci : c )
       ci = int( std::rand() % 21 ) - 10;
      b.chg_costs( c , Block::Range( 0 , n ) );
      break;
      }
     case( 3 ): {
      const unsigned first = std::rand() % b.get_number_clauses();
      const unsigned k = 1 + std::rand() %
		std::min( 3u , unsigned( b.get_number_clauses() ) - first );
      std::vector< double > w( k );
      for( auto & wi : w )
       wi = rnd_weight();
      b.chg_weights( w , Block::Range( first , first + k ) );
      break;
      }
     case( 4 ): {
      SATBlock::v_Clause nc( 1 + std::rand() % 2 );
      SATBlock::v_Weight nw( nc.size() );
      for( unsigned c = 0 ; c < nc.size() ; ++c ) {
       nc[ c ] = rnd_clause();
       nw[ c ] = rnd_weight();
       }
      b.add_clauses( std::move( nc ) , std::move( nw ) );
      break;
      }
     case( 5 ): case( 6 ): {
      const unsigned i = std::rand() % n;
      fixed[ i ] = std::rand() % 2;
      x[ i ].is_fixed( false );
      x[ i ].set_value( fixed[ i ] );
      x[ i ].is_fixed( true );
      break;
      }
     default:
      for( unsigned i = 0 ; i < n ; ++i ) {
       fixed[ i ] = -1;
       x[ i ].is_fixed( false );
       }
     }

   // the optimum by enumeration, among the values of the fixed variables
   double best = inf;
   for( unsigned long mask = 0 ; mask < ( 1ul << n ) ; ++mask ) {
    bool agree = true;
    for( unsigned i = 0 ; i < n ; ++i )
     if( ( fixed[ i ] >= 0 ) && ( int( ( mask >> i ) & 1 ) != fixed[ i ] ) )
      agree = false;
    if( ! agree )
     continue;
    set_values( b , mask );
    if( b.is_feasible() )
     best = std::min( best , b.get_objective_value() );
    }

   const int status = s->compute();
   if( best == inf ) {
    assert( status == Solver::kInfeasible );
    ++infeasible;
    }
   else {
    assert( status == Solver::kOK );
    assert( ( s->get_lb() == best ) && ( s->get_ub() == best ) );
    s->get_var_solution();
    assert( b.is_feasible() && ( b.get_objective_value() == best ) );
    }
   ++steps;
   }
  b.unregister_Solvers( true );
  }
 std::cout << solver_name << ": OLL optimal through " << steps
	   << " Modification kept by the same Solver (" << infeasible
	   << " infeasible)" << std::endl;
 }

/*--------------------------------------------------------------------------*/
/// a depth-first branch and bound on the SATSolver as a RelaxationSolver,
/// returning the best value found below the current node

static double dive( SATSolver * s , double incumbent , unsigned & nodes )
{
 ++nodes;
 const int status = s->compute();
 if( status == Solver::kInfeasible )
  return( incumbent );
 // with intMaxIter, kOK is the relaxation of the cores found so far: the
 // node is closed when its bound meets the incumbent
 assert( status == Solver::kOK );
 if( s->has_true_var_solution() )
  incumbent = std::min( incumbent , double( s->get_true_ub() ) );
 assert( s->get_lb() <= s->get_ub() );
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
/// the SATBlockChange, and the SATSolver as a RelaxationSolver: a branch
/// and bound with a budget of 2 calls of the SAT solver per node against
/// the enumeration on random instances

static void test_branch( void )
{
 const auto inf = Inf< double >();

 // fix, undo, unfix, undo, and the netCDF round trip
 {
  SATBlock b;
  load_string( b , "p cnf 3 1\n1 2 3 0\n" );
  auto & x = b.get_variables();
  SATBlockChange fix( SATBlockChange::eFixX , { 0 , 2 } , { 1 , 0 } );
  auto undo = fix.apply( & b , true );
  assert( x[ 0 ].is_fixed() && ( x[ 0 ].get_value() == 1 ) &&
	  ( ! x[ 1 ].is_fixed() ) &&
	  x[ 2 ].is_fixed() && ( x[ 2 ].get_value() == 0 ) );
  bool threw = false;  // already fixed: the undo could not tell it
  try { fix.apply( & b ); } catch( std::invalid_argument & ) { threw = true; }
  assert( threw );
  auto redo = undo->apply( & b , true );
  assert( ( ! x[ 0 ].is_fixed() ) && ( ! x[ 2 ].is_fixed() ) );
  // the undo of an unfixing fixes back at the old values
  assert( dynamic_cast< SATBlockChange * >( redo )->type() ==
	  SATBlockChange::eFixX );
  delete redo->apply( & b );
  assert( x[ 0 ].is_fixed() && ( x[ 0 ].get_value() == 1 ) &&
	  x[ 2 ].is_fixed() && ( x[ 2 ].get_value() == 0 ) );
  delete undo;
  delete redo;

  const auto file = std::filesystem::temp_directory_path() /
		    "SATBlockChange_test.nc4";
  fix.serialize( file.string() );
  netCDF::NcFile f( file.string() , netCDF::NcFile::read );
  auto back = Change::new_Change( f.getGroup( "Change_0" ) );
  auto sback = dynamic_cast< SATBlockChange * >( back );
  assert( sback && ( sback->type() == SATBlockChange::eFixX ) &&
	  ( sback->nms() == Block::Subset{ 0 , 2 } ) &&
	  ( sback->values() == std::vector< double >{ 1 , 0 } ) );
  delete back;
  std::filesystem::remove( file );
  }

 std::srand( 2718 );
 unsigned checked = 0 , branched = 0 , nodes = 0;
 for( unsigned t = 0 ; t < 100 ; ++t ) {
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
  if( t % 2 ) {
   std::vector< double > costs( n );
   for( auto & c : costs )
    c = int( std::rand() % 21 ) - 10;
   b.chg_costs( costs , Block::Range( 0 , n ) );
   }

  double best = inf;
  for( unsigned long mask = 0 ; mask < ( 1ul << n ) ; ++mask ) {
   set_values( b , mask );
   if( b.is_feasible() )
    best = std::min( best , b.get_objective_value() );
   }

  auto s = dynamic_cast< SATSolver * >( Solver::new_Solver( solver_name ) );
  s->set_par( SATSolver::intMaxSAT , 1 );
  s->set_par( Solver::intMaxIter , 2 );
  b.register_Solver( s );
  unsigned here = 0;
  const double found = dive( s , inf , here );
  assert( found == best );
  for( auto & xi : b.get_variables() )  // the undos have unfixed them all
   assert( ! xi.is_fixed() );
  nodes += here;
  if( here > 1 )
   ++branched;
  b.unregister_Solvers( true );
  ++checked;
  }
 std::cout << solver_name << ": branch and bound optimal on " << checked
	   << " random instances, " << branched << " of them branched, "
	   << nodes << " nodes" << std::endl;
 }

/*--------------------------------------------------------------------------*/
/// the graph of the residual formula: its vertices, edges and rows

static void test_residual_graph( void )
{
 const auto inf = Inf< double >();

 // x1 or x2 hard, not x1 of weight 4, x2 or not x3 or x4 of weight 2, x4
 // alone of weight 1, and the cost 3 on x3
 SATBlock b;
 b.load( 4 , { { 1 , 2 } , { -1 } , { 2 , -3 , 4 } , { 4 } } ,
	 { inf , 4 , 2 , 1 } );
 b.generate_abstract_variables();
 std::vector< double > c = { 0 , 0 , 3 , 0 };
 b.chg_costs( c , Block::Range( 0 , 4 ) );
 auto s = dynamic_cast< SATSolver * >( Solver::new_Solver( solver_name ) );
 b.register_Solver( s );
 auto & x = b.get_variables();

 // nothing fixed: 4 variables, 4 clauses, 2 edges per literal
 SATResidualGraph g;
 assert( g.build( * s ) && ( g.n_col == 2 ) && ( g.n_var == 4 ) &&
	 ( g.n_clause == 4 ) && ( g.source.size() == 2 * 7 ) &&
	 ( g.edge.size() == 2 * g.source.size() ) &&
	 ( g.vertex.size() == 8 * 2 ) );
 // the first literal, x1 positive, from variable 0 to clause 4 and back
 assert( ( g.source[ 0 ] == 0 ) && ( g.target[ 0 ] == 4 ) &&
	 ( g.source[ 1 ] == 4 ) && ( g.target[ 1 ] == 0 ) &&
	 ( g.edge[ 0 ] == 0 ) && ( g.edge[ 1 ] == 1 ) );
 // not x1 in clause 5: the row [ 1 , 0 ]
 assert( ( g.target[ 4 ] == 5 ) && ( g.edge[ 8 ] == 1 ) &&
	 ( g.edge[ 9 ] == 0 ) );
 assert( ( g.vertex[ 0 ] == 1 ) && ( g.vertex[ 1 ] == 0 ) &&
	 ( g.vertex[ 4 * 2 ] == 0 ) && ( g.vertex[ 4 * 2 + 1 ] == 1 ) );

 // x2 fixed true satisfies the 1st and the 3rd clause; x4 fixed false
 // leaves the 4th with no literal: x1 and x3 are left, and not x1
 x[ 1 ].set_value( 1 ); x[ 1 ].is_fixed( true );
 x[ 3 ].set_value( 0 ); x[ 3 ].is_fixed( true );
 assert( g.build( * s , SATResidualGraph::eMaxSAT ) && ( g.n_col == 7 ) &&
	 ( g.n_var == 2 ) && ( g.n_clause == 1 ) &&
	 ( g.var == std::vector< unsigned int >{ 0 , 2 } ) &&
	 ( g.source.size() == 2 ) );
 // no solution yet: 1/2; no core: 0; the cost of x3 over the weight 4
 assert( ( g.vertex[ 2 ] == 0.5f ) && ( g.vertex[ 3 ] == 0 ) &&
	 ( g.vertex[ 4 ] == 0 ) && ( g.vertex[ 7 + 4 ] == 0.75f ) );
 // the clause not x1, of weight 4, the largest
 assert( ( g.vertex[ 14 + 5 ] == 1 ) && ( g.vertex[ 14 + 6 ] == 0 ) );

 // x1 fixed true too: not x1 has no literal left, nothing to say
 x[ 0 ].set_value( 1 ); x[ 0 ].is_fixed( true );
 assert( ( ! g.build( * s ) ) && ( g.n_var == 0 ) && g.vertex.empty() );
 // and all fixed: no variable
 x[ 2 ].set_value( 0 ); x[ 2 ].is_fixed( true );
 assert( ! g.build( * s ) );
 b.unregister_Solvers( true );
 }

/*--------------------------------------------------------------------------*/
/// OLL on the instances of the MaxSAT Evaluation whose optimum is known: the
/// optimum if it finishes, bounds around it if the time limit stops it

static void test_oll_mse( void )
{
 std::ifstream opt( "../data/wcnf/mse24-small/optima.txt" );
 assert( opt );
 unsigned optimal = 0 , stopped = 0;
 std::string line;
 while( std::getline( opt , line ) ) {
  if( line.empty() || ( line[ 0 ] == '#' ) )
   continue;
  std::istringstream ls( line );
  std::string name;
  double z;
  ls >> name >> z;

  std::ifstream in( "../data/wcnf/mse24-small/" + name );
  assert( in );
  SATBlock b;
  b.load( in , 'W' );
  auto s = dynamic_cast< SATSolver * >( Solver::new_Solver( solver_name ) );
  s->set_par( SATSolver::intMaxSAT , 1 );
  s->set_par( Solver::dblMaxTime , 2.0 );
  b.register_Solver( s );
  const int status = s->compute();
  if( status == Solver::kOK ) {
   assert( ( s->get_lb() == z ) && ( s->get_ub() == z ) );
   s->get_var_solution();
   assert( b.is_feasible() && ( b.get_violated_weight() == z ) );
   ++optimal;
   }
  else {
   assert( status == Solver::kStopTime );
   assert( ( s->get_lb() <= z ) && ( z <= s->get_ub() ) );
   ++stopped;
   }
  b.unregister_Solvers( true );
  }
 std::cout << solver_name << ": OLL optimal on " << optimal
	   << " MaxSAT Evaluation instances, stopped by the time on "
	   << stopped << std::endl;
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
 test_costs();
 test_satlib();
#ifdef SATBLOCK_HAS_CADICAL
 solver_name = "CaDiCaLSATSolver";
 test_solver();
 test_oll();
 test_oll_incremental();
 test_branch();
 test_residual_graph();
 test_oll_mse();
 test_solver_satlib();
#endif
#ifdef SATBLOCK_HAS_MINISAT
 solver_name = "MiniSATSolver";
 test_solver();
 test_oll();
 test_oll_incremental();
 test_branch();
 test_residual_graph();
 test_oll_mse();
 test_solver_satlib();
#endif

 std::cout << "SATBlock: all tests passed" << std::endl;

 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------------- End File test.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
