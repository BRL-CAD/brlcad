/*            G E O G R A M _ O P T I M I Z E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <geogram/numerics/optimizer.h>

#include "bu.h"

#include <cmath>

namespace {

    int evaluation_count = 0;
    int iteration_count = 0;
    bool callback_values_are_finite = true;

    void rosenbrock(
        GEOBRL::index_t n, double* x, double& function_value, double* gradient
    ) {
        if(n != 2) {
            function_value = 0.0;
            return;
        }
        const double t1 = 1.0 - x[0];
        const double t2 = 10.0 * (x[1] - x[0] * x[0]);
        gradient[1] = 20.0 * t2;
        gradient[0] = -2.0 * (x[0] * gradient[1] + t1);
        function_value = t1 * t1 + t2 * t2;
        ++evaluation_count;
    }

    void new_iteration(
        GEOBRL::index_t n, const double* x, double function_value,
        const double* gradient, double gradient_norm
    ) {
        callback_values_are_finite = callback_values_are_finite && n == 2 &&
            std::isfinite(x[0]) && std::isfinite(x[1]) &&
            std::isfinite(function_value) &&
            std::isfinite(gradient[0]) && std::isfinite(gradient[1]) &&
            std::isfinite(gradient_norm);
        ++iteration_count;
    }

    GEOBRL::Optimizer_var make_optimizer(int max_iterations, double epsilon) {
        GEOBRL::Optimizer_var optimizer = GEOBRL::Optimizer::create("LBFGS");
        if(optimizer) {
            optimizer->set_N(2);
            optimizer->set_M(7);
            optimizer->set_max_iter(GEOBRL::index_t(max_iterations));
            optimizer->set_epsg(epsilon);
            optimizer->set_epsf(0.0);
            optimizer->set_epsx(0.0);
            optimizer->set_verbose(false);
            optimizer->set_funcgrad_callback(rosenbrock);
            optimizer->set_newiteration_callback(new_iteration);
        }
        return optimizer;
    }

}

int main(int UNUSED(argc), const char* argv[])
{
    bu_setprogname(argv[0]);

    if(
        !GEOBRL::Optimizer::create("default") ||
        !GEOBRL::Optimizer::create("HLBFGS") ||
        GEOBRL::Optimizer::create("HLBFGS_HESS") ||
        GEOBRL::Optimizer::create("not-an-optimizer")
    ) {
        bu_log("FAIL optimizer name selection\n");
        return 1;
    }

    evaluation_count = 0;
    iteration_count = 0;
    callback_values_are_finite = true;
    double fixed_x[2] = {-1.2, 1.0};
    GEOBRL::Optimizer_var fixed_optimizer = make_optimizer(5, 0.0);
    if(!fixed_optimizer) {
        bu_log("FAIL unable to create fixed-iteration optimizer\n");
        return 1;
    }
    fixed_optimizer->optimize(fixed_x);
    if(
        iteration_count != 5 || evaluation_count <= iteration_count ||
        !callback_values_are_finite
    ) {
        bu_log(
            "FAIL fixed iteration behavior: %d iterations, %d evaluations\n",
            iteration_count, evaluation_count
        );
        return 1;
    }

    evaluation_count = 0;
    iteration_count = 0;
    callback_values_are_finite = true;
    double converged_x[2] = {-1.2, 1.0};
    GEOBRL::Optimizer_var converged_optimizer = make_optimizer(200, 1e-8);
    converged_optimizer->optimize(converged_x);
    if(
        std::fabs(converged_x[0] - 1.0) > 1e-5 ||
        std::fabs(converged_x[1] - 1.0) > 1e-5 ||
        iteration_count <= 0 || iteration_count >= 200 ||
        !callback_values_are_finite
    ) {
        bu_log(
            "FAIL convergence: x=(%.17g, %.17g), %d iterations\n",
            converged_x[0], converged_x[1], iteration_count
        );
        return 1;
    }

    bu_log(
        "PASS LBFGS optimizer (%d convergence iterations, %d evaluations)\n",
        iteration_count, evaluation_count
    );
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
