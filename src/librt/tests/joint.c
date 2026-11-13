/*                         J O I N T . C
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

#include <math.h>

#include "bu/app.h"
#include "raytrace.h"
#include "rt/primitives/joint.h"

static int
close_enough(fastf_t actual, fastf_t expected)
{
    return NEAR_EQUAL(actual, expected, 1.0e-12);
}

static int
make_test_joint(struct rt_db_internal *intern)
{
    const char *arguments[] = {
	"type", "revolute",
	"parent", "upper-arm",
	"child", "forearm",
	"parent_path", "/robot/upper-arm",
	"child_path", "/robot/forearm",
	"parent_frame", "{1 0 0 10 0 1 0 0 0 0 1 0 0 0 0 1}",
	"child_frame", "{1 0 0 0 0 1 0 5 0 0 1 0 0 0 0 1}",
	"pose_translation", "{1 2 3}",
	"pose_rotation", "{0 0 0.7071067811865475 0.7071067811865476}",
	"pose_scale", "{2 3 4}",
	"dof0", "rotation limited {0 0 1} -1 1 0.5"
    };
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    point_t origin = VINIT_ZERO;
    int result;

    RT_DB_INTERNAL_INIT(intern);
    result = OBJ[ID_JOINT].ft_make(&OBJ[ID_JOINT], intern, NULL, origin, 1.0);
    if (result == BRLCAD_OK)
	result = OBJ[ID_JOINT].ft_adjust(&messages, intern,
		(int)(sizeof(arguments) / sizeof(arguments[0])), arguments);
    if (result != BRLCAD_OK)
	bu_log("joint setup failed: %s\n", bu_vls_cstr(&messages));
    bu_vls_free(&messages);
    return result;
}

static int
check_joint(const struct rt_db_internal *intern)
{
    const struct rt_joint_internal *joint = (const struct rt_joint_internal *)intern->idb_ptr;

    if (joint->type != RT_JOINT_REVOLUTE || joint->dof_count != 1 ||
	!BU_STR_EQUAL(bu_vls_cstr(&joint->parent_link), "upper-arm") ||
	!BU_STR_EQUAL(bu_vls_cstr(&joint->child_link), "forearm") ||
	!BU_STR_EQUAL(bu_vls_cstr(&joint->parent_path), "/robot/upper-arm") ||
	!BU_STR_EQUAL(bu_vls_cstr(&joint->child_path), "/robot/forearm") ||
	!close_enough(joint->pose_translation[X], 1.0) ||
	!close_enough(joint->pose_translation[Y], 2.0) ||
	!close_enough(joint->pose_translation[Z], 3.0) ||
	!close_enough(joint->pose_scale[X], 2.0) ||
	!close_enough(joint->pose_scale[Y], 3.0) ||
	!close_enough(joint->pose_scale[Z], 4.0) ||
	joint->dofs[0].motion != RT_JOINT_ROTATION ||
	!(joint->dofs[0].flags & RT_JOINT_DOF_LIMITED) ||
	!close_enough(joint->dofs[0].current, 0.5))
	return BRLCAD_ERROR;
    return BRLCAD_OK;
}

static int
test_data_round_trip(void)
{
    struct rt_db_internal source;
    struct rt_db_internal restored;
    struct rt_data_context context = RT_DATA_CONTEXT_INIT;
    struct rt_data_node *data = NULL;
    int result = BRLCAD_ERROR;

    if (make_test_joint(&source) != BRLCAD_OK)
	return BRLCAD_ERROR;
    RT_DB_INTERNAL_INIT(&restored);
    if (rt_obj_data_export(&data, &source, &context) != BRLCAD_OK)
	goto cleanup;
    if (rt_obj_data_import(&restored, data, &context) != BRLCAD_OK)
	goto cleanup;
    result = check_joint(&restored);

cleanup:
    rt_data_free(data);
    rt_db_free_internal(&source);
    rt_db_free_internal(&restored);
    return result;
}

static int
test_db5_round_trip(void)
{
    struct rt_db_internal source;
    struct rt_db_internal restored;
    struct bu_external external = BU_EXTERNAL_INIT_ZERO;
    int result = BRLCAD_ERROR;

    if (make_test_joint(&source) != BRLCAD_OK)
	return BRLCAD_ERROR;
    RT_DB_INTERNAL_INIT(&restored);
    if (OBJ[ID_JOINT].ft_export5(&external, &source, 1.0, NULL) != BRLCAD_OK)
	goto cleanup;
    if (OBJ[ID_JOINT].ft_import5(&restored, &external, bn_mat_identity, NULL) != BRLCAD_OK)
	goto cleanup;
    result = check_joint(&restored);

cleanup:
    bu_free_external(&external);
    rt_db_free_internal(&source);
    rt_db_free_internal(&restored);
    return result;
}

static int
test_relative_matrix(void)
{
    struct rt_db_internal intern;
    mat_t actual;
    mat_t expected;
    mat_t rotation;
    mat_t child_inverse;
    mat_t combined;
    mat_t pose;
    mat_t posed;
    point_t origin = VINIT_ZERO;
    size_t i;
    int result = BRLCAD_ERROR;

    if (make_test_joint(&intern) != BRLCAD_OK)
	return BRLCAD_ERROR;
    bn_mat_arb_rot(rotation, origin, ((struct rt_joint_internal *)intern.idb_ptr)->dofs[0].axis, 0.5);
    if (rt_joint_pose_mat(pose, (struct rt_joint_internal *)intern.idb_ptr) != BRLCAD_OK)
	goto cleanup;
    bn_mat_inverse(child_inverse, ((struct rt_joint_internal *)intern.idb_ptr)->child_frame);
    bn_mat_mul(combined, ((struct rt_joint_internal *)intern.idb_ptr)->parent_frame, rotation);
    bn_mat_mul(posed, combined, pose);
    bn_mat_mul(expected, posed, child_inverse);
    if (rt_joint_relative_mat(actual, (struct rt_joint_internal *)intern.idb_ptr) != BRLCAD_OK)
	goto cleanup;
    for (i = 0; i < ELEMENTS_PER_MAT; i++) {
	if (!close_enough(actual[i], expected[i]))
	    goto cleanup;
    }
    result = BRLCAD_OK;

cleanup:
    rt_db_free_internal(&intern);
    return result;
}

static int
test_pose_matrix(void)
{
    struct rt_db_internal intern;
    mat_t actual;
    const fastf_t expected[ELEMENTS_PER_MAT] = {
	0.0, -3.0, 0.0, 1.0,
	2.0, 0.0, 0.0, 2.0,
	0.0, 0.0, 4.0, 3.0,
	0.0, 0.0, 0.0, 1.0
    };
    size_t i;
    int result = BRLCAD_ERROR;

    if (make_test_joint(&intern) != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (rt_joint_pose_mat(actual, (struct rt_joint_internal *)intern.idb_ptr) != BRLCAD_OK)
	goto cleanup;
    for (i = 0; i < ELEMENTS_PER_MAT; i++) {
	if (!close_enough(actual[i], expected[i]))
	    goto cleanup;
    }
    result = BRLCAD_OK;

cleanup:
    rt_db_free_internal(&intern);
    return result;
}

static int
make_graph_joint(struct rt_db_internal *intern, const char *parent,
	const char *child, const char *dof)
{
    const char *arguments[] = {
	"type", "general",
	"parent", parent,
	"child", child,
	"dof0", dof
    };
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    point_t origin = VINIT_ZERO;
    int result;

    RT_DB_INTERNAL_INIT(intern);
    result = OBJ[ID_JOINT].ft_make(&OBJ[ID_JOINT], intern, NULL, origin, 1.0);
    if (result == BRLCAD_OK)
	result = OBJ[ID_JOINT].ft_adjust(&messages, intern,
		(int)(sizeof(arguments) / sizeof(arguments[0])), arguments);
    if (result != BRLCAD_OK)
	bu_log("graph joint setup failed: %s\n", bu_vls_cstr(&messages));
    bu_vls_free(&messages);
    return result;
}

static int
test_forward_kinematics(void)
{
    struct rt_db_internal first;
    struct rt_db_internal second;
    const struct rt_joint_internal *joints[2];
    struct rt_joint_eval *evaluation = NULL;
    struct rt_joint_eval_options options = RT_JOINT_EVAL_OPTIONS_INIT;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    mat_t matrix;
    mat_t root_matrix = MAT_INIT_IDN;
    int result = BRLCAD_ERROR;

    if (make_graph_joint(&first, "base", "slide",
	    "translation unlimited {0 0 1} 0 0 5") != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (make_graph_joint(&second, "slide", "tool",
	    "translation unlimited {1 0 0} 0 0 2") != BRLCAD_OK)
	goto cleanup;
    joints[0] = (const struct rt_joint_internal *)first.idb_ptr;
    joints[1] = (const struct rt_joint_internal *)second.idb_ptr;
    options.root_link = "base";
    MAT_DELTAS(root_matrix, 10.0, 0.0, 0.0);
    options.root_matrix = root_matrix;
    options.messages = &messages;
    if (rt_joint_eval_create(&evaluation, joints, 2, &options) != BRLCAD_OK) {
	bu_log("forward kinematics failed: %s\n", bu_vls_cstr(&messages));
	goto cleanup;
    }
    if (rt_joint_eval_count(evaluation) != 3 ||
	rt_joint_eval_find(matrix, evaluation, "tool") != BRLCAD_OK ||
	!close_enough(matrix[3], 12.0) ||
	!close_enough(matrix[7], 0.0) ||
	!close_enough(matrix[11], 5.0))
	goto cleanup;
    result = BRLCAD_OK;

cleanup:
    rt_joint_eval_destroy(evaluation);
    bu_vls_free(&messages);
    rt_db_free_internal(&second);
    rt_db_free_internal(&first);
    return result;
}

static int
test_forward_kinematics_cycle(void)
{
    struct rt_db_internal first;
    struct rt_db_internal second;
    const struct rt_joint_internal *joints[2];
    struct rt_joint_eval *evaluation = NULL;
    struct rt_joint_eval_options options = RT_JOINT_EVAL_OPTIONS_INIT;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    int result = BRLCAD_ERROR;

    if (make_graph_joint(&first, "base", "slide",
	    "translation unlimited {0 0 1} 0 0 5") != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (make_graph_joint(&second, "slide", "base",
	    "translation unlimited {1 0 0} 0 0 2") != BRLCAD_OK)
	goto cleanup;
    joints[0] = (const struct rt_joint_internal *)first.idb_ptr;
    joints[1] = (const struct rt_joint_internal *)second.idb_ptr;
    options.root_link = "base";
    options.messages = &messages;
    if (rt_joint_eval_create(&evaluation, joints, 2, &options) == BRLCAD_ERROR &&
	strstr(bu_vls_cstr(&messages), "cycle"))
	result = BRLCAD_OK;

cleanup:
    rt_joint_eval_destroy(evaluation);
    bu_vls_free(&messages);
    rt_db_free_internal(&second);
    rt_db_free_internal(&first);
    return result;
}

static int
test_joint_problem(void)
{
    struct rt_db_internal first;
    struct rt_db_internal second;
    struct rt_joint_internal *joints[2];
    struct rt_joint_problem *problem = NULL;
    struct rt_joint_eval_options options = RT_JOINT_EVAL_OPTIONS_INIT;
    struct rt_joint_point_constraint constraint = RT_JOINT_POINT_CONSTRAINT_INIT;
    struct rt_joint_solve_options solve_options = RT_JOINT_SOLVE_OPTIONS_INIT;
    struct rt_joint_solve_options limited_options = RT_JOINT_SOLVE_OPTIONS_INIT;
    struct rt_joint_solve_result solve_result = RT_JOINT_SOLVE_RESULT_INIT;
    fastf_t residuals[3];
    fastf_t jacobian[6];
    fastf_t values[2];
    int result = BRLCAD_ERROR;

    if (make_graph_joint(&first, "base", "slide",
	    "translation unlimited {0 0 1} 0 0 5") != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (make_graph_joint(&second, "slide", "tool",
	    "translation unlimited {1 0 0} 0 0 2") != BRLCAD_OK)
	goto cleanup;
    joints[0] = (struct rt_joint_internal *)first.idb_ptr;
    joints[1] = (struct rt_joint_internal *)second.idb_ptr;
    options.root_link = "base";
    if (rt_joint_problem_create(&problem, joints, 2, &options) != BRLCAD_OK)
	goto cleanup;
    constraint.link = "tool";
    VSET(constraint.target, 3.0, 0.0, 7.0);
    if (rt_joint_problem_add_point(problem, &constraint) != BRLCAD_OK ||
	rt_joint_problem_variable_count(problem) != 2 ||
	rt_joint_problem_residual_count(problem) != 3 ||
	rt_joint_problem_evaluate(problem, residuals, 3, jacobian, 6) != BRLCAD_OK ||
	!close_enough(residuals[X], -1.0) ||
	!close_enough(residuals[Y], 0.0) ||
	!close_enough(residuals[Z], -2.0) ||
	!close_enough(jacobian[0], 0.0) ||
	!close_enough(jacobian[1], 1.0) ||
	!close_enough(jacobian[4], 1.0) ||
	!close_enough(jacobian[5], 0.0))
	goto cleanup;
    V2SET(values, 7.0, 3.0);
    if (rt_joint_problem_set_values(problem, values, 2) != BRLCAD_OK ||
	rt_joint_problem_evaluate(problem, residuals, 3, NULL, 0) != BRLCAD_OK ||
	!close_enough(residuals[X], 0.0) || !close_enough(residuals[Z], 0.0))
	goto cleanup;
    V2SET(values, 5.0, 2.0);
    limited_options.max_iterations = 1;
    if (rt_joint_problem_set_values(problem, values, 2) != BRLCAD_OK ||
	rt_joint_problem_solve(problem, &limited_options, &solve_result) != BRLCAD_OK ||
	solve_result.status != RT_JOINT_SOLVE_MAX_ITERATIONS ||
	solve_result.iterations != 1 || solve_result.accepted_steps != 1)
	goto cleanup;
    V2SET(values, 5.0, 2.0);
    if (rt_joint_problem_set_values(problem, values, 2) != BRLCAD_OK ||
	rt_joint_problem_solve(problem, &solve_options, &solve_result) != BRLCAD_OK ||
	solve_result.status != RT_JOINT_SOLVE_CONVERGED_RESIDUAL ||
	solve_result.rank != 2 || solve_result.accepted_steps == 0 ||
	rt_joint_problem_values(problem, values, 2) != BRLCAD_OK ||
	!NEAR_EQUAL(values[0], 7.0, 1.0e-8) ||
	!NEAR_EQUAL(values[1], 3.0, 1.0e-8))
	goto cleanup;
    result = BRLCAD_OK;

cleanup:
    rt_joint_problem_destroy(problem);
    rt_db_free_internal(&second);
    rt_db_free_internal(&first);
    return result;
}

static int
test_joint_solver_bounds(void)
{
    struct rt_db_internal intern;
    struct rt_joint_internal *joints[1];
    struct rt_joint_problem *problem = NULL;
    struct rt_joint_eval_options eval_options = RT_JOINT_EVAL_OPTIONS_INIT;
    struct rt_joint_point_constraint constraint = RT_JOINT_POINT_CONSTRAINT_INIT;
    struct rt_joint_solve_options solve_options = RT_JOINT_SOLVE_OPTIONS_INIT;
    struct rt_joint_solve_result solve_result = RT_JOINT_SOLVE_RESULT_INIT;
    fastf_t value;
    int result = BRLCAD_ERROR;

    if (make_graph_joint(&intern, "base", "tool",
	    "translation limited {1 0 0} -1 1 0") != BRLCAD_OK)
	return BRLCAD_ERROR;
    joints[0] = (struct rt_joint_internal *)intern.idb_ptr;
    eval_options.root_link = "base";
    constraint.link = "tool";
    constraint.target[X] = 3.0;
    if (rt_joint_problem_create(&problem, joints, 1, &eval_options) != BRLCAD_OK ||
	rt_joint_problem_add_point(problem, &constraint) != BRLCAD_OK ||
	rt_joint_problem_solve(problem, &solve_options, &solve_result) != BRLCAD_OK ||
	solve_result.status != RT_JOINT_SOLVE_CONVERGED_STEP ||
	rt_joint_problem_values(problem, &value, 1) != BRLCAD_OK ||
	!close_enough(value, 1.0))
	goto cleanup;
    result = BRLCAD_OK;

cleanup:
    rt_joint_problem_destroy(problem);
    rt_db_free_internal(&intern);
    return result;
}

static int
test_joint_solver_rank_deficient(void)
{
    struct rt_db_internal first;
    struct rt_db_internal second;
    struct rt_joint_internal *joints[2];
    struct rt_joint_problem *problem = NULL;
    struct rt_joint_eval_options eval_options = RT_JOINT_EVAL_OPTIONS_INIT;
    struct rt_joint_point_constraint constraint = RT_JOINT_POINT_CONSTRAINT_INIT;
    struct rt_joint_solve_options solve_options = RT_JOINT_SOLVE_OPTIONS_INIT;
    struct rt_joint_solve_result solve_result = RT_JOINT_SOLVE_RESULT_INIT;
    fastf_t values[2];
    int result = BRLCAD_ERROR;

    if (make_graph_joint(&first, "base", "middle",
	    "translation unlimited {1 0 0} 0 0 0") != BRLCAD_OK)
	return BRLCAD_ERROR;
    if (make_graph_joint(&second, "middle", "tool",
	    "translation unlimited {1 0 0} 0 0 0") != BRLCAD_OK)
	goto cleanup;
    joints[0] = (struct rt_joint_internal *)first.idb_ptr;
    joints[1] = (struct rt_joint_internal *)second.idb_ptr;
    eval_options.root_link = "base";
    constraint.link = "tool";
    constraint.target[X] = 2.0;
    if (rt_joint_problem_create(&problem, joints, 2, &eval_options) != BRLCAD_OK ||
	rt_joint_problem_add_point(problem, &constraint) != BRLCAD_OK ||
	rt_joint_problem_solve(problem, &solve_options, &solve_result) != BRLCAD_OK ||
	solve_result.status != RT_JOINT_SOLVE_CONVERGED_RESIDUAL ||
	solve_result.rank != 1 ||
	!(solve_result.flags & RT_JOINT_SOLVE_RESULT_USED_SVD) ||
	rt_joint_problem_values(problem, values, 2) != BRLCAD_OK ||
	!NEAR_EQUAL(values[0] + values[1], 2.0, 1.0e-8))
	goto cleanup;
    result = BRLCAD_OK;

cleanup:
    rt_joint_problem_destroy(problem);
    rt_db_free_internal(&second);
    rt_db_free_internal(&first);
    return result;
}

int
main(int argc, char **argv)
{
    int failures = 0;

    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    if (test_data_round_trip() != BRLCAD_OK) {
	bu_log("typed data round trip failed\n");
	failures++;
    }
    if (test_db5_round_trip() != BRLCAD_OK) {
	bu_log("DB5 round trip failed\n");
	failures++;
    }
    if (test_pose_matrix() != BRLCAD_OK) {
	bu_log("pose matrix evaluation failed\n");
	failures++;
    }
    if (test_relative_matrix() != BRLCAD_OK) {
	bu_log("relative matrix evaluation failed\n");
	failures++;
    }
    if (test_forward_kinematics() != BRLCAD_OK) {
	bu_log("forward kinematics failed\n");
	failures++;
    }
    if (test_forward_kinematics_cycle() != BRLCAD_OK) {
	bu_log("forward kinematics cycle detection failed\n");
	failures++;
    }
    if (test_joint_problem() != BRLCAD_OK) {
	bu_log("joint residual and Jacobian problem failed\n");
	failures++;
    }
    if (test_joint_solver_bounds() != BRLCAD_OK) {
	bu_log("joint bounded solver failed\n");
	failures++;
    }
    if (test_joint_solver_rank_deficient() != BRLCAD_OK) {
	bu_log("joint rank-deficient solver failed\n");
	failures++;
    }
    return failures;
}

