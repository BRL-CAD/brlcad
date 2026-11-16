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

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "bu/opt.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/animation.h"
#include "rt/primitives/bot.h"
#include "rt/primitives/joint.h"
#include "wdb.h"

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
_ged_joint_animation_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (!argc) {
	bu_vls_strcat(gedp->ged_result_str,
	    "Usage: joint animation list\n"
	    "       joint animation show animation_name\n"
	    "       joint animation evaluate animation_name time\n"
	    "       joint animation apply animation_name time joint_name [joint_name ...]\n"
	    "       joint animation sample animation_name time joint_name [joint_name ...]");
	return GED_HELP;
    }

    if (BU_STR_EQUAL(argv[0], "list")) {
	std::vector<std::string> names;
	struct directory *directory;
	if (argc != 1)
	    return BRLCAD_ERROR;
	FOR_ALL_DIRECTORY_START(directory, gedp->dbip) {
	    if (directory->d_major_type == DB5_MAJORTYPE_BRLCAD &&
		directory->d_minor_type == ID_ANIMATION)
		names.emplace_back(directory->d_namep);
	} FOR_ALL_DIRECTORY_END;
	std::sort(names.begin(), names.end());
	for (const std::string &name : names)
	    bu_vls_printf(gedp->ged_result_str, "%s\n", name.c_str());
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(argv[0], "apply") || BU_STR_EQUAL(argv[0], "sample"))
	GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    if (argc < 2)
	return BRLCAD_ERROR;
    struct directory *directory = db_lookup(gedp->dbip, argv[1], LOOKUP_QUIET);
    if (!directory) {
	bu_vls_printf(gedp->ged_result_str, "animation object not found: %s", argv[1]);
	return BRLCAD_ERROR;
    }
    struct rt_db_internal animation_internal;
    RT_DB_INTERNAL_INIT(&animation_internal);
    if (rt_db_get_internal(&animation_internal, directory, gedp->dbip,
	    bn_mat_identity) < 0 || animation_internal.idb_type != ID_ANIMATION) {
	bu_vls_printf(gedp->ged_result_str, "%s is not an animation", argv[1]);
	rt_db_free_internal(&animation_internal);
	return BRLCAD_ERROR;
    }
    const auto *animation = static_cast<const struct rt_animation_internal *>(
	animation_internal.idb_ptr);

    if (BU_STR_EQUAL(argv[0], "show")) {
	fastf_t start;
	fastf_t end;
	if (argc != 2 || rt_animation_time_range(&start, &end, animation) != BRLCAD_OK) {
	    rt_db_free_internal(&animation_internal);
	    return BRLCAD_ERROR;
	}
	bu_vls_printf(gedp->ged_result_str,
	    "%s: %.17g %.17g %zu channel%s\n", argv[1], start, end,
	    animation->channel_count, animation->channel_count == 1 ? "" : "s");
	for (size_t channel_index = 0; channel_index < animation->channel_count;
	    ++channel_index) {
	    const struct rt_animation_channel &channel =
		animation->channels[channel_index];
	    bu_vls_printf(gedp->ged_result_str, "%s %s %s %zu %.17g %.17g\n",
		bu_vls_cstr(&channel.target_link),
		rt_animation_target_name(channel.target),
		rt_animation_interpolation_name(channel.interpolation),
		channel.keyframe_count, channel.times[0],
		channel.times[channel.keyframe_count - 1]);
	}
	rt_db_free_internal(&animation_internal);
	return BRLCAD_OK;
    }

    if ((!BU_STR_EQUAL(argv[0], "evaluate") || argc != 3) &&
	(!BU_STR_EQUAL(argv[0], "apply") || argc < 4) &&
	(!BU_STR_EQUAL(argv[0], "sample") || argc < 4)) {
	rt_db_free_internal(&animation_internal);
	return BRLCAD_ERROR;
    }
    fastf_t time;
    if (bu_opt_fastf_t(nullptr, 1, &argv[2], &time) != 1 || !std::isfinite(time)) {
	bu_vls_printf(gedp->ged_result_str, "invalid animation time: %s", argv[2]);
	rt_db_free_internal(&animation_internal);
	return BRLCAD_ERROR;
    }
    struct rt_animation_eval_options options = RT_ANIMATION_EVAL_OPTIONS_INIT;
    struct rt_animation_eval *evaluation = nullptr;
    options.messages = gedp->ged_result_str;
    if (rt_animation_eval_create(&evaluation, animation, time, &options) != BRLCAD_OK) {
	rt_db_free_internal(&animation_internal);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "evaluate")) {
	bu_vls_trunc(gedp->ged_result_str, 0);
	for (size_t entry = 0; entry < rt_animation_eval_count(evaluation); ++entry) {
	    for (uint32_t target = RT_ANIMATION_TRANSLATION;
		target <= RT_ANIMATION_SCALE; ++target) {
		fastf_t value[4];
		if (rt_animation_eval_value(value, evaluation, entry, target) != BRLCAD_OK)
		    continue;
		bu_vls_printf(gedp->ged_result_str, "%s %s",
		    rt_animation_eval_link(evaluation, entry),
		    rt_animation_target_name(target));
		for (size_t component = 0;
		    component < (target == RT_ANIMATION_ROTATION ? 4u : 3u);
		    ++component)
		    bu_vls_printf(gedp->ged_result_str, " %.17g", value[component]);
		bu_vls_putc(gedp->ged_result_str, '\n');
	    }
	}
	rt_animation_eval_destroy(evaluation);
	rt_db_free_internal(&animation_internal);
	return BRLCAD_OK;
    }
    joint_object_collection objects;
    int result = BRLCAD_ERROR;
    if (objects.load(gedp, argc - 3, argv + 3) != BRLCAD_OK)
	goto cleanup;
    for (size_t entry = 0; entry < rt_animation_eval_count(evaluation); ++entry) {
	struct rt_joint_internal *joint = nullptr;
	const char *link = rt_animation_eval_link(evaluation, entry);
	for (struct rt_joint_internal *candidate : objects.joints) {
	    if (!BU_STR_EQUAL(bu_vls_cstr(&candidate->child_link), link))
		continue;
	    if (joint) {
		bu_vls_printf(gedp->ged_result_str,
		    "multiple joints have animation target link: %s", link);
		goto cleanup;
	    }
	    joint = candidate;
	}
	if (!joint) {
	    bu_vls_printf(gedp->ged_result_str,
		"animation target link is not a supplied joint child: %s", link);
	    goto cleanup;
	}
	(void)rt_animation_eval_value(joint->pose_translation, evaluation, entry,
	    RT_ANIMATION_TRANSLATION);
	(void)rt_animation_eval_value(joint->pose_rotation, evaluation, entry,
	    RT_ANIMATION_ROTATION);
	(void)rt_animation_eval_value(joint->pose_scale, evaluation, entry,
	    RT_ANIMATION_SCALE);
    }
    for (struct rt_joint_internal *joint : objects.joints) {
	if (rt_joint_validate(joint, gedp->ged_result_str) != 0)
	    goto cleanup;
    }
    if (objects.write(gedp) != BRLCAD_OK)
	goto cleanup;
    bu_vls_printf(gedp->ged_result_str, "%s %s at %.17g",
	BU_STR_EQUAL(argv[0], "sample") ? "sampled" : "applied", argv[1], time);
    result = BRLCAD_OK;

cleanup:
    rt_animation_eval_destroy(evaluation);
    rt_db_free_internal(&animation_internal);
    return result;
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

extern "C" int
_ged_joint_skin_core(struct ged *gedp, int argc, const char *argv[])
{
    joint_object_collection objects;
    struct rt_joint_eval_options options = RT_JOINT_EVAL_OPTIONS_INIT;
    struct rt_joint_eval *evaluation = nullptr;
    struct rt_animation_eval_options animation_options = RT_ANIMATION_EVAL_OPTIONS_INIT;
    struct rt_animation_eval *animation_evaluation = nullptr;
    struct rt_db_internal animation_internal;
    struct rt_db_internal bind_internal;
    struct rt_db_internal posed_internal;
    struct rt_bot_internal *posed = nullptr;
    const struct rt_bot_internal *bind_bot = nullptr;
    struct directory *animation_directory = nullptr;
    struct directory *bind_directory = nullptr;
    struct rt_wdb *wdbp = nullptr;
    mat_t *joint_matrices = nullptr;
    std::vector<const struct rt_joint_internal *> joints;
    fastf_t time = 0.0;
    bool animated;
    int bind_index;
    int output_index;
    int root_index;
    int joint_index;
    int result = BRLCAD_ERROR;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);
    animated = argc > 0 && BU_STR_EQUAL(argv[0], "pose-at");
    if ((!animated && (argc < 5 || !BU_STR_EQUAL(argv[0], "pose"))) ||
	(animated && argc < 7)) {
	bu_vls_strcat(gedp->ged_result_str,
	    "Usage: joint skin pose bind_bot posed_bot root_link "
	    "joint_name [joint_name ...]\n"
	    "       joint skin pose-at animation_name time bind_bot posed_bot "
	    "root_link joint_name [joint_name ...]");
	return argc ? BRLCAD_ERROR : GED_HELP;
    }
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    bind_index = animated ? 3 : 1;
    output_index = bind_index + 1;
    root_index = bind_index + 2;
    joint_index = bind_index + 3;
    RT_DB_INTERNAL_INIT(&animation_internal);
    RT_DB_INTERNAL_INIT(&bind_internal);
    RT_DB_INTERNAL_INIT(&posed_internal);
    if (animated) {
	animation_directory = db_lookup(gedp->dbip, argv[1], LOOKUP_QUIET);
	if (bu_opt_fastf_t(nullptr, 1, &argv[2], &time) != 1 ||
	    !std::isfinite(time)) {
	    bu_vls_printf(gedp->ged_result_str, "invalid animation time: %s", argv[2]);
	    return BRLCAD_ERROR;
	}
	if (!animation_directory ||
	    rt_db_get_internal(&animation_internal, animation_directory, gedp->dbip,
		bn_mat_identity) < 0 || animation_internal.idb_type != ID_ANIMATION) {
	    bu_vls_printf(gedp->ged_result_str,
		"animation object not found or invalid: %s", argv[1]);
	    rt_db_free_internal(&animation_internal);
	    return BRLCAD_ERROR;
	}
	animation_options.messages = gedp->ged_result_str;
	if (rt_animation_eval_create(&animation_evaluation,
		static_cast<const struct rt_animation_internal *>(
		    animation_internal.idb_ptr), time,
		&animation_options) != BRLCAD_OK)
	    goto cleanup;
    }
    bind_directory = db_lookup(gedp->dbip, argv[bind_index], LOOKUP_QUIET);
    if (!bind_directory) {
	bu_vls_printf(gedp->ged_result_str, "skin bind BOT not found: %s",
	    argv[bind_index]);
	goto cleanup;
    }
    if (db_lookup(gedp->dbip, argv[output_index], LOOKUP_QUIET)) {
	bu_vls_printf(gedp->ged_result_str, "skin pose output already exists: %s",
	    argv[output_index]);
	goto cleanup;
    }
    if (rt_db_get_internal(&bind_internal, bind_directory, gedp->dbip,
	    bn_mat_identity) < 0) {
	bu_vls_printf(gedp->ged_result_str, "unable to read skin bind BOT: %s",
	    argv[bind_index]);
	goto cleanup;
    }
    if (bind_internal.idb_type != ID_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a BOT", argv[bind_index]);
	goto cleanup;
    }
    bind_bot = static_cast<const struct rt_bot_internal *>(bind_internal.idb_ptr);
    if (rt_bot_skin_validate(bind_bot, gedp->ged_result_str) != BRLCAD_OK)
	goto cleanup;
    if (objects.load(gedp, argc - joint_index, argv + joint_index) != BRLCAD_OK)
	goto cleanup;
    options.root_link = argv[root_index];
    options.messages = gedp->ged_result_str;
    joints.assign(objects.joints.begin(), objects.joints.end());
    if ((animated && rt_joint_eval_create_animated(&evaluation, joints.data(),
	    joints.size(), &options, animation_evaluation) != BRLCAD_OK) ||
	(!animated && rt_joint_eval_create(&evaluation, joints.data(), joints.size(),
	    &options) != BRLCAD_OK))
	goto cleanup;

    joint_matrices = static_cast<mat_t *>(bu_calloc(bind_bot->skin->joint_count,
	sizeof(mat_t), "joint skin pose matrices"));
    for (size_t joint = 0; joint < bind_bot->skin->joint_count; ++joint) {
	const char *link = bu_vls_cstr(&bind_bot->skin->joint_links[joint]);
	if (rt_joint_eval_find(joint_matrices[joint], evaluation, link) != BRLCAD_OK) {
	    bu_vls_printf(gedp->ged_result_str,
		"skin joint link is not in evaluated graph: %s", link);
	    goto cleanup;
	}
    }
    if (rt_bot_skin_pose(&posed, bind_bot, joint_matrices,
	    bind_bot->skin->joint_count, gedp->ged_result_str) != BRLCAD_OK)
	goto cleanup;

    posed_internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    posed_internal.idb_type = ID_BOT;
    posed_internal.idb_meth = &OBJ[ID_BOT];
    posed_internal.idb_ptr = posed;
    posed = nullptr;
    wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (!wdbp || wdb_put_internal(wdbp, argv[output_index],
	    &posed_internal, 1.0) < 0) {
	bu_vls_printf(gedp->ged_result_str, "unable to write skin pose: %s",
	    argv[output_index]);
	goto cleanup;
    }
    bu_vls_printf(gedp->ged_result_str, "posed %s", argv[output_index]);
    result = BRLCAD_OK;

cleanup:
    if (posed) {
	rt_bot_internal_free(posed);
	BU_PUT(posed, struct rt_bot_internal);
    }
    bu_free(joint_matrices, "joint skin pose matrices");
    rt_joint_eval_destroy(evaluation);
    rt_animation_eval_destroy(animation_evaluation);
    rt_db_free_internal(&animation_internal);
    rt_db_free_internal(&posed_internal);
    rt_db_free_internal(&bind_internal);
    return result;
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
    struct rt_joint_closure_constraint closure = RT_JOINT_CLOSURE_CONSTRAINT_INIT;
    struct rt_joint_solve_options solve_options = RT_JOINT_SOLVE_OPTIONS_INIT;
    struct rt_joint_solve_result solve_result = RT_JOINT_SOLVE_RESULT_INIT;
    struct rt_joint_problem *problem = nullptr;
    fastf_t coordinates[6];
    int joint_start;
    bool point_mode;
    int result = BRLCAD_ERROR;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    bu_vls_trunc(gedp->ged_result_str, 0);
    point_mode = argc > 0 && BU_STR_EQUAL(argv[0], "point");
    if ((!point_mode && !(argc > 0 && BU_STR_EQUAL(argv[0], "closure"))) ||
	(point_mode && argc < 10) || (!point_mode && argc < 11)) {
	bu_vls_strcat(gedp->ged_result_str,
		"Usage: joint solve point root_link target_link "
		"point_x point_y point_z target_x target_y target_z "
		"joint_name [joint_name ...]\n"
		"       joint solve closure root_link first_link "
		"first_x first_y first_z second_link "
		"second_x second_y second_z joint_name [joint_name ...]");
	return argc ? BRLCAD_ERROR : GED_HELP;
    }
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    for (size_t i = 0; i < 6; ++i) {
	size_t argument = point_mode ? i + 3 : i + 3 + (i >= 3 ? 1 : 0);
	if (bu_opt_fastf_t(NULL, 1, &argv[argument], &coordinates[i]) != 1 ||
	    !std::isfinite(coordinates[i])) {
	    bu_vls_printf(gedp->ged_result_str, "invalid point coordinate: %s",
		argv[argument]);
	    return BRLCAD_ERROR;
	}
	coordinates[i] *= gedp->dbip->dbi_local2base;
    }
    joint_start = point_mode ? 9 : 10;
    if (objects.load(gedp, argc - joint_start, argv + joint_start) != BRLCAD_OK)
	return BRLCAD_ERROR;
    eval_options.root_link = argv[1];
    eval_options.messages = gedp->ged_result_str;
    if (point_mode) {
	constraint.link = argv[2];
	VMOVE(constraint.point, coordinates);
	VMOVE(constraint.target, &coordinates[3]);
    } else {
	closure.first_link = argv[2];
	VMOVE(closure.first_point, coordinates);
	closure.second_link = argv[6];
	VMOVE(closure.second_point, &coordinates[3]);
    }
    if (rt_joint_problem_create(&problem, objects.joints.data(),
	    objects.joints.size(), &eval_options) != BRLCAD_OK ||
	(point_mode && rt_joint_problem_add_point(problem, &constraint) != BRLCAD_OK) ||
	(!point_mode && rt_joint_problem_add_closure(problem, &closure) != BRLCAD_OK))
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
