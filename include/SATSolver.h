/*--------------------------------------------------------------------------*/
/*---------------------------- File SATSolver.h ----------------------------*/
/*--------------------------------------------------------------------------*/
/** @file
 * Header file for the *abstract* class SATSolver, which implements the
 * Solver concept [see Solver.h] for a SATBlock [see SATBlock.h] by means of
 * an incremental SAT solver, the one of each derived class (CaDiCaL,
 * MiniSat, ...): SATSolver does all that does not depend on the SAT solver,
 * and asks the derived class for a few primitives only.
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

#ifndef __SATSolver
 #define __SATSolver
                      /* self-identification: #endif at the end of the file */

/*--------------------------------------------------------------------------*/
/*------------------------------ INCLUDES ----------------------------------*/
/*--------------------------------------------------------------------------*/

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ChangeSolver.h"
#include "SATBlock.h"
#include "Solver.h"

/*--------------------------------------------------------------------------*/
/*------------------------------ NAMESPACE ---------------------------------*/
/*--------------------------------------------------------------------------*/

/// namespace for the Structured Modeling System++ (SMS++)
namespace SMSpp_di_unipi_it
{
/*--------------------------------------------------------------------------*/
/*-------------------------------- CLASSES ---------------------------------*/
/*--------------------------------------------------------------------------*/
/** @defgroup SATSolver_CLASSES Classes in SATSolver.h
 *  @{ */

/*--------------------------------------------------------------------------*/
/*---------------------------- CLASS SATSolver -----------------------------*/
/*--------------------------------------------------------------------------*/
/// base class of the Solver of a SATBlock through an incremental SAT solver
/** The SATSolver class implements the Solver concept [see Solver.h] for a
 * SATBlock [see SATBlock.h] by means of an incremental SAT solver, which a
 * derived class provides through a few protected primitives: create a new
 * empty SAT solver, give it a clause, solve under assumptions, and read the
 * value of a variable and whether an assumption is in the reason of an
 * unsatisfiable answer. Everything else is here, so that the derived
 * classes (CaDiCaLSATSolver, MiniSATSolver, ...) behave the same way,
 * as the :MILPSolver do for the MILP solvers they wrap.
 *
 * compute() gives the SAT solver the hard clauses of the physical
 * representation of the SATBlock, leaving out those whose FRowConstraint is
 * relaxed if the abstract representation has been generated. The soft
 * clauses are not given to the SAT solver, which therefore looks for a
 * solution of the hard clauses only, whatever it costs. A ColVariable x
 * that is fixed [see Variable::is_fixed()] is fixed at its current value
 * (true if larger than 1/2) by an *assumption*,
 * i.e., a literal that holds for this compute() only: when the SATBlock is
 * unsatisfiable under the assumptions, is_failed() tells which of them are
 * in the reason of it, which is what a core-guided MaxSAT algorithm needs.
 *
 * The SAT solver being incremental, the clauses added to the SATBlock [see
 * SATBlock::add_clauses()] are given to it on top of those it has, and so
 * are the clauses turned from soft to hard [see SATBlock::chg_weights()];
 * the weights and the costs are read anew by each compute(), and a Variable
 * being fixed or unfixed is taken care of by the assumptions. A clause
 * turned from hard to soft, and any other Modification, make the clauses be
 * given again to the SAT solver from scratch at the next compute().
 *
 * The status returned by compute() is kOK if the hard clauses are
 * satisfiable, with a solution that get_var_solution() writes into the
 * ColVariable x, kInfeasible if they are not, kStopTime if the time limit
 * dblMaxTime is reached first, and kError if the SAT solver gives up for any
 * other reason. After kOK get_ub() is the value of the solution, i.e., the
 * sum of the weights of the soft clauses it violates plus the costs of its
 * true variables, and get_lb() the sum of the negative costs, both 0 if all
 * the clauses are hard and there are no costs; both are +INF after
 * kInfeasible, and -INF and +INF otherwise.
 *
 * With the parameter intMaxSAT set to 1, compute() solves instead the
 * weighted MaxSAT problem, i.e., it looks for a solution of the hard clauses
 * minimizing the sum of the weights of the soft clauses it violates, by the
 * core-guided algorithm OLL (Andres, Kaufmann, Matheis, Schaub, ICLP 2012;
 * Morgado, Dodaro, Marques-Silva, CP 2014). Each soft clause is satisfied by
 * an assumption: its literal if it is a unit clause, the negation of a new
 * relaxation variable added to it otherwise; the cost c_i of a variable is
 * the unit soft clause "not x_i" of weight c_i if it is positive, and "x_i"
 * of weight - c_i if it is negative, c_i being then paid anyway. Each time
 * the SAT solver finds that the assumptions cannot hold together, the soft
 * ones in the reason (the *core*) have their weight lowered by the smallest
 * one among them, which is added to the lower bound, and a totalizer over
 * the core gives a new assumption, "at most one of them is violated", with
 * that weight; when such an assumption is in a core in turn, "at most k"
 * becomes "at most k + 1".
 *
 * The assumptions are *stratified* by weight (Ansotegui, Bonet, Gabas,
 * Levy, SAT 2012): only those whose weight is at least a threshold are
 * given to the SAT solver, starting from the largest weights, and when they
 * hold together the threshold goes down, taking the next weights until the
 * assumptions are at least 1.25 per distinct weight, or all of them. The
 * cores are then found among the heavy soft clauses first, and each
 * solution found on the way is an upper bound; the solution found with all
 * the assumptions is optimal.
 *
 * Each core is *reduced* before being used, since a smaller core gives a
 * stronger totalizer and a larger weight: it is first *trimmed*, i.e., given
 * again to the SAT solver as the only assumptions, whose reason is a core in
 * turn, up to intMaxSATTrim times or until it stops shrinking; then it is
 * *minimized* by deletion, i.e., each of its assumptions is left out in
 * turn and the rest given to the SAT solver with a budget of
 * intMaxSATMinBudget conflicts, the assumption staying out if the rest is
 * found unsatisfiable within the budget.
 *
 * With intMaxSATHarden set to 1, the default, the assumptions are
 * *hardened* as soon as a solution is known: one whose weight is larger
 * than the gap between the value of the best solution and the lower bound
 * holds in every solution better than that, and it is given to the SAT
 * solver for the rest of the compute() whatever the threshold of the
 * stratification; hence, if the SAT solver finds a reason made of fixed
 * variables and hardened assumptions alone, and at least one of the
 * latter, the best solution is optimal. Since the weights change from one
 * compute() to the next, a hardened assumption does not become a clause: a
 * core whose reason has one holds under it, like one under a fixed
 * variable, and it is not relaxed again by the following compute().
 *
 * With intMaxSATWCE set to 1 the cores are extracted *weight-aware* (Berg,
 * Jarvisalo, CP 2017): the weights of a core are lowered at once, but the
 * new assumptions of its totalizer are left out of the SAT solver until
 * the other ones hold together, so that several disjoint cores are found
 * before the totalizers make the formula grow; when they hold, those
 * assumptions come in, and the threshold of the stratification goes down
 * only when all of them are there.
 *
 * After kOK get_lb() and get_ub() are both the optimal value. After
 * kStopTime get_lb() is the lower bound reached and get_ub() the value of
 * the best solution found, +INF if none, which get_var_solution() writes;
 * kInfeasible means that the hard clauses are unsatisfiable, as without
 * intMaxSAT.
 *
 * OLL is *incremental*: what it makes stays with the SAT solver for the
 * following compute(), i.e., the clauses the SAT solver has learnt, the
 * relaxation variables of the soft clauses, the totalizers and the cores.
 * A core depends on the hard clauses, which can only grow as long as the
 * SAT solver is kept, and on the fixed variables in its reason, but not on
 * the weights: at the beginning of each compute() the cores found so far
 * whose fixed variables are still fixed so are relaxed again, in the order
 * they were found, with the weights and the costs of now, the smallest
 * weight of each going to the lower bound as if the SAT solver had just
 * found it, and a core none of whose assumptions weighs anything any longer
 * is left aside. The SAT solver is then called only for what these cores do
 * not already say, which is what makes a sequence of close instances, such
 * as the subproblems of a Lagrangian decomposition with different
 * multipliers, cheaper to solve than each of them from scratch. A core
 * with a fixed variable in its reason is kept only for the compute() whose
 * fixed variables include those, and it never becomes a clause. A
 * variable added to the SATBlock [see SATBlock::add_variables()] gets the
 * next variable of the SAT solver, so that what OLL has made stays. What is
 * kept also weighs on each call of the SAT solver, the totalizers of all
 * the cores found so far being there whether they are of use or not: with
 * intMaxSATRestart set to k > 0, a compute() starting when OLL has given
 * the SAT solver more than k times as many clauses as in the first
 * compute() with it makes a new SAT solver, which starts from the hard
 * clauses only; with intMaxSATKeepCores set to 1 it starts also from the
 * cores that the last compute() relaxed (either found then or relaxed again
 * with weight left), and from those whose totalizers they are made of,
 * rebuilt in the same order, while the others are dropped together with
 * their totalizers.
 *
 * A SATSolver is also a RelaxationSolver [see ChangeSolver.h], so that the
 * BranchAndXSolver can enumerate on it. A node is a set of fixed x, which
 * the SATBlockChange of the branching fix [see SATBlock.h] and which are
 * assumptions: the cores found in the nodes below keep holding while those
 * fixings are there, which is what the enumeration reuses going down. With
 * intMaxIter set, OLL stops after that many calls of the SAT solver in its
 * main loop (but in a node whose x are all fixed, which has nothing left to
 * branch on), and its relaxation is then the one made of the cores found so
 * far: compute() returns kOK, as a RelaxationSolver does when its bound is
 * there, get_lb() being that bound and get_ub() the value of the best
 * solution found (+INF if none), which is also the "true" solution [see
 * get_true_ub()], so that kOK means that get_lb() == get_ub() only without
 * intMaxIter; the enumeration closes a node when the two meet. branch()
 * fixes the unfixed x that is in the most soft assumptions of the cores
 * found so far (a relaxation variable counting for the variables of its
 * clause, shared among them), making two children, the first with the value
 * x has in the best solution found, so that diving follows that solution.
 * With strBranchRule, the SATBranchRule of that name is asked first, the
 * rule of the cores deciding only if it has nothing to say [see
 * SATBranchRule]. */

/*--------------------------------------------------------------------------*/
/*-------------------------- CLASS SATBranchRule ---------------------------*/
/*--------------------------------------------------------------------------*/
/// a rule choosing the variable SATSolver::branch() fixes
/** A SATBranchRule chooses, among the unfixed ColVariable x of a SATBlock,
 * the one that SATSolver::branch() fixes, and the value of the first child.
 * The rules are made by name out of a factory of their own [see add() and
 * make()], so that a library can add one without SATSolver knowing it, such
 * as the learned rule of SATBlockML, which needs Torch. */

class SATSolver;  // the Solver whose branch() the rule serves

class SATBranchRule
{
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/

 public:

/*------------------------------- DESTRUCTOR -------------------------------*/
 /// destructor, does nothing

 virtual ~SATBranchRule() = default;

/*----------------------- PUBLIC METHODS OF THE CLASS ----------------------*/
 /// reads what the rule needs, e.g., the file of a model; does nothing

 virtual void load( const std::string & ) {}

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// chooses the unfixed x to branch on and the value of the first child
 /** Sets \p var to the index of an unfixed ColVariable x of the SATBlock
  * of \p solver and \p first to the value (0 or 1) of the first child,
  * returning true; returns false if the rule has nothing to say, the rule
  * of SATSolver deciding then. The rule may read what the SATSolver knows
  * of the node, e.g., its best solution and the scores of its cores. */

 virtual bool choose( const SATSolver & solver , unsigned int & var ,
		      double & first ) = 0;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// adds a rule to the factory, under the given name; returns true

 static bool add( const std::string & name ,
		  std::function< SATBranchRule * ( void ) > maker );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// makes a new rule of the given name, nullptr if there is none

 static SATBranchRule * make( const std::string & name );

/*----------------------- PRIVATE PART OF THE CLASS ------------------------*/

 private:

/*--------------------------- PRIVATE METHODS ------------------------------*/
 /// the factory: the maker of each rule, by name

 static std::map< std::string , std::function< SATBranchRule * ( void ) > >
 & rules( void );

/*--------------------------------------------------------------------------*/

 };  // end( class( SATBranchRule ) )

/*--------------------------------------------------------------------------*/
/*---------------------------- CLASS SATSolver -----------------------------*/
/*--------------------------------------------------------------------------*/

class SATSolver : public Solver , public RelaxationSolver
{
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/

 public:

/*------------------------------ CONSTRUCTOR -------------------------------*/
 /// constructor: it does nothing, the SAT solver is created by compute()

 SATSolver( void ) : Solver() {}

/*------------------------------- DESTRUCTOR -------------------------------*/
 /// destructor: the derived class releases its SAT solver

 ~SATSolver() override = default;

/*------------------------- OTHER INITIALIZATIONS --------------------------*/
 /// sets the SATBlock to solve; throws if the Block is not a SATBlock

 void set_Block( Block * block ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// sets the double parameters; dblMaxTime is the time limit of compute()

 void set_par( idx_type par , double value ) override;

 using Solver::set_par;  // keep the other set_par() visible

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the double parameters, dblMaxTime included

 [[nodiscard]] double get_dbl_par( idx_type par ) const override {
  return( par == dblMaxTime ? MaxTime : Solver::get_dbl_par( par ) );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// the int parameters of SATSolver, on top of those of Solver

 enum int_par_type_SATS {
  intMaxSAT = intLastAlgPar ,  ///< 1 to solve the weighted MaxSAT by OLL
                               /**< 0 (the default) looks for a solution of
				* the hard clauses only, 1 for one of
				* minimum weight of the soft clauses violated,
				* by the algorithm OLL [see the class]. */
  intMaxSATTrim ,              ///< times a core of OLL is trimmed at most
                               /**< The most times a core of OLL is given
				* again to the SAT solver to shrink it [see
				* the class]; 0 means never, the default is
				* 5. */
  intMaxSATMinBudget ,         ///< conflicts for minimizing a core of OLL
                               /**< The budget of conflicts of each call of
				* the SAT solver that minimizes a core of OLL
				* by deletion [see the class]; 0 means no
				* minimization, the default is 1000. */
  intMaxSATRestart ,           ///< growth of OLL making a new SAT solver
                               /**< A compute() starting when OLL has given
				* the SAT solver more than this many times as
				* many clauses as in the first compute() with
				* it makes a new SAT solver [see the class]; 0
				* (the default) means never. */
  intMaxSATWCE ,               ///< 1 to extract the cores weight-aware
                               /**< With 1, the assumptions of the
				* totalizers that OLL makes are left out of
				* the SAT solver until the others hold
				* together [see the class]; 0 (the default)
				* gives them at once. */
  intMaxSATKeepCores ,         ///< 1 to keep the live cores in a restart
                               /**< With 1, the new SAT solver that
				* intMaxSATRestart makes has the cores that
				* the last compute() relaxed, and those they
				* are made of, rather than none [see the
				* class]; 0 (the default) keeps none. */
  intMaxSATHarden ,            ///< 1 to harden the assumptions of OLL
                               /**< With 1 (the default), an assumption
				* whose weight is larger than the gap between
				* the best solution and the lower bound is
				* kept in the SAT solver for the rest of the
				* compute() [see the class]; 0 never. */
  intLastAlgParSATS            ///< first new int parameter of derived classes
  };

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// sets the int parameters, intMaxSAT included

 void set_par( idx_type par , int value ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the number of int parameters

 [[nodiscard]] idx_type get_num_int_par( void ) const override {
  return( idx_type( intLastAlgParSATS ) );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the default of the int parameters

 [[nodiscard]] int get_dflt_int_par( idx_type par ) const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the int parameters

 [[nodiscard]] int get_int_par( idx_type par ) const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the index of the int parameter with the given name

 [[nodiscard]] idx_type int_par_str2idx( const std::string & name )
  const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the name of the int parameter with the given index

 [[nodiscard]] const std::string & int_par_idx2str( idx_type idx )
  const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// the string parameters of SATSolver, on top of those of Solver

 enum str_par_type_SATS {
  strBranchRule = strLastAlgPar ,  ///< the SATBranchRule of branch()
                               /**< The name of the SATBranchRule [see
				* SATBranchRule::make()] that branch() asks
				* first; empty (the default) means the rule
				* of the cores [see the class]. An unknown
				* name throws. */
  strBranchRuleFile ,          ///< the file the SATBranchRule reads
                               /**< The file given to SATBranchRule::load(),
				* e.g., the model of a learned rule; empty by
				* default. */
  strLastAlgParSATS            ///< first new string parameter of derived
                               ///< classes
  };

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// sets the string parameters, making and loading the SATBranchRule

 void set_par( idx_type par , std::string && value ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the number of string parameters

 [[nodiscard]] idx_type get_num_str_par( void ) const override {
  return( idx_type( strLastAlgParSATS ) );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the default of the string parameters

 [[nodiscard]] const std::string & get_dflt_str_par( idx_type par )
  const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the string parameters

 [[nodiscard]] const std::string & get_str_par( idx_type par )
  const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the index of the string parameter with the given name

 [[nodiscard]] idx_type str_par_str2idx( const std::string & name )
  const override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the name of the string parameter with the given index

 [[nodiscard]] const std::string & str_par_idx2str( idx_type idx )
  const override;

/*--------------------- METHODS FOR SOLVING THE MODEL ----------------------*/
 /// solves the SATBlock
 /** Solves the SATBlock, giving the SAT solver the clauses again if a
  * Modification requires it and the fixed ColVariable x as assumptions;
  * the returned status is described in the comments to the class. */

 int compute( bool changedvars = true ) override;

/*---------------------- METHODS FOR READING RESULTS -----------------------*/
 /// the lower bound: see the comments to the class

 [[nodiscard]] OFValue get_lb( void ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// the value of the solution: violated weight plus costs, +INF if none

 [[nodiscard]] OFValue get_ub( void ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// true if the last compute() has found a solution

 [[nodiscard]] bool has_var_solution( void ) override {
  if( MaxSATAlg == 1 )  // OLL keeps the best solution it finds
   return( ( ( f_status == kOK ) || ( f_status == kStopTime ) ) &&
	   ( ! v_model.empty() ) );
  return( f_status == kOK );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// writes the solution found into the ColVariable of the SATBlock
 /** Writes the solution found by the last compute() into the ColVariable x
  * of the SATBlock, generating them if needed, and into the r, 1 for the
  * soft clauses the solution violates and 0 for all the others; an
  * exception is thrown if there is no solution [see has_var_solution()]. */

 void get_var_solution( Configuration * solc = nullptr ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// true if the assumption on the i-th variable made it unsatisfiable
 /** After a compute() that returned kInfeasible, returns true if the i-th
  * variable was fixed, i.e., it was an assumption, and that assumption is
  * in the reason the SAT solver has found for the clauses being
  * unsatisfiable; false in any other case, in particular if the clauses are
  * unsatisfiable whatever the fixed variables are. */

 [[nodiscard]] bool is_failed( unsigned int i ) const;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// returns the name and version of the SAT solver

 [[nodiscard]] virtual std::string signature( void ) const = 0;

/*--------------------- METHODS FOR READING THE NODE -----------------------*/
 /// the SATBlock being solved

 [[nodiscard]] const SATBlock * get_SATBlock( void ) const {
  return( f_sat );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// the best solution OLL has found in the last compute(), empty if none

 [[nodiscard]] const std::vector< unsigned char > & get_best_solution( void )
  const { return( v_model ); }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// the score of each x in the cores found so far
 /** The number of soft assumptions of the cores found so far (by this SAT
  * solver) each ColVariable x is in, a relaxation variable counting for
  * the variables of its clause, shared among them; what branch() fixes by
  * default is the unfixed x of largest score. */

 [[nodiscard]] std::vector< double > core_scores( void ) const;

/*------------------- METHODS OF THE RelaxationSolver ----------------------*/
 /// applies a Change to the SATBlock, returning the undo if asked

 Change * apply( Change * chg , bool doUndo = false ) override {
  return( chg->apply( f_sat , doUndo ) );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// the children of the current node: an unfixed x fixed to either value
 /** Returns two SATBlockChange of type eFixX on the same unfixed x, chosen
  * and ordered as the comments to the class say; throws if all the x are
  * fixed, in which case compute() has solved the node. */

 std::vector< Change * > branch( void ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// a change of the costs only touches the objective, anything else may do
 /// more

 [[nodiscard]] int classify( const sp_Mod & mod ) override;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// the value of the best solution found, +INF if none

 [[nodiscard]] OFValue get_true_ub( void ) override {
  return( has_var_solution() ? OFValue( f_ub ) : Inf< OFValue >() );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// true if the last compute() has found a solution

 [[nodiscard]] bool has_true_var_solution( void ) override {
  return( has_var_solution() );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// writes the best solution found, as get_var_solution()

 void get_true_var_solution( Configuration * solc = nullptr ) override {
  get_var_solution( solc );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// a ColVariableSolution of the best solution found, which is written into
 /// the SATBlock first

 Solution * get_true_solution( Configuration * solc = nullptr ) override {
  get_var_solution( solc );
  return( f_sat->get_Solution( solc , false ) );
  }

/*---------------------- PROTECTED PART OF THE CLASS -----------------------*/

 protected:

/*--------------------------- PROTECTED METHODS ----------------------------*/
 /// reads the Modification, deciding if the clauses have to be given again

 void process_outstanding_Modification( void );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// the clauses the SAT solver has to have: 1 for a hard one, 0 otherwise
 /** Returns, for each clause of the physical representation, 1 if it is
  * hard and its FRowConstraint (if the abstract representation has been
  * generated) is not relaxed, 0 otherwise. */

 [[nodiscard]] std::vector< unsigned char > hard_clauses( void ) const;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// creates the SAT solver anew and gives it the clauses of the SATBlock
 /** Creates the SAT solver anew and gives it the hard clauses [see
  * hard_clauses()], throwing away what OLL has made with the previous
  * one. */

 void load_clauses( void );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// gives the SAT solver the hard clauses of the SATBlock it does not have
 /** Gives the SAT solver the hard clauses it does not have, i.e., those
  * added to the SATBlock and those turned from soft to hard since it had
  * its clauses; if a clause it has is no longer hard, which cannot be taken
  * away from it, it calls load_clauses() instead. */

 void sync_clauses( void );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// solves the weighted MaxSAT by OLL, under the given assumptions
 /** Solves the weighted MaxSAT by OLL [see the comments to the class],
  * \p fixed being the assumptions of the fixed ColVariable x, which hold
  * as hard clauses; returns the result of the last SAT call, 10, 20 or 0
  * [see sat_solve()], setting f_lb, and v_failed after 20. */

 int oll( const std::vector< int > & fixed );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// reduces a core of OLL by trimming and minimization
 /** Reduces the core \p core of OLL, soft assumptions that do not hold
  * together with the assumptions \p fixed, by trimming and minimization
  * [see the comments to the class]; what is left is still a core, together
  * with the assumptions of \p fixed in the reason of any of the answers of
  * the SAT solver on the way, which are added to \p cond. */

 void reduce_core( const std::vector< int > & fixed ,
		   std::vector< int > & core , std::vector< int > & cond );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// builds the tree of a totalizer over the given literals, no clause yet
 /** Builds the tree of a totalizer over the literals \p ins[ lo , hi ),
  * returning the index of its root in v_tot; the outputs are made, with
  * their clauses, by tot_extend(). */

 int tot_build( const std::vector< int > & ins , std::size_t lo ,
		std::size_t hi );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// makes the first k outputs of a node of a totalizer, with their clauses
 /** Makes the first \p k outputs of the node \p node of a totalizer (all of
  * them if they are fewer), the j-th output being implied by at least j + 1
  * of the literals under the node being true: only the implications from
  * the literals to the outputs are there, which is what an assumption "the
  * output is false" needs. */

 void tot_extend( int node , std::size_t k );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// the literal of the SAT solver of a literal of the SATBlock

 [[nodiscard]] int sat_lit( int lit ) const {
  const int v = v_ivar[ std::abs( lit ) - 1 ];
  return( lit > 0 ? v : - v );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// the variable of the SATBlock of one of the SAT solver, -1 if of OLL

 [[nodiscard]] int block_var( int v ) const {
  return( v < int( v_uvar.size() ) ? v_uvar[ v ] - 1 : -1 );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// gives the SAT solver a clause of the SATBlock

 void block_clause( std::span< const int > clause );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// makes the variables of the SAT solver of those added to the SATBlock
 /** Gives each variable added to the SATBlock since the SAT solver was made
  * the next variable of the SAT solver, which is that of the same index only
  * as long as OLL has made none [see v_ivar]. */

 void sync_variables( void );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// the assumption of OLL of the soft clause i, made the first time
 /** Returns the assumption that satisfies the soft clause \p i: its literal
  * if it is a unit clause, the negation of a new relaxation variable added
  * to it otherwise, made once for all the compute() of the same SAT
  * solver. */

 int soft_lit( unsigned int i );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// makes a new SAT solver with the cores the last compute() relaxed
 /** Makes a new SAT solver as load_clauses() does, and gives it the cores
  * of the old one that the last compute() relaxed, and those whose
  * totalizers they are made of, in the order they were found, each one
  * with its assumptions translated to the new SAT solver and with a new
  * totalizer [see intMaxSATKeepCores]. */

 void restart_keeping_cores( void );

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// gives the SAT solver a clause made by OLL, counting it in f_oll_clauses

 void oll_clause( const std::vector< int > & clause ) {
  ++f_oll_clauses;
  sat_clause( clause );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// true if the time limit dblMaxTime of the running compute() is reached

 [[nodiscard]] bool time_is_up( void ) const;

/*------------------ THE PRIMITIVES OF THE DERIVED CLASSES -----------------*/
 /// creates a new, empty SAT solver, releasing the previous one if any

 virtual void sat_new( void ) = 0;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// gives the SAT solver a clause, its literals in the DIMACS convention

 virtual void sat_clause( const std::vector< int > & clause ) = 0;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// solves under the given assumptions: 10 if SAT, 20 if UNSAT, 0 if unknown
 /** Solves under the given assumptions, literals in the DIMACS convention
  * holding for this call only, returning 10 if the clauses are
  * satisfiable, 20 if they are not, and 0 if the SAT solver stops before
  * knowing, which it has to do as soon as time_is_up() says so, and after
  * \p conflicts conflicts if it is not negative. */

 virtual int sat_solve( const std::vector< int > & assumptions ,
			long conflicts = -1 ) = 0;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// after 10, the value of the variable var (from 1) in the solution

 [[nodiscard]] virtual bool sat_value( int var ) const = 0;

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// after 20, true if the assumption lit is in the reason of the answer

 [[nodiscard]] virtual bool sat_failed( int lit ) const = 0;

/*---------------------------- PROTECTED FIELDS ----------------------------*/

 SATBlock * f_sat = nullptr;   ///< the SATBlock being solved

 bool f_has_sat = false;       ///< if the SAT solver has been created

 bool f_reload = true;         ///< if the clauses have to be given again

 std::vector< unsigned char > v_hard;  ///< which clauses the SAT solver has

 double f_ub = Inf< double >();  ///< the weight violated by the solution

 double f_lb = - Inf< double >();  ///< the lower bound of the last compute()

 int MaxSATAlg = 0;            ///< the parameter intMaxSAT

 int CoreTrim = 5;             ///< the parameter intMaxSATTrim

 int CoreMinBudget = 1000;     ///< the parameter intMaxSATMinBudget

 int Restart = 0;              ///< the parameter intMaxSATRestart

 bool WCE = false;             ///< the parameter intMaxSATWCE

 bool KeepCores = false;       ///< the parameter intMaxSATKeepCores

 bool Harden = true;           ///< the parameter intMaxSATHarden

 /// the compute() with OLL done with this SAT solver
 long f_oll_computes = 0;

 int MaxIter = Inf< int >();   ///< the parameter intMaxIter

 std::string BranchRule;       ///< the parameter strBranchRule

 std::string BranchRuleFile;   ///< the parameter strBranchRuleFile

 std::unique_ptr< SATBranchRule > f_rule;  ///< the rule of strBranchRule

 bool f_iter_stop = false;     ///< if OLL has stopped for intMaxIter

 int f_next_var = 0;           ///< the last variable of the SAT solver

 /// the variable of the SAT solver of each variable of the SATBlock: i + 1
 /// for those there when the SAT solver was made, the next one free for
 /// those added after, OLL having taken the ones in between
 std::vector< int > v_ivar;

 /// the variable of the SATBlock, plus 1, of each variable of the SAT
 /// solver up to the last one of the SATBlock, 0 for those made by OLL
 std::vector< int > v_uvar;

 /// the index of the variable of each assumption of the fixed x
 std::vector< unsigned int > v_fixed_idx;

 /// the clauses OLL has given the SAT solver since it was made
 std::size_t f_oll_clauses = 0;

 /// the clauses OLL has given the SAT solver in the first compute() with it
 std::size_t f_oll_first = 0;

 /// the best solution found by OLL, empty if none
 std::vector< unsigned char > v_model;

 /// a node of a totalizer: a literal if a leaf, two children otherwise
 struct TotNode {
  int left = -1;               ///< the left child, -1 if a leaf
  int right = -1;              ///< the right child, -1 if a leaf
  std::size_t size = 1;        ///< the number of literals under the node
  std::vector< int > out;      ///< the outputs made so far
  };

 std::vector< TotNode > v_tot;  ///< the nodes of all the totalizers

 /// the assumption satisfying each soft clause, 0 if not made yet
 std::vector< int > v_soft;

 /// a core found by OLL
 struct Core {
  std::vector< int > lits;     ///< its soft assumptions
  std::vector< int > cond;     ///< the fixed variables in its reason, sorted
  int root = -1;               ///< its totalizer, -1 if a single assumption
  long used = 0;               ///< the last compute() that relaxed it
  };

 std::vector< Core > v_cores;  ///< the cores found with this SAT solver

 int f_status = kUnEval;       ///< status of the last compute()

 double MaxTime = Inf< double >();  ///< the time limit of compute() (s)

 std::chrono::steady_clock::time_point f_start;  ///< start of compute()

 std::vector< unsigned char > v_failed;  ///< failed assumptions

/*--------------------------------------------------------------------------*/

 };  // end( class( SATSolver ) )

/*--------------------------------------------------------------------------*/
/*------------------------- CLASS SATResidualGraph -------------------------*/
/*--------------------------------------------------------------------------*/
/// the graph of the residual formula of a node of the enumeration
/** The graph that Graph-Q-SAT (Kurin, Godil, Whiteson, Catanzaro, NeurIPS
 * 2020) makes of a formula, here the residual one of the fixings of the
 * SATBlock of a SATSolver, which is what a learned SATBranchRule reads and
 * what an environment for learning it gives:
 *
 * - the vertices are the unfixed ColVariable x, then the clauses that no
 *   fixed x satisfies, in their order;
 *
 * - each literal of such a clause on an unfixed x gives two edges, from
 *   the variable to the clause and back, with the row [ 0 , 1 ] if the
 *   literal is positive and [ 1 , 0 ] if it is negated.
 *
 * With eGQSAT the rows of the vertices are those of Graph-Q-SAT, [ 1 , 0 ]
 * for a variable and [ 0 , 1 ] for a clause. With eMaxSAT they have seven
 * columns: the same two; for a variable (columns 2 to 4, 0 for a clause),
 * its value in the best solution found (1/2 if none), its score in the
 * cores [see SATSolver::core_scores()] divided by the largest one, and its
 * cost divided by the largest weight; for a clause (columns 5 and 6, 0 for
 * a variable), its weight divided by the largest one (1 if hard), and 1 if
 * it is hard. With eMaxSATIndex they have an eighth one, for a variable the
 * index of its x divided by the number of the x (0 for a clause), which is
 * what tells apart the variables the cores score the same, since the rule
 * of the cores of SATSolver::branch() takes the one of smallest index. */

class SATResidualGraph
{
/*----------------------- PUBLIC PART OF THE CLASS -------------------------*/

 public:

/*------------------------------ PUBLIC TYPES ------------------------------*/
 /// the columns of the rows of the vertices

 enum features_type {
  eGQSAT = 0 ,  ///< those of Graph-Q-SAT: variable or clause
  eMaxSAT ,     ///< those, plus what a weighted MaxSAT node has
  eMaxSATIndex  ///< those, plus the index of the variable
  };

/*----------------------- PUBLIC METHODS OF THE CLASS ----------------------*/
 /// the number of columns of the rows of the vertices

 [[nodiscard]] static unsigned int n_features( int features ) {
  return( features == eMaxSATIndex ? 8 : ( features == eMaxSAT ? 7 : 2 ) );
  }

/*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
 /// builds the graph of the node of \p solver, with the given columns
 /** Builds the graph of the residual formula of the current fixings of the
  * SATBlock of \p solver, whose rows have the columns of \p features;
  * returns false, with an empty graph, if the residual formula has no
  * unfixed variable or no clause. */

 bool build( const SATSolver & solver , int features = eGQSAT );

/*---------------------------- PUBLIC FIELDS -------------------------------*/

 unsigned int n_col = 0;           ///< the columns of the rows of vertices

 long n_var = 0;                   ///< the variable vertices, the first ones

 long n_clause = 0;                ///< the clause vertices, after them

 std::vector< float > vertex;      ///< the rows of the vertices, one by one

 std::vector< int64_t > source;    ///< the source of each edge

 std::vector< int64_t > target;    ///< the target of each edge

 std::vector< float > edge;        ///< the rows of the edges, one by one

 std::vector< unsigned int > var;  ///< the x of each variable vertex

/*--------------------------------------------------------------------------*/

 };  // end( class( SATResidualGraph ) )

/** @} end( group( SATSolver_CLASSES ) ) */

/*--------------------------------------------------------------------------*/

 }  // end( namespace SMSpp_di_unipi_it )

/*--------------------------------------------------------------------------*/

#endif  /* SATSolver.h included */

/*--------------------------------------------------------------------------*/
/*-------------------------- End File SATSolver.h --------------------------*/
/*--------------------------------------------------------------------------*/
