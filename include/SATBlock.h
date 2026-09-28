/*--------------------------------------------------------------------------*/
/*---------------------------- File SATBlock.h -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Header file for the *concrete* class SATBlock, which implements the
 * Block concept [see Block.h] for the satisfiability problems of the
 * propositional logic, i.e., finding values of a set of Boolean variables
 * satisfying a set of clauses, and for their weighted (partial) MaxSAT
 * version, where some clauses are soft and have a weight. Also the
 * Modification classes of the SATBlock are here.
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

#ifndef __SATBlock
 #define __SATBlock
                      /* self-identification: #endif at the end of the file */

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <list>
#include <vector>

#include "Block.h"
#include "ColVariable.h"
#include "FRealObjective.h"
#include "FRowConstraint.h"

/*--------------------------------------------------------------------------*/
/*------------------------------ NAMESPACE ---------------------------------*/
/*--------------------------------------------------------------------------*/

/// namespace for the Structured Modeling System++ (SMS++)
namespace SMSpp_di_unipi_it
{
/*--------------------------------------------------------------------------*/
/*-------------------------------- CLASSES ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @defgroup SATBlock_CLASSES Classes in SATBlock.h
 *  @{ */

/*--------------------------------------------------------------------------*/
/*----------------------------- CLASS SATBlock -----------------------------*/
/*--------------------------------------------------------------------------*/
/// implementation of the Block concept for the satisfiability problems
/** The SATBlock class implements the Block concept [see Block.h] for the
 * satisfiability problems of the propositional logic: given n Boolean
 * variables x_0, ..., x_{n - 1} and m clauses, each clause being the
 * disjunction of a number of literals, i.e., of variables either as they
 * are or negated, find values of the variables that satisfy all the
 * clauses, i.e., such that each clause has at least one true literal.
 *
 * Each clause has a weight: a clause of infinite weight is *hard*, i.e., it
 * has to be satisfied, while one of finite (non-negative) weight is *soft*,
 * i.e., it may be violated at the cost of its weight. Each variable may also
 * have a *cost* c_i, of any sign, paid if it is true. When all the clauses
 * are hard and there are no costs this is the satisfiability problem;
 * otherwise it is the weighted partial MaxSAT problem, i.e., finding values
 * of the variables satisfying all the hard clauses and minimizing the sum of
 * the weights of the violated soft ones plus that of the costs of the true
 * variables. The costs are what the Lagrangian terms of a decomposition
 * become, a unit soft clause being the same as a cost of the other sign.
 *
 * The "physical representation" of the SATBlock is the number n of the
 * variables, the clauses, each one a std::vector of int in the DIMACS
 * convention (the literal i + 1 is x_i, the literal - ( i + 1 ) is its
 * negation), the weights of the clauses and the costs of the variables. A
 * literal appears at most once
 * in a clause, and a clause may be a tautology, i.e., hold both a variable
 * and its negation, in which case it is always satisfied; the empty clause
 * is never satisfied.
 *
 * The "abstract representation" is the MILP formulation of the problem:
 *
 * - one binary ColVariable x_i per variable, the value 1 being true, in the
 *   static group "x";
 *
 * - one binary ColVariable r_c per clause c, 1 if the clause is violated,
 *   fixed to 0 if the clause is hard, in the static group "r";
 *
 * - one FRowConstraint per clause c, with P_c and N_c the indices of the
 *   variables that are in c as they are and negated,
 *
 *   \f[ \sum_{i \in P_c} x_i - \sum_{i \in N_c} x_i + r_c \geq 1 - |N_c| \f]
 *
 *   in the static group "clauses", a tautology being the row r_c \f$\geq\f$
 *   -INF, always satisfied;
 *
 * - the FRealObjective \f$\min \sum_c w_c r_c + \sum_i c_i x_i\f$, w_c
 *   being the weight of the soft clause c and 0 for a hard one, with the
 *   terms of the r first and those of the x after them.
 *
 * A change of the coefficients of the Objective, such as the one of a
 * LagBFunction writing there its Lagrangian term, is a change of the costs
 * and of the weights, which the SATBlock brings into its physical
 * representation [see add_Modification()].
 *
 * The r_c and the rows of the clauses added after the abstract
 * representation has been generated [see add_clauses()] are in the dynamic
 * groups "added r" and "added clauses", in the order they are added. The
 * :MILPSolver, and the decompositions such as the Lagrangian one, then work
 * on a SATBlock as they are; the SAT solvers [see SATSolver.h] read instead
 * the physical representation.
 *
 * A SATBlock can be load()-ed from the DIMACS CNF and WCNF formats, and
 * deserialize() and serialize() it out of and into a netCDF group [see
 * serialize()]. */

class SATBlock : public Block
{
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/

 public:

/*------------------------------ PUBLIC TYPES ------------------------------*/

 /// a clause: its literals in the DIMACS convention
 using Clause = std::vector< int >;

 using v_Clause = std::vector< Clause >;  ///< a vector of Clause

 using c_v_Clause = const v_Clause;       ///< a const vector of Clause

 using v_Weight = std::vector< double >;  ///< the weights of the clauses

 using c_v_Weight = const v_Weight;       ///< a const vector of weights

/*------------------------------ CONSTRUCTOR -------------------------------*/
 /// constructor of SATBlock, taking a pointer to the father Block
 /** Constructor of SATBlock. It accepts a pointer to the father Block
  * (defaulting to nullptr, both because the root Block has no father and so
  * that this can also be used as the void constructor required by the Block
  * factory). The SATBlock is empty: no variable and no clause. */

 explicit SATBlock( Block * father = nullptr )
  : Block( father ) , f_n_var( 0 ) , AR( 0 ) {}

/*------------------------------- DESTRUCTOR -------------------------------*/
 /// destructor of SATBlock

 ~SATBlock() override { guts_of_destructor(); }

/*------------------------- OTHER INITIALIZATIONS --------------------------*/
 /// loads the SATBlock out of the given variables, clauses and weights
 /** Loads the SATBlock out of the number \p n_var of variables, of the
  * clauses in \p clauses, each one a vector of literals in the DIMACS
  * convention, and of their weights in \p weights, replacing whatever the
  * SATBlock held. If \p weights is empty all the clauses are hard, otherwise
  * it must have one weight per clause, +INF for a hard clause and a finite
  * non-negative value for a soft one. A literal that appears more than once
  * in a clause is kept once; an exception is thrown if a literal is 0 or
  * refers to a variable larger than \p n_var, or if a weight is wrong. */

 void load( unsigned int n_var , v_Clause && clauses ,
	    v_Weight && weights = {} );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// loads the SATBlock out of an istream in the DIMACS CNF or WCNF format
 /** Loads the SATBlock out of an istream in the format \p frmt: 'D' for the
  * DIMACS CNF format, 'W' for the WCNF one of MaxSAT, and 0 for either,
  * told apart by what the file holds. In both, comment lines begin with 'c',
  * and a clause is a sequence of non-zero literals ended by a 0.
  *
  * - DIMACS CNF: the line "p cnf <n> <m>" gives the number of variables and
  *   of clauses, and then come the m clauses, all hard, each possibly
  *   spanning several lines. Reading stops after the m-th clause, or at a
  *   line beginning with '%', which ends the instances of the SATLIB
  *   collection; an exception is thrown if fewer than m clauses are found.
  *
  * - WCNF up to 2021: the line "p wcnf <n> <m> [<top>]" is followed by the
  *   m clauses, each on a line of its own beginning with its weight; a
  *   clause whose weight is at least top is hard, and without top all the
  *   clauses are soft.
  *
  * - WCNF from 2022: there is no "p" line, and each clause is on a line of
  *   its own beginning with "h" if it is hard and with its weight if it is
  *   soft; the number of variables is the largest one in the clauses.
  *
  * The weights are read as double, hence integer weights larger than
  * 2^53 are not represented exactly. */

 void load( std::istream & input , char frmt = 0 ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// extends Block::deserialize( netCDF::NcGroup )
 /** Extends Block::deserialize( netCDF::NcGroup ) to the format of
  * SATBlock [see serialize()]. */

 void deserialize( const netCDF::NcGroup & group ) override;

/*--------------------- Methods for handling Variable ----------------------*/
 /// generates the ColVariable x and r of the SATBlock

 void generate_abstract_variables( Configuration * stvv = nullptr ) override;

/*-------------------- Methods for handling Constraint ---------------------*/
 /// generates the FRowConstraint of the SATBlock, one per clause
 /** Generates the FRowConstraint of the SATBlock, one per clause, also
  * generating the ColVariable if they are not there yet. */

 void generate_abstract_constraints( Configuration * stcc = nullptr )
  override;

/*--------------------- Methods for handling Objective ---------------------*/
 /// generates the Objective, the weight of the violated soft clauses

 void generate_objective( Configuration * objc = nullptr ) override;

/*------------------ Methods for reading the data of the SATBlock ----------*/
 /// returns the number of variables

 [[nodiscard]] unsigned int get_number_variables( void ) const {
  return( f_n_var );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the number of clauses

 [[nodiscard]] unsigned int get_number_clauses( void ) const {
  return( v_clauses.size() );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the clauses, in the DIMACS convention

 [[nodiscard]] c_v_Clause & get_clauses( void ) const { return( v_clauses ); }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the weights of the clauses, +INF for the hard ones

 [[nodiscard]] c_v_Weight & get_weights( void ) const { return( v_weights ); }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns true if the i-th clause is hard

 [[nodiscard]] bool is_hard( unsigned int i ) const {
  return( v_weights[ i ] == Inf< double >() );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns true if all the clauses are hard

 [[nodiscard]] bool all_hard( void ) const;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the costs of the variables, 0 if they have none

 [[nodiscard]] c_v_Weight & get_costs( void ) const { return( v_costs ); }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns true if some variable has a nonzero cost

 [[nodiscard]] bool has_costs( void ) const;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns true if the i-th clause holds both a variable and its negation

 [[nodiscard]] bool is_tautology( unsigned int i ) const;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the ColVariable x of the variables, empty if not generated yet

 [[nodiscard]] const std::vector< ColVariable > & get_variables( void ) const {
  return( v_x );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the ColVariable x of the variables, e.g., to fix some of them

 [[nodiscard]] std::vector< ColVariable > & get_variables( void ) {
  return( v_x );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the ColVariable r of the clause i, which must exist

 [[nodiscard]] const ColVariable & get_violation( unsigned int i ) const;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the ColVariable r of the clause i, whose value a Solver writes

 [[nodiscard]] ColVariable & get_violation( unsigned int i ) {
  return( violation( i ) );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the FRowConstraint of the clause i, which must exist

 [[nodiscard]] FRowConstraint & get_clause_constraint( unsigned int i );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the static FRowConstraint of the clauses, empty if not there

 [[nodiscard]] const std::vector< FRowConstraint > & get_clause_constraints(
						       void ) const {
  return( v_c );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the dynamic FRowConstraint, those of the clauses added later

 [[nodiscard]] const std::list< FRowConstraint > &
  get_added_clause_constraints( void ) const { return( l_c ); }

/*--------------------------------------------------------------------------*/
/*------------- Methods for checking the state of the SATBlock -------------*/
 /// returns true if any part of the abstract representation is there

 bool anyone_there( void ) const override
 {
  return( AR ? true : Block::anyone_there() );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns true if the values of the ColVariable x satisfy the hard clauses
 /** Returns true if the current values of the ColVariable x, true when
  * larger than 1/2, satisfy all the hard clauses, read out of the physical
  * representation whatever \p useabstract says (the two coincide); the
  * ColVariable must have been generated, otherwise an exception is
  * thrown. */

 bool is_feasible( bool useabstract = false ,
		   Configuration * fsbc = nullptr ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the sum of the weights of the soft clauses violated
 /** Returns the sum of the weights of the soft clauses that the current
  * values of the ColVariable x violate, i.e., the value of the MaxSAT
  * objective, the hard clauses being left out whether they are satisfied or
  * not [see is_feasible()]; the ColVariable must have been generated,
  * otherwise an exception is thrown. */

 [[nodiscard]] double get_violated_weight( void ) const;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the value of the MaxSAT objective at the values of the x
 /** Returns the value of the MaxSAT objective at the current values of the
  * ColVariable x, i.e., get_violated_weight() plus the costs of the true
  * variables; the ColVariable must have been generated, otherwise an
  * exception is thrown. */

 [[nodiscard]] double get_objective_value( void ) const;

/*------------------------ Methods for the Solution ------------------------*/
 /// returns a ColVariableSolution of the SATBlock
 /** Returns a ColVariableSolution [see ColVariableSolution.h] of the
  * SATBlock, which has read() the current values of its ColVariable unless
  * \p emptys is true. */

 Solution * get_Solution( Configuration * solc = nullptr ,
			  bool emptys = true ) override;

/*-------------------------- Methods for printing --------------------------*/
 /// prints the SATBlock on an ostream with the given verbosity
 /** Prints the SATBlock: the number of variables and of clauses, and, with
  * \p vlvl == 'C', the whole instance in the DIMACS CNF format, which is
  * what load() reads back. */

 void print( std::ostream & output , char vlvl = 0 ) const override;

/*------------------ METHODS FOR HANDLING MODIFICATIONS --------------------*/
 /// extends Block::add_Modification() to the changes of the Objective
 /** Extends Block::add_Modification(): a change of the coefficients of the
  * Objective is brought into the physical representation, the coefficient
  * of an x becoming its cost and that of the r of a soft clause its weight,
  * with the SATBlockMod that go with them; the Modification is then passed
  * on as any other. */

 void add_Modification( sp_Mod mod , ChnlName chnl = 0 ) override;

/*----------- METHODS FOR MODIFYING THE PHYSICAL REPRESENTATION ------------*/
/** @name Methods for modifying the physical representation
 *
 * These methods change the physical representation of the SATBlock, and
 * the abstract one too if it has been generated, issuing the SATBlockMod
 * [see below] as \p issueMod says and the Modification of the abstract
 * representation as \p issueAMod says [see Observer::make_par()].
 *  @{ */

 /// changes the weights of the clauses in the Range
 /** Changes the weights of the clauses in the Range \p rng, the new ones
  * being in \p NWeight, +INF making a clause hard; in the abstract
  * representation the r_c of a clause becoming hard is fixed to 0, that of
  * one becoming soft is unfixed, and the coefficients of the Objective
  * change. An exception is thrown if
  * \p NWeight is shorter than \p rng or a weight is wrong [see load()]. */

 void chg_weights( MF_dbl_sp NWeight , Range rng = INFRange ,
		   ModParam issueMod = eNoBlck ,
		   ModParam issueAMod = eNoBlck );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// changes the weights of the clauses in the Subset
 /** Same as chg_weights( Range ), for the clauses in the Subset \p nms;
  * \p ordered tells if \p nms is ordered increasingly, which it is made to
  * be otherwise, together with \p NWeight. */

 void chg_weights( MF_dbl_sp NWeight , Subset && nms , bool ordered = false ,
		   ModParam issueMod = eNoBlck ,
		   ModParam issueAMod = eNoBlck );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// changes the costs of the variables in the Range
 /** Changes the costs of the variables in the Range \p rng, the new ones
  * being in \p NCost, as well as their coefficients in the Objective if it
  * has been generated. An exception is thrown if \p NCost is shorter than
  * \p rng or a cost is not finite. */

 void chg_costs( MF_dbl_sp NCost , Range rng = INFRange ,
		 ModParam issueMod = eNoBlck , ModParam issueAMod = eNoBlck );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// changes the costs of the variables in the Subset
 /** Same as chg_costs( Range ), for the variables in the Subset \p nms;
  * \p ordered tells if \p nms is ordered increasingly, which it is made to
  * be otherwise, together with \p NCost. */

 void chg_costs( MF_dbl_sp NCost , Subset && nms , bool ordered = false ,
		 ModParam issueMod = eNoBlck , ModParam issueAMod = eNoBlck );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// adds the given clauses, with their weights, after the existing ones
 /** Adds the clauses in \p clauses after the existing ones, with the
  * weights in \p weights, all hard if it is empty [see load()]. In the
  * abstract representation, if it has been generated, the new r_c and
  * FRowConstraint join the dynamic groups, and the new r_c the Objective.
  * The SATBlockRngdMod issued has type
  * eAddClauses and the Range of the indices of the new clauses. */

 void add_clauses( v_Clause && clauses , v_Weight && weights = {} ,
		   ModParam issueMod = eNoBlck ,
		   ModParam issueAMod = eNoBlck );

/** @} ---------------------------------------------------------------------*/

/*------------------------- Methods for serializing ------------------------*/
 /// serializes the SATBlock into a netCDF group
 /** Serializes the SATBlock into a netCDF group, besides the "type"
  * attribute written by Block::serialize(), as:
  *
  * - the dimension "NumberVariables", the number n of the variables;
  *
  * - the int variables "Clauses", all the literals of all the clauses in
  *   the DIMACS convention one after the other, over the dimension
  *   "NumberLiterals", and "ClausesStart", the position in "Clauses" of the
  *   first literal of each clause, over the dimension "NumberClauses"; both
  *   are absent if there are no clauses [see serialize() in SMSTypedefs.h
  *   for the matrices with rows of different length];
  *
  * - the double variable "Weights", over the dimension "NumberClauses", the
  *   weights of the clauses, +INF for the hard ones; it is absent if all the
  *   clauses are hard;
  *
  * - the double variable "Costs", over the dimension "NumberVariables", the
  *   costs of the variables; it is absent if they are all 0. */

 void serialize( netCDF::NcGroup & group ) const override;

/*---------------------- PROTECTED PART OF THE CLASS -----------------------*/

 protected:

/*--------------------------- PROTECTED METHODS ----------------------------*/
 /// checks the literals of the clauses and removes the repeated ones
 /** Checks the literals of the clauses from the \p first -th on and removes
  * the repeated ones. */

 void normalize_clauses( unsigned int first = 0 );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// checks the weights of the clauses from the first-th on

 void check_weights( unsigned int first = 0 ) const;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// sets the ColVariable r of the i-th clause: binary, fixed if hard

 void set_violation( ColVariable & r , unsigned int i );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// sets the FRowConstraint c of the i-th clause, whose r is r

 void set_clause_constraint( FRowConstraint & c , unsigned int i ,
			     ColVariable * r );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the ColVariable r of the i-th clause, static or dynamic

 ColVariable & violation( unsigned int i );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// changes the cost of the i-th variable, in the Objective too

 void set_cost( unsigned int i , double c , ModParam issueAMod );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// the index of the clause whose r is \p r, Inf< Index >() if none

 [[nodiscard]] Index violation_index( const Variable * r ) const;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// changes the weight of the i-th clause, in the abstract representation too

 void set_weight( unsigned int i , double w , ModParam issueAMod );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// empties the SATBlock, abstract representation included

 void guts_of_destructor( void );

/*---------------------------- PROTECTED FIELDS ----------------------------*/

 unsigned int f_n_var;     ///< number of variables

 v_Clause v_clauses;       ///< the clauses, in the DIMACS convention

 v_Weight v_weights;       ///< the weights of the clauses, +INF if hard

 v_Weight v_costs;         ///< the costs of the variables

 std::vector< ColVariable > v_x;     ///< the ColVariable x of the variables

 std::vector< ColVariable > v_r;     ///< the static ColVariable r

 std::list< ColVariable > l_r;       ///< the dynamic ColVariable r

 std::vector< FRowConstraint > v_c;  ///< the static rows of the clauses

 std::list< FRowConstraint > l_c;    ///< the dynamic rows of the clauses

 FRealObjective f_obj;     ///< the weight of the violated soft clauses

 unsigned char AR;     ///< bit-wise coded: what abstract is there

 static constexpr unsigned char HasVar = 1;
 ///< first bit of AR == 1 if the Variable have been constructed
 static constexpr unsigned char HasObj = 2;
 ///< second bit of AR == 1 if the Objective has been constructed
 static constexpr unsigned char HasCns = 4;
 ///< third bit of AR == 1 if the Constraint have been constructed

/*----------------------- PRIVATE PART OF THE CLASS ------------------------*/

 private:

/*---------------------------- PRIVATE METHODS -----------------------------*/

 SMSpp_insert_in_factory_h;  // insert SATBlock in the Block factory

/*--------------------------------------------------------------------------*/

 };  // end( class( SATBlock ) )

/*--------------------------------------------------------------------------*/
/*--------------------------- CLASS SATBlockMod ----------------------------*/
/*--------------------------------------------------------------------------*/
/// derived class from Modification for the changes of a SATBlock
/** Derived class from Modification for the changes of the physical
 * representation of a SATBlock: its type says what has changed, and the
 * derived classes SATBlockRngdMod and SATBlockSbstMod which clauses (or
 * which variables, for eChgCost). */

class SATBlockMod : public Modification
{
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/

 public:

/*------------------------------ PUBLIC TYPES ------------------------------*/
 /// the types of SATBlockMod

 enum SATBlock_mod_type {
  eChgWeight = 0 ,  ///< the weights of some clauses have changed
  eAddClauses ,     ///< some clauses have been added
  eChgCost          ///< the costs of some variables have changed
  };

/*------------------------ CONSTRUCTOR & DESTRUCTOR ------------------------*/
 /// constructor: takes the SATBlock and the type

 SATBlockMod( SATBlock * const fblock , int type )
  : f_Block( fblock ) , f_type( type ) {}

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// destructor, does nothing

 ~SATBlockMod() override = default;

/*----------------------- PUBLIC METHODS OF THE CLASS ----------------------*/
 /// returns the SATBlock the SATBlockMod refers to

 Block * get_Block( void ) const override { return( f_Block ); }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the type of the SATBlockMod

 [[nodiscard]] int type( void ) const { return( f_type ); }

/*---------------------- PROTECTED PART OF THE CLASS -----------------------*/

 protected:

/*--------------------------- PROTECTED METHODS ----------------------------*/
 /// prints the SATBlockMod

 void print( std::ostream & output ) const override {
  output << "SATBlockMod[" << this << "]: ";
  switch( f_type ) {
   case( eChgWeight ):  output << "change weights "; break;
   case( eAddClauses ): output << "add clauses "; break;
   case( eChgCost ):    output << "change costs "; break;
   default:             output << "type " << f_type << " ";
   }
  }

/*---------------------------- PROTECTED FIELDS ----------------------------*/

 SATBlock * f_Block;  ///< the SATBlock the SATBlockMod refers to

 int f_type;          ///< the type of the SATBlockMod

/*--------------------------------------------------------------------------*/

 };  // end( class( SATBlockMod ) )

/*--------------------------------------------------------------------------*/
/*------------------------- CLASS SATBlockRngdMod --------------------------*/
/*--------------------------------------------------------------------------*/
/// derived from SATBlockMod for the changes of a Range of clauses

class SATBlockRngdMod : public SATBlockMod
{
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/

 public:

/*------------------------ CONSTRUCTOR & DESTRUCTOR ------------------------*/
 /// constructor: takes the SATBlock, the type and the Range

 SATBlockRngdMod( SATBlock * const fblock , int type , Block::Range rng )
  : SATBlockMod( fblock , type ) , f_rng( rng ) {}

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// destructor, does nothing

 ~SATBlockRngdMod() override = default;

/*----------------------- PUBLIC METHODS OF THE CLASS ----------------------*/
 /// returns the Range of the clauses

 [[nodiscard]] Block::c_Range & rng( void ) const { return( f_rng ); }

/*---------------------- PROTECTED PART OF THE CLASS -----------------------*/

 protected:

/*--------------------------- PROTECTED METHODS ----------------------------*/
 /// prints the SATBlockRngdMod

 void print( std::ostream & output ) const override {
  SATBlockMod::print( output );
  output << "[ " << f_rng.first << ", " << f_rng.second << " )" << std::endl;
  }

/*---------------------------- PROTECTED FIELDS ----------------------------*/

 Block::Range f_rng;  ///< the Range of the clauses

/*--------------------------------------------------------------------------*/

 };  // end( class( SATBlockRngdMod ) )

/*--------------------------------------------------------------------------*/
/*------------------------- CLASS SATBlockSbstMod --------------------------*/
/*--------------------------------------------------------------------------*/
/// derived from SATBlockMod for the changes of a Subset of clauses

class SATBlockSbstMod : public SATBlockMod
{
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/

 public:

/*------------------------ CONSTRUCTOR & DESTRUCTOR ------------------------*/
 /// constructor: takes the SATBlock, the type and the Subset, "consumed"

 SATBlockSbstMod( SATBlock * const fblock , int type , Block::Subset && nms )
  : SATBlockMod( fblock , type ) , f_nms( std::move( nms ) ) {}

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// destructor, does nothing

 ~SATBlockSbstMod() override = default;

/*----------------------- PUBLIC METHODS OF THE CLASS ----------------------*/
 /// returns the Subset of the clauses, ordered increasingly

 [[nodiscard]] Block::c_Subset & nms( void ) const { return( f_nms ); }

/*---------------------- PROTECTED PART OF THE CLASS -----------------------*/

 protected:

/*--------------------------- PROTECTED METHODS ----------------------------*/
 /// prints the SATBlockSbstMod

 void print( std::ostream & output ) const override {
  SATBlockMod::print( output );
  output << "(# " << f_nms.size() << ")" << std::endl;
  }

/*---------------------------- PROTECTED FIELDS ----------------------------*/

 Block::Subset f_nms;  ///< the Subset of the clauses

/*--------------------------------------------------------------------------*/

 };  // end( class( SATBlockSbstMod ) )

/** @} end( group( SATBlock_CLASSES ) ) */

/*--------------------------------------------------------------------------*/

 }  // end( namespace SMSpp_di_unipi_it )

/*--------------------------------------------------------------------------*/

#endif  /* SATBlock.h included */

/*--------------------------------------------------------------------------*/
/*-------------------------- End File SATBlock.h ---------------------------*/
/*--------------------------------------------------------------------------*/
