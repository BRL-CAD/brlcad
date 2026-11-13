/*                    J O I N T _ S Y S T E M . C P P
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

#include <cmath>
#include <vector>

#include "bu/opt.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/joint.h"

#include "ged_private.h"

class joint_object_collection {
    public:
	~joint_object_collection()
	{
	    for (auto &internal : internals)
		rt_db_free_internal(&internal);
	}

	int load(struct ged *gedp, int count, const char *names[])
	{
	    directories.reserve((size_t)count);
	    internals.reserve((size_t)count);
	    joints.reserve((size_t)count);
	    for (int i = 0; i < count; ++i) {
		struct directory *directory = db_lookup(gedp->dbip, names[i], LOOKUP_QUIET);
		struct rt_db_internal internal;
		if (!directory) {
		    bu_vls_printf(gedp->ged_result_str, "joint object not found: %s", names[i]);
		    return BRLCAD_ERROR;
		}
		RT_DB_INTERNAL_INIT(&internal);
		if (rt_db_get_internal(&internal, directory, gedp->dbip, bn_mat_identity) < 0) {
		    bu_vls_printf(gedp->ged_result_str, "unable to read joint object: %s", names[i]);
		    return BRLCAD_ERROR;
		}
		if (internal.idb_type != ID_JOINT) {
		    bu_vls_printf(gedp->ged_result_str, "%s is not a joint", names[i]);
		    rt_db_free_internal(&internal);
		    return BRLCAD_ERROR;
		}
		directories.push_back(directory);
		internals.push_back(internal);
		joints.push_back((struct rt_joint_internal *)internals.back().idb_ptr);
	    }
	    return BRLCAD_OK;
	}

	int write(struct ged *gedp)
	{
	    for (size_t i = 0; i < internals.size(); ++i) {
		if (rt_db_put_internal(directories[i], gedp->dbip, &internals[i]) < 0) {
		    bu_vls_printf(gedp->ged_result_str, "unable to update joint object: %s",
			    directories[i]->d_namep);
		    return BRLCAD_ERROR;
		}
	    }
	    return BRLCAD_OK;
	}

	std::vector<struct directory *> directories;
	std::vector<struct rt_db_internal> internals;
	std::vector<struct rt_joint_internal *> joints;
};

static void
joint_system_print_matrix(struct bu_vls *output, const mat_t matrix)
{
    for (size_t i = 0; i < ELEMENTS_PER_MAT; ++i)
	bu_vls_printf(output, "%s%.17g", i ? " " : "", matrix[i]);
}

extern "C" int
_ged_joint_graph_core(struct ged *gedp, int argc, const char *argv[])
{
    joint_object_collection objects;
    struct rt_joint_eval_options options = RT_JOINT_EVAL_OPTIONS_INIT;
    struct rt_joint_eval *evaluation = nullptr;
    std::vector<const struct rt_joint_internal *> joints;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (argc < 3 || !BU_STR_EQUAL(argv[0], "evaluate")) {
	bu_vls_strcat(gedp->ged_result_str,
		"Usage: joint graph evaluate root_link joint_name [joint_name ...]");
	return argc ? BRLCAD_ERROR : GED_HELP;
    }
    if (objects.load(gedp, argc - 2, argv + 2) != BRLCAD_OK)
	return BRLCAD_ERROR;
    joints.assign(objects.joints.begin(), objects.joints.end());
    options.root_link = argv[1];
    options.messages = gedp->ged_result_str;
    if (rt_joint_eval_create(&evaluation, joints.data(), joints.size(), &options) != BRLCAD_OK)
	return BRLCAD_ERROR;
    bu_vls_trunc(gedp->ged_result_str, 0);
    for (size_t i = 0; i < rt_joint_eval_count(evaluation); ++i) {
	mat_t matrix;
	rt_joint_eval_matrix(matrix, evaluation, i);
	bu_vls_printf(gedp->ged_result_str, "%s%s ", i ? "\n" : "",
		rt_joint_eval_link(evaluation, i));
	joint_system_print_matrix(gedp->ged_result_str, matrix);
    }
    rt_joint_eval_destroy(evaluation);
    return BRLCAD_OK;
}

static const char *
joint_solve_status_name(uint32_t status)
{
    switch (status) {
	case RT_JOINT_SOLVE_CONVERGED_RESIDUAL: return "residual";
	case RT_JOINT_SOLVE_CONVERGED_GRADIENT: return "gradient";
	case RT_JOINT_SOLVE_CONVERGED_STEP: return "step";
	case RT_JOINT_SOLVE_MAX_ITERATIONS: return "maximum iterations";
	default: return "numerical failure";
    }
}

extern "C" int
_ged_joint_solve_core(struct ged *gedp, int argc, const char *argv[])
{
    joint_object_collection objects;
    struct rt_joint_eval_options eval_options = RT_JOINT_EVAL_OPTIONS_INIT;
    struct rt_joint_point_constraint constraint = RT_JOINT_POINT_CONSTRAINT_INIT;
    struct rt_joint_solve_options solve_options = RT_JOINT_SOLVE_OPTIONS_INIT;
    struct rt_joint_solve_result solve_result = RT_JOINT_SOLVE_RESULT_INIT;
    struct rt_joint_problem *problem = nullptr;
    fastf_t coordinates[6];
    int result = BRLCAD_ERROR;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (argc < 10 || !BU_STR_EQUAL(argv[0], "point")) {
	bu_vls_strcat(gedp->ged_result_str,
		"Usage: joint solve point root_link target_link "
		"point_x point_y point_z target_x target_y target_z "
		"joint_name [joint_name ...]");
	return argc ? BRLCAD_ERROR : GED_HELP;
    }
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    for (size_t i = 0; i < 6; ++i) {
	if (bu_opt_fastf_t(NULL, 1, &argv[i + 3], &coordinates[i]) != 1 ||
	    !std::isfinite(coordinates[i])) {
	    bu_vls_printf(gedp->ged_result_str, "invalid point coordinate: %s", argv[i + 3]);
	    return BRLCAD_ERROR;
	}
	coordinates[i] *= gedp->dbip->dbi_local2base;
    }
    if (objects.load(gedp, argc - 9, argv + 9) != BRLCAD_OK)
	return BRLCAD_ERROR;
    eval_options.root_link = argv[1];
    eval_options.messages = gedp->ged_result_str;
    constraint.link = argv[2];
    VMOVE(constraint.point, coordinates);
    VMOVE(constraint.target, &coordinates[3]);
    if (rt_joint_problem_create(&problem, objects.joints.data(), objects.joints.size(),
	    &eval_options) != BRLCAD_OK ||
	rt_joint_problem_add_point(problem, &constraint) != BRLCAD_OK)
	goto cleanup;
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (rt_joint_problem_solve(problem, &solve_options, &solve_result) != BRLCAD_OK)
	goto cleanup;
    if (solve_result.status == RT_JOINT_SOLVE_MAX_ITERATIONS ||
	solve_result.status == RT_JOINT_SOLVE_NUMERICAL_FAILURE) {
	bu_vls_printf(gedp->ged_result_str,
		"joint solve did not converge: %s, iterations %zu, cost %.17g",
		joint_solve_status_name(solve_result.status), solve_result.iterations,
		solve_result.final_cost);
	goto cleanup;
    }
    if (objects.write(gedp) != BRLCAD_OK)
	goto cleanup;
    bu_vls_printf(gedp->ged_result_str,
	    "converged %s iterations %zu accepted %zu rejected %zu rank %zu cost %.17g",
	    joint_solve_status_name(solve_result.status), solve_result.iterations,
	    solve_result.accepted_steps, solve_result.rejected_steps,
	    solve_result.rank, solve_result.final_cost);
    result = BRLCAD_OK;

cleanup:
    rt_joint_problem_destroy(problem);
    return result;
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
