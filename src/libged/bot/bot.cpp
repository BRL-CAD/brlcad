/*                         B O T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file libged/bot/bot.cpp
 *
 * The LIBGED bot command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/opt.h"
#include "bg/chull.h"
#include "bg/trimesh.h"
#include "rt/geom.h"
#include "wdb.h"

#include "./ged_bot.h"
#include "../ged_private.h"

int
_bot_obj_setup(struct _ged_bot_info *gb, const char *name)
{
    gb->dp = db_lookup(gb->gedp->dbip, name, LOOKUP_NOISY);
    if (gb->dp == RT_DIR_NULL) {
	bu_vls_printf(gb->gedp->ged_result_str, ": %s is not a solid or does not exist in database", name);
	return GED_ERROR;
    } else {
	int real_flag = (gb->dp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	if (!real_flag) {
	    /* solid doesn't exist */
	    bu_vls_printf(gb->gedp->ged_result_str, ": %s is not a real solid", name);
	    return GED_ERROR;
	}
    }

    gb->solid_name = std::string(name);

    BU_GET(gb->intern, struct rt_db_internal);

    GED_DB_GET_INTERNAL(gb->gedp, gb->intern, gb->dp, bn_mat_identity, &rt_uniresource, GED_ERROR);
    RT_CK_DB_INTERNAL(gb->intern);

    if (gb->intern->idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type bot\n", gb->solid_name.c_str());
	return GED_ERROR;
    }

    return GED_OK;
}


int
_bot_cmd_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n", ps);
	return 1;
    }
    return 0;
}

extern "C" int
_bot_cmd_get(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot get <faces|minEdge|maxEdge|orientation|type|vertices> <objname>";
    const char *purpose_string = "Report specific information about a BoT shape";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    if (argc != 2) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return GED_ERROR;
    }

    if (_bot_obj_setup(gb, argv[1]) & GED_ERROR) {
	return GED_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);

    fastf_t propVal = rt_bot_propget(bot, argv[0]);

    /* print result string */
    if (!EQUAL(propVal, -1.0)) {
	int intprop = (int) propVal;
	fastf_t tmp = (int) propVal;

	if (EQUAL(propVal, tmp)) {
	    /* int result */
	    /* for orientation and mode, print something more informative than just
	     * the number */
	    if (BU_STR_EQUAL(argv[0], "orientation")) {
		switch (intprop) {
		    case RT_BOT_UNORIENTED:
			bu_vls_printf(gb->gedp->ged_result_str, "none");
			break;
		    case RT_BOT_CCW:
			bu_vls_printf(gb->gedp->ged_result_str, "ccw");
			break;
		    case RT_BOT_CW:
			bu_vls_printf(gb->gedp->ged_result_str, "cw");
			break;
		}
	    } else if (BU_STR_EQUAL(argv[0], "type") || BU_STR_EQUAL(argv[0], "mode")){
		switch (intprop) {
		    case RT_BOT_SURFACE:
			bu_vls_printf(gb->gedp->ged_result_str, "surface");
			break;
		    case RT_BOT_SOLID:
			bu_vls_printf(gb->gedp->ged_result_str, "solid");
			break;
		    case RT_BOT_PLATE:
			bu_vls_printf(gb->gedp->ged_result_str, "plate");
			break;
		    case RT_BOT_PLATE_NOCOS:
			bu_vls_printf(gb->gedp->ged_result_str, "plate_nocos");
			break;
		}
	    } else {
		bu_vls_printf(gb->gedp->ged_result_str, "%d", (int) propVal);
	    }
	} else {
	    /* float result */
	    bu_vls_printf(gb->gedp->ged_result_str, "%f", propVal);
	}
    } else {
	bu_vls_printf(gb->gedp->ged_result_str, "%s is not a valid argument!", argv[1]);
	return GED_ERROR;
    }

    return GED_OK;
}

extern "C" int
_bot_cmd_set(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot set <orientation|type> <objname> <val>";
    const char *purpose_string = "Set BoT object properties";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    if (argc != 3) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return GED_ERROR;
    }

    if (_bot_obj_setup(gb, argv[1]) & GED_ERROR) {
	return GED_ERROR;
    }

    if (!BU_STR_EQUAL(argv[0], "orientation") && !BU_STR_EQUAL(argv[0], "type") && !BU_STR_EQUAL(argv[0], "mode")) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(argv[0], "orientation")) {
	int mode = INT_MAX;
	struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
	if (BU_STR_EQUIV(argv[2], "none") || BU_STR_EQUIV(argv[2], "unoriented") || BU_STR_EQUIV(argv[2], "no")) {
	    mode = RT_BOT_UNORIENTED;
	}
	if (BU_STR_EQUIV(argv[2], "ccw") || BU_STR_EQUIV(argv[2], "counterclockwise") || BU_STR_EQUIV(argv[2], "rh")) {
	    mode = RT_BOT_CCW;
	}
	if (BU_STR_EQUIV(argv[2], "cw") || BU_STR_EQUIV(argv[2], "clockwise") || BU_STR_EQUIV(argv[2], "lh")) {
	    mode = RT_BOT_CW;
	}
	if (mode == INT_MAX) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Possible orientations are: none (no), ccw (rh), and cw (lh)");
	    return GED_ERROR;
	}
	bot->orientation = mode;
    }

    if (BU_STR_EQUAL(argv[0], "type") || BU_STR_EQUAL(argv[0], "mode")) {
	int mode = INT_MAX;
	struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
	if (BU_STR_EQUIV(argv[2], "surface") || BU_STR_EQUIV(argv[2], "surf")) {
	    mode = RT_BOT_SURFACE;
	}
	if (BU_STR_EQUIV(argv[2], "solid") || BU_STR_EQUIV(argv[2], "sol")) {
	    mode = RT_BOT_SOLID;
	}
	if (BU_STR_EQUIV(argv[2], "plate")) {
	    mode = RT_BOT_PLATE;
	}
	if (BU_STR_EQUIV(argv[2], "plate_nocos")) {
	    mode = RT_BOT_PLATE_NOCOS;
	}
	if (mode == INT_MAX) {
	    bu_vls_printf(gb->gedp->ged_result_str, "Possible types are: surface solid plate plate_nocos");
	    return GED_ERROR;
	}
	int old_mode = bot->mode;
	bot->mode = mode;
	if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	    if (old_mode != RT_BOT_PLATE && old_mode != RT_BOT_PLATE_NOCOS) {
		/* need to create some thicknesses */
		bot->thickness = (fastf_t *)bu_calloc(bot->num_faces, sizeof(fastf_t), "BOT thickness");
		bot->face_mode = bu_bitv_new(bot->num_faces);
	    }
	} else {
	    if (old_mode == RT_BOT_PLATE || old_mode == RT_BOT_PLATE_NOCOS) {
		/* free the per face memory */
		bu_free((char *)bot->thickness, "BOT thickness");
		bot->thickness = (fastf_t *)NULL;
		bu_free((char *)bot->face_mode, "BOT face_mode");
		bot->face_mode = (struct bu_bitv *)NULL;
	    }
	}
    }

    if (rt_db_put_internal(gb->dp, gb->gedp->dbip, gb->intern, &rt_uniresource) < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to update BoT");
	return GED_ERROR;
    }

    return GED_OK;
}

extern "C" int
_bot_cmd_chull(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] chull <objname> [output_bot]";
    const char *purpose_string = "Generate the BoT's convex hull and store it in an object";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--; argv++;

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    if (!argc) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s\n%s\n", usage_string, purpose_string);
	return GED_ERROR;
    }


    if (_bot_obj_setup(gb, argv[0]) == GED_ERROR) {
	return GED_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    int retval = 0;
    int fc = 0;
    int vc = 0;
    point_t *vert_array;
    int *faces;
    unsigned char err = 0;

    retval = bg_3d_chull(&faces, &fc, &vert_array, &vc, (const point_t *)bot->vertices, (int)bot->num_vertices);

    if (retval != 3) {
	return GED_ERROR;
    }

    struct bu_vls out_name = BU_VLS_INIT_ZERO;
    if (argc > 1) {
        bu_vls_sprintf(&out_name, "%s", argv[1]);
    } else {
        bu_vls_sprintf(&out_name, "%s.hull", gb->dp->d_namep);
    }

    if (db_lookup(gb->gedp->dbip, bu_vls_cstr(&out_name), LOOKUP_QUIET) != RT_DIR_NULL) {
        bu_vls_printf(gb->gedp->ged_result_str, "Object %s already exists!\n", bu_vls_cstr(&out_name));
        bu_vls_free(&out_name);
        return GED_ERROR;
    }

    retval = mk_bot(gb->gedp->ged_wdbp, bu_vls_cstr(&out_name), RT_BOT_SOLID, RT_BOT_CCW, err, vc, fc, (fastf_t *)vert_array, faces, NULL, NULL);

    bu_vls_free(&out_name);
    bu_free(faces, "free faces");
    bu_free(vert_array, "free verts");

    if (retval) {
	return GED_ERROR;
    }

    return GED_OK;
}

extern "C" int
_bot_cmd_isect(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot [options] isect <objname> <objname2>";
    const char *purpose_string = "(TODO) Test if BoT <objname> intersects with BoT <objname2>";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    if (argc != 2) {
        bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
        return GED_ERROR;
    }

    if (_bot_obj_setup(gb, argv[0]) == GED_ERROR) {
	return GED_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)gb->intern->idb_ptr;

    struct directory *bot_dp_2;
    struct rt_db_internal intern_2;
    GED_DB_LOOKUP(gb->gedp, bot_dp_2, argv[1], LOOKUP_NOISY, GED_ERROR & GED_QUIET);
    GED_DB_GET_INTERNAL(gb->gedp, &intern_2, bot_dp_2, bn_mat_identity, &rt_uniresource, GED_ERROR);
    if (intern_2.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern_2.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gb->gedp->ged_result_str, ": object %s is not of type bot\n", argv[1]);
	rt_db_free_internal(&intern_2);
	return GED_ERROR;
    }
    struct rt_bot_internal *bot_2 = (struct rt_bot_internal *)intern_2.idb_ptr;

    int fc_1 = (int)bot->num_faces;
    int fc_2 = (int)bot_2->num_faces;
    int vc_1 = (int)bot->num_vertices;
    int vc_2 = (int)bot_2->num_vertices;
    point_t *verts_1 = (point_t *)bot->vertices;
    point_t *verts_2 = (point_t *)bot_2->vertices;
    int *faces_1 = bot->faces;
    int *faces_2 = bot_2->faces;

    (void)bg_trimesh_isect(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	    faces_1, fc_1, verts_1, vc_1, faces_2, fc_2, verts_2, vc_2);

    rt_db_free_internal(&intern_2);

    return GED_OK;
}

extern "C" int
_bot_cmd_sync(void *bs, int argc, const char **argv)
{
    const char *usage_string = "bot sync <objname>";
    const char *purpose_string = "Synchronize connected BoT triangle orientations";
    if (_bot_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    struct _ged_bot_info *gb = (struct _ged_bot_info *)bs;

    argc--; argv++;

    if (argc != 1) {
	bu_vls_printf(gb->gedp->ged_result_str, "%s", usage_string);
	return GED_ERROR;
    }

    if (_bot_obj_setup(gb, argv[0]) & GED_ERROR) {
	return GED_ERROR;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(gb->intern->idb_ptr);
    int flip_cnt = bg_trimesh_sync(bot->faces, bot->faces, bot->num_faces);
    if (flip_cnt < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to perform BoT sync");
	return GED_ERROR;
    }

    if (rt_db_put_internal(gb->dp, gb->gedp->dbip, gb->intern, &rt_uniresource) < 0) {
	bu_vls_printf(gb->gedp->ged_result_str, "Failed to update BoT");
	return GED_ERROR;
    }

    bu_vls_printf(gb->gedp->ged_result_str, "Performed %d face flipping operations", flip_cnt);
    return GED_OK;
}

const struct bu_cmdtab _bot_cmds[] = {
    { "extrude",    _bot_cmd_extrude},
    { "get",        _bot_cmd_get},
    { "check",      _bot_cmd_check},
    { "chull",      _bot_cmd_chull},
    { "isect",      _bot_cmd_isect},
    { "remesh",     _bot_cmd_remesh},
    { "set",        _bot_cmd_set},
    { "sync",       _bot_cmd_sync},
    { (char *)NULL,      NULL}
};


static int
_ged_bot_opt_color(struct bu_vls *msg, size_t argc, const char **argv, void *set_c)
{
    struct bu_color **set_color = (struct bu_color **)set_c;
    BU_GET(*set_color, struct bu_color);
    return bu_opt_color(msg, argc, argv, (void *)(*set_color));
}

extern "C" int
ged_bot_core(struct ged *gedp, int argc, const char *argv[])
{
    int help = 0;
    struct _ged_bot_info gb;
    gb.gedp = gedp;
    gb.cmds = _bot_cmds;
    gb.verbosity = 0;
    gb.visualize = 0;
    struct bu_color *color = NULL;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return GED_ERROR;
    }

    // Clear results
    bu_vls_trunc(gedp->ged_result_str, 0);

    // We know we're the bot command - start processing args
    argc--; argv++;

    // See if we have any high level options set
    struct bu_opt_desc d[5];
    BU_OPT(d[0], "h", "help",    "",      NULL,                 &help,         "Print help");
    BU_OPT(d[1], "v", "verbose", "",      NULL,                 &gb.verbosity, "Verbose output");
    BU_OPT(d[2], "V", "visualize", "",    NULL,                 &gb.visualize, "Visualize results");
    BU_OPT(d[3], "C", "color",   "r/g/b", &_ged_bot_opt_color,  &color,        "Set plotting color");
    BU_OPT_NULL(d[4]);

    gb.gopts = d;

    const char *b_args = "[options] <objname> subcommand [args]";
    const struct bu_cmdtab *bcmds = (const struct bu_cmdtab *)_bot_cmds;
    struct bu_opt_desc *boptd = (struct bu_opt_desc *)d;

    if (!argc) {
	_ged_subcmd_help(gedp, boptd, bcmds, "bot", b_args, &gb, 0, NULL);
	return GED_OK;
    }

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_bot_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;

    int opt_ret = bu_opt_parse(NULL, acnt, argv, d);

    if (help) {
	if (cmd_pos >= 0) {
	    argc = argc - cmd_pos;
	    argv = &argv[cmd_pos];
	    _ged_subcmd_help(gedp, boptd, bcmds, "bot", b_args, &gb, argc, argv);
	} else {
	    _ged_subcmd_help(gedp, boptd, bcmds, "bot", b_args, &gb, 0, NULL);
	}
	return GED_OK;
    }

    // Must have a subcommand
    if (cmd_pos == -1) {
	bu_vls_printf(gedp->ged_result_str, ": no valid subcommand specified\n");
	_ged_subcmd_help(gedp, boptd, bcmds, "bot", b_args, &gb, 0, NULL);
	return GED_ERROR;
    }

    if (opt_ret < 0) {
	_ged_subcmd_help(gedp, boptd, bcmds, "bot", b_args, &gb, 0, NULL);
	return GED_ERROR;
    }

    // Jump the processing past any options specified
    for (int i = cmd_pos; i < argc; i++) {
	argv[i - cmd_pos] = argv[i];
    }
    argc = argc - cmd_pos;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    if (gb.visualize) {
	GED_CHECK_DRAWABLE(gedp, GED_ERROR);
	gb.vbp = bv_vlblock_init(&RTG.rtg_vlfree, 32);
    }
    gb.color = color;

    int ret = GED_ERROR;
    if (bu_cmd(_bot_cmds, argc, argv, 0, (void *)&gb, &ret) == BRLCAD_OK) {
	ret = GED_OK;
	goto bot_cleanup;
    }

    bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);

bot_cleanup:
    if (gb.intern) {
	rt_db_free_internal(gb.intern);
	BU_PUT(gb.intern, struct rt_db_internal);
    }
    if (gb.visualize) {
	bv_vlblock_free(gb.vbp);
	gb.vbp = (struct bv_vlblock *)NULL;
    }
    if (color) {
	BU_PUT(color, struct bu_color);
    }
    return ret;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl bot_cmd_impl = { "bot", ged_bot_core, GED_CMD_DEFAULT };
    const struct ged_cmd bot_cmd = { &bot_cmd_impl };

    struct ged_cmd_impl bot_condense_cmd_impl = {"bot_condense", ged_bot_condense_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_condense_cmd = { &bot_condense_cmd_impl };

    struct ged_cmd_impl bot_decimate_cmd_impl = {"bot_decimate", ged_bot_decimate_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_decimate_cmd = { &bot_decimate_cmd_impl };

    struct ged_cmd_impl bot_dump_cmd_impl = {"bot_dump", ged_bot_dump_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_dump_cmd = { &bot_dump_cmd_impl };

    struct ged_cmd_impl bot_face_fuse_cmd_impl = {"bot_face_fuse", ged_bot_face_fuse_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_face_fuse_cmd = { &bot_face_fuse_cmd_impl };

    struct ged_cmd_impl bot_face_sort_cmd_impl = {"bot_face_sort", ged_bot_face_sort_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_face_sort_cmd = { &bot_face_sort_cmd_impl };

    struct ged_cmd_impl bot_flip_cmd_impl = {"bot_flip", ged_bot_flip_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_flip_cmd = { &bot_flip_cmd_impl };

    struct ged_cmd_impl bot_fuse_cmd_impl = {"bot_fuse", ged_bot_fuse_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_fuse_cmd = { &bot_fuse_cmd_impl };

    struct ged_cmd_impl bot_merge_cmd_impl = {"bot_merge", ged_bot_merge_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_merge_cmd = { &bot_merge_cmd_impl };

    struct ged_cmd_impl bot_smooth_cmd_impl = {"bot_smooth", ged_bot_smooth_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_smooth_cmd = { &bot_smooth_cmd_impl };

    struct ged_cmd_impl bot_split_cmd_impl = {"bot_split", ged_bot_split_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_split_cmd = { &bot_split_cmd_impl };

    struct ged_cmd_impl bot_sync_cmd_impl = {"bot_sync", ged_bot_sync_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_sync_cmd = { &bot_sync_cmd_impl };

    struct ged_cmd_impl bot_vertex_fuse_cmd_impl = {"bot_vertex_fuse", ged_bot_vertex_fuse_core, GED_CMD_DEFAULT};
    const struct ged_cmd bot_vertex_fuse_cmd = { &bot_vertex_fuse_cmd_impl };

    struct ged_cmd_impl dbot_dump_cmd_impl = {"dbot_dump", ged_dbot_dump_core, GED_CMD_DEFAULT};
    const struct ged_cmd dbot_dump_cmd = { &dbot_dump_cmd_impl };

    struct ged_cmd_impl find_bot_edge_cmd_impl = {"find_bot_edge", ged_find_bot_edge_nearest_pnt_core, GED_CMD_DEFAULT};
    const struct ged_cmd find_bot_edge_cmd = { &find_bot_edge_cmd_impl };

    struct ged_cmd_impl find_bot_pnt_cmd_impl = {"find_bot_pnt", ged_find_bot_pnt_nearest_pnt_core, GED_CMD_DEFAULT};
    const struct ged_cmd find_bot_pnt_cmd = { &find_bot_pnt_cmd_impl };

    struct ged_cmd_impl get_bot_edges_cmd_impl = {"get_bot_edges", ged_get_bot_edges_core, GED_CMD_DEFAULT};
    const struct ged_cmd get_bot_edges_cmd = { &get_bot_edges_cmd_impl };

    /*
       struct ged_cmd_impl _cmd_impl = {"", , GED_CMD_DEFAULT};
       const struct ged_cmd _cmd = { &_cmd_impl };
       */


    const struct ged_cmd *bot_cmds[] = {
       	&bot_cmd,
	&bot_condense_cmd,
	&bot_decimate_cmd,
	&bot_dump_cmd,
	&bot_face_fuse_cmd,
	&bot_face_sort_cmd,
	&bot_flip_cmd,
	&bot_fuse_cmd,
	&bot_merge_cmd,
	&bot_smooth_cmd,
	&bot_split_cmd,
	&bot_sync_cmd,
	&bot_vertex_fuse_cmd,
	&dbot_dump_cmd,
	&find_bot_edge_cmd,
	&find_bot_pnt_cmd,
	&get_bot_edges_cmd,
	NULL
    };

    static const struct ged_plugin pinfo = { GED_API,  bot_cmds, 17 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
    {
	return &pinfo;
    }
}
#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
