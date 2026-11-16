/*                    T E S T _ J O I N T 2 . C
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

#include <string.h>

#include "bu/app.h"
#include "ged.h"
#include "raytrace.h"
#include "rt/primitives/animation.h"
#include "rt/primitives/bot.h"
#include "wdb.h"

#define JOINT_SOLVE_TOLERANCE 1.0e-8

static int
create_joint(struct rt_wdb *wdbp)
{
    const char *arguments[] = {
	"type", "prismatic",
	"parent", "parent",
	"child", "child",
	"parent_path", "assembly.c/parent.c",
	"child_path", "assembly.c/parent.c/child.s"
    };
    struct rt_db_internal intern;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    point_t origin = VINIT_ZERO;
    int result;

    RT_DB_INTERNAL_INIT(&intern);
    result = OBJ[ID_JOINT].ft_make(&OBJ[ID_JOINT], &intern, NULL, origin, 1.0);
    if (result == BRLCAD_OK)
	result = OBJ[ID_JOINT].ft_adjust(&messages, &intern,
		(int)(sizeof(arguments) / sizeof(arguments[0])), arguments);
    if (result == BRLCAD_OK && wdb_put_internal(wdbp, "slide.j", &intern, 1.0) < 0)
	result = BRLCAD_ERROR;
    if (result != BRLCAD_OK) {
	bu_log("joint creation failed: %s\n", bu_vls_cstr(&messages));
	rt_db_free_internal(&intern);
    }
    bu_vls_free(&messages);
    return result;
}

static int
create_hierarchy(struct rt_wdb *wdbp)
{
    struct wmember members;
    point_t center = VINIT_ZERO;

    if (mk_sph(wdbp, "child.s", center, 1.0) < 0)
	return BRLCAD_ERROR;
    BU_LIST_INIT(&members.l);
    if (!mk_addmember("child.s", &members.l, NULL, WMOP_UNION) ||
	mk_comb(wdbp, "parent.c", &members.l, 0, NULL, NULL, NULL,
	    0, 0, 0, 0, 0, 0, 0) < 0)
	return BRLCAD_ERROR;
    BU_LIST_INIT(&members.l);
    if (!mk_addmember("parent.c", &members.l, NULL, WMOP_UNION) ||
	mk_comb(wdbp, "assembly.c", &members.l, 0, NULL, NULL, NULL,
	    0, 0, 0, 0, 0, 0, 0) < 0)
	return BRLCAD_ERROR;
    return BRLCAD_OK;
}

static int
create_animation(struct rt_wdb *wdbp)
{
    static const fastf_t times[] = {0.0, 1.0};
    static const fastf_t values[] = {0.0, 0.0, 0.0, 0.0, 0.0, 4.0};
    struct rt_db_internal intern;
    struct rt_animation_internal *animation;
    struct rt_animation_channel *channel;
    point_t origin = VINIT_ZERO;

    RT_DB_INTERNAL_INIT(&intern);
    if (OBJ[ID_ANIMATION].ft_make(&OBJ[ID_ANIMATION], &intern, NULL,
	    origin, 1.0) != BRLCAD_OK)
	return BRLCAD_ERROR;
    animation = (struct rt_animation_internal *)intern.idb_ptr;
    animation->channel_count = 1;
    animation->channels = (struct rt_animation_channel *)bu_calloc(1,
	sizeof(struct rt_animation_channel), "test animation channel");
    channel = &animation->channels[0];
    bu_vls_init(&channel->target_link);
    bu_vls_strcpy(&channel->target_link, "child");
    channel->target = RT_ANIMATION_TRANSLATION;
    channel->interpolation = RT_ANIMATION_LINEAR;
    channel->keyframe_count = 2;
    channel->times = (fastf_t *)bu_malloc(sizeof(times), "test animation times");
    channel->values = (fastf_t *)bu_malloc(sizeof(values), "test animation values");
    memcpy(channel->times, times, sizeof(times));
    memcpy(channel->values, values, sizeof(values));
    if (rt_animation_validate(animation, NULL) != 0 ||
	wdb_put_internal(wdbp, "motion.anim", &intern, 1.0) < 0) {
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }
    rt_db_free_internal(&intern);
    return BRLCAD_OK;
}

static int
create_skinned_bot(struct rt_wdb *wdbp)
{
    static const fastf_t vertices[] = {
	0.0, 0.0, 0.0,
	0.0, 1.0, 0.0,
	1.0, 0.0, 0.0
    };
    static const int faces[] = {0, 1, 2};
    static const uint32_t offsets[] = {0, 1, 2, 4};
    static const uint32_t indices[] = {0, 1, 0, 1};
    static const fastf_t weights[] = {1.0, 1.0, 0.5, 0.5};
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;

    RT_DB_INTERNAL_INIT(&intern);
    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SURFACE;
    bot->orientation = RT_BOT_CCW;
    bot->bot_flags = RT_BOT_HAS_SKIN;
    bot->num_vertices = 3;
    bot->vertices = (fastf_t *)bu_malloc(sizeof(vertices), "test rig vertices");
    memcpy(bot->vertices, vertices, sizeof(vertices));
    bot->num_faces = 1;
    bot->faces = (int *)bu_malloc(sizeof(faces), "test rig faces");
    memcpy(bot->faces, faces, sizeof(faces));
    BU_ALLOC(bot->skin, struct rt_bot_skin);
    bot->skin->joint_count = 2;
    bot->skin->joint_links = (struct bu_vls *)bu_calloc(2,
	sizeof(struct bu_vls), "test rig links");
    bu_vls_init(&bot->skin->joint_links[0]);
    bu_vls_init(&bot->skin->joint_links[1]);
    bu_vls_strcpy(&bot->skin->joint_links[0], "parent");
    bu_vls_strcpy(&bot->skin->joint_links[1], "child");
    bot->skin->inverse_bind_matrices = (mat_t *)bu_malloc(2 * sizeof(mat_t),
	"test rig inverse binds");
    MAT_IDN(bot->skin->inverse_bind_matrices[0]);
    MAT_IDN(bot->skin->inverse_bind_matrices[1]);
    bot->skin->influence_count = 4;
    bot->skin->vertex_offsets = (uint32_t *)bu_malloc(sizeof(offsets),
	"test rig offsets");
    bot->skin->joint_indices = (uint32_t *)bu_malloc(sizeof(indices),
	"test rig indices");
    bot->skin->weights = (fastf_t *)bu_malloc(sizeof(weights), "test rig weights");
    memcpy(bot->skin->vertex_offsets, offsets, sizeof(offsets));
    memcpy(bot->skin->joint_indices, indices, sizeof(indices));
    memcpy(bot->skin->weights, weights, sizeof(weights));

    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = bot;
    if (rt_bot_skin_validate(bot, NULL) != BRLCAD_OK ||
	wdb_put_internal(wdbp, "rig.bot", &intern, 1.0) < 0) {
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }
    rt_db_free_internal(&intern);
    return BRLCAD_OK;
}

static int
pose_and_check_skin(struct ged *gedp, const char *output_name,
	fastf_t child_position, fastf_t blended_position)
{
    const char *arguments[] = {
	"joint", "skin", "pose", "rig.bot", output_name, "parent", "slide.j"
    };
    struct rt_db_internal intern;
    struct directory *directory;
    int result = BRLCAD_ERROR;

    if (ged_exec_joint(gedp, 7, arguments) != BRLCAD_OK) {
	bu_log("skin pose failed: %s\n", bu_vls_cstr(gedp->ged_result_str));
	return BRLCAD_ERROR;
    }
    directory = db_lookup(gedp->dbip, output_name, LOOKUP_QUIET);
    RT_DB_INTERNAL_INIT(&intern);
    if (!directory || rt_db_get_internal(&intern, directory, gedp->dbip,
	    bn_mat_identity) < 0 || intern.idb_type != ID_BOT)
	goto cleanup;
    {
	const struct rt_bot_internal *bot =
	    (const struct rt_bot_internal *)intern.idb_ptr;
	if (!bot->skin && !(bot->bot_flags & RT_BOT_HAS_SKIN) &&
	    NEAR_EQUAL(bot->vertices[2], 0.0, JOINT_SOLVE_TOLERANCE) &&
	    NEAR_EQUAL(bot->vertices[5], child_position, JOINT_SOLVE_TOLERANCE) &&
	    NEAR_EQUAL(bot->vertices[8], blended_position, JOINT_SOLVE_TOLERANCE))
	    result = BRLCAD_OK;
    }

cleanup:
    rt_db_free_internal(&intern);
    return result;
}

static int
check_child_position(struct db_i *dbip, fastf_t expected)
{
    struct db_full_path path = DB_FULL_PATH_INIT_ZERO;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    const char *objects[] = {"assembly.c"};
    mat_t matrix;
    point_t minimum;
    point_t maximum;
    int result = BRLCAD_ERROR;

    db_full_path_init(&path);
    if (db_string_to_path(&path, dbip,
	    "assembly.c/parent.c/child.s") != 0 ||
	db_path_to_mat(dbip, &path, matrix, 0) != 1)
	goto cleanup;
    if (NEAR_EQUAL(matrix[3], 0.0, JOINT_SOLVE_TOLERANCE) &&
	NEAR_EQUAL(matrix[7], 0.0, JOINT_SOLVE_TOLERANCE) &&
	NEAR_EQUAL(matrix[11], expected, JOINT_SOLVE_TOLERANCE) &&
	!(rt_obj_bounds(&messages, dbip, 1, objects, 1, minimum, maximum) & BRLCAD_ERROR) &&
	NEAR_EQUAL(minimum[Z], expected - 1.0, JOINT_SOLVE_TOLERANCE) &&
	NEAR_EQUAL(maximum[Z], expected + 1.0, JOINT_SOLVE_TOLERANCE))
	result = BRLCAD_OK;

cleanup:
    if (result != BRLCAD_OK) {
	bu_log("position check failed: matrix=(%.17g %.17g %.17g) bounds=(%.17g %.17g) expected=%.17g messages=%s\n",
		matrix[3], matrix[7], matrix[11], minimum[Z], maximum[Z], expected,
		bu_vls_cstr(&messages));
    }
    bu_vls_free(&messages);
    db_free_full_path(&path);
    return result;
}

static int
set_and_apply(struct ged *gedp, const char *position, int compatibility_alias)
{
    const char *joint_set_arguments[] = {"joint", "object", "slide.j", "position", "0", position};
    const char *joint_apply_arguments[] = {"joint", "object", "slide.j", "apply"};
    const char *joint2_set_arguments[] = {"joint2", "slide.j", "position", "0", position};
    const char *joint2_apply_arguments[] = {"joint2", "slide.j", "apply"};

    if ((!compatibility_alias && ged_exec_joint(gedp, 6, joint_set_arguments) != BRLCAD_OK) ||
	(compatibility_alias && ged_exec_joint2(gedp, 5, joint2_set_arguments) != BRLCAD_OK)) {
	bu_log("position failed: %s\n", bu_vls_cstr(gedp->ged_result_str));
	return BRLCAD_ERROR;
    }
    if ((!compatibility_alias && ged_exec_joint(gedp, 4, joint_apply_arguments) != BRLCAD_OK) ||
	(compatibility_alias && ged_exec_joint2(gedp, 3, joint2_apply_arguments) != BRLCAD_OK)) {
	bu_log("apply failed: %s\n", bu_vls_cstr(gedp->ged_result_str));
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
solve_and_apply(struct ged *gedp)
{
    const char *solve_arguments[] = {
	"joint", "solve", "point", "parent", "child",
	"0", "0", "0", "0", "0", "7", "slide.j"
    };
    const char *graph_arguments[] = {"joint", "graph", "evaluate", "parent", "slide.j"};
    const char *apply_arguments[] = {"joint", "object", "slide.j", "apply"};

    if (ged_exec_joint(gedp, 12, solve_arguments) != BRLCAD_OK ||
	!strstr(bu_vls_cstr(gedp->ged_result_str), "converged") ||
	ged_exec_joint(gedp, 5, graph_arguments) != BRLCAD_OK ||
	!strstr(bu_vls_cstr(gedp->ged_result_str), "child") ||
	ged_exec_joint(gedp, 4, apply_arguments) != BRLCAD_OK) {
	bu_log("solve/apply failed: %s\n", bu_vls_cstr(gedp->ged_result_str));
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
solve_closure_and_apply(struct ged *gedp)
{
    const char *solve_arguments[] = {
	"joint", "solve", "closure", "parent", "child", "0", "0", "0",
	"parent", "0", "0", "6", "slide.j"
    };
    const char *apply_arguments[] = {"joint", "object", "slide.j", "apply"};

    if (ged_exec_joint(gedp, 13, solve_arguments) != BRLCAD_OK ||
	!strstr(bu_vls_cstr(gedp->ged_result_str), "converged") ||
	ged_exec_joint(gedp, 4, apply_arguments) != BRLCAD_OK) {
	bu_log("closure solve/apply failed: %s\n", bu_vls_cstr(gedp->ged_result_str));
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

static int
sample_animation(struct ged *gedp, const char *time)
{
    const char *arguments[] = {
	"joint", "animation", "sample", "motion.anim", time, "slide.j"
    };
    if (ged_exec_joint(gedp, 6, arguments) != BRLCAD_OK) {
	bu_log("animation sample failed: %s\n", bu_vls_cstr(gedp->ged_result_str));
	return BRLCAD_ERROR;
    }
    return BRLCAD_OK;
}

int
main(int argc, char **argv)
{
    struct ged *gedp;
    struct rt_wdb *wdbp;
    int result = BRLCAD_ERROR;

    bu_setprogname(argv[0]);
    if (argc != 1 && argc != 2)
	return 1;
    if (argc == 2) {
	bu_file_delete(argv[1]);
	wdbp = wdb_fopen(argv[1]);
	if (!wdbp || create_hierarchy(wdbp) != BRLCAD_OK ||
	    create_skinned_bot(wdbp) != BRLCAD_OK ||
	    create_animation(wdbp) != BRLCAD_OK ||
	    create_joint(wdbp) != BRLCAD_OK) {
	    if (wdbp)
		wdb_close(wdbp);
	    bu_log("unable to create joint test database\n");
	    return 1;
	}
	wdb_close(wdbp);
	gedp = ged_open("db", argv[1], 1);
    } else {
	gedp = ged_open("inmem", "0", 0);
	if (!gedp)
	    return 1;
	wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_INMEM);
	if (!wdbp || create_hierarchy(wdbp) != BRLCAD_OK ||
	    create_skinned_bot(wdbp) != BRLCAD_OK ||
	    create_animation(wdbp) != BRLCAD_OK ||
	    create_joint(wdbp) != BRLCAD_OK) {
	    bu_log("unable to create in-memory joint test database\n");
	    ged_close(gedp);
	    return 1;
	}
    }
    if (!gedp)
	return 1;

    const char *legacy_arguments[] = {"joint", "legacy", "list"};
    const char *legacy_alias_arguments[] = {"joint", "list"};

    if (ged_exec_joint(gedp, 3, legacy_arguments) != BRLCAD_OK ||
	ged_exec_joint(gedp, 2, legacy_alias_arguments) != BRLCAD_OK ||
	pose_and_check_skin(gedp, "bind-pose.bot", 0.0, 0.0) != BRLCAD_OK ||
	sample_animation(gedp, "0.5") != BRLCAD_OK ||
	pose_and_check_skin(gedp, "animated-pose.bot", 2.0, 1.0) != BRLCAD_OK ||
	sample_animation(gedp, "0") != BRLCAD_OK ||
	set_and_apply(gedp, "5", 0) != BRLCAD_OK ||
	check_child_position(gedp->dbip, 5.0) != BRLCAD_OK ||
	pose_and_check_skin(gedp, "moved-pose.bot", 5.0, 2.5) != BRLCAD_OK ||
	set_and_apply(gedp, "10", 1) != BRLCAD_OK ||
	check_child_position(gedp->dbip, 10.0) != BRLCAD_OK ||
	solve_and_apply(gedp) != BRLCAD_OK ||
	check_child_position(gedp->dbip, 7.0) != BRLCAD_OK ||
	solve_closure_and_apply(gedp) != BRLCAD_OK ||
	check_child_position(gedp->dbip, 6.0) != BRLCAD_OK)
	goto cleanup;
    result = BRLCAD_OK;

cleanup:
    ged_close(gedp);
    return result == BRLCAD_OK ? 0 : 1;
}
