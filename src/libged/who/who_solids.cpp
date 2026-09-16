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
#include "rt/db_fullpath.h"
#include "rt/directory.h"
#include "rt/tree.h"
}

#include "bv/util.h"
#include "bv/view_sets.h"

#include "../alphanum.h"
#include "../ged_private.h"

#define LAST_SOLID(_sp) DB_FULL_PATH_CUR_DIR(&(_sp)->s_fullpath)

static void
who_solids_usage(struct ged *gedp, const char *cmd_name, int subcmd_usage)
{
    const char *subcmd = (gedp->new_cmd_forms) ?
	"  who solids [-V view] [-m #] [level]\n" :
	"  who solids [level]\n";

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
who_solids_print_scene_obj(struct bv_scene_obj *sp, struct db_i *dbip, int lvl, struct bu_vls *vls)
{
    struct db_tree_state ts = RT_DBTS_INIT_ZERO;
    unsigned char basecolor[3];
    int region_id = -1;
    int dflag = 0;
    int cflag = 0;
    size_t nvlist = 0;
    size_t npts = 0;
    const char *leaf = NULL;

    struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

    if (lvl <= -2) {
	if (bdata && LAST_SOLID(bdata))
	    leaf = LAST_SOLID(bdata)->d_namep;
	if (!leaf && bu_vls_strlen(&sp->s_name)) {
	    leaf = strrchr(bu_vls_cstr(&sp->s_name), '/');
	    leaf = leaf ? leaf + 1 : bu_vls_cstr(&sp->s_name);
	}
	if (leaf)
	    bu_vls_printf(vls, "%s ", leaf);
	return;
    }

    bu_vls_printf(vls, "%s", bu_vls_cstr(&sp->s_name));
    if ((lvl != -1) && (sp->s_iflag == UP))
	bu_vls_printf(vls, " ILLUM");
    bu_vls_printf(vls, "\n");

    if (lvl <= 0)
	return;

    basecolor[0] = sp->s_color[0];
    basecolor[1] = sp->s_color[1];
    basecolor[2] = sp->s_color[2];
    if (who_solids_path_state(dbip, bu_vls_cstr(&sp->s_name), &ts)) {
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

    cflag = (basecolor[0] != sp->s_color[0] ||
	     basecolor[1] != sp->s_color[1] ||
	     basecolor[2] != sp->s_color[2]);

    bu_vls_printf(vls, "  cent=(%.3f, %.3f, %.3f) sz=%g ",
		  sp->s_center[X] * dbip->dbi_base2local,
		  sp->s_center[Y] * dbip->dbi_base2local,
		  sp->s_center[Z] * dbip->dbi_base2local,
		  sp->s_size * dbip->dbi_base2local);
    bu_vls_printf(vls, "reg=%d\n", region_id);
    bu_vls_printf(vls, "  basecolor=(%d, %d, %d) color=(%d, %d, %d)%s%s\n",
		  basecolor[0],
		  basecolor[1],
		  basecolor[2],
		  sp->s_color[0],
		  sp->s_color[1],
		  sp->s_color[2],
		  dflag ? " D" : "",
		  cflag ? " C" : "");

    if (lvl <= 1)
	return;

    struct bv_vlist *vp;
    for (BU_LIST_FOR(vp, bv_vlist, &sp->s_vlist)) {
	size_t i;
	size_t nused = vp->nused;
	int *cmd = vp->cmd;
	point_t *pt = vp->pt;

	BV_CK_VLIST(vp);
	nvlist++;
	npts += nused;

	if (lvl <= 2)
	    continue;

	for (i = 0; i < nused; i++, cmd++, pt++) {
	    bu_vls_printf(vls, "  %s (%g, %g, %g)\n",
			  bv_vlist_get_cmd_description(*cmd),
			  V3ARGS(*pt));
	}
    }

    bu_vls_printf(vls, "  %zu vlist structures, %zu pts\n", nvlist, npts);
    bu_vls_printf(vls, "  %zu pts (via bv_ck_vlist)\n", bv_ck_vlist(&sp->s_vlist));
}


static void
who_solids_print_view(struct bview *v, struct db_i *dbip, int mode, int lvl, struct bu_vls *vls)
{
    std::set<struct bv_scene_obj *> uniq;
    std::vector<struct bv_scene_obj *> objs;
    struct bu_ptbl *tbls[2];

    if (!v)
	return;

    tbls[0] = bv_view_objs(v, BV_DB_OBJS);
    tbls[1] = bv_view_objs(v, BV_DB_OBJS | BV_LOCAL_OBJS);

    for (size_t t = 0; t < 2; t++) {
	struct bu_ptbl *tbl = tbls[t];
	if (!tbl)
	    continue;
	for (size_t i = 0; i < BU_PTBL_LEN(tbl); i++) {
	    struct bv_scene_obj *sp = (struct bv_scene_obj *)BU_PTBL_GET(tbl, i);
	    if (!sp || uniq.find(sp) != uniq.end())
		continue;
	    if (mode >= 0 && sp->s_os->s_dmode != mode)
		continue;
	    uniq.insert(sp);
	    objs.push_back(sp);
	}
    }

    std::sort(objs.begin(), objs.end(),
	      [](const struct bv_scene_obj *a, const struct bv_scene_obj *b) {
		  return alphanum_impl(bu_vls_cstr(&a->s_name), bu_vls_cstr(&b->s_name), NULL) < 0;
	      });

    for (size_t i = 0; i < objs.size(); i++)
	who_solids_print_scene_obj(objs[i], dbip, lvl, vls);
}


static void
who_solids_print_display(struct bu_list *hdlp, struct db_i *dbip, int lvl, struct bu_vls *vls)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;

    if (dbip == DBI_NULL)
	return;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (!sp->s_u_data)
		continue;
	    struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

	    if (lvl <= -2) {
		if (bdata && LAST_SOLID(bdata))
		    bu_vls_printf(vls, "%s ", LAST_SOLID(bdata)->d_namep);
		continue;
	    }

	    db_path_to_vls(vls, &bdata->s_fullpath);
	    if ((lvl != -1) && (sp->s_iflag == UP))
		bu_vls_printf(vls, " ILLUM");
	    bu_vls_printf(vls, "\n");

	    if (lvl <= 0)
		continue;

	    bu_vls_printf(vls, "  cent=(%.3f, %.3f, %.3f) sz=%g ",
			  sp->s_center[X] * dbip->dbi_base2local,
			  sp->s_center[Y] * dbip->dbi_base2local,
			  sp->s_center[Z] * dbip->dbi_base2local,
			  sp->s_size * dbip->dbi_base2local);
	    bu_vls_printf(vls, "reg=%d\n", sp->s_old.s_regionid);
	    bu_vls_printf(vls, "  basecolor=(%d, %d, %d) color=(%d, %d, %d)%s%s%s\n",
			  sp->s_old.s_basecolor[0],
			  sp->s_old.s_basecolor[1],
			  sp->s_old.s_basecolor[2],
			  sp->s_color[0],
			  sp->s_color[1],
			  sp->s_color[2],
			  sp->s_old.s_uflag ? " U" : "",
			  sp->s_old.s_dflag ? " D" : "",
			  sp->s_old.s_cflag ? " C" : "");

	    if (lvl <= 1)
		continue;

	    size_t nvlist = 0;
	    size_t npts = 0;
	    struct bv_vlist *vp;
	    for (BU_LIST_FOR(vp, bv_vlist, &sp->s_vlist)) {
		size_t i;
		size_t nused = vp->nused;
		int *cmd = vp->cmd;
		point_t *pt = vp->pt;

		BV_CK_VLIST(vp);
		nvlist++;
		npts += nused;

		if (lvl <= 2)
		    continue;

		for (i = 0; i < nused; i++, cmd++, pt++) {
		    bu_vls_printf(vls, "  %s (%g, %g, %g)\n",
				  bv_vlist_get_cmd_description(*cmd),
				  V3ARGS(*pt));
		}
	    }

	    bu_vls_printf(vls, "  %zu vlist structures, %zu pts\n", nvlist, npts);
	    bu_vls_printf(vls, "  %zu pts (via bv_ck_vlist)\n", bv_ck_vlist(&sp->s_vlist));
	}

	gdlp = next_gdlp;
    }
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

    if (!gedp->new_cmd_forms) {
	if (bu_vls_strlen(&cvls) || mode != -1) {
	    who_solids_usage(gedp, subcmd_usage ? "who" : cmd_name, subcmd_usage);
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
	who_solids_print_display(gedp->i->ged_gdp->gd_headDisplay, gedp->dbip, lvl, gedp->ged_result_str);
	bu_vls_free(&cvls);
	return BRLCAD_OK;
    }

    struct bview *v = gedp->ged_gvp;
    if (bu_vls_strlen(&cvls)) {
	v = bv_set_find_view(&gedp->ged_views, bu_vls_cstr(&cvls));
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
