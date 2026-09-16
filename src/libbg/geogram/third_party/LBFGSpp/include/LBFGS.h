// Copyright (C) 2016-2026 Yixuan Qiu <yixuan.qiu@cos.name>
// Under MIT license

#ifndef LBFGSPP_LBFGS_H
#define LBFGSPP_LBFGS_H

#include <Eigen/Core>
#include "LBFGSpp/Param.h"
#include "LBFGSpp/BFGSMat.h"
#include "LBFGSpp/LineSearchBacktracking.h"
#include "LBFGSpp/LineSearchBracketing.h"
#include "LBFGSpp/LineSearchNocedalWright.h"
#include "LBFGSpp/LineSearchMoreThuente.h"

namespace LBFGSpp {

///
/// L-BFGS solver for unconstrained numerical optimization
///
template <typename Scalar,
          template <class> class LineSearch = LineSearchNocedalWright>
class LBFGSSolver
{
private:
    using Vector = Eigen::Matrix<Scalar, Eigen::Dynamic, 1>;
    using Matrix = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>;
    using MapVec = Eigen::Map<Vector>;

    const LBFGSParam<Scalar>& m_param;  // Parameters to control the LBFGS algorithm
    BFGSMat<Scalar> m_bfgs;             // Approximation to the Hessian matrix
    Vector m_fx;                        // History of the objective function values
    Vector m_xp;                        // Old x
    Vector m_grad;                      // New gradient
    Scalar m_gnorm;                     // Norm of the gradient
    Vector m_gradp;                     // Old gradient
    Vector m_drt;                       // Moving direction

    // Reset internal variables
    // n: dimension of the vector to be optimized
    inline void reset(int n)
    {
        const int m = m_param.m;
        m_bfgs.reset(n, m);
        m_xp.resize(n);
        m_grad.resize(n);
        m_gradp.resize(n);
        m_drt.resize(n);
        if (m_param.past > 0)
            m_fx.resize(m_param.past);
    }

public:
    ///
    /// Constructor for the L-BFGS solver.
    ///
    /// \param param An object of \ref LBFGSParam to store parameters for the
    ///        algorithm
    ///
    LBFGSSolver(const LBFGSParam<Scalar>& param) :
        m_param(param)
    {
        m_param.check_param();
    }

    ///
    /// Minimizing a multivariate function using the L-BFGS algorithm.
    /// Exceptions will be thrown if error occurs.
    ///
    /// \param f  A function object such that `f(x, grad)` returns the
    ///           objective function value at `x`, and overwrites `grad` with
    ///           the gradient.
    /// \param x  In: An initial guess of the optimal point. Out: The best point
    ///           found.
    /// \param fx Out: The objective function value at `x`.
    ///
    /// \return Number of iterations used.
    ///
    template <typename Foo>
    inline int minimize(Foo& f, Vector& x, Scalar& fx)
    {
        using std::abs;

        // Dimension of the vector
        const int n = x.size();
        reset(n);

        // The length of lag for objective function value to test convergence
        const int fpast = m_param.past;

        // Evaluate function and compute gradient
        fx = f(x, m_grad);
        m_gnorm = m_grad.norm();
        if (fpast > 0)
            m_fx[0] = fx;

        // std::cout << "x0 = " << x.transpose() << std::endl;
        // std::cout << "f(x0) = " << fx << ", ||grad|| = " << m_gnorm << std::endl << std::endl;

        // Early exit if the initial x is already a minimizer
        if (m_gnorm <= m_param.epsilon || m_gnorm <= m_param.epsilon_rel * x.norm())
        {
            return 1;
        }

        // Initial direction
        m_drt.noalias() = -m_grad;
        // Initial step size
        Scalar step = Scalar(1) / m_drt.norm();
        // Tolerance for s'y >= eps * (y'y)
        constexpr Scalar eps = std::numeric_limits<Scalar>::epsilon();
        // s and y vectors
        Vector vecs(n), vecy(n);

        // Number of iterations used
        int k = 1;
        for (;;)
        {
            // std::cout << "Iter " << k << " begins" << std::endl << std::endl;

            // Save the curent x and gradient
            m_xp.noalias() = x;
            m_gradp.noalias() = m_grad;
            Scalar dg = m_grad.dot(m_drt);
            const Scalar step_max = m_param.max_step;

            // Line search to update x, fx and gradient
            LineSearch<Scalar>::LineSearch(f, m_param, m_xp, m_drt, step_max, step, fx, m_grad, dg, x);

            // New gradient norm
            m_gnorm = m_grad.norm();

            // std::cout << "Iter " << k << " finished line search" << std::endl;
            // std::cout << "   x = " << x.transpose() << std::endl;
            // std::cout << "   f(x) = " << fx << ", ||grad|| = " << m_gnorm << std::endl << std::endl;

            // Convergence test -- gradient
            if (m_gnorm <= m_param.epsilon || m_gnorm <= m_param.epsilon_rel * x.norm())
            {
                return k;
            }
            // Convergence test -- objective function value
            if (fpast > 0)
            {
                const Scalar fxd = m_fx[k % fpast];
                if (k >= fpast && abs(fxd - fx) <= m_param.delta * std::max(std::max(abs(fx), abs(fxd)), Scalar(1)))
                    return k;

                m_fx[k % fpast] = fx;
            }
            // Maximum number of iterations
            if (m_param.max_iterations != 0 && k >= m_param.max_iterations)
            {
                return k;
            }

            // Update s and y
            // s_{k+1} = x_{k+1} - x_k
            // y_{k+1} = g_{k+1} - g_k
            vecs.noalias() = x - m_xp;
            vecy.noalias() = m_grad - m_gradp;
            if (vecs.dot(vecy) > eps * vecy.squaredNorm())
                m_bfgs.add_correction(vecs, vecy);

            // Recursive formula to compute d = -H * g
            m_bfgs.apply_Hv(m_grad, -Scalar(1), m_drt);

            // Reset step = 1.0 as initial guess for the next line search
            step = Scalar(1);
            k++;
        }

        return k;
    }

    ///
    /// Returning the gradient vector on the last iterate.
    /// Typically used to debug and test convergence.
    /// Should only be called after the `minimize()` function.
    ///
    /// \return A const reference to the gradient vector.
    ///
    const Vector& final_grad() const { return m_grad; }

    ///
    /// Returning the Euclidean norm of the final gradient.
    ///
    Scalar final_grad_norm() const { return m_gnorm; }

    ///
    /// Returning the approximate Hessian matrix on the last iterate.
    ///
    Matrix final_approx_hessian() const { return m_bfgs.get_Bmat(); }

    ///
    /// Returning the approximate inverse Hessian matrix on the last iterate.
    ///
    Matrix final_approx_inverse_hessian() const { return m_bfgs.get_Hmat(); }
};

}  // namespace LBFGSpp

#endif  // LBFGSPP_LBFGS_H
