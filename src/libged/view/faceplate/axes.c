/*                        A X E S . C
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
/** @file libged/view/faceplate/axes.c
 *
 * Commands for HUD axes
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

struct _ged_fp_axes_info {
    struct _ged_view_info *gd;
    struct bv_axes *a;
};

int
_fp_axes_cmd_size(void *bs, int argc, const char **argv)
{
    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes size [#]";
    const char *purpose_string = "adjust axes size";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
     if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%f\n", a->axes_size);
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

    a->axes_size = val;

    return GED_OK;
}

int
_fp_axes_cmd_linewidth(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes linewidth [#]";
    const char *purpose_string = "adjust axes line width";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->line_width);
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

    a->line_width = val;

    return GED_OK;
}

int
_fp_axes_cmd_pos_only(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes pos_only [0|1]";
    const char *purpose_string = "enable/disable axes decorations";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->pos_only);
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

    a->pos_only = val;

    return GED_OK;
}

int
_fp_axes_cmd_fp_axes_color(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes color [r/g/b]";
    const char *purpose_string = "get/set color of axes";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d %d\n", a->axes_color[0], a->axes_color[1], a->axes_color[2]);
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

    bu_color_to_rgb_ints(&c, &a->axes_color[0], &a->axes_color[1], &a->axes_color[2]);

    return GED_OK;
}

int
_fp_axes_cmd_label(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes label [0|1]";
    const char *purpose_string = "enable/disable text labels for axes";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->label_flag);
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

    a->label_flag = val;

    return GED_OK;
}

int
_fp_axes_cmd_label_color(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes label_color [r/g/b]";
    const char *purpose_string = "get/set color of text labels for axes";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d %d\n", a->label_color[0], a->label_color[1], a->label_color[2]);
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

    bu_color_to_rgb_ints(&c, &a->label_color[0], &a->label_color[1], &a->label_color[2]);


    return GED_OK;
}

int
_fp_axes_cmd_triple_color(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes triple_color [0|1]";
    const char *purpose_string = "enable/disable tri-color mode for axes coloring";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->triple_color);
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

    a->triple_color = val;

    return GED_OK;
}

int
_fp_axes_cmd_tick(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes tick [0|1]";
    const char *purpose_string = "enable/disable axes tick drawing";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->tick_enabled);
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

    a->tick_enabled = val;

    return GED_OK;
}

int
_fp_axes_cmd_tick_length(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes tick_length [#]";
    const char *purpose_string = "get/set tick length";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->tick_length);
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

    a->tick_length = val;

    return GED_OK;
}

int
_fp_axes_cmd_tick_major_length(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes tick_major_length [#]";
    const char *purpose_string = "get/set tick major length";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->tick_major_length);
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

    a->tick_major_length = val;

    return GED_OK;
}

int
_fp_axes_cmd_tick_interval(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes tick_interval [#]";
    const char *purpose_string = "get/set tick interval";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%f\n", a->tick_interval);
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

    a->tick_interval = val;

    return GED_OK;
}

int
_fp_axes_cmd_ticks_per_major(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes ticks_per_major [#]";
    const char *purpose_string = "get/set ticks per major";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->ticks_per_major);
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

    if (val < 0) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is 0\n");
	return GED_ERROR;
    }

    a->ticks_per_major = val;

    return GED_OK;
}

int
_fp_axes_cmd_tick_threshold(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes tick_threshold [#]";
    const char *purpose_string = "get/set tick threshold";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->tick_threshold);
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

    if (val < 0) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is 0\n");
	return GED_ERROR;
    }

    a->tick_threshold = val;

    return GED_OK;
}

int
_fp_axes_cmd_tick_color(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes tick_color [r/g/b]";
    const char *purpose_string = "get/set color of ticks";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d %d\n", a->tick_color[0], a->tick_color[1], a->tick_color[2]);
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

    bu_color_to_rgb_ints(&c, &a->tick_color[0], &a->tick_color[1], &a->tick_color[2]);


    return GED_OK;
}

int
_fp_axes_cmd_tick_major_color(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    const char *usage_string = "view faceplate [model|view]_axes tick_major_color [r/g/b]";
    const char *purpose_string = "get/set tick_major_color";
    if (_view_cmd_msgs((void *)gd, argc, argv, usage_string, purpose_string))
	return GED_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d %d\n", a->tick_major_color[0], a->tick_major_color[1], a->tick_major_color[2]);
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

    bu_color_to_rgb_ints(&c, &a->tick_major_color[0], &a->tick_major_color[1], &a->tick_major_color[2]);

    return GED_OK;
}

const struct bu_cmdtab _fp_axes_cmds[] = {
    { "size",              _fp_axes_cmd_size},
    { "line_width",        _fp_axes_cmd_linewidth},
    { "pos_only",          _fp_axes_cmd_pos_only},
    { "axes_color",        _fp_axes_cmd_fp_axes_color},
    { "label",             _fp_axes_cmd_label},
    { "label_color",       _fp_axes_cmd_label_color},
    { "triple_color",      _fp_axes_cmd_triple_color},
    { "tick",              _fp_axes_cmd_tick},
    { "tick_length",       _fp_axes_cmd_tick_length},
    { "tick_major_length", _fp_axes_cmd_tick_major_length},
    { "tick_interval",     _fp_axes_cmd_tick_interval},
    { "ticks_per_major",   _fp_axes_cmd_ticks_per_major},
    { "tick_threshold",    _fp_axes_cmd_tick_threshold},
    { "tick_color",        _fp_axes_cmd_tick_color},
    { "tick_major_color",  _fp_axes_cmd_tick_major_color},
    { (char *)NULL,      NULL}
};

int
_fp_cmd_model_axes(void *bs, int argc, const char **argv)
{
    int help = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    const char *usage_string = "view faceplate model_axes subcmd [args]";
    const char *purpose_string = "manipulate view axes";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return GED_ERROR;
    }

    // We know we're the axes command - start processing args
    argc--; argv++;

    if (argc == 1) {
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_model_axes.draw = 1;
	    return GED_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_model_axes.draw = 0;
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
	if (bu_cmd_valid(_fp_axes_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    (void)bu_opt_parse(NULL, acnt, argv, d);

    struct _ged_fp_axes_info ainfo;
    ainfo.gd = gd;
    ainfo.a = &v->gv_model_axes;

    return _ged_subcmd_exec(gedp, d, _fp_axes_cmds, "view faceplate model_axes", "[options] subcommand [args]", (void *)&ainfo, argc, argv, help, cmd_pos);
}

int
_fp_cmd_view_axes(void *bs, int argc, const char **argv)
{
    int help = 0;
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    const char *usage_string = "view faceplate view_axes subcmd [args]";
    const char *purpose_string = "manipulate view axes";
    if (_view_cmd_msgs(bs, argc, argv, usage_string, purpose_string))
	return GED_OK;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return GED_ERROR;
    }


    // We know we're the axes command - start processing args
    argc--; argv++;

    if (argc == 1) {
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_view_axes.draw = 1;
	    return GED_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_view_axes.draw = 0;
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
	if (bu_cmd_valid(_fp_axes_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    (void)bu_opt_parse(NULL, acnt, argv, d);

    struct _ged_fp_axes_info ainfo;
    ainfo.gd = gd;
    ainfo.a = &v->gv_view_axes;

    return _ged_subcmd_exec(gedp, d, _fp_axes_cmds, "view faceplate view_axes", "[options] subcommand [args]", (void *)&ainfo, argc, argv, help, cmd_pos);
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
