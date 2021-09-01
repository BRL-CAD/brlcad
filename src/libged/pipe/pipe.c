/*                        E D P I P E . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2021 United States Government as represented by
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

#include "../ged_private.h"

int
ged_pipe_append_pnt_core(struct ged *gedp, int argc, const char *argv[])
{
    return _ged_pipe_append_pnt_common(gedp, argc, argv, _ged_pipe_add_pnt);
}


int
ged_pipe_delete_pnt_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "pipe seg_i";
    struct rt_db_internal intern;
    struct wdb_pipe_pnt *ps;
    struct rt_pipe_internal *pipeip;
    int seg_i;
    char *last;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    dp = db_lookup(gedp->dbip, last, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (sscanf(argv[2], "%d", &seg_i) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad pipe segment index - %s", argv[0], argv[3]);
	return GED_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to get internal for %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a PIPE", argv[1]);
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    pipeip = (struct rt_pipe_internal *)intern.idb_ptr;
    if ((ps = _ged_get_pipe_seg_i(pipeip, seg_i)) == (struct wdb_pipe_pnt *)NULL) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: bad pipe segment index - %s", argv[0], argv[2]);
	return GED_ERROR;
    }

    if (_ged_pipe_delete_pnt(ps) == ps) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: cannot delete pipe segment %d", argv[0], seg_i);
	return GED_ERROR;
    }

    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);

    rt_db_free_internal(&intern);
    return GED_OK;
}


int
ged_find_pipe_pnt_nearest_pnt_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "pipe x y z";
    struct rt_db_internal intern;
    struct wdb_pipe_pnt *nearest;
    point_t model_pt;
    double scan[3];
    mat_t mat;
    int seg_i;
    char *last;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3 && argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "%s: illegal input - %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    dp = db_lookup(gedp->dbip, last, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (argc == 3) {
	if (sscanf(argv[2], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad point - %s", argv[0], argv[2]);
	    return GED_ERROR;
	}
    } else if (sscanf(argv[2], "%lf", &scan[X]) != 1 ||
	       sscanf(argv[3], "%lf", &scan[Y]) != 1 ||
	       sscanf(argv[4], "%lf", &scan[Z]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad X, Y or Z", argv[0]);
	return GED_ERROR;
    }
    /* convert from double to fastf_t */
    VMOVE(model_pt, scan);

    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], gedp->ged_wdbp, mat) & GED_ERROR)
	return GED_ERROR;

    nearest = find_pipe_pnt_nearest_pnt(&((struct rt_pipe_internal *)intern.idb_ptr)->pipe_segs_head,
				     model_pt, gedp->ged_gvp->gv_view2model);
    seg_i = _ged_get_pipe_i_seg((struct rt_pipe_internal *)intern.idb_ptr, nearest);
    rt_db_free_internal(&intern);

    if (seg_i < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find segment for %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "%d", seg_i);
    return GED_OK;
}


int
ged_pipe_move_pnt_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    static const char *usage = "[-r] pipe seg_i pt";
    struct rt_db_internal intern;
    struct wdb_pipe_pnt *ps;
    struct rt_pipe_internal *pipeip;
    mat_t mat;
    point_t ps_pt;
    double scan[3];
    int seg_i;
    int rflag = 0;
    char *last;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 4 || 5 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (argc == 5) {
	if (argv[1][0] != '-' || argv[1][1] != 'r' || argv[1][2] != '\0') {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}

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
	return GED_ERROR;
    }

    dp = db_lookup(gedp->dbip, last, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (sscanf(argv[2], "%d", &seg_i) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad pipe segment index - %s", argv[0], argv[2]);
	return GED_ERROR;
    }

    if (sscanf(argv[3], "%lf %lf %lf", &scan[X], &scan[Y], &scan[Z]) != 3) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad point - %s", argv[0], argv[3]);
	return GED_ERROR;
    }
    VSCALE(ps_pt, scan, gedp->dbip->dbi_local2base);

    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], gedp->ged_wdbp, mat) == GED_ERROR)
	return GED_ERROR;

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_PIPE) {
	bu_vls_printf(gedp->ged_result_str, "Object not a PIPE");
	rt_db_free_internal(&intern);

	return GED_ERROR;
    }

    pipeip = (struct rt_pipe_internal *)intern.idb_ptr;
    if ((ps = _ged_get_pipe_seg_i(pipeip, seg_i)) == (struct wdb_pipe_pnt *)NULL) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: bad pipe segment index - %s", argv[0], argv[2]);
	return GED_ERROR;
    }

    if (rflag) {
	VADD2(ps_pt, ps_pt, ps->pp_coord);
    }

    if (_ged_pipe_move_pnt(pipeip, ps, ps_pt)) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "%s: cannot move point there", argv[0]);
	return GED_ERROR;
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

	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    }

    rt_db_free_internal(&intern);
    return GED_OK;
}


int
ged_pipe_prepend_pnt_core(struct ged *gedp, int argc, const char *argv[])
{
    return _ged_pipe_append_pnt_common(gedp, argc, argv, _ged_pipe_ins_pnt);
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl find_pipe_pnt_cmd_impl = {"find_pipe_pnt", ged_find_pipe_pnt_nearest_pnt_core, GED_CMD_DEFAULT};
const struct ged_cmd find_pipe_pnt_cmd = { &find_pipe_pnt_cmd_impl };

struct ged_cmd_impl pipe_move_pnt_cmd_impl = {"pipe_move_pnt", ged_pipe_move_pnt_core, GED_CMD_DEFAULT};
const struct ged_cmd pipe_move_pnt_cmd = { &pipe_move_pnt_cmd_impl };

struct ged_cmd_impl pipe_append_pnt_cmd_impl = {"pipe_append_pnt", ged_pipe_append_pnt_core, GED_CMD_DEFAULT};
const struct ged_cmd pipe_append_pnt_cmd = { &pipe_append_pnt_cmd_impl };

struct ged_cmd_impl pipe_prepend_pnt_cmd_impl = {"pipe_prepend_pnt", ged_pipe_prepend_pnt_core, GED_CMD_DEFAULT};
const struct ged_cmd pipe_prepend_pnt_cmd = { &pipe_prepend_pnt_cmd_impl };

struct ged_cmd_impl pipe_delete_pnt_cmd_impl = {"pipe_delete_pnt", ged_pipe_delete_pnt_core, GED_CMD_DEFAULT};
const struct ged_cmd pipe_delete_pnt_cmd = { &pipe_delete_pnt_cmd_impl };

struct ged_cmd_impl mouse_pipe_move_pnt_cmd_impl = {"mouse_move_pipe_pnt", ged_pipe_move_pnt_core, GED_CMD_DEFAULT};
const struct ged_cmd mouse_pipe_move_pnt_cmd = { &mouse_pipe_move_pnt_cmd_impl };

struct ged_cmd_impl mouse_pipe_append_pnt_cmd_impl = {"mouse_append_pipe_pnt", ged_pipe_append_pnt_core, GED_CMD_DEFAULT};
const struct ged_cmd mouse_pipe_append_pnt_cmd = { &mouse_pipe_append_pnt_cmd_impl };

struct ged_cmd_impl mouse_pipe_prepend_pnt_cmd_impl = {"mouse_prepend_pipe_pnt", ged_pipe_prepend_pnt_core, GED_CMD_DEFAULT};
const struct ged_cmd mouse_pipe_prepend_pnt_cmd = { &mouse_pipe_prepend_pnt_cmd_impl };


const struct ged_cmd *pipe_cmds[] = {
    &find_pipe_pnt_cmd,
    &pipe_move_pnt_cmd,
    &pipe_append_pnt_cmd,
    &pipe_prepend_pnt_cmd,
    &pipe_delete_pnt_cmd,
    &mouse_pipe_move_pnt_cmd,
    &mouse_pipe_append_pnt_cmd,
    &mouse_pipe_prepend_pnt_cmd,
    NULL };

static const struct ged_plugin pinfo = { GED_API,  pipe_cmds, 8 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
