/*                 J O I N T _ S O L V E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <Eigen/Core>
#include <Eigen/QR>
#include <Eigen/SVD>

#include "rt/primitives/joint.h"

static bool
joint_solver_options_valid(const struct rt_joint_solve_options *options)
{
    const size_t minimum_size = offsetof(struct rt_joint_solve_options, reserved);

    return options && options->size >= minimum_size &&
	options->version == RT_JOINT_SOLVE_OPTIONS_VERSION && !options->flags &&
	options->max_iterations > 0 && std::isfinite(options->initial_damping) &&
	options->initial_damping > 0.0 && std::isfinite(options->damping_increase) &&
	options->damping_increase > 1.0 && std::isfinite(options->damping_decrease) &&
	options->damping_decrease > 0.0 && options->damping_decrease < 1.0 &&
	std::isfinite(options->residual_tolerance) && options->residual_tolerance >= 0.0 &&
	std::isfinite(options->gradient_tolerance) && options->gradient_tolerance >= 0.0 &&
	std::isfinite(options->step_tolerance) && options->step_tolerance >= 0.0;
}

static fastf_t
joint_solver_infinity_norm(const Eigen::VectorXd &values)
{
    return values.size() ? values.cwiseAbs().maxCoeff() : 0.0;
}

static int
joint_solver_evaluate(const struct rt_joint_problem *problem,
	Eigen::VectorXd &residual, Eigen::MatrixXd &jacobian)
{
    const size_t residual_count = rt_joint_problem_residual_count(problem);
    const size_t variable_count = rt_joint_problem_variable_count(problem);
    std::vector<fastf_t> residual_values(residual_count);
    std::vector<fastf_t> jacobian_values(residual_count * variable_count);

    if (rt_joint_problem_evaluate(problem, residual_values.data(), residual_count,
	    jacobian_values.data(), jacobian_values.size()) != BRLCAD_OK)
	return BRLCAD_ERROR;
    residual.resize((Eigen::Index)residual_count);
    jacobian.resize((Eigen::Index)residual_count, (Eigen::Index)variable_count);
    for (size_t row = 0; row < residual_count; ++row) {
	residual[(Eigen::Index)row] = residual_values[row];
	for (size_t column = 0; column < variable_count; ++column)
	    jacobian((Eigen::Index)row, (Eigen::Index)column) =
		jacobian_values[row * variable_count + column];
    }
    if (!residual.allFinite() || !jacobian.allFinite())
	return BRLCAD_ERROR;
    return BRLCAD_OK;
}

static int
joint_solver_set_values(struct rt_joint_problem *problem, const Eigen::VectorXd &values)
{
    std::vector<fastf_t> raw_values((size_t)values.size());

    for (Eigen::Index i = 0; i < values.size(); ++i)
	raw_values[(size_t)i] = values[i];
    return rt_joint_problem_set_values(problem, raw_values.data(), raw_values.size());
}

static Eigen::VectorXd
joint_solver_project(const struct rt_joint_problem *problem,
	const Eigen::VectorXd &candidate)
{
    Eigen::VectorXd projected = candidate;

    for (Eigen::Index i = 0; i < candidate.size(); ++i) {
	fastf_t lower;
	fastf_t upper;
	rt_joint_problem_variable(problem, (size_t)i, nullptr, nullptr, &lower, &upper);
	projected[i] = std::max(lower, std::min(upper, candidate[i]));
    }
    return projected;
}

static Eigen::VectorXd
joint_solver_step(const Eigen::MatrixXd &jacobian, const Eigen::VectorXd &residual,
	fastf_t damping, size_t *rank, bool *used_svd)
{
    const Eigen::Index rows = jacobian.rows();
    const Eigen::Index columns = jacobian.cols();
    Eigen::MatrixXd augmented(rows + columns, columns);
    Eigen::VectorXd right_hand_side(rows + columns);
    Eigen::VectorXd scales = jacobian.colwise().squaredNorm().cwiseMax(1.0).cwiseSqrt();
    Eigen::ColPivHouseholderQR<Eigen::MatrixXd> rank_qr(jacobian);

    *rank = (size_t)rank_qr.rank();
    *used_svd = false;
    augmented.topRows(rows) = jacobian;
    augmented.bottomRows(columns).setZero();
    augmented.bottomRows(columns).diagonal() = std::sqrt(damping) * scales;
    right_hand_side.head(rows) = -residual;
    right_hand_side.tail(columns).setZero();
    if (rank_qr.rank() < columns) {
	Eigen::JacobiSVD<Eigen::MatrixXd> svd(augmented,
		Eigen::ComputeThinU | Eigen::ComputeThinV);
	*used_svd = true;
	return svd.solve(right_hand_side);
    }
    Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr(augmented);
    return qr.solve(right_hand_side);
}

extern "C" int
rt_joint_problem_solve(struct rt_joint_problem *problem,
	const struct rt_joint_solve_options *options,
	struct rt_joint_solve_result *result)
{
    const size_t minimum_result_size = offsetof(struct rt_joint_solve_result, damping) + sizeof(result->damping);
    const size_t variable_count = rt_joint_problem_variable_count(problem);
    const size_t residual_count = rt_joint_problem_residual_count(problem);
    std::vector<fastf_t> initial_values(variable_count);
    Eigen::VectorXd values((Eigen::Index)variable_count);
    Eigen::VectorXd residual;
    Eigen::MatrixXd jacobian;
    fastf_t damping;

    if (!problem || !joint_solver_options_valid(options) || !result ||
	result->size < minimum_result_size || result->version != RT_JOINT_SOLVE_RESULT_VERSION ||
	!variable_count || !residual_count ||
	rt_joint_problem_values(problem, initial_values.data(), variable_count) != BRLCAD_OK)
	return BRLCAD_ERROR;
    for (size_t i = 0; i < variable_count; ++i)
	values[(Eigen::Index)i] = initial_values[i];
    if (joint_solver_evaluate(problem, residual, jacobian) != BRLCAD_OK)
	return BRLCAD_ERROR;

    result->status = RT_JOINT_SOLVE_MAX_ITERATIONS;
    result->flags = 0;
    result->iterations = 0;
    result->accepted_steps = 0;
    result->rejected_steps = 0;
    result->rank = 0;
    result->initial_cost = result->final_cost = 0.5 * residual.squaredNorm();
    result->gradient_norm = 0.0;
    result->step_norm = 0.0;
    damping = options->initial_damping;

    for (size_t iteration = 0; iteration < options->max_iterations; ++iteration) {
	const fastf_t residual_norm = residual.norm();
	Eigen::VectorXd gradient = jacobian.transpose() * residual;
	Eigen::VectorXd step;
	Eigen::VectorXd candidate;
	Eigen::VectorXd candidate_residual;
	Eigen::MatrixXd candidate_jacobian;
	fastf_t candidate_cost;
	bool used_svd;

	result->iterations = iteration;
	result->final_cost = 0.5 * residual.squaredNorm();
	result->gradient_norm = joint_solver_infinity_norm(gradient);
	result->damping = damping;
	if (residual_norm <= options->residual_tolerance) {
	    result->status = RT_JOINT_SOLVE_CONVERGED_RESIDUAL;
	    return BRLCAD_OK;
	}
	if (result->gradient_norm <= options->gradient_tolerance) {
	    result->status = RT_JOINT_SOLVE_CONVERGED_GRADIENT;
	    return BRLCAD_OK;
	}
	result->iterations = iteration + 1;
	step = joint_solver_step(jacobian, residual, damping, &result->rank, &used_svd);
	if (used_svd)
	    result->flags |= RT_JOINT_SOLVE_RESULT_USED_SVD;
	if (!step.allFinite()) {
	    result->status = RT_JOINT_SOLVE_NUMERICAL_FAILURE;
	    return BRLCAD_ERROR;
	}
	candidate = joint_solver_project(problem, values + step);
	step = candidate - values;
	result->step_norm = step.norm();
	if (result->step_norm <= options->step_tolerance *
		(values.norm() + options->step_tolerance)) {
	    result->status = RT_JOINT_SOLVE_CONVERGED_STEP;
	    return BRLCAD_OK;
	}
	if (joint_solver_set_values(problem, candidate) != BRLCAD_OK ||
	    joint_solver_evaluate(problem, candidate_residual, candidate_jacobian) != BRLCAD_OK) {
	    joint_solver_set_values(problem, values);
	    result->status = RT_JOINT_SOLVE_NUMERICAL_FAILURE;
	    return BRLCAD_ERROR;
	}
	candidate_cost = 0.5 * candidate_residual.squaredNorm();
	if (candidate_cost < result->final_cost) {
	    values = candidate;
	    residual = candidate_residual;
	    jacobian = candidate_jacobian;
	    damping = std::max(damping * options->damping_decrease,
		std::numeric_limits<fastf_t>::min());
	    result->accepted_steps++;
	} else {
	    joint_solver_set_values(problem, values);
	    damping *= options->damping_increase;
	    result->rejected_steps++;
	    if (!std::isfinite(damping)) {
		result->status = RT_JOINT_SOLVE_NUMERICAL_FAILURE;
		return BRLCAD_ERROR;
	    }
	}
    }
    result->iterations = options->max_iterations;
    result->final_cost = 0.5 * residual.squaredNorm();
    result->damping = damping;
    return BRLCAD_OK;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
