/*                      G O B J S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/view/gobjs.c
 *
 * Create and manage transient view features that are used to
 * interactively edit geometry parameters for database solids
 * and combs.  These edits change only the locally stored info,
 * without out disturbing the original object in the .g, until
 * the changes are specifically written back to that object
 * with an explicit command.
 *
 */

#include "common.h"

#include <ctype.h>
#include <cstdlib>
#include <cstring>
#include <string>

#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "bu/vls.h"
#include "bsg.h"
#include "bsg/defines.h"
#include "bsg/draw_source.h"
#include "bsg/feature.h"
#include "bsg/scene_object.h"

#include "ged/view.h"
#include "../ged_private.h"
#include "./ged_view.h"

int
_gobjs_cmd_create(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    struct db_i *dbip = gedp->dbip;
    struct bsg_view *v = gd->cv;
    const char *usage_string = "view gobjs name create";
    const char *purpose_string = "create an editing view obj from a database solid/comb";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "view gobjs create g_obj_name view_obj_name\n");
	return BRLCAD_ERROR;
    }
    gd->vobj = argv[0];

    bsg_feature_ref ref = bsg_feature_find(gedp->ged_gvp, argv[1]);
    if (!bsg_feature_ref_is_null(ref)) {
	bu_vls_printf(gedp->ged_result_str, "View feature %s already exists\n", argv[1]);
	return BRLCAD_ERROR;
    }

    /* Make sure we have a valid db object as an argument */
    struct db_full_path *fp;
    BU_GET(fp, struct db_full_path);
    db_full_path_init(fp);
    int ret = db_string_to_path(fp, dbip, gd->vobj);
    if (ret < 0) {
	// Invalid path
	db_free_full_path(fp);
	BU_PUT(fp, struct db_full_path);
	bu_vls_printf(gedp->ged_result_str, "Invalid path: %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    mat_t mat;
    MAT_IDN(mat);
    if (!db_path_to_mat(dbip, fp, mat, fp->fp_len-1)) {
	db_free_full_path(fp);
	BU_PUT(fp, struct db_full_path);
	bu_vls_printf(gedp->ged_result_str, "Invalid path matrix: %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }

    // We will need a local copy of the internal structure to support manipulating
    // the scene object (and generating new wireframes) without altering the on-disk
    // .g object.
    struct rt_db_internal *ip;
    BU_GET(ip, struct rt_db_internal);
    RT_DB_INTERNAL_INIT(ip);
    ret = rt_db_get_internal(ip, DB_FULL_PATH_CUR_DIR(fp), dbip, mat);
    if (ret < 0) {
	db_free_full_path(fp);
	BU_PUT(fp, struct db_full_path);
	return BRLCAD_ERROR;
    }

    /* Set up the toplevel object */
    ref = bsg_feature_create_overlay(v, argv[1], 0);
    if (bsg_feature_ref_is_null(ref))
	return BRLCAD_ERROR;
    bsg_scene_ref g_ref = bsg_feature_ref_as_scene(ref);
    if (bsg_scene_ref_is_null(g_ref))
	return BRLCAD_ERROR;
    ged_draw_shape_state *state = ged_draw_shape_ref_set_fullpath(g_ref, gedp, fp);
    if (state) {
	state->u_data = (void *)ip;
	state->u_data_kind = GED_DRAW_SHAPE_USER_DATA_RT_DB_INTERNAL;
    }

    // Set up drawing settings
    unsigned char wcolor[3] = {255,255,255};
    struct bsg_appearance_settings vs = BSG_APPEARANCE_SETTINGS_INIT;

    // We have a tree walk ahead to populate the wireframe - set up the client
    // data structure.
    std::map<struct directory *, fastf_t> s_size;
    struct draw_data_t dd;
    dd.gedp = gedp;
    dd.dbip = gedp->dbip;
    dd.v = v;
    dd.tol = &wdbp->wdb_tol;
    dd.ttol = &wdbp->wdb_ttol;
    dd.mesh_c = gedp->ged_lod;
    dd.color_inherit = 0;
    dd.bound_only = 0;
    dd.s_size = &s_size;
    bu_color_from_rgb_chars(&dd.c, wcolor);
    dd.vs = &vs;
    dd.g_ref = g_ref;

    // Create a wireframe from the current state of the specified object
    draw_gather_paths(fp, &mat, (void *)&dd);

    // TODO - set the object callbacks

    // TODO - in principle, we should be sharing a lot of logic with the edit command -
    // the only difference is we won't be unpacking and writing the rt_db_internal.

    return BRLCAD_OK;
}

int
_gobjs_cmd_delete(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view gobjs name delete";
    const char *purpose_string = "delete view feature";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!gd->vobj || bsg_feature_ref_is_null(bsg_feature_find(gd->cv, gd->vobj)) ||
	    !bsg_feature_remove(gd->cv, gd->vobj)) {
	bu_vls_printf(gedp->ged_result_str, "No view feature named %s\n", gd->vobj ? gd->vobj : "");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

extern "C" int
_view_cmd_gobjs(void *bs, int argc, const char **argv)
{
    const char *usage_string = "view [options] gobjs [create|del] ...";
    const char *purpose_string = "deprecated alias for 'view obj -g <dbpath> ...'";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    if (!gd || !gd->gedp)
	return BRLCAD_ERROR;
    struct ged *gedp = gd->gedp;

    argc--; argv++; /* skip gobjs */
    if (argc <= 0) {
	const char *nargv[3] = {"view", "obj", "list"};
	return _view_cmd_objs(bs, 3, nargv);
    }

    if (BU_STR_EQUAL(argv[0], "create")) {
	if (argc != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: view gobjs create <dbpath> <name>\n");
	    return BRLCAD_ERROR;
	}
	const char *nargv[7] = {"view", "obj", "-g", argv[1], "create", argv[2], NULL};
	return _view_cmd_objs(bs, 6, nargv);
    }

    if (BU_STR_EQUAL(argv[0], "del") || BU_STR_EQUAL(argv[0], "delete") || BU_STR_EQUAL(argv[0], "remove")) {
	if (argc != 2) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: view gobjs del <name>\n");
	    return BRLCAD_ERROR;
	}
	const char *nargv[5] = {"view", "obj", "remove", argv[1], NULL};
	return _view_cmd_objs(bs, 4, nargv);
    }

    bu_vls_printf(gedp->ged_result_str, "view gobjs is deprecated - use 'view obj -g <dbpath> create <name>' or 'view obj remove <name>'\n");
    return BRLCAD_ERROR;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
