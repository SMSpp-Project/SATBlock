/*--------------------------------------------------------------------------*/
/*--------------------------- File SATBlock.cppm ---------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Interface of the C++20 module smspp.satblock, which exports every name that
 * the headers of SATBlock declare at namespace scope, together with the
 * modules it depends on; a translation unit can then write
 *
 *     import smspp.satblock;
 *
 * in place of the #include of the headers. The macros (e.g., those of the
 * factory in SMSFactory.h) are not exported by a module, so a file that uses
 * them still includes the header that defines them.
 *
 * This file is written by SMS++/cmake/smspp_module_exports.py, which reads
 * the names from the AST of the headers: it is not to be edited by hand.
 *
 * The headers GQSATBranchRule.h are not in this module: they need a library
 * of their own, or only a source includes them.
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*------------------------- GLOBAL MODULE FRAGMENT -------------------------*/
/*--------------------------------------------------------------------------*/

module;

#include "CaDiCaLSATSolver.h"
#include "MiniSATSolver.h"
#include "SATBlock.h"
#include "SATSolver.h"

/*--------------------------------------------------------------------------*/
/*----------------------------- MODULE PURVIEW -----------------------------*/
/*--------------------------------------------------------------------------*/

export module smspp.satblock;

export import smspp;

export namespace CaDiCaL {
}

export namespace Minisat {
}

export namespace SMSpp_di_unipi_it {

 // CaDiCaLSATSolver.h
 using SMSpp_di_unipi_it::CaDiCaLSATSolver;

 // MiniSATSolver.h
 using SMSpp_di_unipi_it::MiniSATSolver;

 // SATBlock.h
 using SMSpp_di_unipi_it::SATBlock;
 using SMSpp_di_unipi_it::SATBlockChange;
 using SMSpp_di_unipi_it::SATBlockMod;
 using SMSpp_di_unipi_it::SATBlockRngdMod;
 using SMSpp_di_unipi_it::SATBlockSbstMod;

 // SATSolver.h
 using SMSpp_di_unipi_it::SATBranchRule;
 using SMSpp_di_unipi_it::SATResidualGraph;
 using SMSpp_di_unipi_it::SATSolver;
}

/*--------------------------------------------------------------------------*/
/*------------------------- End File SATBlock.cppm -------------------------*/
/*--------------------------------------------------------------------------*/
