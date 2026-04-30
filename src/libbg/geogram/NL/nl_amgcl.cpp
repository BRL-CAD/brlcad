/**
 * Interface between OpenNL and the AMGCL solver.
 * Works both on the CPU and the GPU.
 */

#include "common.h"
#include <geogram/NL/nl.h>

extern "C" {
#include <geogram/NL/nl_amgcl.h>
#include <geogram/NL/nl_context.h>
}

#include <chrono>
#include <geogram/NL/nl.h>
#include <geogram/NL/nl_matrix.h>

#ifdef NL_WITH_AMGCL

/*************************************************************************/

// switch off some warnings else it complains too much when
// compiling AMGCL code.
#ifdef __GNUC__
#ifndef __ICC
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#endif

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wshadow-field-in-constructor"
#pragma GCC diagnostic ignored "-Wsource-uses-openmp"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma GCC diagnostic ignored "-Wconditional-uninitialized"
#pragma GCC diagnostic ignored "-Wunused-template"
#endif

#ifdef _MSC_VER
#pragma warning( disable : 4244 4018 4458 4267 4701 )
#endif

//#define WITH_BOOST
// AMGCL has a runtime that lets you dynamically select
// the preconditioner, iterative solver etc..., however
// it depends on BOOST (a super heavy dependency), so we
// use hardwired settings here to avoid this dependency.


# define AMGCL_NO_BOOST
# include <amgcl/amg.hpp>
# include <amgcl/coarsening/smoothed_aggregation.hpp>
# include <amgcl/relaxation/spai0.hpp>
# include <amgcl/solver/cg.hpp>


#include <type_traits>
#include <amgcl/make_solver.hpp>
#include <amgcl/adapter/zero_copy.hpp>
#include <amgcl/solver/skyline_lu.hpp> // for cuda backend

# include <amgcl/preconditioner/dummy.hpp>


#ifdef AMGCL_PROFILING
#include <amgcl/profiler.hpp>
namespace amgcl {
    profiler<> prof;
}
#endif

/*************************************************************************/

// Types for column index and row pointers, as expected
// by AMGCL. Note: AMGCL prefers signed indices.
// In standard mode,  32-bit indices everywhere
// In GARGANTUA mode, 64-bit indices for row pointers
//               and  32-bit indices for column indices
// (in GARGANTUA mode, NNZ can be larger than 4 billions).
typedef int colind_t;
#ifdef GARGANTUA
typedef ptrdiff_t rowptr_t;
#else
typedef int rowptr_t;
#endif

/*************************************************************************/
/***** Generic function to call AMGCL on OpenNL system (CPU and GPU) *****/
/*************************************************************************/

/**
 * \brief Wrapper around solver to use rhs and x directly (CPU) or copy
 *  them to/from GPU memory.
 * \details Nothing in this version, it is specialized below.
 */
template <class Backend> struct solve_linear_system_impl {
};

/**
 * \brief Wrapper around solver that uses rhs and x directly, on the CPU.
 */
template <> struct solve_linear_system_impl<
    amgcl::backend::builtin<double, colind_t, rowptr_t>
> {
    template <class Solver> static std::tuple<size_t, double> apply(
	Solver& solve, size_t n, double* b, double* x
    ) {
        return solve(
            amgcl::make_iterator_range(b, b + n),
            amgcl::make_iterator_range(x, x + n)
        );
    }
};


/**
 * \brief Solves the linear system in the current OpenNL context using AMGCL
 * \tparam Backend a AMGCL backend, can be one of
 *   - amgcl::backend::builtin<double, colind_t, rowptr_t> (CPU)
 *  to support other backends, one needs to write a specialization
 *  of solve_linear_system_impl
 */
template <class Backend> NLboolean nlSolveAMGCL_generic() {

    typedef amgcl::amg<
	Backend,
	amgcl::coarsening::smoothed_aggregation, amgcl::relaxation::spai0
    > Precond;
    typedef amgcl::solver::cg<Backend> IterativeSolver;
    typedef amgcl::make_solver<Precond,IterativeSolver> Solver;
    typedef typename Solver::params Params;

    // Get linear system to solve from OpenNL context
    NLContextStruct* ctxt = (NLContextStruct*)nlGetCurrent();

    if(ctxt->verbose) {
    }

    if(ctxt->M->type == NL_MATRIX_SPARSE_DYNAMIC) {
        if(ctxt->verbose) {
        }
        nlMatrixCompress(&ctxt->M);
    }

    nl_assert(ctxt->M->type == NL_MATRIX_CRS);
    NLCRSMatrix* M = (NLCRSMatrix*)(ctxt->M);
    size_t n = size_t(M->m);
    NLdouble* b = ctxt->b;
    NLdouble* x = ctxt->x;

    Params prm;

    // Initialize AMGCL parameters from OpenNL context.
    // solver.type: "cg" for symmetric, else the other ones ("bicgstab"...)
    // solver.tol: converged as soon as || Ax - b || / || b || < solver.tol
    // solver.maxiter: or stop if using more than solver.maxiter
    // solver.verbose: display || Ax - b || / || b || every 5 iterations
    prm.solver.tol = float(ctxt->threshold);
    prm.solver.maxiter = int(ctxt->max_iterations);
    prm.solver.verbose = int(ctxt->verbose);

    // using the zero-copy interface of AMGCL
    auto M_amgcl = amgcl::adapter::zero_copy_direct(
        size_t(n), (rowptr_t*)M->rowptr, (colind_t *)M->colind, M->val
    );

    auto geo_amgcl_t0 = std::chrono::system_clock::now();

    Solver solver(M_amgcl,prm);

    if(ctxt->verbose) {
    }

    if(ctxt->verbose) {
        auto geo_amgcl_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - geo_amgcl_t0).count();
        (void)geo_amgcl_ms;
    }

    if(ctxt->verbose) {
    }

    // Start timer when running iterative solver
    // (do not count construction in gflops stats)
    nlCurrentContext->start_time = nlCurrentTime();

    // There can be several linear systems to solve in OpenNL
    for(int k=0; k<ctxt->nb_systems; ++k) {

        if(ctxt->no_variables_indirection) {
            x = (double*)ctxt->variable_buffer[k].base_address;
            nl_assert(ctxt->variable_buffer[k].stride == sizeof(double));
        }

	std::tie(ctxt->used_iterations, ctxt->error) =
	    solve_linear_system_impl<Backend>::apply(solver,n,b,x);

        b += n;
        x += n;
    }

    return NL_TRUE;
}

/*******************************************************************************/

NLboolean nlSolveAMGCL() {
    typedef amgcl::backend::builtin<double, colind_t, rowptr_t> CPU;
    return nlSolveAMGCL_generic<CPU>() ;
}

/******************************************************************************/

#else

nlBoolean nlSolveAMGCL() {
    return NL_FALSE;
}

#endif
