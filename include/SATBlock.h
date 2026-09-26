/*--------------------------------------------------------------------------*/
/*---------------------------- File SATBlock.h -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Header file for the *concrete* class SATBlock, which implements the
 * Block concept [see Block.h] for the satisfiability problems of the
 * propositional logic, i.e., finding values of a set of BooleanVariable
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
#include "BooleanVariable.h"
#include "ClauseConstraint.h"

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
 * i.e., it may be violated at the cost of its weight. When all the clauses
 * are hard this is the satisfiability problem; otherwise it is the weighted
 * partial MaxSAT problem, i.e., finding values of the variables satisfying
 * all the hard clauses and minimizing the sum of the weights of the violated
 * soft ones.
 *
 * The "physical representation" of the SATBlock is the number n of the
 * variables, the clauses, each one a std::vector of int in the DIMACS
 * convention (the literal i + 1 is x_i, the literal - ( i + 1 ) is its
 * negation), and the weights of the clauses. A literal appears at most once
 * in a clause, and a clause may be a tautology, i.e., hold both a variable
 * and its negation, in which case it is always satisfied; the empty clause
 * is never satisfied.
 *
 * The "abstract representation" has one BooleanVariable per variable, in a
 * single static group, and one ClauseConstraint per clause, the i-th
 * ClauseConstraint being the i-th clause: those of the clauses the SATBlock
 * has when the abstract representation is generated are in a static group,
 * those added afterwards [see add_clauses()] in a dynamic one. A soft clause
 * is a relaxed ClauseConstraint, since it does not have to be satisfied,
 * and a tautology is a relaxed ClauseConstraint with no literals, since a
 * ClauseConstraint cannot hold a BooleanVariable twice and a relaxed
 * Constraint is always satisfied, which is what a tautology is. There is no
 * Objective, the weights of the soft clauses being in the physical
 * representation only.
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

 /// the dynamic ClauseConstraint of the clauses added by add_clauses()
 using l_ClauseConstraint = std::list< ClauseConstraint >;

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
 /// generates the BooleanVariable of the SATBlock, one per variable

 void generate_abstract_variables( Configuration * stvv = nullptr ) override;

/*-------------------- Methods for handling Constraint ---------------------*/
 /// generates the ClauseConstraint of the SATBlock, one per clause
 /** Generates the ClauseConstraint of the SATBlock, one per clause, also
  * generating the BooleanVariable if they are not there yet. */

 void generate_abstract_constraints( Configuration * stcc = nullptr )
  override;

/*--------------------- Methods for handling Objective ---------------------*/
 /// the SATBlock is a feasibility problem, hence it has no Objective

 void generate_objective( Configuration * objc = nullptr ) override {
  AR |= HasObj;
  }

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
 /// returns true if the i-th clause holds both a variable and its negation

 [[nodiscard]] bool is_tautology( unsigned int i ) const;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the BooleanVariable, empty if not generated yet

 [[nodiscard]] const std::vector< BooleanVariable > & get_variables( void )
  const { return( v_x ); }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the BooleanVariable, whose values a Solver writes

 [[nodiscard]] std::vector< BooleanVariable > & get_variables( void ) {
  return( v_x );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the static ClauseConstraint, empty if not generated yet
 /** Returns the static ClauseConstraint, those of the clauses the SATBlock
  * had when the abstract representation was generated; empty if it has not
  * been generated yet. */

 [[nodiscard]] const std::vector< ClauseConstraint > & get_clause_constraints(
						       void ) const {
  return( v_c );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the static ClauseConstraint, e.g., to relax some of them

 [[nodiscard]] std::vector< ClauseConstraint > & get_clause_constraints(
								  void ) {
  return( v_c );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the dynamic ClauseConstraint, those of the clauses added later
 /** Returns the dynamic ClauseConstraint, those of the clauses added by
  * add_clauses() after the abstract representation was generated, in the
  * order they were added: together with get_clause_constraints() they are
  * all the clauses, in the same order. */

 [[nodiscard]] const l_ClauseConstraint & get_added_clause_constraints(
						       void ) const {
  return( l_c );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the dynamic ClauseConstraint, e.g., to relax some of them

 [[nodiscard]] l_ClauseConstraint & get_added_clause_constraints( void ) {
  return( l_c );
  }

/*------------- Methods for checking the state of the SATBlock -------------*/
 /// returns true if any part of the abstract representation is there

 bool anyone_there( void ) const override
 {
  return( AR ? true : Block::anyone_there() );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns true if the values of the BooleanVariable satisfy the hard
 /// clauses
 /** Returns true if the current values of the BooleanVariable satisfy all
  * the hard clauses, read out of the physical representation whatever
  * \p useabstract says (the two coincide); the BooleanVariable must have
  * been generated, otherwise an exception is thrown. */

 bool is_feasible( bool useabstract = false ,
		   Configuration * fsbc = nullptr ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the sum of the weights of the soft clauses violated
 /** Returns the sum of the weights of the soft clauses that the current
  * values of the BooleanVariable violate, i.e., the value of the MaxSAT
  * objective, the hard clauses being left out whether they are satisfied or
  * not [see is_feasible()]; the BooleanVariable must have been generated,
  * otherwise an exception is thrown. */

 [[nodiscard]] double get_violated_weight( void ) const;

/*------------------------ Methods for the Solution ------------------------*/
 /// returns a BooleanVariableSolution of the SATBlock
 /** Returns a BooleanVariableSolution [see BooleanVariableSolution.h] of
  * the SATBlock, which has read() the current values of the BooleanVariable
  * unless \p emptys is true. */

 Solution * get_Solution( Configuration * solc = nullptr ,
			  bool emptys = true ) override;

/*-------------------------- Methods for printing --------------------------*/
 /// prints the SATBlock on an ostream with the given verbosity
 /** Prints the SATBlock: the number of variables and of clauses, and, with
  * \p vlvl == 'C', the whole instance in the DIMACS CNF format, which is
  * what load() reads back. */

 void print( std::ostream & output , char vlvl = 0 ) const override;

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
  * representation a clause becoming soft is relaxed, and one becoming hard
  * is enforced (unless it is a tautology). An exception is thrown if
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
 /// adds the given clauses, with their weights, after the existing ones
 /** Adds the clauses in \p clauses after the existing ones, with the
  * weights in \p weights, all hard if it is empty [see load()]. In the
  * abstract representation, if it has been generated, the new
  * ClauseConstraint join the dynamic group [see
  * get_added_clause_constraints()]. The SATBlockRngdMod issued has type
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
  *   clauses are hard. */

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
 /// sets the literals of the ClauseConstraint c out of the i-th clause
 /** Sets the literals of the ClauseConstraint \p c out of the i-th clause,
  * relaxing it if the clause is soft, or a tautology, which has no
  * literals. */

 void set_clause_constraint( ClauseConstraint & c , unsigned int i );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the ClauseConstraint of the i-th clause, static or dynamic

 ClauseConstraint & clause_constraint( unsigned int i );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// changes the weight of the i-th clause, relaxing or enforcing it

 void set_weight( unsigned int i , double w , ModParam issueAMod );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// empties the SATBlock, abstract representation included

 void guts_of_destructor( void );

/*---------------------------- PROTECTED FIELDS ----------------------------*/

 unsigned int f_n_var;     ///< number of variables

 v_Clause v_clauses;       ///< the clauses, in the DIMACS convention

 v_Weight v_weights;       ///< the weights of the clauses, +INF if hard

 std::vector< BooleanVariable > v_x;   ///< the BooleanVariable

 std::vector< ClauseConstraint > v_c;  ///< the static ClauseConstraint

 l_ClauseConstraint l_c;   ///< the dynamic ClauseConstraint

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
 * derived classes SATBlockRngdMod and SATBlockSbstMod which clauses. */

class SATBlockMod : public Modification
{
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/

 public:

/*------------------------------ PUBLIC TYPES ------------------------------*/
 /// the types of SATBlockMod

 enum SATBlock_mod_type {
  eChgWeight = 0 ,  ///< the weights of some clauses have changed
  eAddClauses       ///< some clauses have been added
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
