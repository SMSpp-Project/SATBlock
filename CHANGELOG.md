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
  kept once and a tautology kept as it is; the netCDF format; one
  `BooleanVariable` per variable and one `ClauseConstraint` per clause as its
  abstract representation, a tautology being a relaxed `ClauseConstraint`
  with no literals; `is_feasible()` and a `BooleanVariableSolution`
- `SATSolver`, the base of the Solver of a `SATBlock` through an incremental
  SAT solver: the fixed `BooleanVariable` are assumptions and `is_failed()`
  tells which of them are in the reason of an unsatisfiable answer,
  `dblMaxTime` stops the SAT solver, and the clauses are given again only
  after a Modification that changes them; `CaDiCaLSATSolver` and
  `MiniSatSATSolver`, the ones for CaDiCaL and MiniSat, each built only if
  its SAT solver is found
- the SATLIB instances in `data/cnf`, with the scripts that compress and
  upload them as the other modules do; the test checks that the uniform
  random families and the aim ones are satisfiable or not as their name says
