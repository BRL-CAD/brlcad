/*                I N T E R A C T I V E _ R E C T . C
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
/** @file libged/view/faceplate/interactive_rect.c
 *
 * Commands for HUD interactive rectangle overlay
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

struct _ged_fp_irect_info {
    struct _ged_view_info *gd;
    struct bv_interactive_rect_state *r;
};

int
_fp_irect_cmd_draw(void *bs, int argc, const char **argv)
{

    struct _ged_fp_irect_info *rinfo = (struct _ged_fp_irect_info *)bs;
    struct _ged_view_info *gd = rinfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate irect draw [0|1]";
    const char *purpose_string = "enable/disable irect rubber band drawing";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_interactive_rect_state *r = rinfo->r;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", r->draw);
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

    r->draw = val;

    return GED_OK;
}

int
_fp_irect_cmd_line_width(void *bs, int argc, const char **argv)
{

    struct _ged_fp_irect_info *rinfo = (struct _ged_fp_irect_info *)bs;
    struct _ged_view_info *gd = rinfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate irect line_width [#]";
    const char *purpose_string = "irect line width";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_interactive_rect_state *r = rinfo->r;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", r->line_width);
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

    val = (val <= 0) ? 1 : val;

    r->line_width = val;

    return GED_OK;
}

int
_fp_irect_cmd_line_style(void *bs, int argc, const char **argv)
{

    struct _ged_fp_irect_info *rinfo = (struct _ged_fp_irect_info *)bs;
    struct _ged_view_info *gd = rinfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate irect line_style [#]";
    const char *purpose_string = "irect line style";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_interactive_rect_state *r = rinfo->r;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", r->line_style);
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

    val = (val <= 0) ? 1 : val;

    r->line_style = val;

    return GED_OK;
}

int
_fp_irect_cmd_pos(void *bs, int argc, const char **argv)
{
    struct _ged_fp_irect_info *rinfo = (struct _ged_fp_irect_info *)bs;
    struct _ged_view_info *gd = rinfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate irect pos [# #]";
    const char *purpose_string = "report/adjust irect position ";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_interactive_rect_state *r = rinfo->r;
     if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d\n", r->pos[0], r->pos[1]);
	return GED_OK;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }

    int pos[2];
    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&(pos[0])) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&(pos[1])) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return GED_ERROR;
    }

    V2MOVE(r->pos, pos);

    return GED_OK;
}

int
_fp_irect_cmd_dim(void *bs, int argc, const char **argv)
{
    struct _ged_fp_irect_info *rinfo = (struct _ged_fp_irect_info *)bs;
    struct _ged_view_info *gd = rinfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate irect dim [# #]";
    const char *purdime_string = "report/adjust irect size";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purdime_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_interactive_rect_state *r = rinfo->r;
     if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d\n", r->dim[0], r->dim[1]);
	return GED_OK;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }

    int dim[2];
    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&(dim[0])) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&(dim[1])) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return GED_ERROR;
    }

    V2MOVE(r->dim, dim);

    return GED_OK;
}

int
_fp_irect_cmd_x(void *bs, int argc, const char **argv)
{

    struct _ged_fp_irect_info *rinfo = (struct _ged_fp_irect_info *)bs;
    struct _ged_view_info *gd = rinfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate irect x";
    const char *purpose_string = "report irect x position in normalized (+-1.0) view coords";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_interactive_rect_state *r = rinfo->r;
    bu_vls_printf(gedp->ged_result_str, "%g\n", r->x);
    return GED_OK;
}

int
_fp_irect_cmd_y(void *bs, int argc, const char **argv)
{

    struct _ged_fp_irect_info *rinfo = (struct _ged_fp_irect_info *)bs;
    struct _ged_view_info *gd = rinfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate irect y";
    const char *purpose_string = "report irect y position in normalized (+-1.0) view coords";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_interactive_rect_state *r = rinfo->r;
    bu_vls_printf(gedp->ged_result_str, "%g\n", r->y);
    return GED_OK;
}

int
_fp_irect_cmd_width(void *bs, int argc, const char **argv)
{

    struct _ged_fp_irect_info *rinfo = (struct _ged_fp_irect_info *)bs;
    struct _ged_view_info *gd = rinfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate irect width";
    const char *purpose_string = "report irect width position in normalized view coords";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_interactive_rect_state *r = rinfo->r;
    bu_vls_printf(gedp->ged_result_str, "%g\n", r->width);
    return GED_OK;
}

int
_fp_irect_cmd_height(void *bs, int argc, const char **argv)
{

    struct _ged_fp_irect_info *rinfo = (struct _ged_fp_irect_info *)bs;
    struct _ged_view_info *gd = rinfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate irect height";
    const char *purpose_string = "report irect height position in normalized view coords";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_interactive_rect_state *r = rinfo->r;
    bu_vls_printf(gedp->ged_result_str, "%g\n", r->height);
    return GED_OK;
}

int
_fp_irect_cmd_bg(void *bs, int argc, const char **argv)
{

    struct _ged_fp_irect_info *rinfo = (struct _ged_fp_irect_info *)bs;
    struct _ged_view_info *gd = rinfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_irect bg [r/g/b]";
    const char *purpose_string = "get/set color of irect background";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_interactive_rect_state *r = rinfo->r;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d %d\n", V3ARGS(r->bg));
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

    bu_color_to_rgb_ints(&c, &r->bg[0], &r->bg[1], &r->bg[2]);

    return GED_OK;
}

int
_fp_irect_cmd_color(void *bs, int argc, const char **argv)
{

    struct _ged_fp_irect_info *rinfo = (struct _ged_fp_irect_info *)bs;
    struct _ged_view_info *gd = rinfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_irect color [r/g/b]";
    const char *purpose_string = "get/set color of irect";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_interactive_rect_state *r = rinfo->r;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d %d\n", V3ARGS(r->color));
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

    bu_color_to_rgb_ints(&c, &r->color[0], &r->color[1], &r->color[2]);

    return GED_OK;
}

// TODO - shouldn't this just be gv_width/gv_height?
int
_fp_irect_cmd_cdim(void *bs, int argc, const char **argv)
{
    struct _ged_fp_irect_info *rinfo = (struct _ged_fp_irect_info *)bs;
    struct _ged_view_info *gd = rinfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate irect cdim [# #]";
    const char *purcdime_string = "report/adjust irect canvas dimension";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purcdime_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_interactive_rect_state *r = rinfo->r;
     if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d\n", r->cdim[0], r->cdim[1]);
	return GED_OK;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", usage_string);
	return GED_ERROR;
    }

    int cdim[2];
    if (bu_opt_int(NULL, 1, (const char **)&argv[0], (void *)&(cdim[0])) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return GED_ERROR;
    }
    if (bu_opt_int(NULL, 1, (const char **)&argv[1], (void *)&(cdim[1])) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[1]);
	return GED_ERROR;
    }

    cdim[0] = (cdim[0] < 0) ? 0 : cdim[0];
    cdim[1] = (cdim[1] < 0) ? 0 : cdim[1];
    V2MOVE(r->cdim, cdim);

    return GED_OK;
}

const struct bu_cmdtab _fp_irect_cmds[] = {
    { "draw",        _fp_irect_cmd_draw},
    { "line_width",  _fp_irect_cmd_line_width},
    { "line_style",  _fp_irect_cmd_line_style},
    { "pos",         _fp_irect_cmd_pos},
    { "dim",         _fp_irect_cmd_dim},
    { "x",           _fp_irect_cmd_x},
    { "y",           _fp_irect_cmd_y},
    { "width",       _fp_irect_cmd_width},
    { "height",      _fp_irect_cmd_height},
    { "bg",          _fp_irect_cmd_bg},
    { "color",       _fp_irect_cmd_color},
    { "cdim",        _fp_irect_cmd_cdim},
    { (char *)NULL,  NULL}
};

int
_fp_cmd_irect(void *bs, int argc, const char **argv)
{
    int help = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    const char *usage_string = "view faceplate irect subcmd [args]";
    const char *purpose_string = "manipulate faceplate interactive rectangle";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return GED_ERROR;
    }

    // We know we're the irect command - start processing args
    argc--; argv++;

    if (argc == 1) {
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_s->gv_rect.draw = 1;
	    return GED_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_s->gv_rect.draw = 0;
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
	if (bu_cmd_valid(_fp_irect_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    (void)bu_opt_parse(NULL, acnt, argv, d);

    struct _ged_fp_irect_info rinfo;
    rinfo.gd = gd;
    rinfo.r = &v->gv_s->gv_rect;

    return _ged_subcmd_exec(gedp, d, _fp_irect_cmds, "view faceplate irect", "[options] subcommand [args]", (void *)&rinfo, argc, argv, help, cmd_pos);
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
