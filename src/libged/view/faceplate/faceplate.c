/*                      F A C E P L A T E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/faceplate/faceplate.c
 *
 * Controls for view elements (center dot, model axes, view axes,
 * etc.) that are built into BRL-CAD views.
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

#ifndef COMMA
#  define COMMA ','
#endif

#define HELPFLAG "--print-help"
#define PURPOSEFLAG "--print-purpose"

struct _ged_fp_info {
    struct ged *gedp;
    long verbosity;
    const struct bu_cmdtab *cmds;
    struct bu_opt_desc *gopts;
};

static int
_fp_cmd_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_fp_info *gd = (struct _ged_fp_info *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gd->gedp->ged_result_str, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gd->gedp->ged_result_str, "%s\n", ps);
	return 1;
    }
    return 0;
}


int
_fp_cmd_center_dot(void *ds, int argc, const char **argv)
{
    const char *usage_string = "faceplate [options] center_dot [0|1] [color r/g/b]";
    const char *purpose_string = "Enable/disable center dot and set its color.";
    if (_fp_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_fp_info *gd = (struct _ged_fp_info *)ds;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    if (!argc) {
	if (gd->verbosity) {
	    bu_vls_printf(gedp->ged_result_str, "%d (%d/%d/%d)", v->gv_s->gv_center_dot.gos_draw,
		    v->gv_s->gv_center_dot.gos_line_color[0], v->gv_s->gv_center_dot.gos_line_color[1],
		    v->gv_s->gv_center_dot.gos_line_color[2]);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_center_dot.gos_draw);
	}
	return BRLCAD_OK;
    }

    if (argc == 1) {
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_s->gv_center_dot.gos_draw = 1;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_s->gv_center_dot.gos_draw = 0;
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "value %s is invalid - valid values are 0 or 1\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (argc > 1) {
	if (!BU_STR_EQUAL("color", argv[0])) {
	    bu_vls_printf(gedp->ged_result_str, "unknown subcommand %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	argc--; argv++;
	struct bu_color c;
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	if (bu_opt_color(&msg, argc, argv, &c) == -1) {
	    bu_vls_printf(gedp->ged_result_str, "invalid color specification\n");
	}
	int *cls = (int *)(v->gv_s->gv_center_dot.gos_line_color);
	bu_color_to_rgb_ints(&c, &cls[0], &cls[1], &cls[2]);
	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "invalid command\n");

    return BRLCAD_OK;
}

int
_fp_cmd_fps(void *ds, int argc, const char **argv)
{
    const char *usage_string = "faceplate [options] fps [0|1]";
    const char *purpose_string = "Enable/disable frames-per-second";
    if (_fp_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_fp_info *gd = (struct _ged_fp_info *)ds;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    if (!argc) {
	if (gd->verbosity) {
	    bu_vls_printf(gedp->ged_result_str, "%d (%d/%d/%d)", v->gv_s->gv_center_dot.gos_draw,
		    v->gv_s->gv_center_dot.gos_line_color[0], v->gv_s->gv_center_dot.gos_line_color[1],
		    v->gv_s->gv_center_dot.gos_line_color[2]);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_center_dot.gos_draw);
	}
	return BRLCAD_OK;
    }

    if (argc == 1) {
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_s->gv_fps = 1;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_s->gv_fps = 0;
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "value %s is invalid - valid values are 0 or 1\n", argv[0]);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "invalid command\n");

    return BRLCAD_OK;
}

int
_fp_cmd_fb(void *ds, int argc, const char **argv)
{
    const char *usage_string = "faceplate [options] fb [0|1|2]";
    const char *purpose_string = "Report/set framebuffer mode";
    if (_fp_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_fp_info *gd = (struct _ged_fp_info *)ds;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_fb_mode);
	return BRLCAD_OK;
    }

    if (argc == 1) {
	if (BU_STR_EQUAL("2", argv[0])) {
	    v->gv_s->gv_fb_mode = 2;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_s->gv_fb_mode = 1;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_s->gv_fb_mode = 0;
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "value %s is invalid - valid values are 0, 1 and 2\n", argv[0]);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "invalid command\n");

    return BRLCAD_OK;
}

int
_fp_cmd_scale(void *ds, int argc, const char **argv)
{
    const char *usage_string = "faceplate [options] scale [0|1] [color r/g/b]";
    const char *purpose_string = "Enable/disable scale and set its color.";
    if (_fp_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_fp_info *gd = (struct _ged_fp_info *)ds;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    if (!argc) {
	if (gd->verbosity) {
	    bu_vls_printf(gedp->ged_result_str, "%d (%d/%d/%d)", v->gv_s->gv_view_scale.gos_draw,
		    v->gv_s->gv_view_scale.gos_line_color[0], v->gv_s->gv_view_scale.gos_line_color[1],
		    v->gv_s->gv_view_scale.gos_line_color[2]);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_view_scale.gos_draw);
	}
	return BRLCAD_OK;
    }

    if (argc == 1) {
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_s->gv_view_scale.gos_draw = 1;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_s->gv_view_scale.gos_draw = 0;
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "value %s is invalid - valid values are 0 or 1\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (argc > 1) {
	if (!BU_STR_EQUAL("color", argv[0])) {
	    bu_vls_printf(gedp->ged_result_str, "unknown subcommand %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	argc--; argv++;
	struct bu_color c;
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	if (bu_opt_color(&msg, argc, argv, &c) == -1) {
	    bu_vls_printf(gedp->ged_result_str, "invalid color specification\n");
	}
	int *cls = (int *)(v->gv_s->gv_view_scale.gos_line_color);
	bu_color_to_rgb_ints(&c, &cls[0], &cls[1], &cls[2]);
	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "invalid command\n");

    return BRLCAD_OK;
}

int
_fp_cmd_params(void *ds, int argc, const char **argv)
{
    const char *usage_string = "faceplate [options] params [0|1] [color r/g/b]";
    const char *purpose_string = "Enable/disable params and set its color.";
    if (_fp_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_fp_info *gd = (struct _ged_fp_info *)ds;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    if (!argc) {
	if (gd->verbosity) {
	    bu_vls_printf(gedp->ged_result_str, "%d (%d/%d/%d)", v->gv_s->gv_view_params.gos_draw,
		    v->gv_s->gv_view_params.gos_text_color[0], v->gv_s->gv_view_params.gos_text_color[1],
		    v->gv_s->gv_view_params.gos_text_color[2]);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%d", v->gv_s->gv_view_params.gos_draw);
	}
	return BRLCAD_OK;
    }

    if (argc == 1) {
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_s->gv_view_params.gos_draw = 1;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_s->gv_view_params.gos_draw = 0;
	    return BRLCAD_OK;
	}
	bu_vls_printf(gedp->ged_result_str, "value %s is invalid - valid values are 0 or 1\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (argc > 1) {
	if (!BU_STR_EQUAL("color", argv[0])) {
	    bu_vls_printf(gedp->ged_result_str, "unknown subcommand %s\n", argv[0]);
	    return BRLCAD_ERROR;
	}
	argc--; argv++;
	struct bu_color c;
	struct bu_vls msg = BU_VLS_INIT_ZERO;
	if (bu_opt_color(&msg, argc, argv, &c) == -1) {
	    bu_vls_printf(gedp->ged_result_str, "invalid color specification\n");
	}
	int *cls = (int *)(v->gv_s->gv_view_params.gos_text_color);
	bu_color_to_rgb_ints(&c, &cls[0], &cls[1], &cls[2]);
	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "invalid command\n");

    return BRLCAD_OK;
}

const struct bu_cmdtab _fp_cmds[] = {
    { "center_dot",      _fp_cmd_center_dot},
    { "grid",            _fp_cmd_grid},
    { "irect",           _fp_cmd_irect},
    { "fb",              _fp_cmd_fb},
    { "fps",             _fp_cmd_fps},
    { "model_axes",      _fp_cmd_model_axes},
    { "params",          _fp_cmd_params},
    { "scale",           _fp_cmd_scale},
    { "view_axes",       _fp_cmd_view_axes},
    { (char *)NULL,      NULL}
};

int
ged_faceplate_core(struct ged *gedp, int argc, const char *argv[])
{
    int help = 0;
    struct _ged_fp_info gd;
    gd.gedp = gedp;
    gd.cmds = _fp_cmds;
    gd.verbosity = 0;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return BRLCAD_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    // We know we're the faceplate command - start processing args
    argc--; argv++;

    // See if we have any high level options set
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",    "",  NULL,               &help,         "Print help");
    BU_OPT(d[1], "v", "verbose", "",  &bu_opt_incr_long,  &gd.verbosity, "Verbose output");
    BU_OPT_NULL(d[2]);

    gd.gopts = d;

    int ac = bu_opt_parse(NULL, argc, argv, d);

    if (!ac || help) {
	_ged_subcmd_help(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_fp_cmds,
	       	"view faceplate", "[options] subcommand [args]", &gd, 0, NULL);
	return BRLCAD_OK;
    }

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no current view set");
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd(_fp_cmds, ac, argv, 0, (void *)&gd, &ret) == BRLCAD_OK) {
	return ret;
    } else {
	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    }

    return BRLCAD_ERROR;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
