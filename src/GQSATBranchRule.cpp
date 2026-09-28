/*--------------------------------------------------------------------------*/
/*----------------------- File GQSATBranchRule.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Implementation of the GQSATBranchRule class.
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*---------------------------- IMPLEMENTATION ------------------------------*/
/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <cstdlib>
#include <vector>

#include "GQSATBranchRule.h"

/*--------------------------------------------------------------------------*/
/*------------------------- NAMESPACE AND USING ----------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- FUNCTIONS -------------------------------*/
/*--------------------------------------------------------------------------*/

// the symbol a linker that drops the libraries no symbol is taken from can
// be asked to keep, so that the rule below is registered

SMSpp_define_force_load( SATBlockML )

// register GQSATBranchRule among the rules of SATSolver::branch()

static const bool GQSATBranchRule_added = SATBranchRule::add( "GQSAT" ,
 []( void ) -> SATBranchRule * { return( new GQSATBranchRule() ); } );

/*--------------------------------------------------------------------------*/
/*---------------------- METHODS OF GQSATBranchRule ------------------------*/
/*--------------------------------------------------------------------------*/

void GQSATBranchRule::load( const std::string & file )
{
 set_policy( torch::jit::load( file ) );
 }

/*--------------------------------------------------------------------------*/

void GQSATBranchRule::set_policy( const torch::jit::Module & policy )
{
 f_policy = std::make_unique< torch::jit::Module >( policy );
 f_policy->eval();
 }

/*--------------------------------------------------------------------------*/

bool GQSATBranchRule::choose( const SATBlock & sat , unsigned int & var ,
			      double & first )
{
 if( ! f_policy )
  return( false );

 // the unfixed variables, numbered from 0 in their order
 const auto & x = sat.get_variables();
 std::vector< long > vertex( x.size() , -1 );
 std::vector< unsigned int > orig;
 for( unsigned int i = 0 ; i < x.size() ; ++i )
  if( ! x[ i ].is_fixed() ) {
   vertex[ i ] = long( orig.size() );
   orig.push_back( i );
   }
 const long nv = long( orig.size() );
 if( nv == 0 )
  return( false );

 // the clauses no fixed variable satisfies, with their literals on the
 // unfixed ones, the others being false
 std::vector< int64_t > src , dst;
 std::vector< float > positive;
 long nc = 0;
 std::vector< int > kept;
 for( const auto & clause : sat.get_clauses() ) {
  kept.clear();
  bool satisfied = false;
  for( auto lit : clause ) {
   const auto & xi = x[ std::abs( lit ) - 1 ];
   if( ! xi.is_fixed() )
    kept.push_back( lit );
   else
    if( ( xi.get_value() > 0.5 ) == ( lit > 0 ) ) {
     satisfied = true;
     break;
     }
   }
  if( satisfied || kept.empty() )
   continue;
  const long c = nv + nc++;
  for( auto lit : kept ) {
   const long v = vertex[ std::abs( lit ) - 1 ];
   src.push_back( v ); dst.push_back( c );
   src.push_back( c ); dst.push_back( v );
   positive.push_back( lit > 0 ? 1 : 0 );
   positive.push_back( lit > 0 ? 1 : 0 );
   }
  }
 if( nc == 0 )
  return( false );

 const long ne = long( src.size() );
 auto xt = torch::zeros( { nv + nc , 2 } );
 xt.index_put_( { torch::indexing::Slice( 0 , nv ) , 0 } , 1 );
 xt.index_put_( { torch::indexing::Slice( nv , nv + nc ) , 1 } , 1 );
 auto ei = torch::empty( { 2 , ne } , torch::kLong );
 auto ea = torch::zeros( { ne , 2 } );
 auto eip = ei.accessor< int64_t , 2 >();
 auto eap = ea.accessor< float , 2 >();
 for( long j = 0 ; j < ne ; ++j ) {
  eip[ 0 ][ j ] = src[ j ];
  eip[ 1 ][ j ] = dst[ j ];
  eap[ j ][ positive[ j ] > 0.5 ? 1 : 0 ] = 1;
  }
 auto u = torch::zeros( { 1 , 1 } );

 torch::NoGradGuard no_grad;
 const auto q = f_policy->forward( { xt , ei , ea , u } ).toTensor();

 // the rows of the variables, "true" then "false" for each
 const auto a = q.index( { torch::indexing::Slice( 0 , nv ) } ).flatten()
		 .argmax().item< int64_t >();
 var = orig[ a / 2 ];
 first = ( a % 2 == 0 ) ? 1 : 0;
 return( true );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File GQSATBranchRule.cpp ----------------------*/
/*--------------------------------------------------------------------------*/
