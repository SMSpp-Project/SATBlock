# SATBlock

`SATBlock` is a SMS++ :Block for the satisfiability problems of the
propositional logic, i.e., finding values of a set of Boolean variables that
satisfy a set of clauses, each clause being the disjunction of a number of
literals (a variable, either as it is or negated), and for their weighted
partial MaxSAT version, where the soft clauses may be violated at the cost of
their weight. Its physical representation is the number of the variables, the
clauses in the DIMACS convention and their weights; its abstract
representation is the MILP formulation of the problem, i.e., a binary
`ColVariable` per variable, a binary `ColVariable` per clause that is 1 if the
clause is violated (fixed to 0 for the hard ones), a `FRowConstraint` per
clause and the weight of the violated soft clauses as the `FRealObjective`, so
that the :MILPSolver and the decompositions of SMS++, such as the Lagrangian
one, work on a `SATBlock` as they are.

A `SATBlock` is read from the DIMACS CNF format of the SAT competitions and of
the SATLIB collection and from the WCNF formats of the MaxSAT Evaluations, and
it is serialized into and deserialized out of netCDF like any other :Block;
its solution is a `ColVariableSolution`. Its clauses can be added and their
weights changed, with the Modification that tell the Solver what changed.

`SATSolver` is the base of the Solver of a `SATBlock` through an incremental
SAT solver, as `MILPSolver` is for the MILP solvers: it gives the SAT solver
the hard clauses, those added on top of those it has, the fixed variables as
assumptions (so that, after an unsatisfiable answer, it tells which of them
are in its reason) and the time limit. With the parameter `intMaxSAT` it
solves the weighted MaxSAT by the core-guided algorithm OLL, with the
assumptions stratified by weight and the cores trimmed and minimized; OLL is
incremental, i.e., the cores it finds are used again, with the weights of
the moment, by the following solves of a changed instance. A `SATSolver` is
also a relaxation for the `BranchAndXSolver`, which enumerates on it by
fixing variables (`SATBlockChange`), OLL running within a budget of calls of
the SAT solver in each node; the variable it fixes is chosen by the cores,
or by a `SATBranchRule` given by name, such as the one of `SATBlockML`, a
library built if Torch is found, which reads a policy learned by Graph-Q-SAT.
`CaDiCaLSATSolver` and `MiniSATSolver` are the ones for CaDiCaL and MiniSat,
each built if its SAT solver is found.

`smspp_satgen` generates random weighted partial MaxSAT instances made of
groups of clauses bound by a tunable fraction of linking clauses, which is the
structure the decompositions are for.


## Getting started

These instructions will let you build the `SATBlock` module on
your system.

### Requirements

- The [SMS++ core library](https://gitlab.com/smspp/smspp) and its
  requirements.
- Optionally, [CaDiCaL](https://github.com/arminbiere/cadical) for
  `CaDiCaLSATSolver` (found through `CADICAL_ROOT`) and
  [MiniSat](https://github.com/stp/minisat) for `MiniSATSolver` (found
  through `MINISAT_ROOT`); the module is built without the ones that are not
  found, and the options `SATBlock_USE_CADICAL` and `SATBlock_USE_MINISAT`
  leave out one that is. How to install them, from a package or from their
  sources, is in the [installation guide of
  SMS++](https://gitlab.com/smspp/smspp-project/-/wikis/Installing-SMS++#cadical);
  `INSTALL.sh` of the umbrella project does it.

### Build and install with CMake

Configure and build the library with:

```sh
mkdir build
cd build
cmake ..
cmake --build .
```

The library has the same configuration options of
[SMS++](https://gitlab.com/smspp/smspp-project/-/wikis/Customize-the-configuration).

Optionally, install the library in the system with:

```sh
cmake --install .
```

### Usage with CMake

After the library is built, you can use it in your CMake project with:

```cmake
find_package(SATBlock)
target_link_libraries(<my_target> SMS++::SATBlock)
```

### Build and install with makefiles

Carefully hand-crafted makefiles have also been developed for those unwilling
to use CMake. Makefiles build the executable in-source (in the same directory
tree where the code is) as opposed to out-of-source (in the copy of the
directory tree constructed in the build/ folder) and therefore it is more
convenient when having to recompile often, such as when developing/debugging
a new module, as opposed to the compile-and-forget usage envisioned by CMake.

Each executable using `SATBlock` has to include a "main makefile" of
the module, which typically is either [makefile-c](makefile-c) including all
necessary libraries comprised the "core SMS++" one, or
[makefile-s](makefile-s) including all necessary libraries but not the "core
SMS++" one (for the common case in which this is used together with other
modules that already include them). These in turn recursively include all the
required other makefiles, hence one should only need to edit the "main
makefile" for compilation type (C++ compiler and its options) and it all
should be good to go. In case some of the external libraries are not at their
default location, it should only be necessary to create the
`../extlib/makefile-paths` out of the `extlib/makefile-default-paths-*` for
your OS `*` and edit the relevant bits (commenting out all the rest).

Check the [SMS++ installation wiki](https://gitlab.com/smspp/smspp-project/-/wikis/Customize-the-configuration#location-of-required-libraries)
for further details.


## Getting help

If you need support, you want to submit bugs or propose a new feature, you
can [open a new issue](https://gitlab.com/smspp/satblock/-/issues/new).


## Contributing

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for details on our code of
conduct, and the process for submitting merge requests to us.


## Authors

### Current Lead Authors

- **Donato Meoli**  
  Dipartimento di Informatica  
  Università di Pisa

### Contributors


## License

This code is provided free of charge under the [GNU Lesser General Public
License version 3.0](https://opensource.org/licenses/lgpl-3.0.html) -
see the [LICENSE](LICENSE) file for details.


## Disclaimer

The code is currently provided free of charge under an open-source license.
As such, it is provided "*as is*", without any explicit or implicit warranty
that it will properly behave or it will suit your needs. The Authors of
the code cannot be considered liable, either directly or indirectly, for
any damage or loss that anybody could suffer for having used it. More
details about the non-warranty attached to this code are available in the
license description file.
