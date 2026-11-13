/*                J O I N T _ P R O B L E M . C P P
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
#include <string>
#include <unordered_map>
#include <vector>

#include "raytrace.h"
#include "rt/primitives/joint.h"

struct joint_problem_variable {
    size_t joint;
    size_t dof;
};

struct joint_point_residual {
    std::string link;
    point_t point;
    point_t target;
    fastf_t weight;
};

struct rt_joint_problem {
    std::vector<struct rt_joint_internal *> joints;
    std::vector<joint_problem_variable> variables;
    std::vector<joint_point_residual> points;
    std::unordered_map<std::string, size_t> incoming;
    std::string root;
    mat_t root_matrix;
    struct bu_vls *messages;
};

static void
joint_dof_step(mat_t step, const struct rt_joint_internal *joint, size_t dof_index)
{
    const auto &dof = joint->dofs[dof_index];
    MAT_IDN(step);
    if (dof.motion == RT_JOINT_ROTATION) {
	point_t origin = VINIT_ZERO;
	bn_mat_arb_rot(step, origin, dof.axis, dof.current);
    } else {
	MAT_DELTAS(step, dof.axis[X] * dof.current,
		dof.axis[Y] * dof.current, dof.axis[Z] * dof.current);
    }
}

static bool
joint_affects_link(const struct rt_joint_problem *problem, size_t joint_index,
	const std::string &link)
{
    std::string current = link;

    while (current != problem->root) {
	auto incoming = problem->incoming.find(current);
	if (incoming == problem->incoming.end())
	    return false;
	if (incoming->second == joint_index)
	    return true;
	current = bu_vls_cstr(&problem->joints[incoming->second]->parent_link);
    }
    return false;
}

extern "C" int
rt_joint_problem_create(struct rt_joint_problem **result,
	struct rt_joint_internal * const *joints, size_t joint_count,
	const struct rt_joint_eval_options *options)
{
    struct rt_joint_eval *evaluation = nullptr;
    std::vector<const struct rt_joint_internal *> const_joints;
    struct rt_joint_problem *problem;

    if (!result || !joints || !joint_count || !options || !options->root_link)
	return BRLCAD_ERROR;
    *result = nullptr;
    const_joints.assign(joints, joints + joint_count);
    if (rt_joint_eval_create(&evaluation, const_joints.data(), joint_count, options) != BRLCAD_OK)
	return BRLCAD_ERROR;
    rt_joint_eval_destroy(evaluation);

    problem = new rt_joint_problem;
    problem->joints.assign(joints, joints + joint_count);
    problem->root = options->root_link;
    problem->messages = options->messages;
    if (options->root_matrix)
	MAT_COPY(problem->root_matrix, options->root_matrix);
    else
	MAT_IDN(problem->root_matrix);
    for (size_t joint_index = 0; joint_index < joint_count; ++joint_index) {
	struct rt_joint_internal *joint = joints[joint_index];
	problem->incoming.emplace(bu_vls_cstr(&joint->child_link), joint_index);
	for (size_t dof = 0; dof < joint->dof_count; ++dof)
	    problem->variables.push_back({joint_index, dof});
    }
    *result = problem;
    return BRLCAD_OK;
}

extern "C" void rt_joint_problem_destroy(struct rt_joint_problem *problem) { delete problem; }
extern "C" size_t rt_joint_problem_variable_count(const struct rt_joint_problem *problem) { return problem ? problem->variables.size() : 0; }
extern "C" size_t rt_joint_problem_residual_count(const struct rt_joint_problem *problem) { return problem ? 3 * problem->points.size() : 0; }

extern "C" int
rt_joint_problem_variable(const struct rt_joint_problem *problem, size_t variable,
	size_t *joint, size_t *dof, fastf_t *lower, fastf_t *upper)
{
    if (!problem || variable >= problem->variables.size())
	return BRLCAD_ERROR;
    const joint_problem_variable &entry = problem->variables[variable];
    const auto &joint_dof = problem->joints[entry.joint]->dofs[entry.dof];
    if (joint) *joint = entry.joint;
    if (dof) *dof = entry.dof;
    if (lower) *lower = (joint_dof.flags & RT_JOINT_DOF_LIMITED) ? joint_dof.lower : -std::numeric_limits<fastf_t>::infinity();
    if (upper) *upper = (joint_dof.flags & RT_JOINT_DOF_LIMITED) ? joint_dof.upper : std::numeric_limits<fastf_t>::infinity();
    return BRLCAD_OK;
}

extern "C" int
rt_joint_problem_values(const struct rt_joint_problem *problem, fastf_t *values, size_t count)
{
    if (!problem || !values || count != problem->variables.size())
	return BRLCAD_ERROR;
    for (size_t i = 0; i < count; ++i) {
	const joint_problem_variable &entry = problem->variables[i];
	values[i] = problem->joints[entry.joint]->dofs[entry.dof].current;
    }
    return BRLCAD_OK;
}

extern "C" int
rt_joint_problem_set_values(struct rt_joint_problem *problem, const fastf_t *values, size_t count)
{
    if (!problem || !values || count != problem->variables.size())
	return BRLCAD_ERROR;
    for (size_t i = 0; i < count; ++i) {
	fastf_t lower, upper;
	if (!std::isfinite(values[i]) || rt_joint_problem_variable(problem, i, nullptr, nullptr, &lower, &upper) != BRLCAD_OK || values[i] < lower || values[i] > upper)
	    return BRLCAD_ERROR;
    }
    for (size_t i = 0; i < count; ++i) {
	const joint_problem_variable &entry = problem->variables[i];
	problem->joints[entry.joint]->dofs[entry.dof].current = values[i];
    }
    return BRLCAD_OK;
}

extern "C" int
rt_joint_problem_add_point(struct rt_joint_problem *problem,
	const struct rt_joint_point_constraint *constraint)
{
    const size_t minimum_size = offsetof(struct rt_joint_point_constraint, reserved);
    joint_point_residual point;

    if (!problem || !constraint || constraint->size < minimum_size ||
	constraint->version != RT_JOINT_POINT_CONSTRAINT_VERSION || constraint->flags ||
	!constraint->link || !constraint->link[0] || !std::isfinite(constraint->weight) ||
	constraint->weight <= 0.0)
	return BRLCAD_ERROR;
    for (size_t i = 0; i < 3; ++i)
	if (!std::isfinite(constraint->point[i]) || !std::isfinite(constraint->target[i]))
	    return BRLCAD_ERROR;
    if (constraint->link != problem->root && problem->incoming.find(constraint->link) == problem->incoming.end())
	return BRLCAD_ERROR;
    point.link = constraint->link;
    VMOVE(point.point, constraint->point);
    VMOVE(point.target, constraint->target);
    point.weight = constraint->weight;
    problem->points.push_back(point);
    return BRLCAD_OK;
}

extern "C" int
rt_joint_problem_evaluate(const struct rt_joint_problem *problem,
	fastf_t *residuals, size_t residual_count,
	fastf_t *jacobian, size_t jacobian_count)
{
    struct rt_joint_eval *evaluation = nullptr;
    struct rt_joint_eval_options options = RT_JOINT_EVAL_OPTIONS_INIT;
    std::vector<const struct rt_joint_internal *> joints;
    const size_t variable_count = problem ? problem->variables.size() : 0;
    const size_t expected_residuals = problem ? 3 * problem->points.size() : 0;

    if (!problem || !residuals || residual_count != expected_residuals ||
	(jacobian && jacobian_count != expected_residuals * variable_count) ||
	(!jacobian && jacobian_count))
	return BRLCAD_ERROR;
    joints.assign(problem->joints.begin(), problem->joints.end());
    options.root_link = problem->root.c_str();
    options.root_matrix = problem->root_matrix;
    options.messages = problem->messages;
    if (rt_joint_eval_create(&evaluation, joints.data(), joints.size(), &options) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (jacobian)
	std::fill(jacobian, jacobian + jacobian_count, 0.0);

    for (size_t constraint_index = 0; constraint_index < problem->points.size(); ++constraint_index) {
	const joint_point_residual &constraint = problem->points[constraint_index];
	mat_t link_matrix;
	point_t world_point;
	if (rt_joint_eval_find(link_matrix, evaluation, constraint.link.c_str()) != BRLCAD_OK) {
	    rt_joint_eval_destroy(evaluation);
	    return BRLCAD_ERROR;
	}
	MAT4X3PNT(world_point, link_matrix, constraint.point);
	for (size_t component = 0; component < 3; ++component)
	    residuals[3 * constraint_index + component] = constraint.weight *
		(world_point[component] - constraint.target[component]);

	if (!jacobian)
	    continue;
	for (size_t variable_index = 0; variable_index < variable_count; ++variable_index) {
	    const joint_problem_variable &variable = problem->variables[variable_index];
	    const struct rt_joint_internal *joint = problem->joints[variable.joint];
	    const auto &dof = joint->dofs[variable.dof];
	    mat_t parent_world;
	    mat_t prefix;
	    vect_t axis;
	    vect_t derivative;
	    point_t origin = VINIT_ZERO;
	    point_t world_origin;

	    if (!joint_affects_link(problem, variable.joint, constraint.link))
		continue;
	    if (rt_joint_eval_find(parent_world, evaluation,
		    bu_vls_cstr(&joint->parent_link)) != BRLCAD_OK) {
		rt_joint_eval_destroy(evaluation);
		return BRLCAD_ERROR;
	    }
	    bn_mat_mul(prefix, parent_world, joint->parent_frame);
	    for (size_t preceding = 0; preceding < variable.dof; ++preceding) {
		mat_t step;
		mat_t combined;
		joint_dof_step(step, joint, preceding);
		bn_mat_mul(combined, prefix, step);
		MAT_COPY(prefix, combined);
	    }
	    MAT4X3VEC(axis, prefix, dof.axis);
	    MAT4X3PNT(world_origin, prefix, origin);
	    if (dof.motion == RT_JOINT_ROTATION) {
		vect_t offset;
		VSUB2(offset, world_point, world_origin);
		VCROSS(derivative, axis, offset);
	    } else {
		VMOVE(derivative, axis);
	    }
	    for (size_t component = 0; component < 3; ++component)
		jacobian[(3 * constraint_index + component) * variable_count + variable_index] =
		    constraint.weight * derivative[component];
	}
    }
    rt_joint_eval_destroy(evaluation);
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
