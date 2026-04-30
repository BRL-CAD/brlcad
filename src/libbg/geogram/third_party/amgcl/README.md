# AMGCL

[![Documentation Status](https://readthedocs.org/projects/amgcl/badge/?version=latest)](http://amgcl.readthedocs.io/en/latest/?badge=latest)
[![DOI](https://zenodo.org/badge/6987353.svg)](https://zenodo.org/badge/latestdoi/6987353)
[![Build Status](https://travis-ci.org/ddemidov/amgcl.svg?branch=master)](https://travis-ci.org/ddemidov/amgcl)
[![Build status](https://ci.appveyor.com/api/projects/status/r0s4lbln4qf9r8aq/branch/master?svg=true)](https://ci.appveyor.com/project/ddemidov/amgcl/branch/master)
[![codecov](https://codecov.io/gh/ddemidov/amgcl/branch/master/graph/badge.svg)](https://codecov.io/gh/ddemidov/amgcl)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/5301/badge.svg)](https://scan.coverity.com/projects/5301)

AMGCL is a header-only C++ library for solving large sparse linear systems with
algebraic multigrid (AMG) method. AMG is one of the most effective iterative
methods for solution of equation systems arising, for example, from
discretizing PDEs on unstructured grids. The method can be used as a black-box
solver for various computational problems, since it does not require any
information about the underlying geometry. AMG is often used not as a
standalone solver but as a preconditioner within an iterative solver (e.g.
Conjugate Gradients, BiCGStab, or GMRES).

AMGCL builds the AMG hierarchy on a CPU and then transfers it to one of the
provided backends. This allows for transparent acceleration of the solution
phase with help of OpenCL, CUDA, or OpenMP technologies. Users may provide
their own backends which enables tight integration between AMGCL and the user
code.

See AMGCL documentation at http://amgcl.readthedocs.io/

## Referencing

Demidov, Denis. AMGCL: An efficient, flexible, and extensible algebraic multigrid
implementation. Lobachevskii Journal of Mathematics, 40(5):535–546, May 2019.
[doi](https://doi.org/10.1134/S1995080219050056) [pdf](https://rdcu.be/bHFsY)
[bib](https://raw.githubusercontent.com/ddemidov/amgcl/master/docs/demidov19.bib)

Demidov, Denis. AMGCL -- A C++ library for efficient solution of large sparse linear
systems.  Software Impacts, 6:100037, November 2020.
[doi](https://doi.org/10.1016/j.simpa.2020.100037)
[bib](https://raw.githubusercontent.com/ddemidov/amgcl/master/docs/demidov20.bib)

Demidov, Denis, Lin Mu, and Bin Wang. Accelerating linear solvers for Stokes
problems with C++ metaprogramming. Journal of Computational Science (2020):
101285.
[doi](https://doi.org/10.1016/j.jocs.2020.101285)
[arxiv](https://arxiv.org/pdf/2006.06052.pdf)
[bib](https://raw.githubusercontent.com/ddemidov/amgcl/master/docs/demidov-mu-wang-20.bib)

## Support

* GitHub issues page: https://github.com/ddemidov/amgcl/issues
* Mailing list: https://groups.google.com/forum/#!forum/amgcl
