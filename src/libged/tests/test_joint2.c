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

int
main(int argc, char **argv)
{
    struct ged *gedp;
    struct rt_wdb *wdbp;
    int result = BRLCAD_ERROR;

    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    gedp = ged_open("inmem", "0", 0);
    if (!gedp)
	return 1;
    wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp || create_hierarchy(wdbp) != BRLCAD_OK ||
	create_joint(wdbp) != BRLCAD_OK)
	goto cleanup;

    const char *legacy_arguments[] = {"joint", "legacy", "list"};
    const char *legacy_alias_arguments[] = {"joint", "list"};

    if (ged_exec_joint(gedp, 3, legacy_arguments) != BRLCAD_OK ||
	ged_exec_joint(gedp, 2, legacy_alias_arguments) != BRLCAD_OK ||
	set_and_apply(gedp, "5", 0) != BRLCAD_OK ||
	check_child_position(gedp->dbip, 5.0) != BRLCAD_OK ||
	set_and_apply(gedp, "10", 1) != BRLCAD_OK ||
	check_child_position(gedp->dbip, 10.0) != BRLCAD_OK ||
	solve_and_apply(gedp) != BRLCAD_OK ||
	check_child_position(gedp->dbip, 7.0) != BRLCAD_OK)
	goto cleanup;
    result = BRLCAD_OK;

cleanup:
    ged_close(gedp);
    return result == BRLCAD_OK ? 0 : 1;
}
