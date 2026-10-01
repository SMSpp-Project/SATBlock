/*--------------------------------------------------------------------------*/
/*----------------------------- File satgen.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Generator of random weighted partial MaxSAT instances made of groups: k
 * groups of n variables each, every group with its own random hard and soft
 * clauses, and a number of hard *linking* clauses whose literals come from
 * different groups. The fraction of the linking clauses among the hard ones
 * says how loosely the groups are bound together, from independent groups
 * (fraction 0) to a formula where the groups are hardly recognizable.
 *
 * The variables of the g-th group (from 0) are g * n + 1, ..., ( g + 1 ) * n
 * in the DIMACS convention. The instance is written in the WCNF format from
 * 2022 on (the DIMACS CNF one if there are no soft clauses), whose comment
 * lines record how it was generated, or as a SATBlock in a netCDF file if the
 * name of the output file ends in .nc4, with the same record in the
 * attribute "satgen" of the file and the groups of the variables in the
 * SATBlock [see SATBlock::set_variable_groups()].
 *
 * The random numbers come from std::mt19937_64 with bounded draws done here,
 * not by the std:: distributions, so that the same seed gives the same
 * instance with any compiler.
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
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <getopt.h>

#include <SATBlock.h>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/
/// the random numbers, the same with any compiler for the same seed

class Rng
{
 public:

 explicit Rng( std::uint64_t seed ) : f_gen( seed ) {}

 /// a uniform integer in [ 0 , m ), by rejection
 std::uint64_t below( std::uint64_t m ) {
  const std::uint64_t lim = std::mt19937_64::max() -
                            ( std::mt19937_64::max() % m + 1 ) % m;
  std::uint64_t r;
  do
   r = f_gen();
  while( r > lim );
  return( r % m );
  }

 /// true or false with the same probability
 bool coin( void ) { return( f_gen() >> 63 ); }

 private:

 std::mt19937_64 f_gen;
 };

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static void usage( const char * name )
{
 std::cerr
  << "Usage: " << name << " [options] out_file" << std::endl
  << "  -k <int>    number of groups [4]" << std::endl
  << "  -n <int>    variables per group [100]" << std::endl
  << "  -r <real>   hard clauses per variable of a group [3.0]" << std::endl
  << "  -l <int>    literals per hard clause [3]" << std::endl
  << "  -f <real>   fraction of linking clauses among the hard ones, in"
  << std::endl
  << "              [ 0 , 1 ) [0.05]" << std::endl
  << "  -g <int>    most groups a linking clause touches, at least 2 [2]"
  << std::endl
  << "  -p <real>   soft clauses per variable of a group [1.0]" << std::endl
  << "  -L <int>    literals per soft clause [1]" << std::endl
  << "  -W <int>    soft weights uniform in 1 .. W, 1 for unweighted [100]"
  << std::endl
  << "  -P          planted: a hidden assignment satisfies all the hard"
  << std::endl
  << "              clauses, which are then satisfiable" << std::endl
  << "  -s <int>    seed [0]" << std::endl
  << "  out_file    WCNF (CNF if there are no soft clauses), or a netCDF"
  << std::endl
  << "              SATBlock if it ends in .nc4" << std::endl
  << "With -p 1 -L 1 every variable has a soft unit clause of its own."
  << std::endl;
 }

/*--------------------------------------------------------------------------*/
/// draws len distinct variables among those in [ first , first + n )

static void draw_vars( Rng & rng , unsigned int first , unsigned int n ,
		       unsigned int len , std::vector< int > & vars )
{
 while( vars.size() < len ) {
  const int v = int( first + rng.below( n ) );
  if( std::find( vars.begin() , vars.end() , v ) == vars.end() )
   vars.push_back( v );
  }
 }

/*--------------------------------------------------------------------------*/
/// gives signs to the variables of a hard clause, satisfying the planted
/// assignment if there is one

static SATBlock::Clause make_hard( Rng & rng ,
				   const std::vector< int > & vars ,
				   const std::vector< bool > & planted )
{
 SATBlock::Clause clause( vars.size() );
 for( ; ; ) {
  bool sat = planted.empty();
  for( std::size_t i = 0 ; i < vars.size() ; ++i ) {
   clause[ i ] = rng.coin() ? vars[ i ] : - vars[ i ];
   if( ( ! sat ) && ( planted[ vars[ i ] ] == ( clause[ i ] > 0 ) ) )
    sat = true;
   }
  if( sat )
   return( clause );
  }
 }

/*--------------------------------------------------------------------------*/
/*--------------------------------- Main -----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 unsigned int k = 4 , n = 100 , l = 3 , g = 2 , L = 1 , W = 100;
 double r = 3.0 , f = 0.05 , p = 1.0;
 bool plant = false;
 std::uint64_t seed = 0;

 int opt;
 while( ( opt = getopt( argc , argv , "k:n:r:l:f:g:p:L:W:Ps:h" ) ) != -1 )
  switch( opt ) {
   case( 'k' ): k = std::stoul( optarg ); break;
   case( 'n' ): n = std::stoul( optarg ); break;
   case( 'r' ): r = std::stod( optarg ); break;
   case( 'l' ): l = std::stoul( optarg ); break;
   case( 'f' ): f = std::stod( optarg ); break;
   case( 'g' ): g = std::stoul( optarg ); break;
   case( 'p' ): p = std::stod( optarg ); break;
   case( 'L' ): L = std::stoul( optarg ); break;
   case( 'W' ): W = std::stoul( optarg ); break;
   case( 'P' ): plant = true; break;
   case( 's' ): seed = std::stoull( optarg ); break;
   default: usage( argv[ 0 ] ); return( 1 );
   }
 if( optind != argc - 1 ) {
  usage( argv[ 0 ] );
  return( 1 );
  }

 if( ( k < 1 ) || ( n < 1 ) || ( l < 1 ) || ( l > n ) || ( L < 1 ) ||
     ( L > n ) || ( W < 1 ) || ( r < 0 ) || ( p < 0 ) || ( f < 0 ) ||
     ( f >= 1 ) || ( g < 2 ) ) {
  std::cerr << "Error: wrong parameters" << std::endl;
  usage( argv[ 0 ] );
  return( 1 );
  }
 // a linking clause touches between 2 and gmax groups, one literal each at
 // least
 const unsigned int gmax = std::min( { g , l , k } );
 if( ( f > 0 ) && ( gmax < 2 ) ) {
  std::cerr << "Error: linking clauses need 2 groups and 2 literals"
	    << std::endl;
  return( 1 );
  }

 Rng rng( seed );

 // the planted assignment, from variable 1 on
 std::vector< bool > planted;
 if( plant ) {
  planted.resize( std::size_t( k ) * n + 1 );
  for( std::size_t v = 1 ; v < planted.size() ; ++v )
   planted[ v ] = rng.coin();
  }

 const auto m_in = unsigned( std::lround( r * n ) );
 const auto m_soft = unsigned( std::lround( p * n ) );
 const auto m_link = unsigned( std::lround( f / ( 1 - f ) * k * m_in ) );

 SATBlock::v_Clause clauses;
 SATBlock::v_Weight weights;
 std::vector< int > vars;

 // the hard clauses inside each group
 for( unsigned int grp = 0 ; grp < k ; ++grp )
  for( unsigned int c = 0 ; c < m_in ; ++c ) {
   vars.clear();
   draw_vars( rng , grp * n + 1 , n , l , vars );
   clauses.push_back( make_hard( rng , vars , planted ) );
   weights.push_back( Inf< double >() );
   }

 // the linking clauses: t groups, each with a literal at least
 std::vector< unsigned int > grps;
 for( unsigned int c = 0 ; c < m_link ; ++c ) {
  const auto t = unsigned( 2 + rng.below( gmax - 1 ) );
  grps.clear();
  while( grps.size() < t ) {
   const auto grp = unsigned( rng.below( k ) );
   if( std::find( grps.begin() , grps.end() , grp ) == grps.end() )
    grps.push_back( grp );
   }
  while( grps.size() < l )
   grps.push_back( grps[ rng.below( t ) ] );
  vars.clear();
  for( auto grp : grps ) {
   // one more variable of that group, distinct from those drawn
   const auto before = vars.size();
   while( vars.size() == before ) {
    const int v = int( grp * n + 1 + rng.below( n ) );
    if( std::find( vars.begin() , vars.end() , v ) == vars.end() )
     vars.push_back( v );
    }
   }
  clauses.push_back( make_hard( rng , vars , planted ) );
  weights.push_back( Inf< double >() );
  }

 // the soft clauses inside each group; units on distinct variables while
 // there are enough of them
 std::vector< int > perm( n );
 for( unsigned int grp = 0 ; grp < k ; ++grp ) {
  if( L == 1 ) {
   for( unsigned int i = 0 ; i < n ; ++i )
    perm[ i ] = int( grp * n + 1 + i );
   for( unsigned int i = n ; i > 1 ; --i )
    std::swap( perm[ i - 1 ] , perm[ rng.below( i ) ] );
   }
  for( unsigned int c = 0 ; c < m_soft ; ++c ) {
   vars.clear();
   if( ( L == 1 ) && ( c < n ) )
    vars.push_back( perm[ c ] );
   else
    draw_vars( rng , grp * n + 1 , n , L , vars );
   SATBlock::Clause clause( vars.size() );
   for( std::size_t i = 0 ; i < vars.size() ; ++i )
    clause[ i ] = rng.coin() ? vars[ i ] : - vars[ i ];
   clauses.push_back( std::move( clause ) );
   weights.push_back( double( 1 + rng.below( W ) ) );
   }
  }

 std::ostringstream record;
 record << "satgen -k " << k << " -n " << n << " -r " << r << " -l " << l
	<< " -f " << f << " -g " << g << " -p " << p << " -L " << L
	<< " -W " << W << ( plant ? " -P" : "" ) << " -s " << seed;

 SATBlock b;
 b.load( k * n , std::move( clauses ) , std::move( weights ) );
 std::vector< int > groups( k * n );
 for( unsigned int i = 0 ; i < groups.size() ; ++i )
  groups[ i ] = int( i / n );
 b.set_variable_groups( std::move( groups ) );

 const std::string name( argv[ optind ] );
 if( ( name.size() > 4 ) && ( name.substr( name.size() - 4 ) == ".nc4" ) ) {
  netCDF::NcFile file( name , netCDF::NcFile::replace );
  file.putAtt( "SMS++_file_type" , netCDF::NcInt() , eBlockFile );
  file.putAtt( "satgen" , record.str() );
  b.Block::serialize( file , eBlockFile );
  }
 else {
  std::ofstream out( name );
  if( ! out ) {
   std::cerr << "Error: cannot write " << name << std::endl;
   return( 1 );
   }
  out << "c " << record.str() << std::endl
      << "c " << k << " groups of " << n << " variables, the g-th (from 0)"
      << " being g * " << n << " + 1 .. ( g + 1 ) * " << n << std::endl
      << "c " << k * m_in << " hard clauses inside the groups, then "
      << m_link << " linking ones, then " << k * m_soft << " soft ones"
      << std::endl;
  b.print( out , 'C' );
  }

 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*--------------------------- End File satgen.cpp --------------------------*/
/*--------------------------------------------------------------------------*/
