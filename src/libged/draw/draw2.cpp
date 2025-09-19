/*                         D R A W 2 . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
#include "bu/opt.h"
#include "bu/sort.h"
#include "nmg.h"
#include "rt/view.h"

#include "ged/view.h"
#include "../ged_private.h"
#include "../dbi.h"

static int
draw_opt_color(struct bu_vls *msg, size_t argc, const char **argv, void *data)
{
    struct bv_obj_settings *vs = (struct bv_obj_settings *)data;
    struct bu_color c;
    int ret = bu_opt_color(msg, argc, argv, (void *)&c);
    if (ret == 1 || ret == 3) {
	vs->color_override = 1;
	bu_color_to_rgb_chars(&c, vs->color);
    }
    return ret;
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

    /* User settings may override various options - set up to collect them.
     * Option defaults may be overridden for the purposes of the current draw
     * command by command line options. */
    struct bv_obj_settings vs = BV_OBJ_SETTINGS_INIT;
    struct bu_vls cvls = BU_VLS_INIT_ZERO;

    int drawing_modes[6] = {-1, 0, 0, 0, 0, 0};
    struct bu_opt_desc d[18];
    BU_OPT(d[0],  "h", "help",          "",                 NULL, &print_help,         "Print help and exit");
    BU_OPT(d[1],  "?", "",              "",                 NULL, &print_help,         "");
    BU_OPT(d[2],  "m", "mode",         "#",          &bu_opt_int, &drawing_modes[0],  "0=wireframe;1=shaded bots;2=shaded;3=evaluated");
    BU_OPT(d[3],   "", "wireframe",     "",                 NULL, &drawing_modes[1],  "Draw using only wireframes (mode = 0)");
    BU_OPT(d[4],   "", "shaded",        "",                 NULL, &drawing_modes[2],  "Shade bots, breps and polysolids (mode = 1)");
    BU_OPT(d[5],   "", "shaded-all",    "",                 NULL, &drawing_modes[3],  "Shade all solids, not evaluated (mode = 2)");
    BU_OPT(d[6],  "E", "evaluate",      "",                 NULL, &drawing_modes[4],  "Wireframe with evaluate booleans (mode = 3)");
    BU_OPT(d[7],   "", "hidden-line",   "",                 NULL, &drawing_modes[5],  "Hidden line wireframes");
    BU_OPT(d[8],  "A", "add-mode",      "",                 NULL, &vs.mixed_modes,    "Don't erase other drawn modes for specified paths (allows simultaneous shaded and wireframe drawing for the same object)");
    BU_OPT(d[9],  "t", "transparency", "#",      &bu_opt_fastf_t, &vs.transparency,   "Set transparency level in drawing: range 0 (clear) to 1 (opaque)");
    BU_OPT(d[10], "x", "",             "#",      &bu_opt_fastf_t, &vs.transparency,   "");
    BU_OPT(d[11], "L", "",             "#",          &bu_opt_int, &bot_threshold,     "Set face count level for drawing bounding boxes instead of BoT triangles (NOTE: passing this updates the global view setting - bot_threshold is a view property).");
    BU_OPT(d[12], "S", "no-subtract",   "",                 NULL, &vs.draw_non_subtract_only,  "Do not draw subtraction solids");
    BU_OPT(d[13],  "", "no-dash",       "",                 NULL, &vs.draw_solid_lines_only,  "Use solid lines rather than dashed for subtraction solids");
    BU_OPT(d[14], "C", "color",         "r/g/b", &draw_opt_color, &vs,                "Override object colors");
    BU_OPT(d[15],  "", "line-width",   "#",          &bu_opt_int, &vs.s_line_width,   "Override default line width");
    BU_OPT(d[16], "R", "no-autoview",   "",                 NULL, &no_autoview,       "Do not calculate automatic view, even if initial scene is empty.");
    BU_OPT(d[17], "V", "view",    "name",            &bu_opt_vls, &cvls,              "Specify specific view to add paths to (instantiates objects only in that view)");
    BU_OPT_NULL(d[17]);

    /* If no args, must be wanting help */
    if (!argc) {
	_ged_cmd_help(gedp, usage, d);
	return BRLCAD_OK;
    }

    /* Process command line args into vs with bu_opt */
    struct bu_vls omsg = BU_VLS_INIT_ZERO;
    int opt_ret = bu_opt_parse(&omsg, argc, argv, d);
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

    // If we have a specified view, make sure it matches one we know about
    BViewState *svs = NULL;
    if (bu_vls_strlen(&cvls)) {
	std::string vname(bu_vls_cstr(&cvls));
	std::vector<BViewState *> svsv = gedp->dbi_state->FindBViewState(vname.c_str());
	if (!svsv.size()) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
	if (svsv.size() > 1) {
	    bu_vls_printf(gedp->ged_result_str, "TODO - More than one view matches %s, need to support that...\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
	svs = svsv[0];
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
	vs.s_dmode = drawing_modes[0];
    }

    // Before we start doing anything with the object set, record if any views are
    // starting out empty.
    gedp->dbi_state->FlagEmpty();

    // If we have a specified view, add JUST to that view.  Otherwise, we're after
    // the default view.
    BViewState *wv = (svs) ? svs : gedp->dbi_state->GetBViewState(NULL);
    for (int i = 0; i < argc; i++)
	wv->AddPath(argv[i], vs.s_dmode, &vs);

    // Path added - one or more views now need to be updated.  Unless we've been
    // told not to, trigger autoview on any previously empty views.
    gedp->dbi_state->Redraw(wv, !no_autoview);
    gedp->dbi_state->Render(wv);

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

    /* Redraw may operate on a specific user specified view, or on
     * all views (default) */
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    struct bu_opt_desc vd[2];
    BU_OPT(vd[0],  "V", "view",    "name",      &bu_opt_vls, &cvls,   "specify view to draw on");
    BU_OPT_NULL(vd[1]);
    int opt_ret = bu_opt_parse(NULL, argc, argv, vd);
    argc = opt_ret;

    BViewState *svs = NULL;
    if (bu_vls_strlen(&cvls)) {
    	std::string vname(bu_vls_cstr(&cvls));
	std::vector<BViewState *> svsv = gedp->dbi_state->FindBViewState(vname.c_str());
	if (!svsv.size()) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
	if (svsv.size() > 1) {
	    bu_vls_printf(gedp->ged_result_str, "TODO - More than one view matches %s, need to support that...\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
	svs = svsv[0];
    }
    bu_vls_free(&cvls);

    BViewState *wv = (svs) ? svs : gedp->dbi_state->GetBViewState(NULL);
    gedp->dbi_state->Redraw(wv);
    gedp->dbi_state->Render(wv);

    return BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

