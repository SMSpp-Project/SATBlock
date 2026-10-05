# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- the string parameters `strCaDiCaLConfig` and `strCaDiCaLOptions` of
  `CaDiCaLSATSolver`, a configuration of CaDiCaL and a list of name=value
  options set each time a SAT solver is made, the latter being "lucky=0"
  by default

- the parameter `intMaxSATHarden` of `SATSolver`, 1 by default: OLL
  hardens the assumptions whose weight is larger than the gap between the
  best solution and the lower bound, giving them to the SAT solver for the
  rest of the `compute()` (a core under them is not relaxed again later)

- only the x of the abstract representation of a `SATBlock` are generated
  when `generate_abstract_variables()` gets (directly or as the
  `f_static_variables_Configuration` of the `BlockConfig`) a
  `SimpleConfiguration< int >` of value 1, which is all that fixing them as
  assumptions needs; the clauses added then have no `r`, and the rows and
  the `Objective` cannot be generated (`has_violations()` tells)

- `SATBlock::add_variables()`, which adds variables of cost 0 after the
  existing ones, their x in the dynamic group `"added x"` of the abstract
  representation and in the `Objective`; a `SATSolver` gives each of them
  the next variable of its SAT solver, so that what OLL has made stays

- the parameter `intMaxSATKeepCores` of `SATSolver`: the new SAT solver
  that `intMaxSATRestart` makes keeps the cores that the last `compute()`
  relaxed, and those whose totalizers they are made of, rebuilt in the same
  order, the others being dropped together with their totalizers

- the parameter `intMaxSATWCE` of `SATSolver`: OLL extracts the cores
  weight-aware, the assumptions of the totalizers it makes being left out
  of the SAT solver until the other ones hold together

- the parameter `intMaxSATRestart` of `SATSolver`: a `compute()` starting
  when OLL has given the SAT solver more than that many times as many
  clauses as in the first `compute()` with it makes a new SAT solver

- `SATResidualGraph::eMaxSATIndex`, the rows of `eMaxSAT` with the index of
  the variable over the number of variables, which tells apart the
  variables the cores score the same as the rule of the cores does, and
  which a policy exported with `features` 2 reads

- `smspp_satpart`, which deals the variables of a CNF, WCNF or netCDF
  instance out to groups, the communities of its variable incidence graph
  found by the Louvain method, merged down to the number of `-k` if given,
  with no group above ( 1 + `-b` ) n / k variables if that is given too,
  and writes it as a netCDF `SATBlock` with them

- the variables of a `SATBlock` may be dealt out to groups, part of its
  physical representation (`set_variable_groups()`, `"VariableGroups"` in
  netCDF), which `smspp_satgen` writes in the `.nc4` files, and
  `set_structure()` builds out of them one sub-`SATBlock` per group with,
  in the father, either the clauses linking the groups (`kRelaxation`) or
  the equalities between the copies of the variables a linking clause
  takes into another group and their originals (`kDecomposition`, the
  Lagrangian decomposition); the father keeps the whole instance as its
  physical representation, so that a `SATSolver` solves it as it is,
  writing the solution into the x of the sub-Block (`var()`), and
  `chg_costs()`, `chg_weights()` and `add_clauses()` bring their changes
  into the sub-Block, save those that would change the structure (a
  linking clause of `kRelaxation` turning hard or soft, a linking clause
  added), which throw
- OLL is incremental: the relaxation variables, the totalizers and the cores
  it finds stay with the SAT solver, with the clauses it has learnt, and at
  the beginning of each `compute()` the cores are relaxed again with the
  weights and the costs of now, since a core depends on the hard clauses
  and on the fixed variables in its reason but not on the weights; a
  sequence of close instances, such as the subproblems of a Lagrangian
  decomposition, calls the SAT solver only for what those cores do not say
- `SATBlockChange`, the `Change` fixing or unfixing a set of ColVariable x
  of a `SATBlock`, returning the change that undoes it, and its netCDF form
- `SATSolver` is a `RelaxationSolver`, so that `BranchAndXSolver` enumerates
  on it: with `intMaxIter` OLL stops after that many calls of the SAT
  solver in a node and returns `kOK`, as a relaxation does, its lower bound
  (of the cores found so far) and best solution being what the enumeration
  reads, and `branch()` fixes the unfixed x that is in
  the most assumptions of the cores found, the child with the value of the
  best solution first, so that a dive follows that solution; the fixings
  are assumptions, whose cores the nodes below reuse
- `SATBranchRule`, a rule choosing the variable `SATSolver::branch()` fixes,
  made by name out of a factory of its own, and the parameters
  `strBranchRule` and `strBranchRuleFile` of `SATSolver`, which ask it
  first, the rule of the cores deciding if it has nothing to say
- `SATBlockML`, a library built only if Torch is found, with
  `GQSATBranchRule` ("GQSAT"), which chooses by the Q-values of a policy
  learned by Graph-Q-SAT (such as its variant with graph attention),
  read as a TorchScript module, on the graph Graph-Q-SAT makes of the
  residual formula of the node
- `SATResidualGraph`, the graph of the residual formula of a node, with the
  rows of Graph-Q-SAT or, with `eMaxSAT`, also the best solution, the
  scores of the cores and the costs of the variables and the weights of the
  clauses, which `GQSATBranchRule` builds as the integer attribute
  "features" of its module says; `SATSolver::get_best_solution()` and
  `SATSolver::core_scores()` tell what the SATSolver knows of the node, and
  `SATBranchRule::choose()` is given the SATSolver

### Changed

- the clauses of a `SATBlock` are stored as a single vector of literals
  and the index where each clause starts (`SATBlock::Clauses`), which takes
  a fraction of the memory of a vector per clause: `get_clauses()` returns
  them, the i-th one being a `std::span` of its literals

- a clause turned from soft to hard is given to the SAT solver on top of
  those it has, and only one turned from hard to soft makes the clauses be
  given again from scratch

### Fixed

- `SATSolver::get_ub()` after `kStopTime` gave +INF, while the value of the
  best solution found is what it documents
- the data archives are extracted by `cmake -E tar`, which also works with
  the tar of macOS, where the option `--warning=no-unknown-keyword` of GNU
  tar stopped the build.

## [0.1.0] - 2026-09-28

### Added

- initial module skeleton generated from ModuleTemplate
- `SATBlock`, the :Block of the satisfiability problems: the number of the
  variables and the clauses as its physical representation, read from the
  DIMACS CNF format (comments, clauses over several lines, the '%' ending the
  SATLIB instances) and written back to it, a literal repeated in a clause
  kept once and a tautology kept as it is; the netCDF format; the MILP
  formulation as its abstract representation, i.e., a binary `ColVariable`
  per variable, a binary `ColVariable` per clause that is 1 if the clause is
  violated (fixed to 0 for the hard ones), a `FRowConstraint` per clause and
  the weight of the violated soft clauses as the `FRealObjective`, so that
  the :MILPSolver and the decompositions work on it as they are;
  `is_feasible()` and a `ColVariableSolution`
- `SATSolver`, the base of the Solver of a `SATBlock` through an incremental
  SAT solver, reading the physical representation: the fixed variables are
  assumptions and `is_failed()`
  tells which of them are in the reason of an unsatisfiable answer,
  `dblMaxTime` stops the SAT solver, and the clauses are given again only
  after a Modification that changes them; `CaDiCaLSATSolver` and
  `MiniSATSolver`, the ones for CaDiCaL and MiniSat, each built only if
  its SAT solver is found
- a compatibility shim for the packaged MiniSat, whose
  `minisat/core/SolverTypes.h` declares `mkLit()` as a friend with a default
  argument that the recent compilers reject: at configure time a private
  copy of the header, with the default argument moved to the definition,
  shadows the system one, a no-op on an already-patched MiniSat
- the SATLIB instances in `data/cnf`, with the scripts that compress and
  upload them as the other modules do; the test checks that the uniform
  random families and the aim ones are satisfiable or not as their name says
- 42 instances of the exact weighted track of the MaxSAT Evaluation 2024 in
  `data/wcnf`, one per family, with their optimum, which the test checks OLL
  against; the two archives `cnf.tgz` and `wcnf.tgz` are downloaded from the
  Package Registry under the version `DATA_VERSION`, and extracted once per
  version
- the weighted partial MaxSAT in `SATBlock`: a weight per clause, +INF for
  the hard ones, read from the WCNF formats (the one up to 2021 with its top
  weight and the one from 2022 on) and written back to the latter, and in
  netCDF; `is_feasible()` looks at the hard clauses and
  `get_violated_weight()` at the soft ones
- the physical Modification of `SATBlock`: `chg_weights()` on a Range or a
  Subset, fixing or unfixing the variable of the clauses that turn hard or
  soft and changing the Objective, and `add_clauses()`, whose variables and
  rows join dynamic groups and the Objective, with
  `SATBlockMod`, `SATBlockRngdMod` and `SATBlockSbstMod`
- `SATSolver` gives the SAT solver the hard clauses only, the clauses added
  on top of those it has, and reports as upper bound the weight of the soft
  clauses its solution violates; with `intMaxSAT` = 1 it solves the weighted
  MaxSAT by the core-guided algorithm OLL, with incremental totalizers and
  the assumptions stratified by weight, each core trimmed and minimized
  (`intMaxSATTrim`, `intMaxSATMinBudget`), the best solution found being kept
  when the time limit stops it, the test checking it against the
  enumeration on random instances
- the costs of the variables in `SATBlock`, paid when a variable is true,
  which is what the Lagrangian term of a decomposition becomes: in the
  physical representation, in netCDF (`Costs`), in the WCNF written (as unit
  soft clauses, the constant of the negative ones in a comment) and in the
  Objective, after the terms of the clauses; `chg_costs()` on a Range or a
  Subset, with the `SATBlockMod` of type `eChgCost`, and
  `get_objective_value()`; a change of the coefficients of the Objective,
  such as the one a `LagBFunction` makes, is brought into the costs and the
  weights by `add_Modification()`. OLL makes of each cost a unit soft
  clause, and the bounds of `SATSolver` count the costs
- the SAT solvers are found by `cmake/FindCaDiCaL.cmake` and
  `cmake/FindMiniSat.cmake`, which define imported targets and are installed
  with the package configuration, and chosen by the options
  `SATBlock_USE_CADICAL` and `SATBlock_USE_MINISAT`; the configuration of an
  installed SATBlock finds again the SAT solvers it was built with, and the
  makefile builds `SATSolver` and, through `extlib`, the wrapper of each SAT
  solver that is there, with the same shim of MiniSat
- `smspp_satgen`, the generator of random weighted partial MaxSAT instances
  made of groups of variables with their own hard and soft clauses, bound by
  a tunable fraction of hard linking clauses, possibly with a planted
  assignment that makes the hard clauses satisfiable, written as WCNF or as a
  netCDF `SATBlock`; the same seed gives the same instance with any compiler

[Unreleased]: https://gitlab.com/smspp/satblock/-/compare/0.1.0...develop
[0.1.0]: https://gitlab.com/smspp/satblock/-/tags/0.1.0
