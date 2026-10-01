/*--------------------------------------------------------------------------*/
/*---------------------------- File satpart.cpp ----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Deals the variables of a SAT or weighted MaxSAT instance out to groups,
 * which is what the structures of a SATBlock are made of [see
 * SATBlock::set_structure()], for the instances that do not come with
 * groups of their own.
 *
 * The groups are the communities of the variable incidence graph of the
 * formula, whose vertices are the variables and in which each clause of
 * length L > 1 adds 1 / ( L - 1 ) to the weight of the edge between any two
 * of its variables, found by the Louvain method: each variable is moved,
 * in turn, to the community of a neighbour that increases the modularity
 * most, until no move increases it, and then the communities become the
 * vertices of a smaller graph, on which the same is done, until nothing
 * changes. With -k the communities are then merged, the two with the
 * heaviest edge between them first, until there are k of them or no edge
 * is left between them, those left being then dealt to the k largest ones,
 * the largest first into the smallest, which changes no linking clause; the
 * linking clauses are those whose variables end up in more than one group.
 * With -b eps no merge makes a group of more than ( 1 + eps ) n / k of the
 * n variables, which keeps the sub-Block of the same size: the edges that
 * would are skipped, and so there may be more than k groups at the end, as
 * there may be groups larger than that if a community already is.
 *
 * The clauses longer than the value of -c are left out of the graph, since
 * each one would add a clique of quadratic size; they link whatever groups
 * their variables end up in. The order in which the variables are visited
 * is drawn with the seed of -s, the same seed giving the same groups with
 * any compiler.
 *
 * The instance is read in the DIMACS CNF or WCNF format, or as a netCDF
 * SATBlock if the name ends in .nc4, and written as a netCDF SATBlock with
 * the groups of its variables, the record of how they were made being in
 * the attribute "satpart" of the file.
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
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <random>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <getopt.h>

#include <SATBlock.h>

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- TYPES -----------------------------------*/
/*--------------------------------------------------------------------------*/
/// a weighted undirected graph by adjacency lists, each edge listed at both
/// its ends, a self loop once

using Graph = std::vector< std::vector< std::pair< unsigned int , double > > >;

/*--------------------------------------------------------------------------*/
/*------------------------------ FUNCTIONS ---------------------------------*/
/*--------------------------------------------------------------------------*/

static void usage( const char * prog )
{
 std::cerr << "Usage: " << prog << " [options] in_file out_file.nc4"
  << std::endl
  << "Deals the variables of a CNF / WCNF / .nc4 instance out to groups,"
  << std::endl
  << "the communities of its variable incidence graph, and writes it as a"
  << std::endl
  << "netCDF SATBlock with them." << std::endl
  << "  -k <int>    merge the communities down to this many groups [0 = no]"
  << std::endl
  << "  -b <real>   with -k, no group above ( 1 + b ) n / k variables [inf]"
  << std::endl
  << "  -c <int>    longest clause in the graph [50]" << std::endl
  << "  -s <int>    seed of the order of the visits [0]" << std::endl;
 }

/*--------------------------------------------------------------------------*/
/// a uniform integer in [ 0 , m ), by rejection, the same with any compiler

static std::uint64_t below( std::mt19937_64 & gen , std::uint64_t m )
{
 const std::uint64_t lim = std::mt19937_64::max() -
                           ( std::mt19937_64::max() % m + 1 ) % m;
 std::uint64_t r;
 do
  r = gen();
 while( r > lim );
 return( r % m );
 }

/*--------------------------------------------------------------------------*/
/// the variable incidence graph of the clauses up to maxlen literals

static Graph incidence_graph( const SATBlock & b , unsigned int maxlen )
{
 std::vector< std::unordered_map< unsigned int , double > > adj(
					       b.get_number_variables() );
 for( const auto & cl : b.get_clauses() ) {
  if( ( cl.size() < 2 ) || ( cl.size() > maxlen ) )
   continue;
  const double w = 1.0 / double( cl.size() - 1 );
  for( std::size_t a = 0 ; a < cl.size() ; ++a )
   for( std::size_t c = a + 1 ; c < cl.size() ; ++c ) {
    const unsigned int u = std::abs( cl[ a ] ) - 1;
    const unsigned int v = std::abs( cl[ c ] ) - 1;
    if( u == v )  // a tautology
     continue;
    adj[ u ][ v ] += w;
    adj[ v ][ u ] += w;
    }
  }

 Graph g( adj.size() );
 for( unsigned int u = 0 ; u < adj.size() ; ++u ) {
  g[ u ].assign( adj[ u ].begin() , adj[ u ].end() );
  std::sort( g[ u ].begin() , g[ u ].end() );  // independent of the hashing
  }
 return( g );
 }

/*--------------------------------------------------------------------------*/
/// one level of the Louvain method: moves the vertices of g among the
/// communities in comm, true if any has moved

static bool louvain_level( const Graph & g , std::vector< unsigned int > & comm ,
			   std::mt19937_64 & gen )
{
 const unsigned int n = g.size();
 std::vector< double > k( n , 0 );  // the degrees
 double m2 = 0;                     // twice the total weight
 for( unsigned int u = 0 ; u < n ; ++u )
  for( const auto & [ v , w ] : g[ u ] ) {
   k[ u ] += ( v == u ) ? 2 * w : w;
   m2 += ( v == u ) ? 2 * w : w;
   }
 if( m2 == 0 )
  return( false );

 std::vector< double > tot( n , 0 );  // the degree of each community
 for( unsigned int u = 0 ; u < n ; ++u )
  tot[ comm[ u ] ] += k[ u ];

 std::vector< unsigned int > order( n );
 for( unsigned int i = 0 ; i < n ; ++i )
  order[ i ] = i;
 for( unsigned int i = n ; i > 1 ; --i )
  std::swap( order[ i - 1 ] , order[ below( gen , i ) ] );

 bool moved = false , improved = true;
 std::map< unsigned int , double > links;  // weight towards each community
 while( improved ) {
  improved = false;
  for( auto u : order ) {
   links.clear();
   for( const auto & [ v , w ] : g[ u ] )
    if( v != u )
     links[ comm[ v ] ] += w;

   // u out of its community, then into the one with the best gain
   const unsigned int c0 = comm[ u ];
   tot[ c0 ] -= k[ u ];
   unsigned int best = c0;
   double gain0 = links.count( c0 ) ? links[ c0 ] - tot[ c0 ] * k[ u ] / m2
				    : 0;
   double bestgain = gain0;
   for( const auto & [ c , w ] : links ) {
    const double gain = w - tot[ c ] * k[ u ] / m2;
    if( gain > bestgain + 1e-12 ) {
     bestgain = gain;
     best = c;
     }
    }
   tot[ best ] += k[ u ];
   if( best != c0 ) {
    comm[ u ] = best;
    improved = moved = true;
    }
   }
  }
 return( moved );
 }

/*--------------------------------------------------------------------------*/
/// renumbers the communities from 0 in the order they appear, returning
/// how many there are

static unsigned int renumber( std::vector< unsigned int > & comm )
{
 std::map< unsigned int , unsigned int > id;
 for( auto & c : comm ) {
  auto it = id.emplace( c , id.size() ).first;
  c = it->second;
  }
 return( id.size() );
 }

/*--------------------------------------------------------------------------*/
/// the graph of the communities: one vertex each, the weights summed

static Graph aggregate( const Graph & g , const std::vector< unsigned int > & comm ,
			unsigned int nc )
{
 std::vector< std::map< unsigned int , double > > adj( nc );
 for( unsigned int u = 0 ; u < g.size() ; ++u )
  for( const auto & [ v , w ] : g[ u ] )
   if( u <= v )  // each edge once, the self loops too
    if( comm[ u ] == comm[ v ] )
     adj[ comm[ u ] ][ comm[ u ] ] += w;
    else {
     adj[ comm[ u ] ][ comm[ v ] ] += w;
     adj[ comm[ v ] ][ comm[ u ] ] += w;
     }
 Graph a( nc );
 for( unsigned int c = 0 ; c < nc ; ++c )
  a[ c ].assign( adj[ c ].begin() , adj[ c ].end() );
 return( a );
 }

/*--------------------------------------------------------------------------*/
/// the communities of the vertices of g by the Louvain method

static std::vector< unsigned int > louvain( const Graph & g ,
					    std::mt19937_64 & gen )
{
 std::vector< unsigned int > part( g.size() );  // of the original vertices
 for( unsigned int u = 0 ; u < g.size() ; ++u )
  part[ u ] = u;

 Graph cur = g;
 for( ; ; ) {
  std::vector< unsigned int > comm( cur.size() );
  for( unsigned int u = 0 ; u < cur.size() ; ++u )
   comm[ u ] = u;
  if( ! louvain_level( cur , comm , gen ) )
   break;
  const auto nc = renumber( comm );
  for( auto & p : part )
   p = comm[ p ];
  if( nc == cur.size() )
   break;
  cur = aggregate( cur , comm , nc );
  }
 renumber( part );
 return( part );
 }

/*--------------------------------------------------------------------------*/
/// merges the communities down to k: the two with the heaviest edge between
/// them first, and then, when no edge is left, each of those that remain,
/// the largest first, into the smallest of the k largest ones, which changes
/// no linking clause

static void merge_down( const Graph & g , std::vector< unsigned int > & part ,
			unsigned int k , double cap )
{
 unsigned int nc = renumber( part );
 if( nc <= k )
  return;

 // the graph of the communities, without the self loops
 std::vector< std::map< unsigned int , double > > adj( nc );
 for( unsigned int u = 0 ; u < g.size() ; ++u )
  for( const auto & [ v , w ] : g[ u ] )
   if( ( u < v ) && ( part[ u ] != part[ v ] ) ) {
    adj[ part[ u ] ][ part[ v ] ] += w;
    adj[ part[ v ] ][ part[ u ] ] += w;
    }
 std::vector< std::size_t > size( nc , 0 );
 for( auto p : part )
  ++size[ p ];

 // the community each one has been merged into, followed to the end
 std::vector< unsigned int > into( nc );
 for( unsigned int c = 0 ; c < nc ; ++c )
  into[ c ] = c;
 auto find = [ & into ]( unsigned int c ) {
  while( into[ c ] != c )
   c = into[ c ] = into[ into[ c ] ];
  return( c );
  };

 // the edges by weight, the heaviest first, the stale ones skipped; ties
 // broken by the indices, so that the result does not depend on the heap
 using Edge = std::tuple< double , unsigned int , unsigned int >;
 auto lighter = []( const Edge & a , const Edge & b ) {
  if( std::get< 0 >( a ) != std::get< 0 >( b ) )
   return( std::get< 0 >( a ) < std::get< 0 >( b ) );
  return( std::make_pair( std::get< 1 >( a ) , std::get< 2 >( a ) ) >
	  std::make_pair( std::get< 1 >( b ) , std::get< 2 >( b ) ) );
  };
 std::priority_queue< Edge , std::vector< Edge > , decltype( lighter ) >
  heap( lighter );
 for( unsigned int u = 0 ; u < nc ; ++u )
  for( const auto & [ v , w ] : adj[ u ] )
   if( u < v )
    heap.emplace( w , u , v );

 unsigned int alive = nc;
 while( ( alive > k ) && ( ! heap.empty() ) ) {
  auto [ w , u , v ] = heap.top();
  heap.pop();
  if( ( into[ u ] != u ) || ( into[ v ] != v ) )
   continue;
  auto it = adj[ u ].find( v );
  if( ( it == adj[ u ].end() ) || ( it->second != w ) )
   continue;
  if( double( size[ u ] + size[ v ] ) > cap )  // too large a group
   continue;

  // v into u, the smaller adjacency into the larger one
  if( adj[ v ].size() > adj[ u ].size() )
   std::swap( u , v );
  adj[ u ].erase( v );
  adj[ v ].erase( u );
  for( const auto & [ x , wx ] : adj[ v ] ) {
   adj[ x ].erase( v );
   const double nw = ( adj[ u ][ x ] += wx );
   adj[ x ][ u ] = nw;
   heap.emplace( nw , std::min( u , x ) , std::max( u , x ) );
   }
  adj[ v ].clear();
  size[ u ] += size[ v ];
  into[ v ] = u;
  --alive;
  }

 if( alive > k ) {  // no edge left: the k largest take the others
  std::vector< unsigned int > rest;
  for( unsigned int c = 0 ; c < nc ; ++c )
   if( into[ c ] == c )
    rest.push_back( c );
  std::sort( rest.begin() , rest.end() , [ & size ]( auto x , auto y ) {
   return( size[ x ] > size[ y ] || ( size[ x ] == size[ y ] && x < y ) );
   } );
  for( std::size_t r = k ; r < rest.size() ; ++r ) {
   unsigned int best = rest[ 0 ];
   for( std::size_t b = 1 ; b < k ; ++b )
    if( size[ rest[ b ] ] < size[ best ] )
     best = rest[ b ];
   if( double( size[ best ] + size[ rest[ r ] ] ) > cap )
    continue;  // a group of its own
   size[ best ] += size[ rest[ r ] ];
   into[ rest[ r ] ] = best;
   }
  }

 for( auto & p : part )
  p = find( p );
 renumber( part );
 }

/*--------------------------------------------------------------------------*/
/*--------------------------------- Main -----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 unsigned int k = 0 , maxlen = 50;
 double eps = -1;  // no cap
 std::uint64_t seed = 0;

 int opt;
 while( ( opt = getopt( argc , argv , "k:b:c:s:h" ) ) != -1 )
  switch( opt ) {
   case( 'k' ): k = unsigned( std::stoul( optarg ) ); break;
   case( 'b' ): eps = std::stod( optarg ); break;
   case( 'c' ): maxlen = unsigned( std::stoul( optarg ) ); break;
   case( 's' ): seed = std::stoull( optarg ); break;
   default: usage( argv[ 0 ] ); return( opt == 'h' ? 0 : 1 );
   }
 if( argc - optind != 2 ) {
  usage( argv[ 0 ] );
  return( 1 );
  }
 if( maxlen < 2 ) {
  std::cerr << "Error: -c must be at least 2" << std::endl;
  return( 1 );
  }

 const std::string in( argv[ optind ] ) , out( argv[ optind + 1 ] );
 auto ends = []( const std::string & s , const std::string & e ) {
  return( ( s.size() > e.size() ) &&
	  ( s.compare( s.size() - e.size() , e.size() , e ) == 0 ) );
  };
 if( ! ends( out , ".nc4" ) ) {
  std::cerr << "Error: the output file must end in .nc4" << std::endl;
  return( 1 );
  }

 // the instance - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 SATBlock * b = nullptr;
 if( ends( in , ".nc4" ) ) {
  b = dynamic_cast< SATBlock * >( Block::deserialize( in ) );
  if( ! b ) {
   std::cerr << "Error: " << in << " holds no SATBlock" << std::endl;
   return( 1 );
   }
  }
 else {
  std::ifstream input( in );
  if( ! input ) {
   std::cerr << "Error: cannot read " << in << std::endl;
   return( 1 );
   }
  b = new SATBlock();
  b->load( input );
  }

 // the groups - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 std::mt19937_64 gen( seed );
 const auto g = incidence_graph( *b , maxlen );
 auto part = louvain( g , gen );
 const auto ncomm = renumber( part );
 if( k && ( ncomm > k ) )
  merge_down( g , part , k , eps < 0 ? double( part.size() ) :
	      ( 1 + eps ) * double( part.size() ) / k );
 const auto ngroups = renumber( part );

 std::vector< int > groups( part.begin() , part.end() );
 b->set_variable_groups( std::move( groups ) );

 // what the groups are like - - - - - - - - - - - - - - - - - - - - - - - -
 std::vector< std::size_t > size( ngroups , 0 );
 for( auto p : part )
  ++size[ p ];
 std::size_t linking = 0;
 for( const auto & cl : b->get_clauses() )
  if( std::any_of( cl.begin() , cl.end() , [ & ]( int lit ) {
       return( part[ std::abs( lit ) - 1 ] !=
	       part[ std::abs( cl[ 0 ] ) - 1 ] ); } ) )
   ++linking;
 std::cout << ncomm << " communities, " << ngroups << " groups of "
	   << *std::min_element( size.begin() , size.end() ) << " to "
	   << *std::max_element( size.begin() , size.end() )
	   << " variables, " << linking << " linking clauses out of "
	   << b->get_number_clauses() << std::endl;

 // the output - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 std::ostringstream record;
 record << "satpart -k " << k;
 if( eps >= 0 )
  record << " -b " << eps;
 record << " -c " << maxlen << " -s " << seed << " " << in;
 {
  netCDF::NcFile file( out , netCDF::NcFile::replace );
  file.putAtt( "SMS++_file_type" , netCDF::NcInt() , eBlockFile );
  file.putAtt( "satpart" , record.str() );
  b->Block::serialize( file , eBlockFile );
  }

 delete b;
 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*--------------------------- End File satpart.cpp -------------------------*/
/*--------------------------------------------------------------------------*/
