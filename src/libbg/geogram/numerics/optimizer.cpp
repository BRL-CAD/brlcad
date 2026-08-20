/*
 *  Copyright (c) 2000-2022 Inria
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  * Neither the name of the ALICE Project-Team nor the names of its
 *  contributors may be used to endorse or promote products derived from this
 *  software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  Contact: Bruno Levy
 *
 *     https://www.inria.fr/fr/bruno-levy
 *
 *     Inria,
 *     Domaine de Voluceau,
 *     78150 Le Chesnay - Rocquencourt
 *     FRANCE
 *
 */

#include "common.h"
#include <geogram/numerics/optimizer.h>
#include <LBFGS.h>

#include <algorithm>
#include <exception>
#include <iostream>
#include <limits>

namespace {

    /**
     * \brief Adapts LBFGS++'s line-search hook to Geogram's accepted-iteration
     * callback.
     *
     * LBFGS++ deliberately keeps its core interface small and does not expose
     * a progress callback.  A line search returns once it has selected the
     * next accepted iterate, so this wrapper provides the same callback timing
     * expected by Geogram without modifying the third-party headers.
     */
    template <typename Scalar>
    class LineSearchWithProgress {
        using Vector = Eigen::Matrix<Scalar, Eigen::Dynamic, 1>;

    public:
        template <typename Function>
        static void LineSearch(
            Function& function,
            const LBFGSpp::LBFGSParam<Scalar>& parameters,
            const Vector& previous_x,
            const Vector& direction,
            const Scalar& max_step,
            Scalar& step,
            Scalar& function_value,
            Vector& gradient,
            Scalar& directional_gradient,
            Vector& x
        ) {
            try {
                LBFGSpp::LineSearchNocedalWright<Scalar>::LineSearch(
                    function, parameters, previous_x, direction, max_step,
                    step, function_value, gradient, directional_gradient, x
                );
            } catch(...) {
                // Match libLBFGS's useful failure behavior: retain the last
                // accepted iterate if a trial line search cannot proceed.
                x = previous_x;
                throw;
            }
            function.accepted_iteration(x, function_value, gradient);
        }
    };

}

namespace GEOBRL {

    /**
     * \brief Implementation of Geogram's optimizer interface using LBFGS++.
     */
    class LBFGSOptimizer final : public Optimizer {
    private:
        using Vector = Eigen::VectorXd;

        class Function {
        public:
            explicit Function(LBFGSOptimizer& optimizer) :
                optimizer_(optimizer) {
            }

            double operator()(const Vector& x, Vector& gradient) {
                double function_value = 0.0;
                optimizer_.funcgrad_callback_(
                    optimizer_.n_, const_cast<double*>(x.data()),
                    function_value, gradient.data()
                );
                return function_value;
            }

            void accepted_iteration(
                const Vector& x, double function_value,
                const Vector& gradient
            ) {
                if(optimizer_.newiteration_callback_ != nullptr) {
                    optimizer_.newiteration_callback_(
                        optimizer_.n_, x.data(), function_value,
                        gradient.data(), gradient.norm()
                    );
                }
            }

        private:
            LBFGSOptimizer& optimizer_;
        };

    public:
        void optimize(double* x) override {
            if(
                x == nullptr || funcgrad_callback_ == nullptr || n_ == 0 ||
                n_ > index_t(std::numeric_limits<int>::max())
            ) {
                if(verbose_) {
                    std::cerr << "LBFGS: invalid optimization problem"
                              << std::endl;
                }
                return;
            }

            LBFGSpp::LBFGSParam<double> parameters;
            parameters.m = (m_ == 0) ? 6 : int(std::min(
                m_, index_t(std::numeric_limits<int>::max())
            ));
            parameters.max_iterations = int(std::min(
                max_iter_, index_t(std::numeric_limits<int>::max())
            ));
            parameters.epsilon = std::max(epsg_, 0.0);

            // Geogram's CVT path sets all three tolerances to zero in order
            // to request a fixed number of iterations.  LBFGS++ has a second,
            // independent relative-gradient tolerance, so it must be disabled
            // explicitly to preserve that behavior.
            parameters.epsilon_rel = 0.0;
            if(epsf_ > 0.0) {
                parameters.past = 1;
                parameters.delta = epsf_;
            } else {
                parameters.past = 0;
                parameters.delta = 0.0;
            }

            Vector variables(static_cast<Eigen::Index>(n_));
            std::copy_n(x, n_, variables.data());

            Function function(*this);
            try {
                LBFGSpp::LBFGSSolver<double, LineSearchWithProgress> solver(
                    parameters
                );
                double function_value = 0.0;
                solver.minimize(function, variables, function_value);
            } catch(const std::exception& error) {
                if(verbose_) {
                    std::cerr << "LBFGS optimization stopped: "
                              << error.what() << std::endl;
                }
            } catch(...) {
                if(verbose_) {
                    std::cerr << "LBFGS optimization stopped" << std::endl;
                }
            }

            std::copy_n(variables.data(), n_, x);
        }
    };

    Optimizer::Optimizer() :
        n_(0),
        m_(6),
        max_iter_(1000),
        funcgrad_callback_(nullptr),
        newiteration_callback_(nullptr),
        evalhessian_callback_(nullptr),
        epsg_(0),
        epsf_(0),
        epsx_(0),
        verbose_(true) {
    }

    Optimizer::~Optimizer() {
    }

    std::shared_ptr<Optimizer> Optimizer::create(const std::string& name) {
        if(
            name == "default" || name == "LBFGS" ||
            name == "HLBFG" || name == "HLBFGS"
        ) {
            return std::make_shared<LBFGSOptimizer>();
        }
        return nullptr;
    }
}
