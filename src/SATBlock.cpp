/*--------------------------------------------------------------------------*/
/*--------------------------- File SATBlock.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of the SATBlock class.
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
#include <sstream>

#include "BooleanVariableSolution.h"
#include "SATBlock.h"

/*--------------------------------------------------------------------------*/
/*------------------------- NAMESPACE AND USING ----------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*----------------------------- STATIC MEMBERS -----------------------------*/
/*--------------------------------------------------------------------------*/

// register SATBlock in the Block factory

SMSpp_insert_in_factory_cpp_1( SATBlock );

/*--------------------------------------------------------------------------*/
/*------------------------- OTHER INITIALIZATIONS --------------------------*/
/*--------------------------------------------------------------------------*/

void SATBlock::normalize_clauses( void )
{
 for( auto & clause : v_clauses ) {
  Clause kept;
  kept.reserve( clause.size() );
  for( auto lit : clause ) {
   if( ( lit == 0 ) || ( unsigned( std::abs( lit ) ) > f_n_var ) )
    throw( std::invalid_argument( "SATBlock::normalize_clauses: literal " +
				  std::to_string( lit ) + " out of range" ) );
   if( std::find( kept.begin() , kept.end() , lit ) == kept.end() )
    kept.push_back( lit );
   }
  clause = std::move( kept );
  }
 }

/*--------------------------------------------------------------------------*/

void SATBlock::load( unsigned int n_var , v_Clause && clauses )
{
 guts_of_destructor();

 f_n_var = n_var;
 v_clauses = std::move( clauses );
 normalize_clauses();

 if( anyone_there() )
  add_Modification( std::make_shared< NBModification >( this ) );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::load( std::istream & input , char frmt )
{
 if( ( frmt != 0 ) && ( frmt != 'D' ) )
  throw( std::invalid_argument( std::string( "SATBlock::load: unknown "
					     "format " ) + frmt ) );

 // the header "p cnf <n> <m>", after any comment line
 std::string line;
 long n = -1 , m = -1;
 while( std::getline( input , line ) ) {
  std::istringstream ls( line );
  std::string tok;
  if( ! ( ls >> tok ) || ( tok[ 0 ] == 'c' ) )
   continue;
  std::string fmt;
  if( ( tok != "p" ) || ! ( ls >> fmt >> n >> m ) || ( fmt != "cnf" ) ||
      ( n < 0 ) || ( m < 0 ) )
   throw( std::invalid_argument( "SATBlock::load: expected \"p cnf <n> "
				 "<m>\", found \"" + line + "\"" ) );
  break;
  }
 if( n < 0 )
  throw( std::invalid_argument( "SATBlock::load: no \"p cnf\" line" ) );

 // the m clauses, each ended by a 0, possibly spanning several lines
 v_Clause clauses;
 clauses.reserve( m );
 Clause current;
 while( ( long( clauses.size() ) < m ) && std::getline( input , line ) ) {
  std::istringstream ls( line );
  std::string tok;
  if( ! ( ls >> tok ) || ( tok[ 0 ] == 'c' ) )
   continue;
  if( tok[ 0 ] == '%' )
   break;
  do {
   char * end;
   const long lit = std::strtol( tok.c_str() , & end , 10 );
   if( *end )
    throw( std::invalid_argument( "SATBlock::load: \"" + tok +
				  "\" is not a literal" ) );
   if( lit == 0 ) {
    clauses.push_back( std::move( current ) );
    current.clear();
    if( long( clauses.size() ) == m )
     break;
    }
   else
    current.push_back( int( lit ) );
   }
  while( ls >> tok );
  }

 if( long( clauses.size() ) < m )
  throw( std::invalid_argument( "SATBlock::load: " +
				std::to_string( clauses.size() ) +
				" clauses found, " + std::to_string( m ) +
				" declared" ) );

 load( unsigned( n ) , std::move( clauses ) );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::deserialize( const netCDF::NcGroup & group )
{
 guts_of_destructor();

 auto nv = group.getDim( "NumberVariables" );
 if( nv.isNull() )
  throw( std::invalid_argument( "SATBlock::deserialize: dimension "
				"NumberVariables is required" ) );
 f_n_var = nv.getSize();

 ::deserialize< int >( group , "Clauses" , "ClausesStart" , v_clauses );

 normalize_clauses();

 Block::deserialize( group );
 }

/*--------------------------------------------------------------------------*/
/*--------------------- Methods for handling Variable ----------------------*/
/*--------------------------------------------------------------------------*/

void SATBlock::generate_abstract_variables( Configuration * stvv )
{
 if( AR & HasVar )  // the Variable are there already
  return;           // nothing to do

 v_x = std::vector< BooleanVariable >( f_n_var );
 add_static_variable( v_x , "x" );

 AR |= HasVar;
 }

/*--------------------------------------------------------------------------*/
/*-------------------- Methods for handling Constraint ---------------------*/
/*--------------------------------------------------------------------------*/

void SATBlock::generate_abstract_constraints( Configuration * stcc )
{
 if( AR & HasCns )  // the Constraint are there already
  return;           // nothing to do

 generate_abstract_variables();

 v_c = std::vector< ClauseConstraint >( v_clauses.size() );
 for( unsigned int i = 0 ; i < v_clauses.size() ; ++i ) {
  if( is_tautology( i ) ) {
   // always satisfied: a relaxed ClauseConstraint with no literals
   v_c[ i ].relax( true , eNoMod );
   continue;
   }
  ClauseConstraint::v_Literal lits;
  lits.reserve( v_clauses[ i ].size() );
  for( auto lit : v_clauses[ i ] )
   lits.emplace_back( & v_x[ std::abs( lit ) - 1 ] , lit < 0 );
  v_c[ i ].set_literals( std::move( lits ) , eNoMod );
  }

 add_static_constraint( v_c , "clauses" );

 AR |= HasCns;
 }

/*--------------------------------------------------------------------------*/
/*------------------ Methods for reading the data of the SATBlock ----------*/
/*--------------------------------------------------------------------------*/

bool SATBlock::is_tautology( unsigned int i ) const
{
 const auto & clause = v_clauses[ i ];
 for( auto lit : clause )
  if( ( lit > 0 ) &&
      ( std::find( clause.begin() , clause.end() , - lit ) != clause.end() ) )
   return( true );
 return( false );
 }

/*--------------------------------------------------------------------------*/

bool SATBlock::is_feasible( bool useabstract , Configuration * fsbc )
{
 if( v_x.size() != f_n_var )
  throw( std::logic_error( "SATBlock::is_feasible: the BooleanVariable have "
			   "not been generated" ) );

 for( const auto & clause : v_clauses )
  if( std::none_of( clause.begin() , clause.end() , [ this ]( int lit ) {
       return( v_x[ std::abs( lit ) - 1 ].get_value() == ( lit > 0 ) ); } ) )
   return( false );

 return( true );
 }

/*--------------------------------------------------------------------------*/

Solution * SATBlock::get_Solution( Configuration * solc , bool emptys )
{
 auto sol = new BooleanVariableSolution();
 if( ! emptys )
  sol->read( this );
 return( sol );
 }

/*--------------------------------------------------------------------------*/
/*------------------------- Methods for serializing ------------------------*/
/*--------------------------------------------------------------------------*/

void SATBlock::serialize( netCDF::NcGroup & group ) const
{
 Block::serialize( group );

 group.addDim( "NumberVariables" , f_n_var );

 if( v_clauses.empty() )
  return;

 std::size_t n_lit = 0;
 for( const auto & clause : v_clauses )
  n_lit += clause.size();

 auto nc = group.addDim( "NumberClauses" , v_clauses.size() );
 auto nl = group.addDim( "NumberLiterals" , n_lit );
 ::serialize< int >( group , "Clauses" , netCDF::NcInt() , "ClausesStart" ,
		     v_clauses , nl , nc );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::print( std::ostream & output , char vlvl ) const
{
 if( vlvl != 'C' ) {
  output << "SATBlock with " << f_n_var << " variables and "
	 << v_clauses.size() << " clauses" << std::endl;
  return;
  }

 output << "p cnf " << f_n_var << " " << v_clauses.size() << std::endl;
 for( const auto & clause : v_clauses ) {
  for( auto lit : clause )
   output << lit << " ";
  output << "0" << std::endl;
  }
 }

/*--------------------------------------------------------------------------*/

void SATBlock::guts_of_destructor( void )
{
 // the ClauseConstraint go before the BooleanVariable they are active in,
 // and are told not to bother with them, both going away together
 for( auto & c : v_c )
  c.clear();

 reset_static_constraints();
 reset_static_variables();
 v_c.clear();
 v_x.clear();

 v_clauses.clear();
 f_n_var = 0;
 AR = 0;
 }

/*--------------------------------------------------------------------------*/
/*------------------------- End File SATBlock.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
