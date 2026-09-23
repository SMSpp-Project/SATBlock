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

#include "BooleanVariableSolution.h"
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
 auto & x = const_cast< std::vector< BooleanVariable > & >(
							b.get_variables() );
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
  for( auto & cc : const_cast< std::vector< ClauseConstraint > & >( c ) ) {
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
 std::ifstream in( "data/uf20-07.cnf" );
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

 std::cout << "SATBlock: all tests passed" << std::endl;

 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------------- End File test.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
