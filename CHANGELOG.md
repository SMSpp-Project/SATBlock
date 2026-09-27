# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
- `smspp_satgen`, the generator of random weighted partial MaxSAT instances
  made of groups of variables with their own hard and soft clauses, bound by
  a tunable fraction of hard linking clauses, possibly with a planted
  assignment that makes the hard clauses satisfiable, written as WCNF or as a
  netCDF `SATBlock`; the same seed gives the same instance with any compiler
