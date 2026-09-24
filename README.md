# SATBlock

`SATBlock` is a SMS++ :Block for the satisfiability problems of the
propositional logic, i.e., finding values of a set of Boolean variables that
satisfy a set of clauses, each clause being the disjunction of a number of
literals (a variable, either as it is or negated). Its physical
representation is the number of the variables and the clauses, the latter in
the DIMACS convention; its abstract representation has one `BooleanVariable`
per variable and one `ClauseConstraint` per clause, the two classes of the
SMS++ core for the propositional logic, and no Objective, the problem being
one of feasibility.

A `SATBlock` is read from the DIMACS CNF format of the SAT competitions and
of the SATLIB collection, and it is serialized into and deserialized out of
netCDF like any other :Block; its solution is a `BooleanVariableSolution`.

`SATSolver` is the base of the Solver of a `SATBlock` through an
incremental SAT solver, as `MILPSolver` is for the MILP solvers: it gives the
SAT solver the clauses, the fixed `BooleanVariable` as assumptions (so that,
after an unsatisfiable answer, it tells which of them are in its reason) and
the time limit, and it gives the clauses again only after a Modification
that changes them. `CaDiCaLSATSolver` and `MiniSatSATSolver` are the ones
for CaDiCaL and MiniSat, each built if its SAT solver is found.


## Getting started

These instructions will let you build the `SATBlock` module on
your system.

### Requirements

- The [SMS++ core library](https://gitlab.com/smspp/smspp) and its
  requirements.
- Optionally, [CaDiCaL](https://github.com/arminbiere/cadical) for
  `CaDiCaLSATSolver` (found through `CADICAL_ROOT`) and
  [MiniSat](http://minisat.se) for `MiniSatSATSolver` (found through
  `MINISAT_ROOT`); the module is built without the ones that are not found.

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
