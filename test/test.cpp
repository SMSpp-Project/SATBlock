/*--------------------------------------------------------------------------*/
/*------------------------------ File test.cpp -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Smoke test for SATBlock: constructs a SATBlock via the Block
 * factory, checking that the module links correctly and the class is
 * registered. Replace it with real tests exercising the module.
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

#include <iostream>

#include "SATBlock.h"

/*--------------------------------------------------------------------------*/
/*-------------------------------- USING -----------------------------------*/
/*--------------------------------------------------------------------------*/

using namespace SMSpp_di_unipi_it;

/*--------------------------------------------------------------------------*/
/*-------------------------------- main() ----------------------------------*/
/*--------------------------------------------------------------------------*/

int main( int argc , char ** argv )
{
 // construct a SATBlock via the Block factory: this checks that the
 // class is registered and the library is linked in (whole-archive)
 auto block = Block::new_Block( "SATBlock" );

 if( ! block ) {
  std::cerr << "SATBlock not present in Block factory" << std::endl;
  return( 1 );
  }

 if( ! dynamic_cast< SATBlock * >( block ) ) {
  std::cerr << "factory did not return a SATBlock" << std::endl;
  delete block;
  return( 1 );
  }

 delete block;

 std::cout << "SATBlock: all tests passed" << std::endl;

 return( 0 );
 }

/*--------------------------------------------------------------------------*/
/*---------------------------- End File test.cpp ---------------------------*/
/*--------------------------------------------------------------------------*/
