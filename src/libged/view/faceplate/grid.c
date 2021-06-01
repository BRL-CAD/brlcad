/*                        G R I D . C
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
/** @file libged/view/faceplate/grid.c
 *
 * Commands for HUD grid overlay
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

#include "../../ged_private.h"
#include "../ged_view.h"
#include "./faceplate.h"

struct _ged_fp_grid_info {
    struct _ged_view_info *gd;
    struct bv_grid_state *g;
};

int
_fp_grid_cmd_draw(void *bs, int argc, const char **argv)
{

    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate grid draw [0|1]";
    const char *purpose_string = "enable/disable grid drawing";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", g->draw);
	return GED_OK;
    }

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    int val;
    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&val) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }

    val = (val) ? 1 : 0;

    g->draw = val;

    return GED_OK;
}

int
_fp_grid_cmd_snap(void *bs, int argc, const char **argv)
{

    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate grid snap [0|1]";
    const char *purpose_string = "enable/disable grid snapping";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", g->snap);
	return GED_OK;
    }

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    int val;
    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&val) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }

    val = (val) ? 1 : 0;

    g->snap = val;

    return GED_OK;
}

int
_fp_grid_cmd_anchor(void *bs, int argc, const char **argv)
{
    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate grid anchor [# # #]";
    const char *purpose_string = "adjust grid size";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
     if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%g %g %g\n", V3ARGS(g->anchor));
	return GED_OK;
    }

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    vect_t val;
    int ret = bu_opt_vect_t(NULL, argc, (const char **)&argv[0], (void *)&val);
    if (ret != 1 && ret != 3) {
	bu_vls_printf(gedp->ged_result_str, "Invalid specification\n");
	return GED_ERROR;
    }

    VMOVE(g->anchor, val);

    return GED_OK;
}

int
_fp_grid_cmd_res_h(void *bs, int argc, const char **argv)
{

    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate grid res_ [#]";
    const char *purpose_string = "adjust grid line width";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%g\n", g->res_h);
	return GED_OK;
    }

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    fastf_t val;
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[0], (void *)&val) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }

    if (val < BN_TOL_DIST) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is %f\n", BN_TOL_DIST);
	return GED_ERROR;
    }

    g->res_h = val;

    return GED_OK;
}

int
_fp_grid_cmd_res_v(void *bs, int argc, const char **argv)
{

    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate grid res_ [#]";
    const char *purpose_string = "adjust grid line width";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%g\n", g->res_v);
	return GED_OK;
    }

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    fastf_t val;
    if (bu_opt_fastf_t(NULL, 1, (const char **)&argv[0], (void *)&val) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }

    if (val < BN_TOL_DIST) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is %f\n", BN_TOL_DIST);
	return GED_ERROR;
    }

    g->res_v = val;

    return GED_OK;
}

int
_fp_grid_cmd_res_major_h(void *bs, int argc, const char **argv)
{

    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate grid res_ [#]";
    const char *purpose_string = "adjust grid line width";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", g->res_major_h);
	return GED_OK;
    }

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    int val;
    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&val) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }

    if (val < 1) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is 1\n");
	return GED_ERROR;
    }

    g->res_major_h = val;

    return GED_OK;
}

int
_fp_grid_cmd_res_major_v(void *bs, int argc, const char **argv)
{

    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate grid res_major_v [#]";
    const char *purpose_string = "adjust grid line width";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", g->res_major_v);
	return GED_OK;
    }

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }
    int val;
    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&val) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }

    if (val < 1) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is 1\n");
	return GED_ERROR;
    }

    g->res_major_v = val;

    return GED_OK;
}

int
_fp_grid_cmd_color(void *bs, int argc, const char **argv)
{

    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_grid color [r/g/b]";
    const char *purpose_string = "get/set color of grid";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d %d\n", g->color[0], g->color[1], g->color[2]);
	return GED_OK;
    }

    // For color need either 1 or 3 non-subcommand args
    if (argc != 1 && argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }

    struct bu_color c;
    int opt_ret = bu_opt_color(NULL, argc, (const char **)argv, (void *)&c);
    if (opt_ret != 1 && opt_ret != 3) {
	bu_vls_printf(gedp->ged_result_str, "Invalid color specifier\n");
	return GED_ERROR;
    }

    bu_color_to_rgb_ints(&c, &g->color[0], &g->color[1], &g->color[2]);

    return GED_OK;
}

const struct bu_cmdtab _fp_grid_cmds[] = {
    { "draw",        _fp_grid_cmd_draw},
    { "snap",        _fp_grid_cmd_snap},
    { "anchor",      _fp_grid_cmd_anchor},
    { "res_h",       _fp_grid_cmd_res_h},
    { "res_v",       _fp_grid_cmd_res_v},
    { "res_major_h", _fp_grid_cmd_res_major_h},
    { "res_major_v", _fp_grid_cmd_res_major_v},
    { "color",       _fp_grid_cmd_color},
    { (char *)NULL,      NULL}
};

int
_fp_cmd_grid(void *bs, int argc, const char **argv)
{
    int help = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    const char *usage_string = "view faceplate grid subcmd [args]";
    const char *purpose_string = "manipulate faceplate grid overlay";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return GED_ERROR;
    }

    // We know we're the grid command - start processing args
    argc--; argv++;

    if (argc == 1) {
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_s->gv_grid.draw = 1;
	    return GED_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_s->gv_grid.draw = 0;
	    return GED_OK;
	}
    }

    // See if we have any high level options set
    struct bu_opt_desc d[2];
    BU_OPT(d[0], "h", "help",  "",  NULL,  &help,      "Print help");
    BU_OPT_NULL(d[1]);

    gd->gopts = d;

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_fp_grid_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    (void)bu_opt_parse(NULL, acnt, argv, d);

    struct _ged_fp_grid_info ginfo;
    ginfo.gd = gd;
    ginfo.g = &v->gv_s->gv_grid;

    return _ged_subcmd_exec(gedp, d, _fp_grid_cmds, "view faceplate grid", "[options] subcommand [args]", (void *)&ginfo, argc, argv, help, cmd_pos);
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
