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
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>

#include "C05Function.h"
#include "ColVariableSolution.h"
#include "LinearFunction.h"
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

// register SATBlockChange in the Change factory

SMSpp_insert_in_factory_cpp_1( SATBlockChange );

/*--------------------------------------------------------------------------*/
/*------------------------- OTHER INITIALIZATIONS --------------------------*/
/*--------------------------------------------------------------------------*/

void SATBlock::normalize_clauses( unsigned int first )
{
 for( auto it = v_clauses.begin() + first ; it != v_clauses.end() ; ++it ) {
  auto & clause = *it;
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

void SATBlock::check_weights( unsigned int first ) const
{
 for( auto i = first ; i < v_weights.size() ; ++i )
  if( std::isnan( v_weights[ i ] ) || ( v_weights[ i ] < 0 ) )
   throw( std::invalid_argument( "SATBlock::check_weights: weight " +
				 std::to_string( v_weights[ i ] ) +
				 " of clause " + std::to_string( i ) ) );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::load( unsigned int n_var , v_Clause && clauses ,
		     v_Weight && weights )
{
 if( ( ! weights.empty() ) && ( weights.size() != clauses.size() ) )
  throw( std::invalid_argument( "SATBlock::load: " +
				std::to_string( weights.size() ) +
				" weights for " +
				std::to_string( clauses.size() ) +
				" clauses" ) );

 guts_of_destructor();

 f_n_var = n_var;
 v_clauses = std::move( clauses );
 normalize_clauses();
 if( weights.empty() )
  v_weights.assign( v_clauses.size() , Inf< double >() );
 else
  v_weights = std::move( weights );
 check_weights();
 v_costs.assign( f_n_var , 0 );

 if( anyone_there() )
  add_Modification( std::make_shared< NBModification >( this ) );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::load( std::istream & input , char frmt )
{
 if( ( frmt != 0 ) && ( frmt != 'D' ) && ( frmt != 'W' ) )
  throw( std::invalid_argument( std::string( "SATBlock::load: unknown "
					     "format " ) + frmt ) );

 // the first line that is not a comment: either the "p" line, or the first
 // clause of a WCNF file from 2022 on
 std::string line;
 std::istringstream ls;
 std::string tok;
 bool found = false;
 while( std::getline( input , line ) ) {
  ls.clear();
  ls.str( line );
  if( ( ls >> tok ) && ( tok[ 0 ] != 'c' ) ) {
   found = true;
   break;
   }
  }
 if( ! found )
  throw( std::invalid_argument( "SATBlock::load: no \"p\" line nor clause" ) );

 // reads the literals of a clause out of ls, tok being the first one
 auto read_clause = [ & ]( Clause & clause ) {
  do {
   char * end;
   const long lit = std::strtol( tok.c_str() , & end , 10 );
   if( *end )
    throw( std::invalid_argument( "SATBlock::load: \"" + tok +
				  "\" is not a literal" ) );
   if( lit == 0 )
    return( true );
   clause.push_back( int( lit ) );
   }
  while( ls >> tok );
  return( false );
  };

 // reads a weight out of tok
 auto read_weight = [ & ]( void ) {
  char * end;
  const double w = std::strtod( tok.c_str() , & end );
  if( *end )
   throw( std::invalid_argument( "SATBlock::load: \"" + tok +
				 "\" is not a weight" ) );
  return( w );
  };

 v_Clause clauses;
 v_Weight weights;

 if( tok != "p" ) {
  // WCNF from 2022 on: one clause per line, "h" or the weight first
  if( frmt == 'D' )
   throw( std::invalid_argument( "SATBlock::load: expected \"p cnf <n> "
				 "<m>\", found \"" + line + "\"" ) );
  long n = 0;
  for( bool more = true ; more ; ) {
   // ls holds a clause, and tok its first token
   const double w = ( tok == "h" ) ? Inf< double >() : read_weight();
   Clause clause;
   if( ! ( ( ls >> tok ) && read_clause( clause ) ) )
    throw( std::invalid_argument( "SATBlock::load: clause not ended by 0 "
				  "in \"" + line + "\"" ) );
   for( auto lit : clause )
    n = std::max( n , long( std::abs( lit ) ) );
   clauses.push_back( std::move( clause ) );
   weights.push_back( w );

   // the next line that is not a comment, if any
   more = false;
   while( std::getline( input , line ) ) {
    ls.clear();
    ls.str( line );
    if( ( ls >> tok ) && ( tok[ 0 ] != 'c' ) ) {
     more = true;
     break;
     }
    }
   }

  load( unsigned( n ) , std::move( clauses ) , std::move( weights ) );
  return;
  }

 // the "p" line
 std::string fmt;
 long n = -1 , m = -1;
 if( ! ( ls >> fmt >> n >> m ) || ( n < 0 ) || ( m < 0 ) ||
     ( ( fmt != "cnf" ) && ( fmt != "wcnf" ) ) ||
     ( ( fmt == "cnf" ) && ( frmt == 'W' ) ) ||
     ( ( fmt == "wcnf" ) && ( frmt == 'D' ) ) )
  throw( std::invalid_argument( "SATBlock::load: wrong \"p\" line \"" +
				line + "\"" ) );
 clauses.reserve( m );

 if( fmt == "wcnf" ) {
  // WCNF up to 2021: one clause per line, the weight first, hard if it is
  // at least top, if any
  double top = Inf< double >();
  if( ls >> tok )
   top = read_weight();
  weights.reserve( m );
  while( ( long( clauses.size() ) < m ) && std::getline( input , line ) ) {
   ls.clear();
   ls.str( line );
   if( ! ( ls >> tok ) || ( tok[ 0 ] == 'c' ) )
    continue;
   const double w = read_weight();
   Clause clause;
   if( ! ( ( ls >> tok ) && read_clause( clause ) ) )
    throw( std::invalid_argument( "SATBlock::load: clause not ended by 0 "
				  "in \"" + line + "\"" ) );
   clauses.push_back( std::move( clause ) );
   weights.push_back( w >= top ? Inf< double >() : w );
   }
  }
 else {
  // DIMACS CNF: the m clauses, each ended by a 0, possibly spanning several
  // lines
  Clause current;
  while( ( long( clauses.size() ) < m ) && std::getline( input , line ) ) {
   ls.clear();
   ls.str( line );
   if( ! ( ls >> tok ) || ( tok[ 0 ] == 'c' ) )
    continue;
   if( tok[ 0 ] == '%' )
    break;
   while( read_clause( current ) ) {
    clauses.push_back( std::move( current ) );
    current.clear();
    if( ( long( clauses.size() ) == m ) || ! ( ls >> tok ) )
     break;
    }
   }
  }

 if( long( clauses.size() ) < m )
  throw( std::invalid_argument( "SATBlock::load: " +
				std::to_string( clauses.size() ) +
				" clauses found, " + std::to_string( m ) +
				" declared" ) );

 load( unsigned( n ) , std::move( clauses ) , std::move( weights ) );
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

 v_weights.assign( v_clauses.size() , Inf< double >() );
 auto w = group.getVar( "Weights" );
 if( ( ! w.isNull() ) && ( ! v_clauses.empty() ) ) {
  if( w.getDimCount() != 1 || w.getDim( 0 ).getSize() != v_clauses.size() )
   throw( std::invalid_argument( "SATBlock::deserialize: Weights must have "
				 "one element per clause" ) );
  w.getVar( v_weights.data() );
  }
 check_weights();

 v_costs.assign( f_n_var , 0 );
 auto c = group.getVar( "Costs" );
 if( ( ! c.isNull() ) && f_n_var ) {
  if( c.getDimCount() != 1 || c.getDim( 0 ).getSize() != f_n_var )
   throw( std::invalid_argument( "SATBlock::deserialize: Costs must have "
				 "one element per variable" ) );
  c.getVar( v_costs.data() );
  if( ! std::all_of( v_costs.begin() , v_costs.end() ,
		     []( double x ) { return( std::isfinite( x ) ); } ) )
   throw( std::invalid_argument( "SATBlock::deserialize: a cost is not "
				 "finite" ) );
  }

 auto g = group.getVar( "VariableGroups" );
 if( ( ! g.isNull() ) && f_n_var ) {
  if( g.getDimCount() != 1 || g.getDim( 0 ).getSize() != f_n_var )
   throw( std::invalid_argument( "SATBlock::deserialize: VariableGroups "
				 "must have one element per variable" ) );
  std::vector< int > groups( f_n_var );
  g.getVar( groups.data() );
  set_variable_groups( std::move( groups ) );
  }

 Block::deserialize( group );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::set_variable_groups( std::vector< int > && groups )
{
 if( f_structure != kNoStructure )
  throw( std::logic_error( "SATBlock::set_variable_groups: the groups of a "
			   "SATBlock with a structure cannot change" ) );

 if( ( ! groups.empty() ) && ( groups.size() != f_n_var ) )
  throw( std::invalid_argument( "SATBlock::set_variable_groups: " +
				std::to_string( groups.size() ) +
				" groups for " + std::to_string( f_n_var ) +
				" variables" ) );

 if( std::any_of( groups.begin() , groups.end() ,
		  []( int g ) { return( g < 0 ); } ) )
  throw( std::invalid_argument( "SATBlock::set_variable_groups: a group is "
				"negative" ) );

 v_group = std::move( groups );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::set_structure( Configuration * strc )
{
 if( ( ! strc ) && f_BlockConfig )
  strc = f_BlockConfig->f_structure_Configuration;

 if( ! strc )  // nobody is choosing: the structure is left as it is
  return;

 auto ci = dynamic_cast< SimpleConfiguration< int > * >( strc );
 if( ! ci )
  throw( std::invalid_argument( "SATBlock::set_structure: the structure of "
				"a SATBlock is a SimpleConfiguration< int >" ) );

 const int type = ci->value();
 if( ( type != kNoStructure ) && ( type != kRelaxation ) &&
     ( type != kDecomposition ) )
  throw( std::invalid_argument( "SATBlock::set_structure: unknown "
				"structure " + std::to_string( type ) ) );

 guts_of_set_structure( type );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::guts_of_reset_structure( void )
{
 for( auto sub : v_Block )
  delete sub;
 v_Block.clear();

 v_local.clear();
 v_clause_linking.clear();
 v_clause_group.clear();
 v_clause_local.clear();
 v_link_clause.clear();
 v_copies.clear();
 f_structure = kNoStructure;
 }

/*--------------------------------------------------------------------------*/

void SATBlock::guts_of_set_structure( int type )
{
 static const std::string _prfx = "SATBlock::set_structure: ";

 if( AR )
  throw( std::logic_error( _prfx + "the abstract representation has been "
			   "generated" ) );

 guts_of_reset_structure();
 if( type == kNoStructure )
  return;

 if( v_group.empty() )
  throw( std::logic_error( _prfx + "the variables have no groups" ) );

 const unsigned int P = unsigned( *std::max_element( v_group.begin() ,
						     v_group.end() ) ) + 1;

 // the variables of each group, numbered in their order, with their costs
 std::vector< unsigned int > n_var( P , 0 );
 std::vector< v_Weight > costs( P );
 v_local.resize( f_n_var );
 for( unsigned int i = 0 ; i < f_n_var ; ++i ) {
  const auto g = v_group[ i ];
  v_local[ i ] = n_var[ g ]++;
  costs[ g ].push_back( v_costs[ i ] );
  }

 std::vector< v_Clause > clauses( P );
 std::vector< v_Weight > weights( P );
 v_clause_linking.assign( v_clauses.size() , false );
 v_clause_group.assign( v_clauses.size() , -1 );
 v_clause_local.assign( v_clauses.size() , Inf< unsigned int >() );

 // the literal of the original variable v in the sub-Block of its group
 auto local = [ this ]( int lit ) {
  const int l = int( v_local[ std::abs( lit ) - 1 ] ) + 1;
  return( lit > 0 ? l : - l );
  };

 // the clause goes to the sub-Block of the group g
 auto put = [ & ]( unsigned int i , unsigned int g , Clause && cl ) {
  v_clause_group[ i ] = int( g );
  v_clause_local[ i ] = clauses[ g ].size();
  clauses[ g ].push_back( std::move( cl ) );
  weights[ g ].push_back( v_weights[ i ] );
  };

 std::map< std::pair< unsigned int , unsigned int > , unsigned int > copy;

 for( unsigned int i = 0 ; i < v_clauses.size() ; ++i ) {
  const auto & cl = v_clauses[ i ];

  if( cl.empty() ) {  // never satisfied, wherever it goes
   put( i , 0 , Clause() );
   continue;
   }

  const unsigned int g0 = v_group[ std::abs( cl[ 0 ] ) - 1 ];
  if( is_tautology( i ) ) {  // always satisfied: the first literal will do
   put( i , g0 , Clause{ local( cl[ 0 ] ) , - local( cl[ 0 ] ) } );
   continue;
   }

  // how many literals in each group
  std::map< unsigned int , unsigned int > count;
  for( auto lit : cl )
   ++count[ v_group[ std::abs( lit ) - 1 ] ];

  if( count.size() == 1 ) {  // a clause of the group g0
   Clause lcl;
   lcl.reserve( cl.size() );
   for( auto lit : cl )
    lcl.push_back( local( lit ) );
   put( i , g0 , std::move( lcl ) );
   continue;
   }

  v_clause_linking[ i ] = true;

  if( type == kRelaxation ) {
   // a row of the father; the r of a soft one in the group of the first
   // literal, as a variable of its own whose cost is the weight
   v_link_clause.push_back( i );
   if( ! is_hard( i ) ) {
    v_clause_group[ i ] = int( g0 );
    v_clause_local[ i ] = n_var[ g0 ]++;
    costs[ g0 ].push_back( v_weights[ i ] );
    }
   continue;
   }

  // kDecomposition: to the group having most of its literals, the smallest
  // one in a tie, with copies of the variables of the other groups
  unsigned int g = count.begin()->first;
  for( const auto & [ h , k ] : count )
   if( k > count[ g ] )
    g = h;

  Clause lcl;
  lcl.reserve( cl.size() );
  for( auto lit : cl ) {
   const unsigned int v = std::abs( lit ) - 1;
   if( unsigned( v_group[ v ] ) == g ) {
    lcl.push_back( local( lit ) );
    continue;
    }
   auto it = copy.find( { g , v } );
   if( it == copy.end() ) {
    it = copy.emplace( std::make_pair( g , v ) , n_var[ g ]++ ).first;
    costs[ g ].push_back( 0 );
    v_copies.emplace_back( g , v , it->second );
    }
   const int l = int( it->second ) + 1;
   lcl.push_back( lit > 0 ? l : - l );
   }
  put( i , g , std::move( lcl ) );
  }

 for( unsigned int g = 0 ; g < P ; ++g ) {
  auto sub = new SATBlock( this );
  sub->load( n_var[ g ] , std::move( clauses[ g ] ) ,
	     std::move( weights[ g ] ) );
  sub->v_costs = std::move( costs[ g ] );
  add_nested_Block( sub );
  }

 f_structure = type;
 }

/*--------------------------------------------------------------------------*/

/*--------------------- Methods for handling Variable ----------------------*/
/*--------------------------------------------------------------------------*/

void SATBlock::set_violation( ColVariable & r , unsigned int i )
{
 r.set_type( ColVariable::kBinary , eNoMod );
 if( is_hard( i ) ) {  // a hard clause is never violated
  r.set_value( 0 );
  r.is_fixed( true , eNoMod );
  }
 }

/*--------------------------------------------------------------------------*/

void SATBlock::generate_abstract_variables( Configuration * stvv )
{
 if( AR & HasVar )  // the Variable are there already
  return;           // nothing to do

 if( f_structure != kNoStructure ) {  // all of them are in the sub-Block
  for( auto sub : v_Block )
   sub->generate_abstract_variables();
  AR |= HasVar;
  return;
  }

 v_x = std::vector< ColVariable >( f_n_var );
 for( auto & x : v_x )
  x.set_type( ColVariable::kBinary , eNoMod );

 v_r = std::vector< ColVariable >( v_clauses.size() );
 for( unsigned int i = 0 ; i < v_r.size() ; ++i )
  set_violation( v_r[ i ] , i );

 add_static_variable( v_x , "x" );
 add_static_variable( v_r , "r" );
 add_dynamic_variable( l_r , "added r" );
 add_dynamic_variable( l_x , "added x" );

 AR |= HasVar;
 }

/*--------------------------------------------------------------------------*/

ColVariable & SATBlock::var( unsigned int i )
{
 if( f_structure == kNoStructure ) {
  if( ( ! ( AR & HasVar ) ) || ( i >= f_n_var ) )
   throw( std::logic_error( "SATBlock::var: the ColVariable have not been "
			    "generated" ) );
  if( i < v_x.size() )
   return( v_x[ i ] );
  return( *v_added_x[ i - v_x.size() ] );
  }

 return( group_Block( v_group[ i ] ).var( v_local[ i ] ) );
 }

/*--------------------------------------------------------------------------*/

ColVariable & SATBlock::violation( unsigned int i )
{
 if( f_structure != kNoStructure ) {
  const int g = v_clause_group[ i ];
  if( g < 0 )
   throw( std::logic_error( "SATBlock::get_violation: the hard linking "
			    "clause " + std::to_string( i ) + " has no r" ) );
  auto & sub = group_Block( g );
  if( ( f_structure == kRelaxation ) && v_clause_linking[ i ] )
   return( sub.var( v_clause_local[ i ] ) );
  return( sub.violation( v_clause_local[ i ] ) );
  }

 if( i < v_r.size() )
  return( v_r[ i ] );
 return( *std::next( l_r.begin() , i - v_r.size() ) );
 }

/*--------------------------------------------------------------------------*/

const ColVariable & SATBlock::get_violation( unsigned int i ) const
{
 return( const_cast< SATBlock * >( this )->violation( i ) );
 }

/*--------------------------------------------------------------------------*/
/*-------------------- Methods for handling Constraint ---------------------*/
/*--------------------------------------------------------------------------*/

void SATBlock::set_clause_constraint( FRowConstraint & c , unsigned int i ,
				      ColVariable * r )
{
 LinearFunction::v_coeff_pair coeffs;
 double lhs = - Inf< double >();  // a tautology is always satisfied
 if( ! is_tautology( i ) ) {
  coeffs.reserve( v_clauses[ i ].size() + 1 );
  lhs = 1;
  for( auto lit : v_clauses[ i ] ) {
   coeffs.emplace_back( & var( std::abs( lit ) - 1 ) , lit > 0 ? 1 : -1 );
   if( lit < 0 )
    --lhs;
   }
  }
 coeffs.emplace_back( r , 1 );

 c.set_function( new LinearFunction( std::move( coeffs ) , 0 ) , eNoMod );
 c.set_lhs( lhs , eNoMod );
 c.set_rhs( Inf< double >() , eNoMod );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::set_father_row( FRowConstraint & c , unsigned int k )
{
 LinearFunction::v_coeff_pair coeffs;

 if( f_structure == kDecomposition ) {  // the copy equals the original
  const auto [ g , v , l ] = v_copies[ k ];
  coeffs.emplace_back( & group_Block( g ).var( l ) , 1 );
  coeffs.emplace_back( & var( v ) , -1 );
  c.set_function( new LinearFunction( std::move( coeffs ) , 0 ) , eNoMod );
  c.set_both( 0 , eNoMod );
  return;
  }

 // kRelaxation: the row of the linking clause, as in the abstract
 // representation of a SATBlock, with the r of a soft one if any
 const auto i = v_link_clause[ k ];
 double lhs = 1;
 coeffs.reserve( v_clauses[ i ].size() + 1 );
 for( auto lit : v_clauses[ i ] ) {
  coeffs.emplace_back( & var( std::abs( lit ) - 1 ) , lit > 0 ? 1 : -1 );
  if( lit < 0 )
   --lhs;
  }
 if( ! is_hard( i ) )
  coeffs.emplace_back( & violation( i ) , 1 );

 c.set_function( new LinearFunction( std::move( coeffs ) , 0 ) , eNoMod );
 c.set_lhs( lhs , eNoMod );
 c.set_rhs( Inf< double >() , eNoMod );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::generate_abstract_constraints( Configuration * stcc )
{
 if( AR & HasCns )  // the Constraint are there already
  return;           // nothing to do

 generate_abstract_variables();

 if( f_structure != kNoStructure ) {  // the rows that tie the sub-Block
  for( auto sub : v_Block )
   sub->generate_abstract_constraints();
  v_link_c = std::vector< FRowConstraint >( f_structure == kRelaxation ?
					    v_link_clause.size() :
					    v_copies.size() );
  for( unsigned int k = 0 ; k < v_link_c.size() ; ++k )
   set_father_row( v_link_c[ k ] , k );
  add_static_constraint( v_link_c , "linking" );
  AR |= HasCns;
  return;
  }

 // the clauses whose r is static have a static row too, the others a
 // dynamic one
 v_c = std::vector< FRowConstraint >( v_r.size() );
 for( unsigned int i = 0 ; i < v_r.size() ; ++i )
  set_clause_constraint( v_c[ i ] , i , & v_r[ i ] );
 unsigned int i = v_r.size();
 for( auto & r : l_r ) {
  l_c.emplace_back();
  set_clause_constraint( l_c.back() , i++ , & r );
  }

 add_static_constraint( v_c , "clauses" );
 add_dynamic_constraint( l_c , "added clauses" );

 AR |= HasCns;
 }

/*--------------------------------------------------------------------------*/

FRowConstraint & SATBlock::get_clause_constraint( unsigned int i )
{
 if( f_structure != kNoStructure ) {
  if( ( f_structure == kRelaxation ) && v_clause_linking[ i ] )
   return( v_link_c[ std::lower_bound( v_link_clause.begin() ,
				       v_link_clause.end() , i ) -
		     v_link_clause.begin() ] );
  return( group_Block( v_clause_group[ i ] ).get_clause_constraint(
						      v_clause_local[ i ] ) );
  }

 if( i < v_c.size() )
  return( v_c[ i ] );
 return( *std::next( l_c.begin() , i - v_c.size() ) );
 }

/*--------------------------------------------------------------------------*/
/*--------------------- Methods for handling Objective ---------------------*/
/*--------------------------------------------------------------------------*/

void SATBlock::generate_objective( Configuration * objc )
{
 if( AR & HasObj )  // the Objective is there already
  return;           // nothing to do

 generate_abstract_variables();

 if( f_structure != kNoStructure ) {  // all the terms are in the sub-Block
  for( auto sub : v_Block )
   sub->generate_objective();
  f_obj.set_function( new LinearFunction( LinearFunction::v_coeff_pair() ,
					  0 ) , eNoMod );
  f_obj.set_sense( Objective::eMin , eNoMod );
  set_objective( & f_obj , eNoMod );
  AR |= HasObj;
  return;
  }

 // the i-th term is the r of the i-th clause, 0 if the clause is hard, and
 // the terms of the x, with their costs, come after them
 LinearFunction::v_coeff_pair coeffs;
 coeffs.reserve( v_clauses.size() + f_n_var );
 for( unsigned int i = 0 ; i < v_clauses.size() ; ++i )
  coeffs.emplace_back( & violation( i ) , is_hard( i ) ? 0 : v_weights[ i ] );
 for( unsigned int i = 0 ; i < f_n_var ; ++i )
  coeffs.emplace_back( & var( i ) , v_costs[ i ] );

 f_obj.set_function( new LinearFunction( std::move( coeffs ) , 0 ) , eNoMod );
 f_obj.set_sense( Objective::eMin , eNoMod );
 set_objective( & f_obj , eNoMod );

 AR |= HasObj;
 }

/*--------------------------------------------------------------------------*/
/*------------------ Methods for reading the data of the SATBlock ----------*/
/*--------------------------------------------------------------------------*/

bool SATBlock::all_hard( void ) const
{
 return( std::all_of( v_weights.begin() , v_weights.end() ,
		      []( double w ) { return( w == Inf< double >() ); } ) );
 }

/*--------------------------------------------------------------------------*/

bool SATBlock::has_costs( void ) const
{
 return( std::any_of( v_costs.begin() , v_costs.end() ,
		      []( double c ) { return( c != 0 ); } ) );
 }

/*--------------------------------------------------------------------------*/

Block::Index SATBlock::violation_index( const Variable * r ) const
{
 if( ( ! v_r.empty() ) && ( r >= v_r.data() ) &&
     ( r < v_r.data() + v_r.size() ) )
  return( Index( static_cast< const ColVariable * >( r ) - v_r.data() ) );
 Index i = v_r.size();
 for( const auto & lr : l_r )
  if( & lr == r )
   return( i );
  else
   ++i;
 return( Inf< Index >() );
 }

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
 if( ! has_variables() )
  throw( std::logic_error( "SATBlock::is_feasible: the ColVariable have "
			   "not been generated" ) );

 for( unsigned int i = 0 ; i < v_clauses.size() ; ++i )
  if( is_hard( i ) &&
      std::none_of( v_clauses[ i ].begin() , v_clauses[ i ].end() ,
		    [ this ]( int lit ) {
       return( ( var( std::abs( lit ) - 1 ).get_value() > 0.5 ) ==
		( lit > 0 ) ); } ) )
   return( false );

 return( true );
 }

/*--------------------------------------------------------------------------*/

double SATBlock::get_violated_weight( void ) const
{
 if( ! has_variables() )
  throw( std::logic_error( "SATBlock::get_violated_weight: the "
			   "ColVariable have not been generated" ) );

 double sum = 0;
 for( unsigned int i = 0 ; i < v_clauses.size() ; ++i )
  if( ( ! is_hard( i ) ) &&
      std::none_of( v_clauses[ i ].begin() , v_clauses[ i ].end() ,
		    [ this ]( int lit ) {
       return( ( var( std::abs( lit ) - 1 ).get_value() > 0.5 ) ==
		( lit > 0 ) ); } ) )
   sum += v_weights[ i ];

 return( sum );
 }

/*--------------------------------------------------------------------------*/

double SATBlock::get_objective_value( void ) const
{
 double value = get_violated_weight();
 for( unsigned int i = 0 ; i < f_n_var ; ++i )
  if( var( i ).get_value() > 0.5 )
   value += v_costs[ i ];
 return( value );
 }

/*--------------------------------------------------------------------------*/

Solution * SATBlock::get_Solution( Configuration * solc , bool emptys )
{
 auto sol = new ColVariableSolution();
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

 auto nv = group.addDim( "NumberVariables" , f_n_var );
 if( has_costs() )
  group.addVar( "Costs" , netCDF::NcDouble() , nv ).putVar( v_costs.data() );
 if( ! v_group.empty() )
  group.addVar( "VariableGroups" , netCDF::NcInt() , nv ).putVar(
							    v_group.data() );

 if( v_clauses.empty() )
  return;

 std::size_t n_lit = 0;
 for( const auto & clause : v_clauses )
  n_lit += clause.size();

 auto nc = group.addDim( "NumberClauses" , v_clauses.size() );
 auto nl = group.addDim( "NumberLiterals" , n_lit );
 ::serialize< int >( group , "Clauses" , netCDF::NcInt() , "ClausesStart" ,
		     v_clauses , nl , nc );

 if( ! all_hard() )
  group.addVar( "Weights" , netCDF::NcDouble() , nc ).putVar(
							  v_weights.data() );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::print( std::ostream & output , char vlvl ) const
{
 if( vlvl != 'C' ) {
  output << "SATBlock with " << f_n_var << " variables and "
	 << v_clauses.size() << " clauses" << std::endl;
  return;
  }

 // all hard: DIMACS CNF, otherwise WCNF in the format from 2022 on, the
 // costs being unit soft clauses, and the constant they leave a comment
 const bool cnf = all_hard() && ( ! has_costs() );
 if( cnf )
  output << "p cnf " << f_n_var << " " << v_clauses.size() << std::endl;
 const auto prec = output.precision(
			     std::numeric_limits< double >::max_digits10 );
 for( unsigned int i = 0 ; i < v_clauses.size() ; ++i ) {
  if( ! cnf ) {
   if( is_hard( i ) )
    output << "h ";
   else
    output << v_weights[ i ] << " ";
   }
  for( auto lit : v_clauses[ i ] )
   output << lit << " ";
  output << "0" << std::endl;
  }
 double constant = 0;
 for( unsigned int i = 0 ; i < f_n_var ; ++i )
  if( v_costs[ i ] > 0 )  // x_i true costs c_i: the clause "not x_i"
   output << v_costs[ i ] << " -" << i + 1 << " 0" << std::endl;
  else
   if( v_costs[ i ] < 0 ) {  // x_i false costs - c_i, and c_i is paid
    output << - v_costs[ i ] << " " << i + 1 << " 0" << std::endl;
    constant += v_costs[ i ];
    }
 if( constant != 0 )
  output << "c constant " << constant << std::endl;
 output.precision( prec );
 }

/*--------------------------------------------------------------------------*/
/*----------- METHODS FOR MODIFYING THE PHYSICAL REPRESENTATION ------------*/
/*--------------------------------------------------------------------------*/

void SATBlock::set_weight( unsigned int i , double w , ModParam issueAMod )
{
 const bool was_hard = is_hard( i );
 v_weights[ i ] = w;
 if( ! ( ( AR & HasVar ) && not_dry_run( issueAMod ) ) )
  return;

 // the abstract representation: the r of a hard clause is fixed to 0, and
 // its coefficient in the Objective is 0
 auto & r = violation( i );
 if( was_hard != is_hard( i ) ) {
  if( is_hard( i ) ) {
   r.set_value( 0 );
   r.is_fixed( true , un_ModBlock( issueAMod ) );
   }
  else
   r.is_fixed( false , un_ModBlock( issueAMod ) );
  }
 if( AR & HasObj ) {
  auto lf = static_cast< LinearFunction * >( f_obj.get_function() );
  lf->modify_coefficient( lf->is_active( & r ) , is_hard( i ) ? 0 : w ,
			  un_ModBlock( issueAMod ) );
  }
 }

/*--------------------------------------------------------------------------*/

void SATBlock::set_cost( unsigned int i , double c , ModParam issueAMod )
{
 v_costs[ i ] = c;
 if( ( AR & HasObj ) && not_dry_run( issueAMod ) ) {
  auto lf = static_cast< LinearFunction * >( f_obj.get_function() );
  lf->modify_coefficient( lf->is_active( & var( i ) ) , c ,
			  un_ModBlock( issueAMod ) );
  }
 }

/*--------------------------------------------------------------------------*/

void SATBlock::chg_costs( MF_dbl_sp NCost , Range rng ,
			  ModParam issueMod , ModParam issueAMod )
{
 rng.second = std::min( rng.second , Index( f_n_var ) );
 if( rng.second <= rng.first )  // nothing to change
  return;

 if( NCost.size() < rng.second - rng.first )
  throw( std::invalid_argument( "SATBlock::chg_costs: the span is shorter "
				"than the Range" ) );

 if( f_structure != kNoStructure ) {  // the costs are in the sub-Block
  Subset nms( rng.second - rng.first );
  std::iota( nms.begin() , nms.end() , rng.first );
  chg_costs( NCost , std::move( nms ) , true , issueMod , issueAMod );
  return;
  }

 if( std::equal( NCost.begin() , NCost.begin() + ( rng.second - rng.first ) ,
		 v_costs.begin() + rng.first ) )
  return;  // nothing changes, avoid issuing the Modification

 for( Index i = rng.first ; i < rng.second ; ++i )
  if( ! std::isfinite( NCost[ i - rng.first ] ) )
   throw( std::invalid_argument( "SATBlock::chg_costs: cost " +
				 std::to_string( NCost[ i - rng.first ] ) +
				 " of variable " + std::to_string( i ) ) );

 if( not_dry_run( issueMod ) )
  for( Index i = rng.first ; i < rng.second ; ++i )
   set_cost( i , NCost[ i - rng.first ] , issueAMod );

 if( issue_pmod( issueMod ) )
  add_Modification( std::make_shared< SATBlockRngdMod >( this ,
					SATBlockMod::eChgCost , rng ) ,
		    Observer::par2chnl( issueMod ) );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::chg_costs( MF_dbl_sp NCost , Subset && nms , bool ordered ,
			  ModParam issueMod , ModParam issueAMod )
{
 if( nms.empty() )  // nothing to change
  return;

 if( NCost.size() < nms.size() )
  throw( std::invalid_argument( "SATBlock::chg_costs: the span is shorter "
				"than the Subset" ) );

 // the Subset ordered, with its costs
 std::vector< std::pair< Index , double > > nc( nms.size() );
 for( Index k = 0 ; k < nms.size() ; ++k ) {
  if( nms[ k ] >= f_n_var )
   throw( std::invalid_argument( "SATBlock::chg_costs: variable " +
				 std::to_string( nms[ k ] ) +
				 " does not exist" ) );
  if( ! std::isfinite( NCost[ k ] ) )
   throw( std::invalid_argument( "SATBlock::chg_costs: cost " +
				 std::to_string( NCost[ k ] ) +
				 " of variable " + std::to_string( nms[ k ] ) ) );
  nc[ k ] = { nms[ k ] , NCost[ k ] };
  }
 if( ! ordered )
  std::sort( nc.begin() , nc.end() , []( const auto & a , const auto & b ) {
   return( a.first < b.first ); } );

 if( std::all_of( nc.begin() , nc.end() , [ this ]( const auto & p ) {
      return( v_costs[ p.first ] == p.second ); } ) )
  return;  // nothing changes, avoid issuing the Modification

 if( not_dry_run( issueMod ) ) {
  if( f_structure == kNoStructure )
   for( const auto & [ i , c ] : nc )
    set_cost( i , c , issueAMod );
  else {
   // each cost to the variable of the sub-Block of its group, which issues
   // the Modification of its own representations
   std::map< unsigned int , std::pair< Subset , std::vector< double > > > sc;
   for( const auto & [ i , c ] : nc ) {
    v_costs[ i ] = c;
    auto & [ loc , val ] = sc[ v_group[ i ] ];
    loc.push_back( v_local[ i ] );
    val.push_back( c );
    }
   for( auto & [ g , lv ] : sc )
    group_Block( g ).chg_costs( lv.second , std::move( lv.first ) , true ,
				issueMod , issueAMod );
   }
  }

 if( issue_pmod( issueMod ) ) {
  for( Index k = 0 ; k < nc.size() ; ++k )
   nms[ k ] = nc[ k ].first;
  add_Modification( std::make_shared< SATBlockSbstMod >( this ,
			     SATBlockMod::eChgCost , std::move( nms ) ) ,
		    Observer::par2chnl( issueMod ) );
  }
 }

/*--------------------------------------------------------------------------*/
/*------------------ METHODS FOR HANDLING MODIFICATIONS --------------------*/
/*--------------------------------------------------------------------------*/

void SATBlock::add_Modification( sp_Mod mod , ChnlName chnl )
{
 // a change of the coefficients of the Objective, e.g., the Lagrangian term
 // a LagBFunction writes there, is a change of the costs of the x and of the
 // weights of the soft clauses, which the physical representation follows
 if( AR & HasObj ) {
  auto lf = static_cast< LinearFunction * >( f_obj.get_function() );
  Subset idx;
  if( auto rm = std::dynamic_pointer_cast< const C05FunctionModLinRngd >(
									mod ) ) {
   if( rm->function() == lf )
    for( auto k = rm->range().first ; k < rm->range().second ; ++k )
     idx.push_back( k );
   }
  else
   if( auto sm = std::dynamic_pointer_cast< const C05FunctionModLinSbst >(
									mod ) )
    if( sm->function() == lf )
     idx.assign( sm->subset().begin() , sm->subset().end() );

  if( ! idx.empty() ) {
   Subset xs , cs;
   std::vector< double > xc , cw;
   for( auto k : idx ) {
    const auto var = lf->get_active_var( k );
    const double coeff = lf->get_coefficient( k );
    if( ( ! v_x.empty() ) && ( var >= v_x.data() ) &&
	( var < v_x.data() + v_x.size() ) ) {
     xs.push_back( Index( static_cast< const ColVariable * >( var ) -
			  v_x.data() ) );
     xc.push_back( coeff );
     continue;
     }
    if( const auto ax = m_added_x.find( var ) ; ax != m_added_x.end() ) {
     xs.push_back( ax->second );
     xc.push_back( coeff );
     continue;
     }
    const auto c = violation_index( var );
    if( c == Inf< Index >() )
     throw( std::logic_error( "SATBlock::add_Modification: a variable of "
			      "the Objective is neither an x nor an r" ) );
    if( ! is_hard( c ) ) {  // the r of a hard clause is fixed to 0
     cs.push_back( c );
     cw.push_back( coeff );
     }
    }
   // the physical representation only, the abstract one being changed
   if( ! xs.empty() )
    chg_costs( xc , std::move( xs ) , false , make_par( eNoBlck , chnl ) ,
	       eDryRun );
   if( ! cs.empty() )
    chg_weights( cw , std::move( cs ) , false , make_par( eNoBlck , chnl ) ,
		 eDryRun );
   }
  }

 Block::add_Modification( mod , chnl );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::chg_weights( MF_dbl_sp NWeight , Range rng ,
			    ModParam issueMod , ModParam issueAMod )
{
 rng.second = std::min( rng.second , Index( v_clauses.size() ) );
 if( rng.second <= rng.first )  // nothing to change
  return;

 if( NWeight.size() < rng.second - rng.first )
  throw( std::invalid_argument( "SATBlock::chg_weights: the span is shorter "
				"than the Range" ) );

 if( f_structure != kNoStructure ) {  // the weights are in the sub-Block
  Subset nms( rng.second - rng.first );
  std::iota( nms.begin() , nms.end() , rng.first );
  chg_weights( NWeight , std::move( nms ) , true , issueMod , issueAMod );
  return;
  }

 if( std::equal( NWeight.begin() , NWeight.begin() +
		 ( rng.second - rng.first ) , v_weights.begin() + rng.first ) )
  return;  // nothing changes, avoid issuing the Modification

 for( Index i = rng.first ; i < rng.second ; ++i )
  if( std::isnan( NWeight[ i - rng.first ] ) ||
      ( NWeight[ i - rng.first ] < 0 ) )
   throw( std::invalid_argument( "SATBlock::chg_weights: weight " +
				 std::to_string( NWeight[ i - rng.first ] ) +
				 " of clause " + std::to_string( i ) ) );

 if( not_dry_run( issueMod ) )
  for( Index i = rng.first ; i < rng.second ; ++i )
   set_weight( i , NWeight[ i - rng.first ] , issueAMod );

 if( issue_pmod( issueMod ) )
  add_Modification( std::make_shared< SATBlockRngdMod >( this ,
					SATBlockMod::eChgWeight , rng ) ,
		    Observer::par2chnl( issueMod ) );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::chg_weights( MF_dbl_sp NWeight , Subset && nms , bool ordered ,
			    ModParam issueMod , ModParam issueAMod )
{
 if( nms.empty() )  // nothing to change
  return;

 if( NWeight.size() < nms.size() )
  throw( std::invalid_argument( "SATBlock::chg_weights: the span is shorter "
				"than the Subset" ) );

 // the Subset ordered, with its weights
 std::vector< std::pair< Index , double > > nw( nms.size() );
 for( Index k = 0 ; k < nms.size() ; ++k ) {
  if( nms[ k ] >= v_clauses.size() )
   throw( std::invalid_argument( "SATBlock::chg_weights: clause " +
				 std::to_string( nms[ k ] ) +
				 " does not exist" ) );
  if( std::isnan( NWeight[ k ] ) || ( NWeight[ k ] < 0 ) )
   throw( std::invalid_argument( "SATBlock::chg_weights: weight " +
				 std::to_string( NWeight[ k ] ) +
				 " of clause " + std::to_string( nms[ k ] ) ) );
  nw[ k ] = { nms[ k ] , NWeight[ k ] };
  }
 if( ! ordered )
  std::sort( nw.begin() , nw.end() , []( const auto & a , const auto & b ) {
   return( a.first < b.first ); } );

 if( std::all_of( nw.begin() , nw.end() , [ this ]( const auto & p ) {
      return( v_weights[ p.first ] == p.second ); } ) )
  return;  // nothing changes, avoid issuing the Modification

 // the r of a linking clause of kRelaxation exists only if it is soft
 if( f_structure == kRelaxation )
  for( const auto & [ i , w ] : nw )
   if( v_clause_linking[ i ] &&
       ( is_hard( i ) != ( w == Inf< double >() ) ) )
    throw( std::logic_error( "SATBlock::chg_weights: the linking clause " +
			     std::to_string( i ) + " cannot turn " +
			     ( is_hard( i ) ? "soft" : "hard" ) +
			     " in the kRelaxation structure" ) );

 if( not_dry_run( issueMod ) ) {
  if( f_structure == kNoStructure )
   for( const auto & [ i , w ] : nw )
    set_weight( i , w , issueAMod );
  else {
   // each weight to the clause of the sub-Block it went to, or to the cost
   // of its r if it is a linking one of kRelaxation
   std::map< unsigned int , std::pair< Subset , std::vector< double > > >
    sw , sc;
   for( const auto & [ i , w ] : nw ) {
    v_weights[ i ] = w;
    if( ( f_structure == kRelaxation ) && v_clause_linking[ i ] ) {
     if( w == Inf< double >() )  // hard, and it was: nothing else
      continue;
     auto & [ loc , val ] = sc[ v_clause_group[ i ] ];
     loc.push_back( v_clause_local[ i ] );
     val.push_back( w );
     }
    else {
     auto & [ loc , val ] = sw[ v_clause_group[ i ] ];
     loc.push_back( v_clause_local[ i ] );
     val.push_back( w );
     }
    }
   for( auto & [ g , lv ] : sw )
    group_Block( g ).chg_weights( lv.second , std::move( lv.first ) , false ,
				  issueMod , issueAMod );
   for( auto & [ g , lv ] : sc )
    group_Block( g ).chg_costs( lv.second , std::move( lv.first ) , false ,
				issueMod , issueAMod );
   }
  }

 if( issue_pmod( issueMod ) ) {
  for( Index k = 0 ; k < nw.size() ; ++k )
   nms[ k ] = nw[ k ].first;
  add_Modification( std::make_shared< SATBlockSbstMod >( this ,
			     SATBlockMod::eChgWeight , std::move( nms ) ) ,
		    Observer::par2chnl( issueMod ) );
  }
 }

/*--------------------------------------------------------------------------*/

void SATBlock::add_clauses( v_Clause && clauses , v_Weight && weights ,
			    ModParam issueMod , ModParam issueAMod )
{
 if( clauses.empty() )  // nothing to add
  return;

 if( ( ! weights.empty() ) && ( weights.size() != clauses.size() ) )
  throw( std::invalid_argument( "SATBlock::add_clauses: " +
				std::to_string( weights.size() ) +
				" weights for " +
				std::to_string( clauses.size() ) +
				" clauses" ) );

 if( ! not_dry_run( issueMod ) )
  return;

 const Index first = v_clauses.size();
 const auto n_new = clauses.size();

 // the physical representation, left as it was if anything is wrong
 v_clauses.insert( v_clauses.end() ,
		   std::make_move_iterator( clauses.begin() ) ,
		   std::make_move_iterator( clauses.end() ) );
 if( weights.empty() )
  v_weights.resize( v_clauses.size() , Inf< double >() );
 else
  v_weights.insert( v_weights.end() , weights.begin() , weights.end() );
 try {
  normalize_clauses( first );
  check_weights( first );
  }
 catch( ... ) {
  v_clauses.resize( first );
  v_weights.resize( first );
  throw;
  }

 if( f_structure != kNoStructure ) {
  // each new clause goes to the sub-Block of its group, a linking one
  // having nowhere to go
  for( Index i = first ; i < v_clauses.size() ; ++i ) {
   const auto & cl = v_clauses[ i ];
   if( cl.empty() || is_tautology( i ) )
    continue;
   const auto g = v_group[ std::abs( cl[ 0 ] ) - 1 ];
   if( std::any_of( cl.begin() , cl.end() , [ this , g ]( int lit ) {
	return( v_group[ std::abs( lit ) - 1 ] != g ); } ) ) {
    v_clauses.resize( first );
    v_weights.resize( first );
    throw( std::logic_error( "SATBlock::add_clauses: the clause " +
			     std::to_string( i - first ) + " links groups, "
			     "which a SATBlock with a structure cannot add" ) );
    }
   }

  std::map< unsigned int , std::pair< v_Clause , v_Weight > > sub;
  for( Index i = first ; i < v_clauses.size() ; ++i ) {
   const auto & cl = v_clauses[ i ];
   const unsigned int g = cl.empty() ? 0 : v_group[ std::abs( cl[ 0 ] ) - 1 ];
   auto & [ scl , sw ] = sub[ g ];
   Clause lcl;
   if( is_tautology( i ) ) {
    const int l = int( v_local[ std::abs( cl[ 0 ] ) - 1 ] ) + 1;
    lcl = { l , - l };
    }
   else
    for( auto lit : cl ) {
     const int l = int( v_local[ std::abs( lit ) - 1 ] ) + 1;
     lcl.push_back( lit > 0 ? l : - l );
     }
   v_clause_linking.push_back( false );
   v_clause_group.push_back( int( g ) );
   v_clause_local.push_back( group_Block( g ).get_number_clauses() +
			     scl.size() );
   scl.push_back( std::move( lcl ) );
   sw.push_back( v_weights[ i ] );
   }
  for( auto & [ g , cw ] : sub )
   group_Block( g ).add_clauses( std::move( cw.first ) ,
				 std::move( cw.second ) , issueMod , issueAMod );

  if( issue_pmod( issueMod ) )
   add_Modification( std::make_shared< SATBlockRngdMod >( this ,
				     SATBlockMod::eAddClauses ,
				     Range( first , v_clauses.size() ) ) ,
		     Observer::par2chnl( issueMod ) );
  return;
  }

 // the abstract representation: the r of the new clauses, their rows and
 // their terms in the Objective
 if( ( AR & HasVar ) && not_dry_run( issueAMod ) ) {
  std::list< ColVariable > nr( n_new );
  Index i = first;
  for( auto & r : nr )
   set_violation( r , i++ );
  add_dynamic_variables( l_r , nr , un_ModBlock( issueAMod ) );

  // the new r are the last ones of l_r
  auto rit = std::prev( l_r.end() , long( n_new ) );
  if( AR & HasCns ) {
   std::list< FRowConstraint > nl( n_new );
   i = first;
   auto it = rit;
   for( auto & c : nl )
    set_clause_constraint( c , i++ , & *(it++) );
   add_dynamic_constraints( l_c , nl , un_ModBlock( issueAMod ) );
   }
  if( AR & HasObj ) {
   LinearFunction::v_coeff_pair coeffs;
   coeffs.reserve( n_new );
   i = first;
   for( auto it = rit ; it != l_r.end() ; ++it , ++i )
    coeffs.emplace_back( & *it , is_hard( i ) ? 0 : v_weights[ i ] );
   static_cast< LinearFunction * >( f_obj.get_function() )->add_variables(
				   std::move( coeffs ) , un_ModBlock( issueAMod ) );
   }
  }

 if( issue_pmod( issueMod ) )
  add_Modification( std::make_shared< SATBlockRngdMod >( this ,
			     SATBlockMod::eAddClauses ,
			     Range( first , v_clauses.size() ) ) ,
		    Observer::par2chnl( issueMod ) );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::add_variables( unsigned int k , ModParam issueMod ,
			      ModParam issueAMod )
{
 if( k == 0 )  // nothing to add
  return;

 if( f_structure != kNoStructure )
  throw( std::logic_error( "SATBlock::add_variables: a SATBlock with a "
			   "structure cannot add variables" ) );

 if( ! not_dry_run( issueMod ) )
  return;

 const Index first = f_n_var;
 f_n_var += k;
 v_costs.resize( f_n_var , 0 );

 // the abstract representation: the new x, and their terms in the Objective
 if( ( AR & HasVar ) && not_dry_run( issueAMod ) ) {
  std::list< ColVariable > nx( k );
  for( auto & x : nx )
   x.set_type( ColVariable::kBinary , eNoMod );
  add_dynamic_variables( l_x , nx , un_ModBlock( issueAMod ) );

  // the new x are the last ones of l_x
  Index i = first;
  for( auto it = std::prev( l_x.end() , long( k ) ) ; it != l_x.end() ;
       ++it , ++i ) {
   v_added_x.push_back( & *it );
   m_added_x.emplace( & *it , i );
   }

  if( AR & HasObj ) {
   LinearFunction::v_coeff_pair coeffs;
   coeffs.reserve( k );
   for( auto it = std::prev( l_x.end() , long( k ) ) ; it != l_x.end() ;
	++it )
    coeffs.emplace_back( & *it , 0 );
   static_cast< LinearFunction * >( f_obj.get_function() )->add_variables(
				   std::move( coeffs ) , un_ModBlock( issueAMod ) );
   }
  }

 if( issue_pmod( issueMod ) )
  add_Modification( std::make_shared< SATBlockRngdMod >( this ,
			     SATBlockMod::eAddVariables ,
			     Range( first , f_n_var ) ) ,
		    Observer::par2chnl( issueMod ) );
 }

/*--------------------------------------------------------------------------*/

void SATBlock::guts_of_destructor( void )
{
 // the rows and the Objective go before the ColVariable they are active in,
 // and are told not to bother with them, both going away together
 for( auto & c : v_c )
  c.clear();
 for( auto & c : l_c )
  c.clear();
 for( auto & c : v_link_c )
  c.clear();
 f_obj.clear();

 reset_objective();
 reset_dynamic_constraints();
 reset_static_constraints();
 reset_dynamic_variables();
 reset_static_variables();
 l_c.clear();
 v_c.clear();
 v_link_c.clear();
 l_r.clear();
 v_r.clear();
 v_x.clear();
 l_x.clear();
 v_added_x.clear();
 m_added_x.clear();
 guts_of_reset_structure();
 v_group.clear();

 v_clauses.clear();
 v_weights.clear();
 f_n_var = 0;
 AR = 0;
 }

/*--------------------------------------------------------------------------*/
/*------------------------ METHODS OF SATBlockChange -----------------------*/
/*--------------------------------------------------------------------------*/

// the x are the abstract representation, and there is no physical one of
// the fixings: issueAMod is what goes with them
Change * SATBlockChange::apply( Block * block , bool doUndo ,
				ModParam , ModParam issueAMod )
{
 auto sat = dynamic_cast< SATBlock * >( block );
 if( ! sat )
  throw( std::invalid_argument( "SATBlockChange::apply: the Block is not a "
				"SATBlock" ) );
 if( ( f_type != eFixX ) && ( f_type != eUnfixX ) )
  throw( std::invalid_argument( "SATBlockChange::apply: empty "
				"SATBlockChange" ) );
 if( ( f_type == eFixX ) && ( f_values.size() != f_nms.size() ) )
  throw( std::invalid_argument( "SATBlockChange::apply: " +
				std::to_string( f_values.size() ) +
				" values for " + std::to_string( f_nms.size() )
				+ " variables" ) );

 sat->generate_abstract_variables();
 auto x = [ sat ]( Block::Index i ) -> ColVariable & {
  return( sat->var( i ) ); };

 // all checked first, so that nothing is changed if anything is wrong
 for( auto i : f_nms ) {
  if( i >= sat->get_number_variables() )
   throw( std::invalid_argument( "SATBlockChange::apply: variable " +
				 std::to_string( i ) + " out of range" ) );
  if( x( i ).is_fixed() == ( f_type == eFixX ) )
   throw( std::invalid_argument( "SATBlockChange::apply: variable " +
				 std::to_string( i ) + ( f_type == eFixX ?
							 " already fixed" :
							 " not fixed" ) ) );
  }

 Change * undo = nullptr;
 if( doUndo ) {
  if( f_type == eFixX )
   undo = new SATBlockChange( eUnfixX , Block::Subset( f_nms ) );
  else {
   std::vector< double > old;
   old.reserve( f_nms.size() );
   for( auto i : f_nms )
    old.push_back( x( i ).get_value() );
   undo = new SATBlockChange( eFixX , Block::Subset( f_nms ) ,
			      std::move( old ) );
   }
  }

 // the values go before the fixing, which set_value() of a fixed Variable
 // would not allow
 for( std::size_t k = 0 ; k < f_nms.size() ; ++k ) {
  auto & xi = x( f_nms[ k ] );
  if( f_type == eFixX ) {
   xi.set_value( f_values[ k ] > 0.5 ? 1 : 0 );
   xi.is_fixed( true , issueAMod );
   }
  else
   xi.is_fixed( false , issueAMod );
  }

 return( undo );
 }

/*--------------------------------------------------------------------------*/

void SATBlockChange::deserialize( const netCDF::NcGroup & group )
{
 auto ftype = group.getAtt( "SATBlockChange_type" );
 if( ftype.isNull() )
  throw( std::invalid_argument( "SATBlockChange::deserialize: no type" ) );
 ftype.getValues( & f_type );

 f_nms.clear();
 f_values.clear();
 const auto nv = group.getDim( "NumVar" );
 if( nv.isNull() )
  return;
 f_nms.resize( nv.getSize() );
 group.getVar( "Index" ).getVar( f_nms.data() );
 const auto val = group.getVar( "Value" );
 if( ! val.isNull() ) {
  f_values.resize( nv.getSize() );
  val.getVar( f_values.data() );
  }
 }

/*--------------------------------------------------------------------------*/

void SATBlockChange::serialize( netCDF::NcGroup & group ) const
{
 Change::serialize( group );
 group.putAtt( "SATBlockChange_type" , netCDF::NcInt() , f_type );
 const auto nv = group.addDim( "NumVar" , f_nms.size() );
 group.addVar( "Index" , netCDF::NcUint() , nv ).putVar( f_nms.data() );
 if( f_type == eFixX )
  group.addVar( "Value" , netCDF::NcDouble() , nv ).putVar(
							     f_values.data() );
 }

/*--------------------------------------------------------------------------*/
/*------------------------- End File SATBlock.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
