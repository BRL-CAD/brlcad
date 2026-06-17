/*                         D R A W 2 . C
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
/** @file libged/draw2.c
 *
 * Testing command for experimenting with drawing routines.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <string>
#include <vector>
#include "bsocket.h"

#include "bu/cmd.h"
#include "bu/opt.h"
#include "bu/sort.h"
#include "nmg.h"
#include "rt/db_attr.h"
#include "rt/view.h"

#include "ged/bsg_ged_draw.h"
#include "ged/view.h"
#include "../ged_private.h"
#include "./ged_draw.h"

static int
draw_opt_color(struct bu_vls *msg, size_t argc, const char **argv, void *data)
{
    struct bsg_appearance_settings *vs = (struct bsg_appearance_settings *)data;
    struct bu_color c;
    int ret = bu_opt_color(msg, argc, argv, (void *)&c);
    if (ret == 1 || ret == 3) {
	vs->color_override = 1;
	bu_color_to_rgb_chars(&c, vs->color);
    }
    return ret;
}


static int
draw_user_mode_to_internal(int mode)
{
    switch (mode) {
	case 0:
	    return BSG_DRAW_MODE_WIRE;
	case 1:
	    return BSG_DRAW_MODE_SHADED_BOTS;
	case 2:
	    return BSG_DRAW_MODE_SHADED;
	case 3:
	    return BSG_DRAW_MODE_EVAL_WIRE;
	case 4:
	    return BSG_DRAW_MODE_HIDDEN_LINE;
	case 5:
	    return BSG_DRAW_MODE_EVAL_POINTS;
	default:
	    return (mode < 0) ? _GED_SHADED_MODE_UNSET : BSG_DRAW_MODE_SHADED;
    }
}

static int
draw_short_option_has(const char *arg, char flag)
{
    if (!arg || arg[0] != '-' || arg[1] == '-' || !arg[1])
	return 0;

    for (const char *c = arg + 1; *c; c++) {
	if (*c == flag)
	    return 1;
    }

    return 0;
}

static int
draw_filter_legacy_attr_option(const char *arg, std::string &out)
{
    if (!arg || arg[0] != '-' || arg[1] == '-' || !arg[1])
	return 0;

    out.clear();
    out.push_back('-');
    for (const char *c = arg + 1; *c; c++) {
	if (*c != 'A' && *c != 'o')
	    out.push_back(*c);
    }

    return out.size() > 1;
}

static int
draw_option_takes_separate_arg(const char *arg)
{
    if (!arg || arg[0] != '-' || arg[1] == '-' || !arg[1])
	return 0;

    size_t len = strlen(arg);
    if (len != 2)
	return 0;

    return strchr("VmtxLC", arg[1]) != NULL;
}

static int
draw_token_looks_like_db_path(struct ged *gedp, const char *arg)
{
    if (!gedp || !gedp->dbip || !arg || !arg[0])
	return 0;

    if (strchr(arg, '/'))
	return 1;

    return db_lookup(gedp->dbip, arg, LOOKUP_QUIET) != RT_DIR_NULL;
}

static int
draw_expand_legacy_attr_args(struct ged *gedp, int argc, const char *argv[],
	std::vector<std::string> &storage, std::vector<const char *> &expanded)
{
    int flag_A_attr = 0;
    int flag_o_nonunique = 1;
    int last_opt = -1;

    for (int i = 0; i < argc; i++) {
	if (!argv[i] || argv[i][0] != '-' || BU_STR_EQUAL(argv[i], "--"))
	    break;

	if (argv[i][1] == '-')
	    continue;

	if (draw_short_option_has(argv[i], 'A'))
	    flag_A_attr = 1;
	if (draw_short_option_has(argv[i], 'o'))
	    flag_o_nonunique = 2;

	last_opt = i;

	if (draw_option_takes_separate_arg(argv[i])) {
	    if (i + 1 >= argc)
		break;
	    last_opt = i + 1;
	    i++;
	}
    }

    if (!flag_A_attr)
	return 0;

    int first_arg = last_opt + 1;
    int remaining_args = argc - first_arg;
    if (remaining_args < 2 || remaining_args % 2)
	return 0;

    for (int i = first_arg; i < argc; i++) {
	if (!argv[i] || argv[i][0] == '-')
	    return 0;
    }

    int all_look_like_paths = 1;
    for (int i = first_arg; i < argc; i++) {
	if (!draw_token_looks_like_db_path(gedp, argv[i])) {
	    all_look_like_paths = 0;
	    break;
	}
    }
    if (all_look_like_paths)
	return 0;

    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
    bu_avs_init(&avs, (remaining_args / 2), "draw legacy attr avs");
    for (int i = first_arg; i < argc; i += 2) {
	if (flag_o_nonunique == 2)
	    bu_avs_add_nonunique(&avs, argv[i], argv[i + 1]);
	else
	    bu_avs_add(&avs, argv[i], argv[i + 1]);
    }

    struct bu_ptbl *tbl = db_lookup_by_attr(gedp->dbip,
	    RT_DIR_REGION | RT_DIR_SOLID | RT_DIR_COMB, &avs,
	    flag_o_nonunique);
    bu_avs_free(&avs);

    if (!tbl) {
	bu_vls_printf(gedp->ged_result_str, "Error: db_lookup_by_attr() failed!!\n");
	return -1;
    }

    if (BU_PTBL_LEN(tbl) < 1) {
	bu_ptbl_free(tbl);
	bu_free((char *)tbl, "draw legacy attr ptbl");
	return 2;
    }

    storage.clear();
    expanded.clear();
    storage.reserve((size_t)argc + BU_PTBL_LEN(tbl));
    expanded.reserve((size_t)argc + BU_PTBL_LEN(tbl));

    for (int i = 0; i <= last_opt; i++) {
	if (draw_short_option_has(argv[i], 'A') || draw_short_option_has(argv[i], 'o')) {
	    std::string filtered;
	    if (draw_filter_legacy_attr_option(argv[i], filtered)) {
		storage.push_back(filtered);
		if (draw_option_takes_separate_arg(filtered.c_str()) && i + 1 < argc) {
		    i++;
		    storage.push_back(argv[i]);
		}
	    } else if (draw_option_takes_separate_arg(argv[i]) && i + 1 < argc) {
		i++;
	    }
	    continue;
	}

	storage.push_back(argv[i]);
	if (draw_option_takes_separate_arg(argv[i]) && i + 1 < argc) {
	    i++;
	    storage.push_back(argv[i]);
	}
    }

    for (size_t i = 0; i < BU_PTBL_LEN(tbl); i++) {
	struct directory *dp = (struct directory *)BU_PTBL_GET(tbl, i);
	if (dp && dp->d_namep)
	    storage.push_back(dp->d_namep);
    }

    bu_ptbl_free(tbl);
    bu_free((char *)tbl, "draw legacy attr ptbl");

    for (std::string &arg : storage)
	expanded.push_back(arg.c_str());

    return expanded.empty() ? 2 : 1;
}


/*
 *  Main drawing command control logic
 */
extern "C" int
ged_draw2_core(struct ged *gedp, int argc, const char *argv[])
{
    int print_help = 0;
    int bot_threshold = -1;
    int no_autoview = 0;
    static const char *usage = "[options] path1 [path2 ...]";
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Draw may operate on a specific user specified view.  If it does so,
     * we want the default settings to reflect those set in that particular
     * view.  In order to set up the correct default views, we need to know
     * if a specific view has in fact been specified.  We do a preliminary
     * option check to figure this out */
    struct bsg_view *cv = gedp->ged_gvp;
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    struct bu_opt_desc vd[2];
    BU_OPT(vd[0],  "V", "view",    "name",      &bu_opt_vls, &cvls,   "specify view to draw on");
    BU_OPT_NULL(vd[1]);
    int opt_ret = bu_opt_parse(NULL, argc, argv, vd);
    argc = opt_ret;
    if (argc < 0) {
	bu_vls_free(&cvls);
	return BRLCAD_ERROR;
    }

    if (bu_vls_strlen(&cvls)) {
	cv = bsg_set_find_view(&gedp->ged_views, bu_vls_cstr(&cvls));
	if (!cv) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}

	if (!bsg_view_is_independent(cv)) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s is not an independent view, and as such does not support specifying db objects for display in only this view.  To change the view's status, the command 'view independent %s 1' may be applied.\n", bu_vls_cstr(&cvls), bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
    }

    // If we don't have a specified view, and the default view isn't a shared view, see if
    // we can find a shared view in the view set.
    if (!bu_vls_strlen(&cvls) && (!cv || bsg_view_is_independent(cv))) {
	struct bu_ptbl *views = bsg_set_views(&gedp->ged_views);
	for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	    struct bsg_view *bv = (struct bsg_view *)BU_PTBL_GET(views, i);
	    if (!bsg_view_is_independent(bv)) {
		cv = bv;
		break;
	    }
	}
    }

    bu_vls_free(&cvls);

    // We need a current view, either from gedp or from the options
    if (!cv) {
	bu_vls_printf(gedp->ged_result_str, "No view specified and no shared views found");
	return BRLCAD_ERROR;
    }

    /* User settings may override various options - set up to collect them.
     * Option defaults may be overridden for the purposes of the current draw
     * command by command line options. */
    struct bsg_appearance_settings vs = BSG_APPEARANCE_SETTINGS_INIT;

    int drawing_modes[6] = {-1, 0, 0, 0, 0, 0};
    struct bu_opt_desc d[19];
    BU_OPT(d[0],   "", "help",          "",                 NULL, &print_help,         "Print help and exit");
    BU_OPT(d[1],  "?", "",              "",                 NULL, &print_help,         "");
    BU_OPT(d[2],  "m", "mode",         "#",          &bu_opt_int, &drawing_modes[0],  "0=wireframe;1=shaded bots;2=shaded;3=evaluated");
    BU_OPT(d[3],   "", "wireframe",     "",                 NULL, &drawing_modes[1],  "Draw using only wireframes (mode = 0)");
    BU_OPT(d[4],   "", "shaded",        "",                 NULL, &drawing_modes[2],  "Shade bots, breps and polysolids (mode = 1)");
    BU_OPT(d[5],   "", "shaded-all",    "",                 NULL, &drawing_modes[3],  "Shade all solids, not evaluated (mode = 2)");
    BU_OPT(d[6],  "E", "evaluate",      "",                 NULL, &drawing_modes[4],  "Wireframe with evaluate booleans (mode = 3)");
    BU_OPT(d[7],  "h", "hidden-line",   "",                 NULL, &drawing_modes[5],  "Hidden line wireframes");
    BU_OPT(d[8],  "A", "add-mode",      "",                 NULL, &vs.mixed_modes,    "Don't erase other drawn modes for specified paths (allows simultaneous shaded and wireframe drawing for the same object)");
    BU_OPT(d[9],  "t", "transparency", "#",      &bu_opt_fastf_t, &vs.transparency,   "Set transparency level in drawing: range 0 (clear) to 1 (opaque)");
    BU_OPT(d[10], "x", "",             "#",      &bu_opt_fastf_t, &vs.transparency,   "");
    BU_OPT(d[11], "L", "",             "#",          &bu_opt_int, &bot_threshold,     "Set face count level for drawing bounding boxes instead of BoT triangles (NOTE: passing this updates the global view setting - bot_threshold is a view property).");
    BU_OPT(d[12], "S", "no-subtract",   "",                 NULL, &vs.draw_non_subtract_only,  "Do not draw subtraction solids");
    BU_OPT(d[13], "s", "no-dash",       "",                 NULL, &vs.draw_solid_lines_only,  "Use solid lines rather than dashed for subtraction solids");
    BU_OPT(d[14], "C", "color",         "r/g/b", &draw_opt_color, &vs,                "Override object colors");
    BU_OPT(d[15],  "", "line-width",   "#",          &bu_opt_int, &vs.s_line_width,   "Override default line width");
    BU_OPT(d[16], "R", "no-autoview",   "",                 NULL, &no_autoview,       "Do not calculate automatic view, even if initial scene is empty.");
    BU_OPT(d[17],  "", "strict",        "",                 NULL, &vs.strict_fallback, "Do not fall back to wireframe when shaded or hidden-line tessellation fails");
    BU_OPT_NULL(d[18]);

    /* If no args, must be wanting help */
    if (!argc) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    std::vector<std::string> attr_storage;
    std::vector<const char *> attr_argv;
    int attr_expand = draw_expand_legacy_attr_args(gedp, argc, argv, attr_storage, attr_argv);
    if (attr_expand < 0)
	return BRLCAD_ERROR;
    if (attr_expand == 2)
	return BRLCAD_OK;
    if (attr_expand == 1) {
	argc = (int)attr_argv.size();
	argv = attr_argv.data();
    }

    /* Process command line args into vs with bu_opt */
    struct bu_vls omsg = BU_VLS_INIT_ZERO;
    opt_ret = bu_opt_parse(&omsg, argc, argv, d);
    if (opt_ret < 0) {
	bu_vls_printf(gedp->ged_result_str, "option parsing error: %s\n", bu_vls_cstr(&omsg));
	bu_vls_free(&omsg);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&omsg);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    // Whatever is left after argument processing are the potential draw paths
    argc = opt_ret;

    // Drawing modes may be set either by -m or by the more verbose options,
    // with the latter taking precedence if both are set.
    int have_override = 0;
    for (int i = 1; i < 6; i++) {
	if (drawing_modes[i]) {
	    have_override++;
	}
    }
    if (have_override > 1 || (have_override &&  drawing_modes[0] > -1)) {
	bu_vls_printf(gedp->ged_result_str, "Multiple view modes specified\n");
	return BRLCAD_ERROR;
    }
    if (have_override) {
	for (int i = 1; i < 6; i++) {
	    if (drawing_modes[i]) {
		drawing_modes[0] = i - 1;
		break;
	    }
	}
    }
    if (drawing_modes[0] > -1) {
	vs.draw_mode = (bsg_draw_mode)draw_user_mode_to_internal(drawing_modes[0]);
    }

    // Before we start doing anything with the object set, record if things are
    // starting out empty.
    int blank_slate = 0;
    /* For autoview gating, base blank-slate detection on DB draw state.
     * View-only objects (overlays, helpers, etc.) should not suppress
     * autoview when no DB geometry is currently drawn. */
    if (!ged_draw_has_paths(gedp, cv, -1)) {
	blank_slate = 1;
    }

    struct ged_draw_transaction txn =
	ged_draw_transaction_make(GED_DRAW_TXN_DRAW, NULL);
    txn.view = cv;
    txn.paths = argv;
    txn.path_count = argc;
    txn.appearance = &vs;
    txn.autoview = blank_slate && !no_autoview;
    return (ged_draw_apply_transaction(gedp, &txn, NULL) < 0) ?
	BRLCAD_ERROR : BRLCAD_OK;
}

extern "C" int
ged_redraw2_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);
    RT_CHECK_DBI(gedp->dbip);

    argc--;argv++;

    bu_vls_trunc(gedp->ged_result_str, 0);

    /* redraw may operate on a specific user specified view, or on
     * all views (default) */
    struct bsg_view *cv = NULL;
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    struct bu_opt_desc vd[2];
    BU_OPT(vd[0],  "V", "view",    "name",      &bu_opt_vls, &cvls,   "specify view to draw on");
    BU_OPT_NULL(vd[1]);
    int opt_ret = bu_opt_parse(NULL, argc, argv, vd);
    argc = opt_ret;
    if (bu_vls_strlen(&cvls)) {
	cv = bsg_set_find_view(&gedp->ged_views, bu_vls_cstr(&cvls));
	if (!cv) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
    }
    bu_vls_free(&cvls);

    if (!cv) {
	struct bu_ptbl *views = bsg_set_views(&gedp->ged_views);
	if (!BU_PTBL_LEN(views)) {
	    bu_vls_printf(gedp->ged_result_str, "No views defined\n");
	    return BRLCAD_OK;
	}
    }

    int ret = BRLCAD_OK;
    if (!argc) {
	struct ged_draw_transaction txn =
	    ged_draw_transaction_make(GED_DRAW_TXN_REDRAW, NULL);
	txn.view = cv;
	if (ged_draw_apply_transaction(gedp, &txn, NULL) < 0)
	    ret = BRLCAD_ERROR;
	return ret;
    }

    for (int i = 0; i < argc; i++) {
	struct ged_draw_transaction txn =
	    ged_draw_transaction_make(GED_DRAW_TXN_REDRAW, argv[i]);
	txn.view = cv;
	if (ged_draw_apply_transaction(gedp, &txn, NULL) < 0)
	    ret = BRLCAD_ERROR;
    }

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
