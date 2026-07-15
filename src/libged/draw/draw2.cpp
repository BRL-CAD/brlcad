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

#include <set>

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "bsocket.h"

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/sort.h"
#include "nmg.h"
#include "rt/view.h"

#include "ged/view.h"
#include "../ged_private.h"
#include "../dbi.h"

struct draw2_args {
    int help;
    int mode;
    int wireframe;
    int shaded;
    int shaded_all;
    int evaluate;
    int hidden_line;
    int add_mode;
    fastf_t transparency;
    int bot_threshold;
    int no_subtract;
    int no_dash;
    struct bu_color color;
    int line_width;
    int no_autoview;
    const char *view;
};

static int
draw2_mode_validate(struct bu_vls *msg, const char *arg)
{
    int mode = -1;

    if (!bu_cmd_integer_from_str(&mode, arg) || mode < 0 || mode > 5) {
	if (msg)
	    bu_vls_printf(msg, "draw mode must be an integer from 0 through 5");
	return -1;
    }
    return 0;
}

static const struct bu_cmd_option draw2_schema_options[] = {
    BU_CMD_FLAG("h", "help", struct draw2_args, help, "Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 0),
    {"m", "mode", "mode", "0..5",
	"0=wireframe; 1=shaded BoTs; 2=shaded; 3=evaluated; 4=hidden-line; 5=points",
	BU_CMD_VALUE_INTEGER, offsetof(struct draw2_args, mode), NULL,
	draw2_mode_validate, "ged.draw_mode", NULL, 0, 0, NULL,
	BU_CMD_ARG_REQUIRED, NULL, NULL, NULL},
    BU_CMD_FLAG(NULL, "wireframe", struct draw2_args, wireframe, "Draw using only wireframes (mode = 0)"),
    BU_CMD_FLAG(NULL, "shaded", struct draw2_args, shaded, "Shade BoTs, B-reps, and polysolids (mode = 1)"),
    BU_CMD_FLAG(NULL, "shaded-all", struct draw2_args, shaded_all, "Shade all solids, not evaluated (mode = 2)"),
    BU_CMD_FLAG("E", "evaluate", struct draw2_args, evaluate, "Wireframe with evaluated booleans (mode = 3)"),
    BU_CMD_FLAG(NULL, "hidden-line", struct draw2_args, hidden_line, "Draw hidden-line wireframes (mode = 4)"),
    BU_CMD_FLAG("A", "add-mode", struct draw2_args, add_mode, "Do not erase other modes for the specified paths"),
    BU_CMD_NUMBER("t", "transparency", struct draw2_args, transparency, "level", "Set transparency level (0=clear, 1=opaque)"),
    BU_CMD_ALIAS_SHORT("x", "transparency", 1),
    BU_CMD_INTEGER("L", NULL, struct draw2_args, bot_threshold, "count", "Draw BoT bounding boxes above this face count"),
    BU_CMD_FLAG("S", "no-subtract", struct draw2_args, no_subtract, "Do not draw subtraction solids"),
    BU_CMD_FLAG(NULL, "no-dash", struct draw2_args, no_dash, "Use solid lines for subtraction solids"),
    BU_CMD_COLOR_COMPAT("C", "color", struct draw2_args, color, "color", "Override object colors"),
    BU_CMD_INTEGER(NULL, "line-width", struct draw2_args, line_width, "pixels", "Override default line width"),
    BU_CMD_FLAG("R", "no-autoview", struct draw2_args, no_autoview, "Do not automatically fit an empty scene"),
    {"V", "view", "view", "name", "Draw on the named view",
	BU_CMD_VALUE_STRING, offsetof(struct draw2_args, view), NULL, NULL,
	"ged.view", NULL, 0, 0, NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL},
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand draw2_schema_operands[] = {
    BU_CMD_OPERAND("paths", BU_CMD_VALUE_DB_PATH, 1, BU_CMD_COUNT_UNLIMITED,
	"Database object paths to draw", "ged.db_path"),
    BU_CMD_OPERAND_NULL
};
extern "C" const struct bu_cmd_schema ged_draw_native_schema = {
    "draw", "Draw database objects", draw2_schema_options, draw2_schema_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
extern "C" const struct bu_cmd_schema ged_draw_alias_native_schema = {
    "e", "Draw database objects", draw2_schema_options, draw2_schema_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

struct redraw2_args {
    const char *view;
};
static const struct bu_cmd_option redraw2_schema_options[] = {
    BU_CMD_STRING("V", "view", struct redraw2_args, view, "name", "Redraw on the named view"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand redraw2_schema_operands[] = {
    BU_CMD_OPERAND("paths", BU_CMD_VALUE_DB_PATH, 0, BU_CMD_COUNT_UNLIMITED,
	"Displayed database object paths to redraw", "ged.db_path"),
    BU_CMD_OPERAND_NULL
};
extern "C" const struct bu_cmd_schema ged_redraw_native_schema = {
    "redraw", "Redraw displayed database objects", redraw2_schema_options,
    redraw2_schema_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static void
draw2_usage(struct ged *gedp, const char *cmd)
{
    char *option_help = bu_cmd_schema_describe(&ged_draw_native_schema);

    bu_vls_printf(gedp->ged_result_str, "Usage: %s [options] path1 [path2 ...]\n", cmd);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s", option_help);
	bu_free(option_help, "draw native option help");
    }
}

/*
 *  Main drawing command control logic
 */
extern "C" int
ged_draw2_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);

    const char *command = argc > 0 ? argv[0] : "draw";
    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    struct draw2_args args = {};
    args.mode = -1;
    args.bot_threshold = -1;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!argc) {
	draw2_usage(gedp, command);
	return BRLCAD_OK;
    }

    const int color_override = bu_cmd_schema_option_present(&ged_draw_native_schema,
	(size_t)argc, argv, "color");
    const int help_requested = bu_cmd_schema_option_present(&ged_draw_native_schema,
	(size_t)argc, argv, "help");
    struct bu_vls omsg = BU_VLS_INIT_ZERO;
    int operand_index = help_requested ?
	bu_cmd_schema_parse(&ged_draw_native_schema, &args, &omsg, argc, argv) :
	bu_cmd_schema_parse_complete(&ged_draw_native_schema, &args, &omsg, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "option parsing error: %s\n", bu_vls_cstr(&omsg));
	bu_vls_free(&omsg);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&omsg);
    if (args.help) {
	draw2_usage(gedp, command);
	return BRLCAD_OK;
    }
    const int path_count = argc - operand_index;
    const char **path_argv = argv + operand_index;

    /* Draw may operate on a specific user specified view.  If it does so,
     * we want the default settings to reflect those set in that particular
     * view. */
    struct bview *cv = gedp->ged_gvp;

    if (args.view) {
	cv = bv_set_find_view(&gedp->ged_views, args.view);
	if (!cv) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", args.view);
	    return BRLCAD_ERROR;
	}

	if (!cv->independent) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s is not an independent view, and as such does not support specifying db objects for display in only this view.  To change the view's status, he command 'view independent %s 1' may be applied.\n", args.view, args.view);
	    return BRLCAD_ERROR;
	}
    }

    // If we don't have a specified view, and the default view isn't a shared view, see if
    // we can find a shared view in the view set.
    if (!args.view && (!cv || cv->independent)) {
	struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
	for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	    struct bview *bv = (struct bview *)BU_PTBL_GET(views, i);
	    if (!bv->independent) {
		cv = bv;
		break;
	    }
	}
    }

    // We need a current view, either from gedp or from the options
    if (!cv) {
	bu_vls_printf(gedp->ged_result_str, "No view specified and no shared views found");
	return BRLCAD_ERROR;
    }

    /* Bind native command-line settings after the selected view is known. */
    struct bv_obj_settings vs = BV_OBJ_SETTINGS_INIT;
    vs.mixed_modes = args.add_mode;
    vs.transparency = args.transparency;
    vs.draw_non_subtract_only = args.no_subtract;
    vs.draw_solid_lines_only = args.no_dash;
    vs.s_line_width = args.line_width;
    if (color_override) {
	vs.color_override = 1;
	bu_color_to_rgb_chars(&args.color, vs.color);
    }

    int drawing_modes[6] = {args.mode, args.wireframe, args.shaded,
	args.shaded_all, args.evaluate, args.hidden_line};

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
    if (drawing_modes[0] < -1 || drawing_modes[0] > 5) {
	bu_vls_printf(gedp->ged_result_str, "Invalid draw mode %d (expected an integer from 0 through 5)\n", drawing_modes[0]);
	return BRLCAD_ERROR;
    }
    if (drawing_modes[0] > -1) {
	vs.s_dmode = drawing_modes[0];
    }

    // Before we start doing anything with the object set, record if things are
    // starting out empty.
    int blank_slate = 0;
    struct bu_ptbl *dobjs = bv_view_objs(cv, BV_DB_OBJS);
    struct bu_ptbl *local_dobjs = bv_view_objs(cv, BV_DB_OBJS);
    struct bu_ptbl *vobjs = bv_view_objs(cv, BV_VIEW_OBJS);
    struct bu_ptbl *vlobjs = bv_view_objs(cv, BV_VIEW_OBJS | BV_LOCAL_OBJS);
    if ((!dobjs || !BU_PTBL_LEN(dobjs)) && (!local_dobjs || !BU_PTBL_LEN(local_dobjs)) &&
	    (!vobjs || !BU_PTBL_LEN(vobjs)) && (!vlobjs || !BU_PTBL_LEN(vlobjs))) {
	blank_slate = 1;
    }

    // Drawing can get complicated when we have multiple active views with
    // different settings. The simplest case is when the current or specified
    // view is an independent view - we just update it and return.
    if (cv->independent) {
	DbiState *dbis = (DbiState *)gedp->dbi_state;
	BViewState *bvs = dbis->get_view_state(cv);
	for (int i = 0; i < path_count; ++i)
	    bvs->add_path(path_argv[i]);
	std::unordered_set<struct bview *> vset;
	vset.insert(cv);
	bvs->redraw(&vs, vset, !(blank_slate && !args.no_autoview));
	return BRLCAD_OK;
    }

    // If we have multiple views, we have to handle each view.  Most of the
    // time the work will be done in the first pass (when objects do not have
    // view specific geometry to generate) but this is not true when adaptive
    // plotting is enabled.
    std::unordered_map<BViewState *, std::unordered_set<struct bview *>> vmap;
    struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
	if (v->independent)
	    continue;
	DbiState *dbis = (DbiState *)gedp->dbi_state;
	BViewState *bvs = dbis->get_view_state(cv);
	if (!bvs)
	    continue;
	vmap[bvs].insert(v);
    }
    std::unordered_map<BViewState *, std::unordered_set<struct bview *>>::iterator bv_it;
    for (bv_it = vmap.begin(); bv_it != vmap.end(); bv_it++) {
	for (int i = 0; i < path_count; ++i)
	    bv_it->first->add_path(path_argv[i]);
	bv_it->first->redraw(&vs, bv_it->second, !(blank_slate && !args.no_autoview));
    }

    return BRLCAD_OK;
}

static int
_ged_redraw_view(struct ged *gedp, struct bview *v, int argc, const char **argv)
{
    if (!gedp || !gedp->dbi_state || !v)
	return BRLCAD_ERROR;
    std::unordered_set<struct bview *> vset;
    DbiState *dbis = (DbiState *)gedp->dbi_state;
    BViewState *bvs = dbis->get_view_state(v);
    if (!bvs)
	return BRLCAD_ERROR;
    bvs->refresh(v, argc, argv);
    return BRLCAD_OK;
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
    struct bview *cv = NULL;
    struct redraw2_args args = {};
    struct bu_vls omsg = BU_VLS_INIT_ZERO;
    int operand_index = bu_cmd_schema_parse_complete(&ged_redraw_native_schema, &args,
	&omsg, argc, argv);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "option parsing error: %s\n", bu_vls_cstr(&omsg));
	bu_vls_free(&omsg);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&omsg);
    const int path_count = argc - operand_index;
    const char **path_argv = argv + operand_index;
    if (args.view) {
	cv = bv_set_find_view(&gedp->ged_views, args.view);
	if (!cv) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", args.view);
	    return BRLCAD_ERROR;
	}
    }

    int ret = BRLCAD_OK;
    if (cv) {
	return _ged_redraw_view(gedp, cv, path_count, path_argv);
    } else {
	struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
	if (!BU_PTBL_LEN(views)) {
	    bu_vls_printf(gedp->ged_result_str, "No views defined\n");
	    return BRLCAD_OK;
	}
	for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	    struct bview *v = (struct bview *)BU_PTBL_GET(views, i);
	    if (!v) {
		bu_log("WARNING, draw2.cpp:%d - null view stored in ged_views index %zu, skipping\n", __LINE__, i);
		continue;
	    }
	int nret = _ged_redraw_view(gedp, v, path_count, path_argv);
	    if (nret != BRLCAD_OK)
		ret = nret;
	}
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
