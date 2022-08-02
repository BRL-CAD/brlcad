/*                      G O B J S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
 * Create and manage transient view objects that are used to
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
#include <map>

#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "bu/vls.h"
#include "bv.h"

#include "../ged_private.h"
#include "./ged_view.h"

int
_gobjs_cmd_create(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct db_i *dbip = gedp->dbip;
    struct bview *v = gd->cv;
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

    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_objs.view_objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_objs.view_objs, i);
	if (BU_STR_EQUAL(argv[1], bu_vls_cstr(&s->s_uuid))) {
	    gd->s = s;
	    break;
	}
    }

    struct bv_scene_obj *s = gd->s;
    if (s) {
	bu_vls_printf(gedp->ged_result_str, "View object %s already exists\n", argv[1]);
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
    if (!db_path_to_mat(dbip, fp, mat, fp->fp_len-1, &rt_uniresource)) {
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
    ret = rt_db_get_internal(ip, DB_FULL_PATH_CUR_DIR(fp), dbip, mat, &rt_uniresource);
    if (ret < 0) {
	db_free_full_path(fp);
	BU_PUT(fp, struct db_full_path);
	return BRLCAD_ERROR;
    }

    /* Set up the toplevel object */
    struct bv_scene_group *g = bv_obj_get(v, BV_DB_OBJS);
    if (!g)
	return BRLCAD_ERROR;
    db_path_to_vls(&g->s_name, fp);
    bu_vls_sprintf(&g->s_uuid, "%s", argv[1]);
    g->s_i_data = (void *)ip;

    // Set up drawing settings
    unsigned char wcolor[3] = {255,255,255};
    struct bv_obj_settings vs = BV_OBJ_SETTINGS_INIT;
    bv_obj_settings_sync(&vs, &v->gv_s->obj_s);
    bv_obj_settings_sync(&g->s_os, &vs);

    // We have a tree walk ahead to populate the wireframe - set up the client
    // data structure.
    std::map<struct directory *, fastf_t> s_size;
    struct draw_data_t dd;
    dd.dbip = gedp->dbip;
    dd.v = v;
    dd.tol = &gedp->ged_wdbp->wdb_tol;
    dd.ttol = &gedp->ged_wdbp->wdb_ttol;
    dd.mesh_c = gedp->ged_lod;
    dd.color_inherit = 0;
    dd.bound_only = 0;
    dd.s_size = &s_size;
    dd.res = &rt_uniresource;
    bu_color_from_rgb_chars(&dd.c, wcolor);
    dd.vs = &vs;
    dd.g = g;

    // Create a wireframe from the current state of the specified object
    draw_gather_paths(fp, &mat, (void *)&dd);

    // Add to the scene
    bu_ptbl_ins(v->gv_objs.view_objs, (long *)g);

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
    const char *purpose_string = "delete view object";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return BRLCAD_ERROR;
    }
    bv_obj_put(s);

    return BRLCAD_OK;
}

const struct bu_cmdtab _gobjs_cmds[] = {
    { "create",     _gobjs_cmd_create},
    { "del",        _gobjs_cmd_delete},
    { (char *)NULL,      NULL}
};

extern "C" int
_view_cmd_gobjs(void *bs, int argc, const char **argv)
{
    int help = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;

    const char *usage_string = "view [options] gobjs [options] [args]";
    const char *purpose_string = "view-only scene objects based on geometry solids/combs";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (!gd->cv) {
	bu_vls_printf(gedp->ged_result_str, ": no view specified or current in GED");
	return BRLCAD_ERROR;
    }

    // See if we have any high level options set
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",        "",  NULL,  &help,      "Print help");
    BU_OPT_NULL(d[1]);

    gd->gopts = d;

    // We know we're the gobjs command - start processing args
    argc--; argv++;

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_gobjs_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    int ac = bu_opt_parse(NULL, acnt, argv, d);

    // If we're not wanting help and we have no subcommand, list current gobjs objects
    struct bview *v = gd->cv;
    if (!ac && cmd_pos < 0 && !help) {
	struct bu_ptbl *view_objs = bv_view_objs(v, BV_VIEW_OBJS);
	for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
	    // TODO - strip gobjs:: prefix
	    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_uuid));
	}

	if (view_objs != v->gv_objs.view_objs) {
	    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_objs.view_objs); i++) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_objs.view_objs, i);
		// TODO - strip gobjs:: prefix
		bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_uuid));
	    }
	}
	return BRLCAD_OK;
    }

    gd->s = NULL;
    // TODO - if independent use gv_objs.view_objs, else use gv_objs.view_shared_objs
    // TODO - append gobjs:: prefix
 
    if (!gd->s) {
	// View object doesn't already exist.  subcommands will either need to create it
	// or handle the error case
    }

    return _ged_subcmd_exec(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_gobjs_cmds,
			    "view gobjs", "[options] subcommand [args]", gd, argc, argv, help, cmd_pos);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

