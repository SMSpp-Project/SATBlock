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

 Block::deserialize( group );
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

 v_x = std::vector< ColVariable >( f_n_var );
 for( auto & x : v_x )
  x.set_type( ColVariable::kBinary , eNoMod );

 v_r = std::vector< ColVariable >( v_clauses.size() );
 for( unsigned int i = 0 ; i < v_r.size() ; ++i )
  set_violation( v_r[ i ] , i );

 add_static_variable( v_x , "x" );
 add_static_variable( v_r , "r" );
 add_dynamic_variable( l_r , "added r" );

 AR |= HasVar;
 }

/*--------------------------------------------------------------------------*/

ColVariable & SATBlock::violation( unsigned int i )
{
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
   coeffs.emplace_back( & v_x[ std::abs( lit ) - 1 ] , lit > 0 ? 1 : -1 );
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

void SATBlock::generate_abstract_constraints( Configuration * stcc )
{
 if( AR & HasCns )  // the Constraint are there already
  return;           // nothing to do

 generate_abstract_variables();

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

 // the i-th term is the r of the i-th clause, 0 if the clause is hard, and
 // the terms of the x, with their costs, come after them
 LinearFunction::v_coeff_pair coeffs;
 coeffs.reserve( v_clauses.size() + f_n_var );
 for( unsigned int i = 0 ; i < v_clauses.size() ; ++i )
  coeffs.emplace_back( & violation( i ) , is_hard( i ) ? 0 : v_weights[ i ] );
 for( unsigned int i = 0 ; i < f_n_var ; ++i )
  coeffs.emplace_back( & v_x[ i ] , v_costs[ i ] );

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
 if( v_x.size() != f_n_var )
  throw( std::logic_error( "SATBlock::is_feasible: the ColVariable have "
			   "not been generated" ) );

 for( unsigned int i = 0 ; i < v_clauses.size() ; ++i )
  if( is_hard( i ) &&
      std::none_of( v_clauses[ i ].begin() , v_clauses[ i ].end() ,
		    [ this ]( int lit ) {
       return( ( v_x[ std::abs( lit ) - 1 ].get_value() > 0.5 ) ==
		( lit > 0 ) ); } ) )
   return( false );

 return( true );
 }

/*--------------------------------------------------------------------------*/

double SATBlock::get_violated_weight( void ) const
{
 if( v_x.size() != f_n_var )
  throw( std::logic_error( "SATBlock::get_violated_weight: the "
			   "ColVariable have not been generated" ) );

 double sum = 0;
 for( unsigned int i = 0 ; i < v_clauses.size() ; ++i )
  if( ( ! is_hard( i ) ) &&
      std::none_of( v_clauses[ i ].begin() , v_clauses[ i ].end() ,
		    [ this ]( int lit ) {
       return( ( v_x[ std::abs( lit ) - 1 ].get_value() > 0.5 ) ==
		( lit > 0 ) ); } ) )
   sum += v_weights[ i ];

 return( sum );
 }

/*--------------------------------------------------------------------------*/

double SATBlock::get_objective_value( void ) const
{
 double value = get_violated_weight();
 for( unsigned int i = 0 ; i < f_n_var ; ++i )
  if( v_x[ i ].get_value() > 0.5 )
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
  lf->modify_coefficient( lf->is_active( & v_x[ i ] ) , c ,
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

 if( not_dry_run( issueMod ) )
  for( const auto & [ i , c ] : nc )
   set_cost( i , c , issueAMod );

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

 if( not_dry_run( issueMod ) )
  for( const auto & [ i , w ] : nw )
   set_weight( i , w , issueAMod );

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

void SATBlock::guts_of_destructor( void )
{
 // the rows and the Objective go before the ColVariable they are active in,
 // and are told not to bother with them, both going away together
 for( auto & c : v_c )
  c.clear();
 for( auto & c : l_c )
  c.clear();
 f_obj.clear();

 reset_objective();
 reset_dynamic_constraints();
 reset_static_constraints();
 reset_dynamic_variables();
 reset_static_variables();
 l_c.clear();
 v_c.clear();
 l_r.clear();
 v_r.clear();
 v_x.clear();

 v_clauses.clear();
 v_weights.clear();
 f_n_var = 0;
 AR = 0;
 }

/*--------------------------------------------------------------------------*/
/*------------------------- End File SATBlock.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
