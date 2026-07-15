/*                        E D P I P E . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/edpipe.c
 *
 * Functions -
 *
 * pipe_split_pnt - split a pipe segment at a given point
 *
 * find_pipe_pnt_nearest_pnt - find which segment of a pipe is nearest
 * the ray from "pt" in the viewing direction (for segment selection
 * in MGED)
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "ged.h"
#include "wdb.h"
#include "bu/cmdschema.h"

#include "../ged_private.h"


static int
pipe_packed_point_schema_validate(const struct bu_cmd_schema *cmd, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result);
static int
pipe_find_schema_validate(const struct bu_cmd_schema *cmd, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result);


static const struct bu_cmd_option pipe_move_schema_options[] = {
    BU_CMD_FLAG_UNBOUND("r", NULL, "r", "Interpret the point relative to the pipe point"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand pipe_find_schema_operands[] = {
    BU_CMD_OPERAND("pipe", BU_CMD_VALUE_DB_PATH, 1, 1, "Pipe object or path", NULL),
    BU_CMD_OPERAND("point", BU_CMD_VALUE_VECTOR, 1, 3,
	"Packed XYZ point or three coordinates", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand pipe_index_schema_operands[] = {
    BU_CMD_OPERAND("pipe", BU_CMD_VALUE_DB_PATH, 1, 1, "Pipe object or path", NULL),
    BU_CMD_OPERAND("segment", BU_CMD_VALUE_INTEGER, 1, 1, "Pipe point index", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand pipe_insert_schema_operands[] = {
    BU_CMD_OPERAND("pipe", BU_CMD_VALUE_DB_PATH, 1, 1, "Pipe object or path", NULL),
    BU_CMD_OPERAND("point", BU_CMD_VALUE_VECTOR, 1, 1, "Packed XYZ point", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand pipe_move_schema_operands[] = {
    BU_CMD_OPERAND("pipe", BU_CMD_VALUE_DB_PATH, 1, 1, "Pipe object or path", NULL),
    BU_CMD_OPERAND("segment", BU_CMD_VALUE_INTEGER, 1, 1, "Pipe point index", NULL),
    BU_CMD_OPERAND("point", BU_CMD_VALUE_VECTOR, 1, 1, "Packed XYZ point", NULL),
    BU_CMD_OPERAND_NULL
};

#define PIPE_SCHEMA(_id, _name, _help, _opts, _operands, _policy, _validate) \
    static const struct bu_cmd_schema _id##_cmd_schema = { \
	_name, _help, _opts, _operands, _policy, \
	BU_CMD_SCHEMA_CONSTRAINTS(_validate, NULL) \
    }
PIPE_SCHEMA(find_pipe_pnt, "find_pipe_pnt", "Find the pipe point nearest a model point", NULL,
	pipe_find_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, pipe_find_schema_validate);
PIPE_SCHEMA(pipe_move_pnt, "pipe_move_pnt", "Move a pipe point", pipe_move_schema_options,
	pipe_move_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST, pipe_packed_point_schema_validate);
PIPE_SCHEMA(pipe_append_pnt, "pipe_append_pnt", "Append a point to a pipe", NULL,
	pipe_insert_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, pipe_packed_point_schema_validate);
PIPE_SCHEMA(pipe_prepend_pnt, "pipe_prepend_pnt", "Prepend a point to a pipe", NULL,
	pipe_insert_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, pipe_packed_point_schema_validate);
PIPE_SCHEMA(pipe_delete_pnt, "pipe_delete_pnt", "Delete a pipe point", NULL,
	pipe_index_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, NULL);
PIPE_SCHEMA(mouse_move_pipe_pnt, "mouse_move_pipe_pnt", "Move a pipe point from mouse input", pipe_move_schema_options,
	pipe_move_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST, pipe_packed_point_schema_validate);
PIPE_SCHEMA(mouse_append_pipe_pnt, "mouse_append_pipe_pnt", "Append a pipe point from mouse input", NULL,
	pipe_insert_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, pipe_packed_point_schema_validate);
PIPE_SCHEMA(mouse_prepend_pipe_pnt, "mouse_prepend_pipe_pnt", "Prepend a pipe point from mouse input", NULL,
	pipe_insert_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, pipe_packed_point_schema_validate);
#undef PIPE_SCHEMA


static const struct bu_cmd_schema *
pipe_schema_for_command(const char *name)
{
    if (BU_STR_EQUAL(name, "find_pipe_pnt"))
	return &find_pipe_pnt_cmd_schema;
    if (BU_STR_EQUAL(name, "pipe_move_pnt"))
	return &pipe_move_pnt_cmd_schema;
    if (BU_STR_EQUAL(name, "mouse_move_pipe_pnt"))
	return &mouse_move_pipe_pnt_cmd_schema;
    if (BU_STR_EQUAL(name, "pipe_append_pnt"))
	return &pipe_append_pnt_cmd_schema;
    if (BU_STR_EQUAL(name, "mouse_append_pipe_pnt"))
	return &mouse_append_pipe_pnt_cmd_schema;
    if (BU_STR_EQUAL(name, "pipe_prepend_pnt"))
	return &pipe_prepend_pnt_cmd_schema;
    if (BU_STR_EQUAL(name, "mouse_prepend_pipe_pnt"))
	return &mouse_prepend_pipe_pnt_cmd_schema;
    if (BU_STR_EQUAL(name, "pipe_delete_pnt"))
	return &pipe_delete_pnt_cmd_schema;
    return NULL;
}


static void
pipe_usage(struct ged *gedp, const struct bu_cmd_schema *schema)
{
    char *help = bu_cmd_schema_describe(schema);

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "pipe native schema help");
    }
}


static int
pipe_schema_preflight(struct ged *gedp, const struct bu_cmd_schema *schema,
	int argc, const char *argv[])
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret = BRLCAD_ERROR;

    if (schema && bu_cmd_schema_parse_complete(schema, NULL, &msg,
	argc - 1, argv + 1) >= 0) {
	ret = BRLCAD_OK;
	} else if (bu_vls_strlen(&msg)) {
	bu_vls_vlscat(gedp->ged_result_str, &msg);
    }
    bu_vls_free(&msg);
    return ret;
}


static void
pipe_vector_validation_result(struct bu_cmd_validate_result *result,
	const struct bu_cmd_validate_result *vector_result, size_t offset)
{
    bu_cmd_validate_result_clear(result);
    *result = *vector_result;
    result->token_start += offset;
    result->token_end += offset;
    result->completion_count = 0;
    result->completion_candidates = NULL;
}


static int
pipe_packed_point_schema_validate(const struct bu_cmd_schema *cmd, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *cmd;
    fastf_t point[3] = {0.0, 0.0, 0.0};
    size_t point_index = BU_STR_EQUAL(cmd->name, "pipe_move_pnt") ||
	BU_STR_EQUAL(cmd->name, "mouse_move_pipe_pnt") ? 2 : 1;
    int ret = 0;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (point_index == 2 && bu_cmd_schema_option_present(cmd, argc, argv, "r"))
	point_index++;
    if (ret || result->state == BU_CMD_VALIDATE_INVALID || argc <= point_index)
	return ret;
    if (bu_cmd_vector3_from_argv(point, 1, (const char * const *)argv + point_index) != 1) {
	bu_cmd_validate_result_clear(result);
	result->state = BU_CMD_VALIDATE_INVALID;
	result->token_start = point_index;
	result->token_end = point_index;
	result->expected = BU_CMD_EXPECT_OPERAND;
	result->hint = "packed finite XYZ point required";
	result->completion_type = BU_CMD_VALUE_VECTOR;
	result->semantic_provider = "ged.vector_group";
    } else {
	/* The command validator owns the whole packed point.  Suppress the
	 * one-token vector semantic check from reinterpreting a quoted argument. */
	result->semantic_provider = "ged.vector_group";
    }
    return 0;
}


static int
pipe_find_schema_validate(const struct bu_cmd_schema *cmd, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *cmd;
    struct bu_cmd_validate_result vector_result = BU_CMD_VALIDATE_RESULT_NULL;
    int ret = 0;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID || argc <= 1)
	return ret;
    if (bu_cmd_vector3_optional_validate(argc - 1, argv + 1,
	cursor_arg > 1 ? cursor_arg - 1 : 0, &vector_result) == 0) {
	vector_result.semantic_provider = "ged.vector_group";
	pipe_vector_validation_result(result, &vector_result, 1);
    }
    bu_cmd_validate_result_clear(&vector_result);
    return 0;
}

int
_ged_pipe_append_pnt_common(struct ged *gedp, int argc, const char *argv[], struct wdb_pipe_pnt *(*func)(struct rt_pipe_internal *, struct wdb_pipe_pnt *, const point_t))
{
    struct directory *dp;
    const struct bu_cmd_schema *schema = pipe_schema_for_command(argv[0]);
    struct rt_db_internal intern;
    struct rt_pipe_internal *pipeip;
    mat_t mat;
    point_t view_ps_pt;
    point_t view_pp_coord;
    point_t ps_pt;
    struct wdb_pipe_pnt *prevpp;
    char *last;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	pipe_usage(gedp, schema);
	return GED_HELP;
    }

    if (pipe_schema_preflight(gedp, schema, argc, argv) != BRLCAD_OK) {
	pipe_usage(gedp, schema);
	return BRLCAD_ERROR;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    dp = db_lookup(gedp->dbip, last, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_cmd_vector3_from_argv(view_ps_pt, 1, argv + 2) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad point - %s", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], wdbp, mat) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE) {
	bu_vls_printf(gedp->ged_result_str, "Object not a PIPE");
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    pipeip = (struct rt_pipe_internal *)intern.idb_ptr;

    /* use the view z from the first or last pipe point, depending on whether we're appending or prepending */
    if (func == rt_pipe_add_pnt)
	prevpp = BU_LIST_LAST(wdb_pipe_pnt, &pipeip->pipe_segs_head);
    else
	prevpp = BU_LIST_FIRST(wdb_pipe_pnt, &pipeip->pipe_segs_head);

    MAT4X3PNT(view_pp_coord, gedp->ged_gvp->gv_model2view, prevpp->pp_coord);
    view_ps_pt[Z] = view_pp_coord[Z];
    MAT4X3PNT(ps_pt, gedp->ged_gvp->gv_view2model, view_ps_pt);

    if ((*func)(pipeip, (struct wdb_pipe_pnt *)NULL, ps_pt) == (struct wdb_pipe_pnt *)NULL) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: cannot move point there", argv[0]);
	return BRLCAD_ERROR;
    }

    {
	mat_t invmat;
	struct wdb_pipe_pnt *curr_ps;
	point_t curr_pt;

	bn_mat_inv(invmat, mat);
	for (BU_LIST_FOR(curr_ps, wdb_pipe_pnt, &pipeip->pipe_segs_head)) {
	    MAT4X3PNT(curr_pt, invmat, curr_ps->pp_coord);
	    VMOVE(curr_ps->pp_coord, curr_pt);
	}

	GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);
    }

    rt_db_free_internal(&intern);
    return BRLCAD_OK;
}

int
ged_pipe_append_pnt_core(struct ged *gedp, int argc, const char *argv[])
{
    return _ged_pipe_append_pnt_common(gedp, argc, argv, rt_pipe_add_pnt);
}


int
ged_pipe_delete_pnt_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    const struct bu_cmd_schema *schema = pipe_schema_for_command(argv[0]);
    struct rt_db_internal intern;
    struct wdb_pipe_pnt *ps;
    struct rt_pipe_internal *pipeip;
    int seg_i;
    char *last;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	pipe_usage(gedp, schema);
	return GED_HELP;
    }

    if (pipe_schema_preflight(gedp, schema, argc, argv) != BRLCAD_OK) {
	pipe_usage(gedp, schema);
	return BRLCAD_ERROR;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    dp = db_lookup(gedp->dbip, last, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if (!bu_cmd_integer_from_str(&seg_i, argv[2])) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad pipe segment index - %s", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to get internal for %s", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a PIPE", argv[1]);
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    pipeip = (struct rt_pipe_internal *)intern.idb_ptr;
    if ((ps = rt_pipe_get_seg_i(pipeip, seg_i)) == (struct wdb_pipe_pnt *)NULL) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: bad pipe segment index - %s", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }

    if (rt_pipe_delete_pnt(ps) == ps) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: cannot delete pipe segment %d", argv[0], seg_i);
	return BRLCAD_ERROR;
    }

    GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);

    rt_db_free_internal(&intern);
    return BRLCAD_OK;
}


int
ged_find_pipe_pnt_nearest_pnt_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    const struct bu_cmd_schema *schema = pipe_schema_for_command(argv[0]);
    struct rt_db_internal intern;
    struct wdb_pipe_pnt *nearest;
    point_t model_pt;
    mat_t mat;
    int seg_i;
    char *last;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	pipe_usage(gedp, schema);
	return GED_HELP;
    }

    if (pipe_schema_preflight(gedp, schema, argc, argv) != BRLCAD_OK) {
	pipe_usage(gedp, schema);
	return BRLCAD_ERROR;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    dp = db_lookup(gedp->dbip, last, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if (bu_cmd_vector3_from_argv(model_pt, argc - 2, argv + 2) != argc - 2) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad X, Y or Z", argv[0]);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], wdbp, mat) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    nearest = rt_pipe_find_pnt_nearest_pnt(&((struct rt_pipe_internal *)intern.idb_ptr)->pipe_segs_head,
				     model_pt, gedp->ged_gvp->gv_view2model);
    seg_i = rt_pipe_get_i_seg((struct rt_pipe_internal *)intern.idb_ptr, nearest);
    rt_db_free_internal(&intern);

    if (seg_i < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find segment for %s", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "%d", seg_i);
    return BRLCAD_OK;
}


int
ged_pipe_move_pnt_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    const struct bu_cmd_schema *schema = pipe_schema_for_command(argv[0]);
    struct rt_db_internal intern;
    struct wdb_pipe_pnt *ps;
    struct rt_pipe_internal *pipeip;
    mat_t mat;
    point_t ps_pt;
    int seg_i;
    int rflag = 0;
    char *last;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	pipe_usage(gedp, schema);
	return GED_HELP;
    }

    if (pipe_schema_preflight(gedp, schema, argc, argv) != BRLCAD_OK) {
	pipe_usage(gedp, schema);
	return BRLCAD_ERROR;
    }

    if (argc == 5) {
	rflag = 1;
	--argc;
	++argv;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    dp = db_lookup(gedp->dbip, last, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if (!bu_cmd_integer_from_str(&seg_i, argv[2])) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad pipe segment index - %s", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }

    if (bu_cmd_vector3_from_argv(ps_pt, 1, argv + 3) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad point - %s", argv[0], argv[3]);
	return BRLCAD_ERROR;
    }
    VSCALE(ps_pt, ps_pt, gedp->dbip->dbi_local2base);

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], wdbp, mat) == BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE) {
	bu_vls_printf(gedp->ged_result_str, "Object not a PIPE");
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    pipeip = (struct rt_pipe_internal *)intern.idb_ptr;
    if ((ps = rt_pipe_get_seg_i(pipeip, seg_i)) == (struct wdb_pipe_pnt *)NULL) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: bad pipe segment index - %s", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }

    if (rflag) {
	VADD2(ps_pt, ps_pt, ps->pp_coord);
    }

    if (rt_pipe_move_pnt(pipeip, ps, ps_pt)) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: cannot move point there", argv[0]);
	return BRLCAD_ERROR;
    }

    {
	mat_t invmat;
	struct wdb_pipe_pnt *curr_ps;
	point_t curr_pt;

	bn_mat_inv(invmat, mat);
	for (BU_LIST_FOR(curr_ps, wdb_pipe_pnt, &pipeip->pipe_segs_head)) {
	    MAT4X3PNT(curr_pt, invmat, curr_ps->pp_coord);
	    VMOVE(curr_ps->pp_coord, curr_pt);
	}

	GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);
    }

    rt_db_free_internal(&intern);
    return BRLCAD_OK;
}


int
ged_pipe_prepend_pnt_core(struct ged *gedp, int argc, const char *argv[])
{
    return _ged_pipe_append_pnt_common(gedp, argc, argv, rt_pipe_ins_pnt);
}


#include "../include/plugin.h"

#define GED_PIPE_COMMANDS(X, XID) \
    X(find_pipe_pnt, ged_find_pipe_pnt_nearest_pnt_core, GED_CMD_DEFAULT, &find_pipe_pnt_cmd_schema) \
    X(pipe_move_pnt, ged_pipe_move_pnt_core, GED_CMD_DEFAULT, &pipe_move_pnt_cmd_schema) \
    X(pipe_append_pnt, ged_pipe_append_pnt_core, GED_CMD_DEFAULT, &pipe_append_pnt_cmd_schema) \
    X(pipe_prepend_pnt, ged_pipe_prepend_pnt_core, GED_CMD_DEFAULT, &pipe_prepend_pnt_cmd_schema) \
    X(pipe_delete_pnt, ged_pipe_delete_pnt_core, GED_CMD_DEFAULT, &pipe_delete_pnt_cmd_schema) \
    X(mouse_move_pipe_pnt, ged_pipe_move_pnt_core, GED_CMD_DEFAULT, &mouse_move_pipe_pnt_cmd_schema) \
    X(mouse_append_pipe_pnt, ged_pipe_append_pnt_core, GED_CMD_DEFAULT, &mouse_append_pipe_pnt_cmd_schema) \
    X(mouse_prepend_pipe_pnt, ged_pipe_prepend_pnt_core, GED_CMD_DEFAULT, &mouse_prepend_pipe_pnt_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_PIPE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_pipe", 1, GED_PIPE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
