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

#include "bu/color.h"
#include "bu/cmdschema.h"
#include "bu/vls.h"
#include "bv.h"

#include "../ged_private.h"
#include "./ged_view.h"

static void
gobjs_scene_free(struct bv_scene_obj *s)
{
    if (!s)
	return;
    if (s->s_path) {
	struct db_full_path *sfp = (struct db_full_path *)s->s_path;
	db_free_full_path(sfp);
	BU_PUT(sfp, struct db_full_path);
    }
}

int
_gobjs_cmd_create(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
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

    struct bv_scene_obj *s = bv_find_obj(gedp->ged_gvp, argv[1]);
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
    struct bv_scene_group *g = bv_obj_get(v, BV_DB_OBJS);
    if (!g)
	return BRLCAD_ERROR;
    BU_GET(g->s_path, struct db_full_path);
    db_full_path_init((struct db_full_path *)g->s_path);
    db_dup_full_path((struct db_full_path *)g->s_path, fp);
    db_path_to_vls(&g->s_name, fp);
    g->s_i_data = (void *)ip;
    g->s_free_callback = &gobjs_scene_free;

    // Set up drawing settings
    unsigned char wcolor[3] = {255,255,255};
    struct bv_obj_settings vs = BV_OBJ_SETTINGS_INIT;
    bv_obj_settings_sync(g->s_os, &vs);

    // We have a tree walk ahead to populate the wireframe - set up the client
    // data structure.
    std::map<struct directory *, fastf_t> s_size;
    struct draw_data_t dd;
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
    dd.g = g;

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

struct gobjs_root_args {
    int help = 0;
};

static const struct bu_cmd_option gobjs_root_options[] = {
    BU_CMD_FLAG("h", "help", gobjs_root_args, help, "Print help"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema gobjs_root_schema = {
    "gobjs", "Manage graphical view objects", gobjs_root_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_operand gobjs_create_operands[] = {
    BU_CMD_OPERAND("database_object", BU_CMD_VALUE_DB_PATH, 1, 1,
	"Database object path", "ged.db_path"),
    BU_CMD_OPERAND("view_object", BU_CMD_VALUE_STRING, 1, 1,
	"New graphical view-object name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema gobjs_create_schema = {
    "create", "Create an editable view object from a database object", NULL,
    gobjs_create_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_operand gobjs_delete_operands[] = {
    BU_CMD_OPERAND("view_object", BU_CMD_VALUE_STRING, 0, 1,
	"Graphical view-object name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema gobjs_delete_schema = {
    "del", "Delete a graphical view object", NULL, gobjs_delete_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const char * const gobjs_delete_aliases[] = {"delete", NULL};
static const struct bu_cmd_tree_node gobjs_subcommands[] = {
    BU_CMD_TREE_NODE(&gobjs_create_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _gobjs_cmd_create),
    BU_CMD_TREE_NODE(&gobjs_delete_schema, gobjs_delete_aliases, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _gobjs_cmd_delete),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree gobjs_tree = {
    &gobjs_root_schema, gobjs_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};

static void
gobjs_show_help(struct ged *gedp)
{
    char *help = bu_cmd_tree_describe(&gobjs_tree);

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "gobjs native tree help");
    }
}

extern "C" int
_view_cmd_gobjs(void *bs, int argc, const char **argv)
{
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct gobjs_root_args args = {};

    const char *usage_string = "view [options] gobjs [options] [args]";
    const char *purpose_string = "view-only scene objects based on geometry solids/combs";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    if (!gd->cv) {
	bu_vls_printf(gedp->ged_result_str, ": no view specified or current in GED");
	return BRLCAD_ERROR;
    }

    gd->gopts = NULL;

    // We know we're the gobjs command - start processing args
    argc--; argv++;

    // Root options are accepted only before the subcommand.
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	int option_span = bu_cmd_schema_option_span(&gobjs_root_schema,
	    (size_t)(argc - i), argv + i);
	if (option_span > 0) {
	    i += option_span - 1;
	    continue;
	}
	if (option_span < 0 || (argv[i][0] == '-' && argv[i][1]))
	    break;
	if (bu_cmd_tree_find_subcommand(&gobjs_tree, argv[i])) {
	    cmd_pos = i;
	    break;
	}
	cmd_pos = i;
	break;
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    int ac = bu_cmd_schema_parse(&gobjs_root_schema, &args,
	gedp->ged_result_str, acnt, argv);
    if (ac < 0 || ac != acnt)
	return BRLCAD_ERROR;

    // If we're not wanting help and we have no subcommand, list current gobjs objects
    struct bview *v = gd->cv;
	if (!ac && cmd_pos < 0 && !args.help) {
	struct bu_ptbl *view_objs = bv_view_objs(v, BV_VIEW_OBJS);
	if (view_objs) {
	    for (size_t i = 0; i < BU_PTBL_LEN(view_objs); i++) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(view_objs, i);
		bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_name));
	    }
	}
	struct bu_ptbl *local_view_objs = bv_view_objs(v, BV_VIEW_OBJS | BV_LOCAL_OBJS);
	if (local_view_objs) {
	    for (size_t i = 0; i < BU_PTBL_LEN(local_view_objs); i++) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(local_view_objs, i);
		bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->s_name));
	    }
	}
	return BRLCAD_OK;
    }

    if (args.help) {
	if (cmd_pos >= 0 && bu_cmd_tree_find_subcommand(&gobjs_tree, argv[cmd_pos])) {
	    const char *child_help[] = {argv[cmd_pos], HELPFLAG};
	    int ignored = BRLCAD_OK;
	    (void)bu_cmd_tree_dispatch(&gobjs_tree, gd, 2, child_help, &ignored);
	} else {
	    gobjs_show_help(gedp);
	}
	return BRLCAD_OK;
    }

    if (cmd_pos < 0) {
	gobjs_show_help(gedp);
	return BRLCAD_ERROR;
    }

    const struct bu_cmd_tree_node *node = bu_cmd_tree_find_subcommand(&gobjs_tree,
	argv[cmd_pos]);
    if (!node || bu_cmd_schema_parse_complete(node->schema, NULL,
	gedp->ged_result_str, argc - cmd_pos - 1, argv + cmd_pos + 1) < 0)
	return BRLCAD_ERROR;

    gd->s = NULL;

    if (!gd->s) {
	// View object doesn't already exist.  subcommands will either need to create it
	// or handle the error case
    }
    int ret;
    if (bu_cmd_tree_dispatch(&gobjs_tree, gd, argc - cmd_pos, argv + cmd_pos, &ret) == 0)
	return ret;
    bu_vls_printf(gedp->ged_result_str, "unknown graphical view-object subcommand: %s\n",
	argv[cmd_pos]);
    gobjs_show_help(gedp);
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
