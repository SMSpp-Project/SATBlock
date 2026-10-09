/*--------------------------------------------------------------------------*/
/*------------------------ File GQSATBranchRule.h --------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Header file for the class GQSATBranchRule, a SATBranchRule [see
 * SATSolver.h] that chooses the variable to branch on by a learned policy,
 * the graph neural network of Graph-Q-SAT with graph attention (GAT-Q-SAT),
 * read as a TorchScript module; it is in the library SATBlockML, which is
 * built only if Torch is found.
 *
 * \author Donato Meoli \n
 *         Dipartimento di Informatica \n
 *         Universita' di Pisa \n
 *
 * \copyright &copy; by Donato Meoli
 */
/*--------------------------------------------------------------------------*/
/*----------------------------- DEFINITIONS --------------------------------*/
/*--------------------------------------------------------------------------*/

#ifndef __GQSATBranchRule
 #define __GQSATBranchRule
                      /* self-identification: #endif at the end of the file */

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <memory>
#include <string>

#include <torch/script.h>

#include "SATSolver.h"

/*--------------------------------------------------------------------------*/
/*------------------------------ NAMESPACE ---------------------------------*/
/*--------------------------------------------------------------------------*/

/// namespace for the Structured Modeling System++ (SMS++)
namespace SMSpp_di_unipi_it
{
/*--------------------------------------------------------------------------*/
/*-------------------------------- CLASSES ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @defgroup GQSATBranchRule_CLASSES Classes in GQSATBranchRule.h
 *  @{ */

/*--------------------------------------------------------------------------*/
/*------------------------- CLASS GQSATBranchRule --------------------------*/
/*--------------------------------------------------------------------------*/
/// the SATBranchRule of a policy learned by Graph-Q-SAT
/** The GQSATBranchRule is the SATBranchRule named "GQSAT": it chooses the
 * variable SATSolver::branch() fixes, and the value of the first child, by
 * the Q-values of a policy learned by Graph-Q-SAT (Kurin, Godil, Whiteson,
 * Catanzaro, NeurIPS 2020), such as its variant with graph attention,
 * GAT-Q-SAT. The policy is a TorchScript module, read by load() out of the
 * file of strBranchRuleFile, whose forward( x , edge_index , edge_attr , u )
 * returns two Q-values per vertex of the graph of the residual formula of
 * the node [see SATResidualGraph], u being the single global row [ 0 ]. The
 * columns of the rows of the vertices are those of the integer attribute
 * "features" of the module [see SATResidualGraph::features_type], if it
 * has one, and those of Graph-Q-SAT otherwise, where the weights of the
 * clauses have no place. The largest of the Q-values of the variables,
 * the first of a variable meaning "true" and the second "false", gives the
 * variable and the value of the first child. choose() has nothing to say if
 * no policy has been read, or if the residual formula has no clause. */

class GQSATBranchRule : public SATBranchRule
{
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/

 public:

/*----------------------- PUBLIC METHODS OF THE CLASS ----------------------*/
 /// reads the policy, a TorchScript module, out of the given file

 void load( const std::string & file ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// sets the policy to the given TorchScript module

 void set_policy( const torch::jit::Module & policy );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// chooses the variable and the value by the Q-values of the policy

 bool choose( const SATSolver & solver , unsigned int & var ,
	      double & first ) override;

/*----------------------- PRIVATE PART OF THE CLASS ------------------------*/

 private:

/*---------------------------- PRIVATE FIELDS ------------------------------*/

 std::unique_ptr< torch::jit::Module > f_policy;  ///< the policy, if read

 int f_features = SATResidualGraph::eGQSAT;  ///< the columns it reads

/*--------------------------------------------------------------------------*/

 };  // end( class( GQSATBranchRule ) )

/** @} end( group( GQSATBranchRule_CLASSES ) ) */

/*--------------------------------------------------------------------------*/

 }  // end( namespace SMSpp_di_unipi_it )

/*--------------------------------------------------------------------------*/

#endif  /* GQSATBranchRule.h included */

/*--------------------------------------------------------------------------*/
/*----------------------- End File GQSATBranchRule.h -----------------------*/
/*--------------------------------------------------------------------------*/
