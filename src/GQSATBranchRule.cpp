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
 f_features = f_policy->hasattr( "features" ) ?
	      int( f_policy->attr( "features" ).toInt() ) :
	      int( SATResidualGraph::eGQSAT );
 }

/*--------------------------------------------------------------------------*/

bool GQSATBranchRule::choose( const SATSolver & solver , unsigned int & var ,
			      double & first )
{
 if( ! f_policy )
  return( false );

 SATResidualGraph g;
 if( ! g.build( solver , f_features ) )
  return( false );

 const long nv = g.n_var + g.n_clause;
 const long ne = long( g.source.size() );
 auto xt = torch::from_blob( g.vertex.data() , { nv , long( g.n_col ) } ,
			     torch::kFloat ).clone();
 auto ei = torch::stack( {
  torch::from_blob( g.source.data() , { ne } , torch::kLong ) ,
  torch::from_blob( g.target.data() , { ne } , torch::kLong ) } ).clone();
 auto ea = torch::from_blob( g.edge.data() , { ne , 2 } ,
			     torch::kFloat ).clone();
 auto u = torch::zeros( { 1 , 1 } );

 torch::NoGradGuard no_grad;
 const auto q = f_policy->forward( { xt , ei , ea , u } ).toTensor();

 // the rows of the variables, "true" then "false" for each
 const auto a = q.index( { torch::indexing::Slice( 0 , g.n_var ) } )
		 .flatten().argmax().item< int64_t >();
 var = g.var[ a / 2 ];
 first = ( a % 2 == 0 ) ? 1 : 0;
 return( true );
 }

/*--------------------------------------------------------------------------*/
/*---------------------- End File GQSATBranchRule.cpp ----------------------*/
/*--------------------------------------------------------------------------*/
