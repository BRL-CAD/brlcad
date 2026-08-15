/*                        A X E S . C
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
#include "bu/cmdschema.h"
#include "bu/color.h"
#include "bu/vls.h"
#include "bv.h"

#include "../../ged_private.h"
#include "../ged_view.h"
#include "./faceplate.h"

struct _ged_fp_axes_info {
    struct _ged_view_info *gd;
    struct bv_axes *a;
    const char *branch;
};

int
_fp_axes_cmd_size(void *bs, int argc, const char **argv)
{
    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
     if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%f\n", a->axes_size);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "size");
	return BRLCAD_ERROR;
    }
    fastf_t val;
    if (!bu_cmd_number_from_str(&val, argv[0])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    a->axes_size = val;

    return BRLCAD_OK;
}

int
_fp_axes_cmd_linewidth(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->line_width);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "line_width");
	return BRLCAD_ERROR;
    }
    int val;
    if (!bu_cmd_integer_from_str(&val, argv[0])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (val < 1) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is 1\n");
	return BRLCAD_ERROR;
    }

    a->line_width = val;

    return BRLCAD_OK;
}

int
_fp_axes_cmd_pos_only(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->pos_only);
	return BRLCAD_OK;
    }

     if (argc != 1) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "pos_only");
	return BRLCAD_ERROR;
    }

    int val;
    if (!bu_cmd_integer_from_str(&val, argv[0]) || (val != 0 && val != 1)) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    a->pos_only = val;

    return BRLCAD_OK;
}

int
_fp_axes_cmd_fp_axes_color(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d %d\n", a->axes_color[0], a->axes_color[1], a->axes_color[2]);
	return BRLCAD_OK;
    }

    // For color need either 1 or 3 non-subcommand args
    if (argc != 1 && argc != 3) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "axes_color");
	return BRLCAD_ERROR;
    }

    struct bu_color c;
    int opt_ret = bu_cmd_color_from_argv(&c, (size_t)argc, (const char * const *)argv);
    if (opt_ret != 1 && opt_ret != 3) {
	bu_vls_printf(gedp->ged_result_str, "Invalid color specifier\n");
	return BRLCAD_ERROR;
    }

    bu_color_to_rgb_ints(&c, &a->axes_color[0], &a->axes_color[1], &a->axes_color[2]);

    return BRLCAD_OK;
}

int
_fp_axes_cmd_label(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->label_flag);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "label");
	return BRLCAD_ERROR;
    }
    int val;
    if (!bu_cmd_integer_from_str(&val, argv[0]) || (val != 0 && val != 1)) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    a->label_flag = val;

    return BRLCAD_OK;
}

int
_fp_axes_cmd_label_color(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d %d\n", a->label_color[0], a->label_color[1], a->label_color[2]);
	return BRLCAD_OK;
    }

    // For color need either 1 or 3 non-subcommand args
    if (argc != 1 && argc != 3) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "label_color");
	return BRLCAD_ERROR;
    }

    struct bu_color c;
    int opt_ret = bu_cmd_color_from_argv(&c, (size_t)argc, (const char * const *)argv);
    if (opt_ret != 1 && opt_ret != 3) {
	bu_vls_printf(gedp->ged_result_str, "Invalid color specifier\n");
	return BRLCAD_ERROR;
    }

    bu_color_to_rgb_ints(&c, &a->label_color[0], &a->label_color[1], &a->label_color[2]);


    return BRLCAD_OK;
}

int
_fp_axes_cmd_triple_color(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->triple_color);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "triple_color");
	return BRLCAD_ERROR;
    }
    int val;
    if (!bu_cmd_integer_from_str(&val, argv[0]) || (val != 0 && val != 1)) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    a->triple_color = val;

    return BRLCAD_OK;
}

int
_fp_axes_cmd_tick(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->tick_enabled);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "tick");
	return BRLCAD_ERROR;
    }
    int val;
    if (!bu_cmd_integer_from_str(&val, argv[0]) || (val != 0 && val != 1)) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    a->tick_enabled = val;

    return BRLCAD_OK;
}

int
_fp_axes_cmd_tick_length(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->tick_length);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "tick_length");
	return BRLCAD_ERROR;
    }
    int val;
    if (!bu_cmd_integer_from_str(&val, argv[0])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (val < 1) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is 1\n");
	return BRLCAD_ERROR;
    }

    a->tick_length = val;

    return BRLCAD_OK;
}

int
_fp_axes_cmd_tick_major_length(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->tick_major_length);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "tick_major_length");
	return BRLCAD_ERROR;
    }
    int val;
    if (!bu_cmd_integer_from_str(&val, argv[0])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (val < 1) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is 1\n");
	return BRLCAD_ERROR;
    }

    a->tick_major_length = val;

    return BRLCAD_OK;
}

int
_fp_axes_cmd_tick_interval(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%f\n", a->tick_interval);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "tick_interval");
	return BRLCAD_ERROR;
    }
    fastf_t val;
    if (!bu_cmd_number_from_str(&val, argv[0])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    a->tick_interval = val;

    return BRLCAD_OK;
}

int
_fp_axes_cmd_ticks_per_major(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->ticks_per_major);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "ticks_per_major");
	return BRLCAD_ERROR;
    }
    int val;
    if (!bu_cmd_integer_from_str(&val, argv[0])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (val < 0) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is 0\n");
	return BRLCAD_ERROR;
    }

    a->ticks_per_major = val;

    return BRLCAD_OK;
}

int
_fp_axes_cmd_tick_threshold(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", a->tick_threshold);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "tick_threshold");
	return BRLCAD_ERROR;
    }
    int val;
    if (!bu_cmd_integer_from_str(&val, argv[0])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (val < 0) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is 0\n");
	return BRLCAD_ERROR;
    }

    a->tick_threshold = val;

    return BRLCAD_OK;
}

int
_fp_axes_cmd_tick_color(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d %d\n", a->tick_color[0], a->tick_color[1], a->tick_color[2]);
	return BRLCAD_OK;
    }

    // For color need either 1 or 3 non-subcommand args
    if (argc != 1 && argc != 3) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "tick_color");
	return BRLCAD_ERROR;
    }

    struct bu_color c;
    int opt_ret = bu_cmd_color_from_argv(&c, (size_t)argc, (const char * const *)argv);
    if (opt_ret != 1 && opt_ret != 3) {
	bu_vls_printf(gedp->ged_result_str, "Invalid color specifier\n");
	return BRLCAD_ERROR;
    }

    bu_color_to_rgb_ints(&c, &a->tick_color[0], &a->tick_color[1], &a->tick_color[2]);


    return BRLCAD_OK;
}

int
_fp_axes_cmd_tick_major_color(void *bs, int argc, const char **argv)
{

    struct _ged_fp_axes_info *ainfo = (struct _ged_fp_axes_info *)bs;
    struct _ged_view_info *gd = ainfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, ainfo->branch))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_axes *a = ainfo->a;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d %d\n", a->tick_major_color[0], a->tick_major_color[1], a->tick_major_color[2]);
	return BRLCAD_OK;
    }

    // For color need either 1 or 3 non-subcommand args
    if (argc != 1 && argc != 3) {
	_fp_cmd_schema_help(gedp, ainfo->branch, "tick_major_color");
	return BRLCAD_ERROR;
    }

    struct bu_color c;
    int opt_ret = bu_cmd_color_from_argv(&c, (size_t)argc, (const char * const *)argv);
    if (opt_ret != 1 && opt_ret != 3) {
	bu_vls_printf(gedp->ged_result_str, "Invalid color specifier\n");
	return BRLCAD_ERROR;
    }

    bu_color_to_rgb_ints(&c, &a->tick_major_color[0], &a->tick_major_color[1], &a->tick_major_color[2]);

    return BRLCAD_OK;
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
    struct ged_faceplate_subcommand_args args = {0};
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    if (_fp_cmd_schema_msgs(gedp, argc, argv, NULL))
	return BRLCAD_OK;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return BRLCAD_ERROR;
    }

    // We know we're the axes command - start processing args
    argc--; argv++;

    if (argc == 1) {
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_s->gv_model_axes.draw = 1;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_s->gv_model_axes.draw = 0;
	    return BRLCAD_OK;
	}
    }

    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    int subcommand_index = bu_cmd_schema_parse(&ged_faceplate_subcommand_schema,
	&args, &parse_msgs, argc, argv);
    if (subcommand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&parse_msgs));
	bu_vls_free(&parse_msgs);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&parse_msgs);
    argc -= subcommand_index;
    argv += subcommand_index;

    struct _ged_fp_axes_info ainfo;
    ainfo.gd = gd;
    ainfo.a = &v->gv_s->gv_model_axes;
    ainfo.branch = "model_axes";

    return _fp_subcmd_exec(gedp, &ged_faceplate_subcommand_schema,
	_fp_axes_cmds, "view faceplate model_axes", "[options] subcommand [args]",
	(void *)&ainfo, argc, argv, args.help);
}

int
_fp_cmd_view_axes(void *bs, int argc, const char **argv)
{
    struct ged_faceplate_subcommand_args args = {0};
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    if (_fp_cmd_schema_msgs(gedp, argc, argv, NULL))
	return BRLCAD_OK;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return BRLCAD_ERROR;
    }


    // We know we're the axes command - start processing args
    argc--; argv++;

    if (argc == 1) {
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_s->gv_view_axes.draw = 1;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_s->gv_view_axes.draw = 0;
	    return BRLCAD_OK;
	}
    }

    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    int subcommand_index = bu_cmd_schema_parse(&ged_faceplate_subcommand_schema,
	&args, &parse_msgs, argc, argv);
    if (subcommand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&parse_msgs));
	bu_vls_free(&parse_msgs);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&parse_msgs);
    argc -= subcommand_index;
    argv += subcommand_index;

    struct _ged_fp_axes_info ainfo;
    ainfo.gd = gd;
    ainfo.a = &v->gv_s->gv_view_axes;
    ainfo.branch = "view_axes";

    return _fp_subcmd_exec(gedp, &ged_faceplate_subcommand_schema,
	_fp_axes_cmds, "view faceplate view_axes", "[options] subcommand [args]",
	(void *)&ainfo, argc, argv, args.help);
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
