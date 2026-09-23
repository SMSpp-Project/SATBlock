/*--------------------------------------------------------------------------*/
/*---------------------------- File SATBlock.h -----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Header file for the *concrete* class SATBlock, which implements the
 * Block concept [see Block.h] for the satisfiability problems of the
 * propositional logic, i.e., finding values of a set of BooleanVariable
 * satisfying a set of clauses.
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
 * The "physical representation" of the SATBlock is the number n of the
 * variables and the clauses, each one a std::vector of int in the DIMACS
 * convention: the literal i + 1 is x_i, the literal - ( i + 1 ) is its
 * negation. A literal appears at most once in a clause, and a clause may be
 * a tautology, i.e., hold both a variable and its negation, in which case
 * it is always satisfied; the empty clause is never satisfied.
 *
 * The "abstract representation" has one BooleanVariable per variable, in a
 * single static group, and one ClauseConstraint per clause, in a single
 * static group, the i-th ClauseConstraint being the i-th clause: a
 * tautology is a relaxed ClauseConstraint with no literals, since a
 * ClauseConstraint cannot hold a BooleanVariable twice and a relaxed
 * Constraint is always satisfied, which is what a tautology is. There is no
 * Objective, the problem being one of feasibility.
 *
 * A SATBlock can be load()-ed from the DIMACS CNF format, and deserialize()
 * and serialize() it out of and into a netCDF group [see serialize()]. */

class SATBlock : public Block
{
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/

 public:

/*------------------------------ PUBLIC TYPES ------------------------------*/

 /// a clause: its literals in the DIMACS convention
 using Clause = std::vector< int >;

 using v_Clause = std::vector< Clause >;  ///< a vector of Clause

 using c_v_Clause = const v_Clause;       ///< a const vector of Clause

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
 /// loads the SATBlock out of the given number of variables and clauses
 /** Loads the SATBlock out of the number \p n_var of variables and of the
  * clauses in \p clauses, each one a vector of literals in the DIMACS
  * convention, replacing whatever the SATBlock held. A literal that
  * appears more than once in a clause is kept once; an exception is thrown
  * if a literal is 0 or refers to a variable larger than \p n_var. */

 void load( unsigned int n_var , v_Clause && clauses );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// loads the SATBlock out of an istream in the DIMACS CNF format
 /** Loads the SATBlock out of an istream in the DIMACS CNF format (\p frmt
  * 0 or 'D', the only one supported): comment lines begin with 'c', the
  * line "p cnf <n> <m>" gives the number of variables and of clauses, and
  * then come the m clauses, each a sequence of non-zero literals ended by a
  * 0, possibly spanning several lines. Reading stops after the m-th clause,
  * or at a line beginning with '%', which ends the instances of the SATLIB
  * collection; an exception is thrown if fewer than m clauses are found. */

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
 /// returns the ClauseConstraint, empty if not generated yet

 [[nodiscard]] const std::vector< ClauseConstraint > & get_clause_constraints(
						       void ) const {
  return( v_c );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the ClauseConstraint, e.g., to relax some of them

 [[nodiscard]] std::vector< ClauseConstraint > & get_clause_constraints(
								  void ) {
  return( v_c );
  }

/*------------- Methods for checking the state of the SATBlock -------------*/
 /// returns true if any part of the abstract representation is there

 bool anyone_there( void ) const override
 {
  return( AR ? true : Block::anyone_there() );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns true if the values of the BooleanVariable satisfy all clauses
 /** Returns true if the current values of the BooleanVariable satisfy all
  * the clauses, read out of the physical representation whatever
  * \p useabstract says (the two coincide); the BooleanVariable must have
  * been generated, otherwise an exception is thrown. */

 bool is_feasible( bool useabstract = false ,
		   Configuration * fsbc = nullptr ) override;

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
  *   for the matrices with rows of different length]. */

 void serialize( netCDF::NcGroup & group ) const override;

/*---------------------- PROTECTED PART OF THE CLASS -----------------------*/

 protected:

/*--------------------------- PROTECTED METHODS ----------------------------*/
 /// checks the literals of the clauses and removes the repeated ones

 void normalize_clauses( void );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// empties the SATBlock, abstract representation included

 void guts_of_destructor( void );

/*---------------------------- PROTECTED FIELDS ----------------------------*/

 unsigned int f_n_var;     ///< number of variables

 v_Clause v_clauses;       ///< the clauses, in the DIMACS convention

 std::vector< BooleanVariable > v_x;   ///< the BooleanVariable

 std::vector< ClauseConstraint > v_c;  ///< the ClauseConstraint

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

/** @} end( group( SATBlock_CLASSES ) ) */

/*--------------------------------------------------------------------------*/

 }  // end( namespace SMSpp_di_unipi_it )

/*--------------------------------------------------------------------------*/

#endif  /* SATBlock.h included */

/*--------------------------------------------------------------------------*/
/*-------------------------- End File SATBlock.h ---------------------------*/
/*--------------------------------------------------------------------------*/
