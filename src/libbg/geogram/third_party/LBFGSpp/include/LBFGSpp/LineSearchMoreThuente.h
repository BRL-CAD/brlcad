// Copyright (C) 2020-2026 Yixuan Qiu <yixuan.qiu@cos.name>
// Under MIT license

#ifndef LBFGSPP_LINE_SEARCH_MORE_THUENTE_H
#define LBFGSPP_LINE_SEARCH_MORE_THUENTE_H

#include <Eigen/Core>
#include <stdexcept>  // std::invalid_argument, std::runtime_error
#include "Param.h"

namespace LBFGSpp {

///
/// The line search algorithm by Moré and Thuente (1994). Can be used for both L-BFGS and L-BFGS-B.
///
/// The target of this line search algorithm is to find a step size \f$\alpha\f$ that satisfies the strong Wolfe condition
/// \f$f(x+\alpha d) \le f(x) + \alpha\mu g(x)^T d\f$ and \f$|g(x+\alpha d)^T d| \le \eta|g(x)^T d|\f$.
/// Our implementation is a simplified version of the algorithm in [1]. We assume that \f$0<\mu<\eta<1\f$, while in [1]
/// they do not assume \f$\eta>\mu\f$. As a result, the algorithm in [1] has two stages, but in our implementation we
/// only need the first stage to guarantee the convergence.
///
/// Reference:
/// [1] Moré, J. J., & Thuente, D. J. (1994). Line search algorithms with guaranteed sufficient decrease.
///
template <typename Scalar>
class LineSearchMoreThuente
{
private:
    using Vector = Eigen::Matrix<Scalar, Eigen::Dynamic, 1>;

    // Minimizer of a quadratic function q(x) = c0 + c1 * x + c2 * x^2
    // that interpolates fa, ga, and fb, assuming the minimizer exists
    // For case I: fb >= fa and ga * (b - a) < 0
    static Scalar quadratic_minimizer(const Scalar& a, const Scalar& b, const Scalar& fa, const Scalar& ga, const Scalar& fb)
    {
        const Scalar ba = b - a;
        const Scalar w = Scalar(0.5) * ba * ga / (fa - fb + ba * ga);
        return a + w * ba;
    }

    // Minimizer of a quadratic function q(x) = c0 + c1 * x + c2 * x^2
    // that interpolates fa, ga and gb, assuming the minimizer exists
    // The result actually does not depend on fa
    // For case II: ga * (b - a) < 0, ga * gb < 0
    // For case III: ga * (b - a) < 0, ga * ga >= 0, |gb| <= |ga|
    static Scalar quadratic_minimizer(const Scalar& a, const Scalar& b, const Scalar& ga, const Scalar& gb)
    {
        const Scalar w = ga / (ga - gb);
        return a + w * (b - a);
    }

    // Local minimizer of a cubic function q(x) = c0 + c1 * x + c2 * x^2 + c3 * x^3
    // that interpolates fa, ga, fb and gb, assuming a != b
    // Also sets a flag indicating whether the minimizer exists
    static Scalar cubic_minimizer(const Scalar& a, const Scalar& b, const Scalar& fa, const Scalar& fb,
                                  const Scalar& ga, const Scalar& gb, bool& exists)
    {
        using std::abs;
        using std::sqrt;

        const Scalar apb = a + b;
        const Scalar ba = b - a;
        const Scalar ba2 = ba * ba;
        const Scalar fba = fb - fa;
        const Scalar gba = gb - ga;
        // z3 = c3 * (b-a)^3, z2 = c2 * (b-a)^3, z1 = c1 * (b-a)^3
        const Scalar z3 = (ga + gb) * ba - Scalar(2) * fba;
        const Scalar z2 = Scalar(0.5) * (gba * ba2 - Scalar(3) * apb * z3);
        const Scalar z1 = fba * ba2 - apb * z2 - (a * apb + b * b) * z3;
        // std::cout << "z1 = " << z1 << ", z2 = " << z2 << ", z3 = " << z3 << std::endl;

        // If c3 = z/(b-a)^3 == 0, reduce to quadratic problem
        const Scalar eps = std::numeric_limits<Scalar>::epsilon();
        if (abs(z3) < eps * abs(z2) || abs(z3) < eps * abs(z1))
        {
            // Minimizer exists if c2 > 0
            exists = (z2 * ba > Scalar(0));
            // Return the end point if the minimizer does not exist
            return exists ? (-Scalar(0.5) * z1 / z2) : b;
        }

        // Now we can assume z3 > 0
        // The minimizer is a solution to the equation c1 + 2*c2 * x + 3*c3 * x^2 = 0
        // roots = -(z2/z3) / 3 (+-) sqrt((z2/z3)^2 - 3 * (z1/z3)) / 3
        //
        // Let u = z2/(3z3) and v = z1/z2
        // The minimizer exists if v/u <= 1
        const Scalar u = z2 / (Scalar(3) * z3), v = z1 / z2;
        const Scalar vu = v / u;
        exists = (vu <= Scalar(1));
        if (!exists)
            return b;

        // We need to find a numerically stable way to compute the roots, as z3 may still be small
        //
        // If |u| >= |v|, let w = 1 + sqrt(1-v/u), and then
        // r1 = -u * w, r2 = -v / w, r1 does not need to be the smaller one
        //
        // If |u| < |v|, we must have uv <= 0, and then
        // r = -u (+-) sqrt(delta), where
        // sqrt(delta) = sqrt(|u|) * sqrt(|v|) * sqrt(1-u/v)
        Scalar r1 = Scalar(0), r2 = Scalar(0);
        if (abs(u) >= abs(v))
        {
            const Scalar w = Scalar(1) + sqrt(Scalar(1) - vu);
            r1 = -u * w;
            r2 = -v / w;
        }
        else
        {
            const Scalar sqrtd = sqrt(abs(u)) * sqrt(abs(v)) * sqrt(1 - u / v);
            r1 = -u - sqrtd;
            r2 = -u + sqrtd;
        }
        return (z3 * ba > Scalar(0)) ? ((std::max)(r1, r2)) : ((std::min)(r1, r2));
    }

    // Select the next step size according to the current step sizes,
    // function values, and derivatives
    static Scalar step_selection(
        const Scalar& al, const Scalar& au, const Scalar& at,
        const Scalar& fl, const Scalar& fu, const Scalar& ft,
        const Scalar& gl, const Scalar& gu, const Scalar& gt)
    {
        using std::abs;

        if (al == au)
            return al;

        // If ft = Inf or gt = Inf, we return the middle point of al and at
        if (!std::isfinite(ft) || !std::isfinite(gt))
            return (al + at) / Scalar(2);

        // ac: cubic interpolation of fl, ft, gl, gt
        // aq: quadratic interpolation of fl, gl, ft
        bool ac_exists;
        // std::cout << "al = " << al << ", at = " << at << ", fl = " << fl << ", ft = " << ft << ", gl = " << gl << ", gt = " << gt << std::endl;
        const Scalar ac = cubic_minimizer(al, at, fl, ft, gl, gt, ac_exists);
        const Scalar aq = quadratic_minimizer(al, at, fl, gl, ft);
        // std::cout << "ac = " << ac << ", aq = " << aq << std::endl;
        // Case 1: ft > fl
        if (ft > fl)
        {
            // This should not happen if ft > fl, but just to be safe
            if (!ac_exists)
                return aq;
            // Then use the scheme described in the paper
            return (abs(ac - al) < abs(aq - al)) ? ac : ((aq + ac) / Scalar(2));
        }

        // as: quadratic interpolation of gl and gt
        const Scalar as = quadratic_minimizer(al, at, gl, gt);
        // Case 2: ft <= fl, gt * gl < 0
        if (gt * gl < Scalar(0))
            return (abs(ac - at) >= abs(as - at)) ? ac : as;

        // Case 3: ft <= fl, gt * gl >= 0, |gt| < |gl|
        const Scalar deltal = Scalar(1.1), deltau = Scalar(0.66);
        if (abs(gt) < abs(gl))
        {
            // We choose either ac or as
            // The case for ac: 1. It exists, and
            //                  2. ac is farther than at from al, and
            //                  3. ac is closer to at than as
            // Cases for as: otherwise
            const Scalar res = (ac_exists &&
                                (ac - at) * (at - al) > Scalar(0) &&
                                abs(ac - at) < abs(as - at)) ?
                ac :
                as;
            // Postprocessing the chosen step
            return (at > al) ?
                (std::min)(at + deltau * (au - at), res) :
                (std::max)(at + deltau * (au - at), res);
        }

        // Simple extrapolation if au, fu, or gu is infinity
        if ((!std::isfinite(au)) || (!std::isfinite(fu)) || (!std::isfinite(gu)))
            return at + deltal * (at - al);

        // ae: cubic interpolation of ft, fu, gt, gu
        bool ae_exists;
        const Scalar ae = cubic_minimizer(at, au, ft, fu, gt, gu, ae_exists);
        // Case 4: ft <= fl, gt * gl >= 0, |gt| >= |gl|
        // The following is not used in the paper, but it seems to be a reasonable safeguard
        return (at > al) ?
            (std::min)(at + deltau * (au - at), ae) :
            (std::max)(at + deltau * (au - at), ae);
    }

public:
    ///
    /// Line search by Moré and Thuente (1994).
    ///
    /// \param f        A function object such that `f(x, grad)` returns the
    ///                 objective function value at `x`, and overwrites `grad` with
    ///                 the gradient.
    /// \param param    An `LBFGSParam` or `LBFGSBParam` object that stores the
    ///                 parameters of the solver.
    /// \param xp       The current point.
    /// \param drt      The current moving direction.
    /// \param step_max The upper bound for the step size that makes x feasible.
    /// \param step     In: The initial step length.
    ///                 Out: The calculated step length.
    /// \param fx       In: The objective function value at the current point.
    ///                 Out: The function value at the new point.
    /// \param grad     In: The current gradient vector.
    ///                 Out: The gradient at the new point.
    /// \param dg       In: The inner product between drt and grad.
    ///                 Out: The inner product between drt and the new gradient.
    /// \param x        Out: The new point moved to.
    ///
    template <typename Foo, typename SolverParam>
    static void LineSearch(Foo& f, const SolverParam& param,
                           const Vector& xp, const Vector& drt, const Scalar& step_max,
                           Scalar& step, Scalar& fx, Vector& grad, Scalar& dg, Vector& x)
    {
        using std::abs;
        // std::cout << "========================= Entering line search =========================\n\n";

        // The strong Wolfe conditions are:
        //           f(x + alpha * d) <= f(x) + alpha * mu * g^T d,
        //     |g(x + alpha * d)^T d| <= eta * |g^T d|,
        // where mu and eta are two constants, and we typically require 0 < mu < eta < 1.
        //
        // Let phi(alpha) = f(x + alpha * d), and then the conditions reduce to
        //        phi(alpha) <= phi(0) + mu * phi'(0) * alpha,
        //     |phi'(alpha)| <= eta * |phi'(0)|.
        //
        // [1] defines a set
        //     T(mu) = {alpha > 0: phi(alpha) <= phi(0) + mu * phi'(0) * alpha,
        //                         |phi'(alpha)| <= mu * |phi'(0)|             }.
        // Clearly, if we have found some alpha that belongs to T(mu), and if mu < eta,
        // then this alpha must satisfy the strong Wolfe conditions.
        //
        // In a practical implementation, we also impose bounds on alpha:
        //     alpha in Ia = [alpha_min, alpha_max]
        // The lower bound alpha_min is used to terminate the iteration.
        // The upper bound is typically used for constrained problems (e.g., L-BFGS-B).
        //
        // The overall framework of the algorithm is to generate a sequence of iterates alpha_k
        // and a sequence of intervals I_k such that:
        // 1. alpha_k is in intersect(I_k, Ia).
        // 2. I_k eventually is a subset of Ia.
        // 3. The length of I_k is shrinking.
        // In each iteration, we test whether alpha_k satisfies the strong Wolfe conditions,
        // and will exit the line search when it meets. Other possible exiting conditions are:
        // 1. alpha reaches alpha_max.
        // 2. alpha reaches alpha_min.
        // 3. The maximum number of iterations is attained.
        //
        // To achieve this goal, we need to make sure that we can generate an interval I such that
        // intersect(I, T(mu)) is not empty, and that for the updated interval I+, intersect(I+, T(mu))
        // is also not empty. Theorem 2.1 of [1] gives conditions for this, which first defines
        // an auxiliary function:
        //     psi(alpha) = phi(alpha) - phi(0) - mu * phi'(0) * alpha.
        // Theorem 2.1 of [1] states that for an interval I = [alpha_l, alpha_u]
        // (alpha_u can be smaller than alpha_l), if
        //     psi(alpha_l) <= psi(alpha_u),                        (*)
        //     psi(alpha_l) <= 0,                                   (*)
        //     psi'(alpha_l) * (alpha_u - alpha_l) < 0,             (*)
        // then there is an alpha* in I such that
        //      psi(alpha*) <= psi(alpha_l),
        //     psi'(alpha*) = 0,
        // and alpha* belongs to T(mu).
        // In this case, alpha* satisfies the strong Wolfe conditions since mu < eta.
        // This theorem motivates us to first locate an interval I that satisfies (*), and then
        // refine I while selecting safeguarded trial values, driving the iterates towards a
        // minimizer of psi(alpha) (or to a point in T(mu)).
        //
        // Note that the sufficient decrease condition is essentially psi(alpha) <= 0, and we have psi(0) = 0.
        // Condition (*) means:
        // 1. alpha_l has smaller psi value.
        // 2. alpha_l satisfies the sufficient decrease condition.
        // 3. alpha_u - alpha_l is a descent direction for psi, so that psi(alpha) < psi(alpha_l) for all
        //    alpha in I sufficiently close to alpha_l.
        //
        // We start with I_0 = [0, Inf] and alpha_0 in Ia. For brevity of notation,
        // let current I_k be I = [I_lo, I_hi], and current alpha_k be step.
        // In [1], the updated interval I+ = [I+_lo, I+_hi] is determined by the following rule:
        // * Case I: If psi(step) > psi(I_lo), then I+_lo = I_lo, I+_hi = step.
        // * Case II: If psi(step) <= psi(I_lo) and psi'(step)(I_lo - step) > 0, then I+_lo = step, I+_hi = I_hi.
        // * Case III: If psi(step) <= psi(I_lo) and psi'(step)(I_lo - step) < 0, then I+_lo = step, I+_hi = I_lo.
        //
        // In Case I and Case III, I+ satisfies (*), and all subsequent intervals also satisfy,
        // so for such cases we have bracketed some point alpha* in T(mu).
        // When Case II persists, we repeat the process, and also enforce a safeguarding rule:
        //     step+ in [min{delta_max * step, alpha_max}, alpha_max],
        // for some delta_max > 1. A concrete implementation that [1] uses is
        //     step+ = min{step + delta * (step - I_lo), alpha_max}
        // for some 1.1 <= delta <= 4. Under this selection of step+, alpha_max will eventually be used as
        // a trial value if the iteration does not go to Case I or Case III. Then we do a test:
        // 1. If psi(alpha_max) <= 0 and psi'(alpha_max) < 0, then we choose to return alpha_max as the final
        //    line search step, although it may not satisfy the Wolfe conditions.
        // 2. Otherwise, we continue the update process, and [1] shows that after a finite number of iterations,
        //    we will obtain an interval that satisfies (*).
        // As a short summary, using this update rule, after a finite number of iterations we will either have
        // an I_k that satisfies (*), or the line search stops at alpha_max (although in this case we may lose
        // a better point that satisfies strong Wolfe conditions).
        //
        // Now assume that we already have an I that satisfies (*), and then the 3-cases update rule
        // continues refining I, in the hope that I will finally be contained in Ia and gradually be shorter.
        // But there is a possibility that alpha_k is decreasing with psi(alpha_k) > 0 or psi'(alpha_k) >= 0,
        // but I_k never becomes a subset of Ia. To avoid such infinite loops, [1] enforces the safeguarding rule
        //     step+ in [alpha_min, max{delta_min * step, alpha_min}]
        // for some delta_min < 1, as long as alpha_min > 0 and
        //     psi(alpha_k) > 0 or psi'(alpha_k) >= 0
        // holds from the beginning. [1] shows that under this setting, after a finite number of iterations,
        // we either:
        // 1. obtain an interval I such that I is a subset of Ia, and I satisfies (*); or
        // 2. we use alpha_min as a trial value, and psi(alpha_min) > 0 or psi'(alpha_min) >= 0.
        // In the latter case, we choose to stop the line search at alpha_min.
        //
        // When we enter the phase that I is contained in Ia and I satisfies (*), we know that all subsequent
        // intervals remain having these properties. Then our task is to shrink the length of the interval
        // so that we can eventually find a point in I that satisfies the strong Wolfe condition. To make this
        // happen, in this phase we impose the following safeguarding rule: if the length of I does not
        // decrease by a factor delta < 1 after two trials, then a bisection step is used for the next iteration.
        // [1] uses delta = 0.66.
        //
        // Now the remaining problem is how to select the next iterate step+ after we obtain the updated
        // interval I+. In [1], it is based on information of (f_lo, f_hi, f_t) and (g_lo, g_hi, g_t),
        // where f_lo = f(I_lo), f_hi = f(I_hi), f_t = f(step), g_lo = f'(I_lo), g_hi = f'(I_hi), and g_t = f'(step).
        // f is initially set to psi, and if for some step we have psi(step) <= 0 and phi'(step) >= 0,
        // we set f to phi in subsequent iterations.
        //
        // First define:
        // 1. ac to be the minimizer of the cubic function that interpolates f_lo, f_t, g_lo, and g_t;
        // 2. aq to be the minimizer of the quadratic function that interpolates f_lo, f_t, and g_lo;
        // 3. as to be the minimizer of the quadratic function that interpolates f_lo, g_lo, and g_t.
        // Then the selection of step+ follows a 4-cases rule:
        // 1. f_t > f_lo:
        //        step+ = { ac,              if |ac - I_lo| < |aq - I_lo|,
        //                { (aq + ac) / 2},  otherwise.
        // 2. f_t <= f_lo and g_t * g_lo < 0:
        //        step+ = { ac,  if |ac - step| >= |as - step|,
        //                { as,  otherwise.
        // 3. f_t <= f_lo, g_t * g_lo >= 0, and |g_t| <= |g_l|:
        //    If ac exists and is at the correct direction, i.e., (ac - step) * (step - I_lo) > 0,
        //        step+ = { ac,  if |ac - step| < |as - step|,
        //                { as,  otherwise.
        //    otherwise,
        //        step+ = as.
        //    Safeguarding:
        //        step+ = { min{step + delta * (I_hi - step), step+},  if step > I_lo,
        //                { max{step + delta * (I_hi - step), step+},  otherwise,
        // where delta < 1. [1] takes delta = 0.66.
        // 4. f_t <= f_lo, g_t * g_lo >= 0, and |g_t| > |g_l|:
        //        step+ is the minimizer of the cubic function that interpolates f_hi, f_t, g_hi, and g_t.
        //
        // Implementation note: the step values computed using the method above are just candidates, and
        // we must ensure that the safeguarding rules mentioned earlier are respected:
        // 1. In Case II, we set step+ = min{step + delta * (step - I_lo), alpha_max}.
        // 2. If alpha_min > 0 and psi(alpha_k) > 0 or psi'(alpha_k) >= 0 holds from the beginning, we clamp
        //    step+ to [alpha_min, max{delta_min * step, alpha_min}].
        // 3. When we enter the phase that I is contained in Ia and I satisfies (*), a bisection step is used
        //    if the length of I does not decrease by a factor delta < 1 after two trials.

        // Check the value of step
        const Scalar step_min = param.min_step;
        if (step <= Scalar(0))
            throw std::invalid_argument("'step' must be positive");
        if (step < step_min)
            throw std::invalid_argument("'step' is smaller than 'param.min_step'");
        if (step > step_max)
            throw std::invalid_argument("'step' exceeds 'step_max'");

        // Save the function value at the current x
        const Scalar fx_init = fx;
        // Projection of gradient on the search direction
        const Scalar dg_init = dg;

        // std::cout << "fx_init = " << fx_init << ", dg_init = " << dg_init << std::endl << std::endl;

        // Make sure d points to a descent direction
        if (dg_init >= Scalar(0))
            throw std::logic_error("the moving direction does not decrease the objective function value");

        // Tolerance for convergence test
        // Sufficient decrease
        const Scalar test_decr = param.ftol * dg_init;
        // Curvature
        const Scalar test_curv = -param.wolfe * dg_init;

        // The bracketing interval
        const Scalar Inf = std::numeric_limits<Scalar>::infinity();
        Scalar I_lo = Scalar(0), I_hi = Inf;
        Scalar fI_lo = Scalar(0), fI_hi = Inf;
        Scalar gI_lo = (Scalar(1) - param.ftol) * dg_init, gI_hi = Inf;
        Scalar psiI_lo = fI_lo;
        // We also need to save x and grad for step=I_lo, since we want to return the best
        // step size along the path when strong Wolfe condition is not met
        Vector x_lo = xp, grad_lo = grad;
        Scalar fx_lo = fx_init, dg_lo = dg_init;

        // Status variables
        bool bracketed = false;
        bool f_is_psi = true;
        bool use_step_min_safeguard = (step_min > Scalar(0));
        Scalar I_width = Inf;
        Scalar I_width_prev = Inf;
        int I_shrink_fail_count = 0;

        // Constants
        const Scalar delta_max = Scalar(1.1);
        const Scalar delta_min = Scalar(7) / Scalar(12);
        const Scalar shrink = Scalar(0.66);
        int iter;
        for (iter = 0; iter < param.max_linesearch; iter++)
        {
            // Function value and gradient at the current step size
            x.noalias() = xp + step * drt;
            fx = f(x, grad);
            dg = grad.dot(drt);

            // std::cout << "[min_tep, max_step] = [" << step_min << ", " << step_max << "], step = " << step << ", fx = " << fx << ", dg = " << dg << std::endl;

            // phi(step) = f(xp + step * drt) = fx
            // phi'(step) = g(xp + step * drt)^T d = dg
            // psi(step) = f(xp + step * drt) - f(xp) - step * test_decr
            // psi'(step) = dg - test_decr
            const Scalar psit = fx - fx_init - step * test_decr;
            const Scalar dpsit = dg - test_decr;

            // std::cout << "psi(step) = " << psit << ", phi'(step) = " << dpsit << std::endl;

            // Convergence test
            if (psit <= Scalar(0) && abs(dg) <= test_curv)
            {
                // std::cout << "** Criteria met\n\n";
                // std::cout << "========================= Leaving line search =========================\n\n";
                return;
            }

            // Test whether step hits the boundaries and satisfies the exit conditions
            if (step <= step_min && (psit > Scalar(0) || dpsit >= Scalar(0)))
            {
                // std::cout << "** Exits at step_min\n\n";
                // std::cout << "========================= Leaving line search =========================\n\n";
                return;
            }
            if (step >= step_max && (psit <= Scalar(0) && dpsit < Scalar(0)))
            {
                // std::cout << "** Exits at step_max\n\n";
                // std::cout << "========================= Leaving line search =========================\n\n";
                return;
            }

            // Check and update the status of f_is_psi
            // f is initially set to psi, and is changed to phi in
            // subsequent iterations if psi(step) <= 0 and phi'(step) >= 0
            //
            // NOTE: empirically we find that using psi is usually better,
            //       so for now we do not follow the implementation of [1]
            /*
            if (f_is_psi && (psit <= Scalar(0) && dg >= Scalar(0)))
            {
                f_is_psi = false;
            }
            */
            const Scalar ft = f_is_psi ? psit : fx;
            const Scalar gt = f_is_psi ? dpsit : dg;

            // Check and update the status of use_step_min_safeguard
            // We impose a safeguarding rule that guarantees testing
            // step_min if psi(alpha_k) > 0 or psi'(alpha_k) >= 0
            // holds from the beginning
            if (use_step_min_safeguard && (psit <= Scalar(0) && dpsit < Scalar(0)))
            {
                use_step_min_safeguard = false;
            }

            // Update new step
            Scalar new_step;
            const bool in_case_2 = (psit <= psiI_lo) && (dpsit * (I_lo - step) > Scalar(0));
            if (in_case_2)
            {
                // For Case 2, we apply the safeguarding rule
                // newat = min(at + delta * (at - al), amax), delta in [1.1, 4]
                new_step = (std::min)(step_max, step + delta_max * (step - I_lo));

                // We can also consider the following scheme:
                // First let step_selection() decide a value, and then project to the range above
                //
                // new_step = step_selection(I_lo, I_hi, step, fI_lo, fI_hi, ft, gI_lo, gI_hi, gt);
                // const Scalar delta2 = Scalar(4)
                // const Scalar t1 = step + delta * (step - I_lo);
                // const Scalar t2 = step + delta2 * (step - I_lo);
                // const Scalar tl = std::min(t1, t2), tu = std::max(t1, t2);
                // new_step = std::min(tu, std::max(tl, new_step));
                // new_step = std::min(step_max, new_step);
            }
            else
            {
                // For Case 1 and Case 3, use information of f and g to select new step
                new_step = step_selection(I_lo, I_hi, step, fI_lo, fI_hi, ft, gI_lo, gI_hi, gt);
                // Force new step in [step_min, step_max]
                new_step = (std::max)(new_step, step_min);
                new_step = (std::min)(new_step, step_max);

                // Apply safeguarding rule related to step_min when necessary:
                //     step+ in [alpha_min, max{delta_min * step, alpha_min}]
                //
                // If use_step_min_safeguard = true, then new_step cannot be obtained
                // from Case 2, since in Case 2 we have
                //     psi(alpha_k) <= 0 and psi'(alpha_k) < 0
                if (use_step_min_safeguard)
                {
                    const Scalar lower = step_min;
                    const Scalar upper = (std::max)(step_min, delta_min * step);
                    new_step = (std::max)(new_step, lower);
                    new_step = (std::min)(new_step, upper);
                }
            }

            // Update bracketing interval
            if (psit > psiI_lo)
            {
                // Case 1: psi(step) > psi(I_lo)
                I_hi = step;
                fI_hi = ft;
                gI_hi = gt;

                // std::cout << "** Case 1" << std::endl;
            }
            else if (in_case_2)
            {
                // Case 2: psi(step) <= psi(I_lo), psi'(step)(I_lo - step) > 0
                I_lo = step;
                fI_lo = ft;
                gI_lo = gt;
                psiI_lo = psit;
                // Move x and grad to x_lo and grad_lo, respectively
                x_lo.swap(x);
                grad_lo.swap(grad);
                fx_lo = fx;
                dg_lo = dg;

                // std::cout << "** Case 2" << std::endl;
            }
            else
            {
                // Case 3: psi(step) <= psi(I_lo), psi'(step)(I_lo - step) <= 0
                I_hi = I_lo;
                fI_hi = fI_lo;
                gI_hi = gI_lo;

                I_lo = step;
                fI_lo = ft;
                gI_lo = gt;
                psiI_lo = psit;
                // Move x and grad to x_lo and grad_lo, respectively
                x_lo.swap(x);
                grad_lo.swap(grad);
                fx_lo = fx;
                dg_lo = dg;

                // std::cout << "** Case 3" << std::endl;
            }

            // Check and update the status of bracketed
            // bracketed is true if we have entered Case 1 or Case 3,
            // and I is contained in [step_min, step_max]
            if ((!bracketed) && (!in_case_2))
            {
                const Scalar I_left = (std::min)(I_lo, I_hi);
                const Scalar I_right = (std::max)(I_lo, I_hi);
                bracketed = (I_left >= step_min && I_right <= step_max);
            }

            // If bracketed, enforce sufficient interval shrink; if not shrinking enough, use bisection
            if (bracketed)
            {
                I_width_prev = I_width;
                I_width = abs(I_hi - I_lo);
                // Test interval shrinkage
                if (I_width_prev < Inf && I_width > shrink * I_width_prev)
                {
                    I_shrink_fail_count += 1;
                }
                else
                {
                    I_shrink_fail_count = 0;
                }
                // If interval fails to shrink enough twice, select new_step using bisection
                if (I_shrink_fail_count >= 2)
                {
                    new_step = (I_lo + I_hi) / Scalar(2);
                    I_shrink_fail_count = 0;
                }
            }

            // Set the new_step
            step = new_step;

            // std::cout << "[I+_lo, I+_hi] = [" << I_lo << ", " << I_hi << "], step+ = " << step << std::endl << std::endl;
        }

        // If we have used up all line search iterations, then the strong Wolfe condition
        // is not met. We choose not to raise an exception, but to return the best
        // step size so far
        if (iter >= param.max_linesearch)
        {
            // std::cout << "** Maximum step size reached\n\n";
            // std::cout << "========================= Leaving line search =========================\n\n";

            // Return everything with _lo
            step = I_lo;
            fx = fx_lo;
            dg = dg_lo;
            // Move {x, grad}_lo back
            x.swap(x_lo);
            grad.swap(grad_lo);
        }
    }
};

}  // namespace LBFGSpp

#endif  // LBFGSPP_LINE_SEARCH_MORE_THUENTE_H
