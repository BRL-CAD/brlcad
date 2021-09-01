/*                           G E D I T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/view/gedit.c
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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "bu/vls.h"
#include "bv.h"

#include "../ged_private.h"
#include "./ged_view.h"

#define GET_BV_SCENE_OBJ(p, fp) { \
    if (BU_LIST_IS_EMPTY(fp)) { \
	BU_ALLOC((p), struct bv_scene_obj); \
    } else { \
	p = BU_LIST_NEXT(bv_scene_obj, fp); \
	BU_LIST_DEQUEUE(&((p)->l)); \
    } \
    BU_LIST_INIT( &((p)->s_vlist) ); }

int
_gedit_cmd_create(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct db_i *dbip = gedp->dbip;
    const char *usage_string = "view gedit name create";
    const char *purpose_string = "create an editing view obj from a database solid/comb";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (s) {
	bu_vls_printf(gedp->ged_result_str, "View object for %s already exists\n", gd->vobj);
	return GED_ERROR;
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
	return GED_ERROR;
    }
    mat_t mat;
    MAT_IDN(mat);
    if (!db_path_to_mat(dbip, fp, mat, fp->fp_len-1, &rt_uniresource)) {
	db_free_full_path(fp);
	BU_PUT(fp, struct db_full_path);
	bu_vls_printf(gedp->ged_result_str, "Invalid path matrix: %s\n", gd->vobj);
	return GED_ERROR;
    }

#if 0
    struct bview *v = gedp->ged_gvp;

    /* Set up the toplevel object */
    struct bv_scene_group *g;
    BU_GET(g, struct bv_scene_group);
    GET_BV_SCENE_OBJ(g, &gedp->free_scene_obj->l);
    bv_scene_obj_init(g, gedp->free_scene_obj);
    db_path_to_vls(&g->s_name, fp);
    db_path_to_vls(&g->s_uuid, fp);


    // Set up drawing settings
    unsigned char wcolor[3] = {255,255,255};
    struct bv_obj_settings vs = BV_OBJ_SETTINGS_INIT;
    if (v)
	bv_obj_settings_sync(&vs, &v->gv_s->obj_s);
    bv_obj_settings_sync(&g->s_os, &vs);

    // We have a tree walk ahead to populate the wireframe - set up the client
    // data structure.
    struct draw_data_t dd;
    dd.dbip = gedp->dbip;
    dd.v = gedp->ged_gvp;
    dd.tol = &gedp->ged_wdbp->wdb_tol;
    dd.ttol = &gedp->ged_wdbp->wdb_ttol;
    dd.free_scene_obj = gedp->free_scene_obj;
    dd.color_inherit = 0;
    dd.bound_only = 0;
    dd.res = &rt_uniresource;
    bu_color_from_rgb_chars(&dd.c, wcolor);
    dd.vs = &vs;
    dd.g = g;

    // Create a wireframe from the current state of the specified object
    db_fullpath_draw(fp, &mat, (void *)&dd);

    // We also need a local copy of the internal structure to edit
    // associated with the scene object
    struct rt_db_internal *ip;
    BU_GET(ip, struct rt_db_internal);
    RT_DB_INTERNAL_INIT(ip);
    ret = rt_db_get_internal(ip, DB_FULL_PATH_CUR_DIR(fp), dbip, mat, dd.res);
    if (ret < 0) {
	db_free_full_path(fp);
	BU_PUT(fp, struct db_full_path);
	bv_scene_obj_free(g, gedp->free_scene_obj);
	return GED_ERROR;
    }
    g->s_i_data = (void *)ip;

    // TODO - set the object callbacks

    // TODO - in principle, we should be sharing a lot of logic with the edit command -
    // the only difference is we won't be unpacking and writing the rt_db_internal.
#endif

    return GED_OK;
}

int
_gedit_cmd_delete(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view gedit name delete";
    const char *purpose_string = "delete view object";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_scene_obj *s = gd->s;
    if (!s) {
	bu_vls_printf(gedp->ged_result_str, "No view object named %s\n", gd->vobj);
	return GED_ERROR;
    }
    // TODO may need gv_view_shared_objs depending on view type
    bu_ptbl_rm(gedp->ged_gvp->gv_view_objs, (long *)s);
    bv_scene_obj_free(s, gedp->free_scene_obj);

    return GED_OK;
}

const struct bu_cmdtab _gedit_cmds[] = {
    { "create",     _gedit_cmd_create},
    { "del",        _gedit_cmd_delete},
    { (char *)NULL,      NULL}
};

int
_view_cmd_gedit(void *bs, int argc, const char **argv)
{
    int help = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;

    const char *usage_string = "view [options] gedit [options] [args]";
    const char *purpose_string = "interactively edit geometry solids/combs";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return GED_ERROR;
    }

    // See if we have any high level options set
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",        "",  NULL,  &help,      "Print help");
    BU_OPT_NULL(d[1]);

    gd->gopts = d;

    // We know we're the gedit command - start processing args
    argc--; argv++;

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_gedit_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    int ac = bu_opt_parse(NULL, acnt, argv, d);

    // If we're not wanting help and we have no subcommand, list current gedit objects
    struct bview *v = gedp->ged_gvp;
    if (!ac && cmd_pos < 0 && !help) {
	for (size_t i = 0; i < BU_PTBL_LEN(v->gv_view_shared_objs); i++) {
	    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_view_shared_objs, i);
	    // TODO - strip gedit:: prefix
	    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_uuid));
	}

	if (v->gv_view_shared_objs != v->gv_view_objs) {
	    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_view_objs); i++) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_view_objs, i);
		// TODO - strip gedit:: prefix
		bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_uuid));
	    }
	}
	return GED_OK;
    }

    // We need a name, even if it doesn't exist yet.  Check if it does, since subcommands
    // will react differently based on that status.
    if (ac != 1) {
	bu_vls_printf(gd->gedp->ged_result_str, "need database object name");
	return GED_ERROR;
    }
    gd->vobj = argv[0];
    gd->s = NULL;
    argc--; argv++;

    // TODO - if independent use gv_view_objs, else use gv_view_shared_objs
    // TODO - append gedit:: prefix
    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_view_objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_view_objs, i);
	if (BU_STR_EQUAL(gd->vobj, bu_vls_cstr(&s->s_uuid))) {
	    gd->s = s;
	    break;
	}
    }

    if (!gd->s) {
	// View object doesn't already exist.  subcommands will either need to create it
	// or handle the error case
    }

    return _ged_subcmd_exec(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_gedit_cmds,
	    "view gedit", "[options] subcommand [args]", gd, argc, argv, help, cmd_pos);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
