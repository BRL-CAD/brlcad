/*                    W H O _ S O L I D S . C P P
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
/** @file libged/who_solids.cpp
 *
 * Shared detailed solid reporting support for who/solid_report/x.
 *
 */

#include "common.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include "bu/opt.h"
#include "ged.h"
#include "ged/bsg_ged_draw.h"
#include "rt/db_fullpath.h"
#include "rt/directory.h"
#include "rt/tree.h"
}

#include "bsg/export.h"
#include "bsg/render.h"
#include "bsg/view_set.h"

#include "../alphanum.h"
#include "../ged_private.h"

static void
who_solids_usage(struct ged *gedp, const char *cmd_name, int subcmd_usage)
{
    const char *subcmd = "  who solids [-V view] [-m #] [level]\n";

    if (subcmd_usage) {
	bu_vls_printf(gedp->ged_result_str,
		"Usage:\n"
		"%s"
		"\n"
		"level <= -2 : print primitive names\n"
		"level == -1 : print full paths\n"
		"level == 0  : print full paths and ILLUM state\n"
		"level == 1  : add center, size, region, and color info\n"
		"level == 2  : add vector list counts\n"
		"level >= 3  : add vector list commands\n",
		subcmd);
	return;
    }

    bu_vls_printf(gedp->ged_result_str,
	    "Usage:\n"
	    "  %s [level]\n"
	    "%s"
	    "\n"
	    "level <= -2 : print primitive names\n"
	    "level == -1 : print full paths\n"
	    "level == 0  : print full paths and ILLUM state\n"
	    "level == 1  : add center, size, region, and color info\n"
	    "level == 2  : add vector list counts\n"
	    "level >= 3  : add vector list commands\n",
	    cmd_name,
	    subcmd);
}


static int
who_solids_level(const char *arg, int *lvl)
{
    if (!arg || !lvl)
	return 0;

    const char *av[1] = {arg};
    if (bu_opt_int(NULL, 1, av, (void *)lvl) != 1)
	return 0;

    if (*lvl > 3)
	*lvl = 3;

    return 1;
}


static int
who_solids_path_state(struct db_i *dbip, const char *path, struct db_tree_state *ts)
{
    struct db_full_path fp;
    int ret = 0;

    if (!dbip || !path || !ts)
	return 0;

    db_full_path_init(&fp);
    RT_DBTS_INIT(ts);
    db_init_db_tree_state(ts, dbip);

    ret = (db_follow_path_for_state(ts, &fp, path, LOOKUP_QUIET) >= 0);

    db_free_full_path(&fp);
    if (!ret)
	db_free_db_tree_state(ts);

    return ret;
}


static void
who_solids_print_export_record(const struct bsg_export_record *rec, struct db_i *dbip, int lvl, struct bu_vls *vls)
{
    struct db_tree_state ts = RT_DBTS_INIT_ZERO;
    unsigned char basecolor[3];
    int region_id = -1;
    int dflag = 0;
    int cflag = 0;
    const char *path = NULL;
    const char *leaf = NULL;

    if (!rec || !dbip || !vls)
	return;

    path = bu_vls_cstr(&rec->path);
    if (lvl <= -2) {
	if (path && *path) {
	    leaf = strrchr(path, '/');
	    leaf = leaf ? leaf + 1 : path;
	}
	if (leaf)
	    bu_vls_printf(vls, "%s ", leaf);
	return;
    }

    bu_vls_printf(vls, "%s", path ? path : "");
    if ((lvl != -1) && rec->highlighted)
	bu_vls_printf(vls, " ILLUM");
    bu_vls_printf(vls, "\n");

    if (lvl <= 0)
	return;

    basecolor[0] = rec->color[0];
    basecolor[1] = rec->color[1];
    basecolor[2] = rec->color[2];
    if (who_solids_path_state(dbip, path, &ts)) {
	region_id = ts.ts_regionid;
	if (ts.ts_mater.ma_color_valid) {
	    basecolor[0] = ts.ts_mater.ma_color[0] * 255.0;
	    basecolor[1] = ts.ts_mater.ma_color[1] * 255.0;
	    basecolor[2] = ts.ts_mater.ma_color[2] * 255.0;
	} else {
	    dflag = 1;
	}
	db_free_db_tree_state(&ts);
    }

    cflag = (basecolor[0] != rec->color[0] ||
	     basecolor[1] != rec->color[1] ||
	     basecolor[2] != rec->color[2]);

    bu_vls_printf(vls, "  cent=(%.3f, %.3f, %.3f) sz=%g ",
		  rec->bounds_center[X] * dbip->dbi_base2local,
		  rec->bounds_center[Y] * dbip->dbi_base2local,
		  rec->bounds_center[Z] * dbip->dbi_base2local,
		  rec->bounds_radius * dbip->dbi_base2local);
    bu_vls_printf(vls, "reg=%d\n", region_id);
    bu_vls_printf(vls, "  basecolor=(%d, %d, %d) color=(%d, %d, %d)%s%s\n",
		  basecolor[0],
		  basecolor[1],
		  basecolor[2],
		  rec->color[0],
		  rec->color[1],
		  rec->color[2],
		  dflag ? " D" : "",
		  cflag ? " C" : "");

    if (lvl <= 1)
	return;

    if (lvl > 2)
	bsg_export_record_geometry_report(rec, vls);

    bu_vls_printf(vls, "  %zu vlist structures, %zu pts\n",
		  rec->vlist_structure_count,
		  rec->vlist_point_count);
    bu_vls_printf(vls, "  %zu pts (via semantic export)\n",
		  rec->vlist_point_count);
}


static void
who_solids_print_view(struct bsg_view *v, struct db_i *dbip, int mode, int lvl, struct bu_vls *vls)
{
    std::vector<const struct bsg_export_record *> objs;

    if (!v)
	return;

    struct bsg_export_request request;
    bsg_export_request_init(&request, v);
    request.query_flags = BSG_EXPORT_QUERY_VISIBLE_ONLY | BSG_EXPORT_QUERY_DB_OBJECTS;
    request.render_flags = BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE;
    if (mode >= 0)
	request.draw_mode = mode;
    struct bsg_export_result *result = bsg_export_query(&request);
    if (!result)
	return;

    for (size_t i = 0; i < bsg_export_result_count(result); i++) {
	const struct bsg_export_record *rec = bsg_export_result_get(result, i);
	if (!rec)
	    continue;
	if (rec->non_database_source)
	    continue;
	objs.push_back(rec);
    }

    std::sort(objs.begin(), objs.end(),
	      [](const struct bsg_export_record *a, const struct bsg_export_record *b) {
		  const char *aname = a ? bu_vls_cstr(&a->path) : "";
		  const char *bname = b ? bu_vls_cstr(&b->path) : "";
		  return alphanum_impl(aname ? aname : "", bname ? bname : "", NULL) < 0;
	      });

    for (size_t i = 0; i < objs.size(); i++)
	who_solids_print_export_record(objs[i], dbip, lvl, vls);

    bsg_export_result_free(result);
}


static int
who_solids_impl(struct ged *gedp, int argc, const char *argv[], int subcmd_usage)
{
    int print_help = 0;
    int lvl = 0;
    int mode = -1;
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    const char *cmd_name = argv[0];
    struct bu_opt_desc d[5];
    BU_OPT(d[0], "h", "help", "",     NULL,        &print_help, "Print help and exit");
    BU_OPT(d[1], "?", "",     "",     NULL,        &print_help, "");
    BU_OPT(d[2], "V", "view", "name", &bu_opt_vls, &cvls,       "Specify view to report");
    BU_OPT(d[3], "m", "mode", "#",    &bu_opt_int, &mode,       "Only report objects drawn in the specified drawing mode");
    BU_OPT_NULL(d[4]);

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);

    if (subcmd_usage) {
	if (argc < 2) {
	    who_solids_usage(gedp, cmd_name, 1);
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
	argc--;
	argv++;
	cmd_name = argv[0];
	argc--;
	argv++;
    } else {
	argc--;
	argv++;
    }

    int opt_ret = bu_opt_parse(NULL, argc, argv, d);
    if (opt_ret < 0) {
	who_solids_usage(gedp, cmd_name, subcmd_usage);
	bu_vls_free(&cvls);
	return BRLCAD_ERROR;
    }

    argc = opt_ret;

    if (print_help) {
	who_solids_usage(gedp, subcmd_usage ? "who" : cmd_name, subcmd_usage);
	bu_vls_free(&cvls);
	return BRLCAD_OK;
    }

    if (argc > 1 || (argc == 1 && !who_solids_level(argv[0], &lvl))) {
	who_solids_usage(gedp, subcmd_usage ? "who" : cmd_name, subcmd_usage);
	bu_vls_free(&cvls);
	return BRLCAD_ERROR;
    }

    struct bsg_view *v = gedp->ged_gvp;
    if (bu_vls_strlen(&cvls)) {
	v = bsg_set_find_view(&gedp->ged_views, bu_vls_cstr(&cvls));
	if (!v) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
    }
    bu_vls_free(&cvls);

    if (!v) {
	bu_vls_printf(gedp->ged_result_str, "No view specified and no current view defined in GED");
	return BRLCAD_ERROR;
    }

    who_solids_print_view(v, gedp->dbip, mode, lvl, gedp->ged_result_str);
    return BRLCAD_OK;
}


extern "C" int
ged_solid_report_shared_core(struct ged *gedp, int argc, const char *argv[])
{
    return who_solids_impl(gedp, argc, argv, 0);
}


extern "C" int
ged_who_solids_core(struct ged *gedp, int argc, const char *argv[])
{
    return who_solids_impl(gedp, argc, argv, 1);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
