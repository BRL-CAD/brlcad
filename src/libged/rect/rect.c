/*                          R E C T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libged/rect.c
 *
 * Rubber band rectangle.
 *
 */

#include "common.h"

#include <string.h>
#include <math.h>


#include "vmath.h"

#include "../ged_private.h"


static void
usage(struct ged *gedp, const char *argv0)
{
    bu_vls_printf(gedp->ged_result_str, "Usage: %s\n", argv0);
    bu_vls_printf(gedp->ged_result_str, " rect vname bg [r g b]		set or get the background color\n");
    bu_vls_printf(gedp->ged_result_str, " rect vname color [r g b]	set or get the color\n");
    bu_vls_printf(gedp->ged_result_str, " rect vname cdim w h		set or get the canvas dimension\n");
    bu_vls_printf(gedp->ged_result_str, " rect vname dim	w h		set or get the rectangle dimension\n");
    bu_vls_printf(gedp->ged_result_str, " rect vname draw [0|1]		set or get the draw parameter\n");
    bu_vls_printf(gedp->ged_result_str, " rect vname help		prints this help message\n");
    bu_vls_printf(gedp->ged_result_str, " rect vname lstyle [0|1]	set or get the line style, 0 - solid, 1 - dashed\n");
    bu_vls_printf(gedp->ged_result_str, " rect vname lwidth w		set or get the line width\n");
    bu_vls_printf(gedp->ged_result_str, " rect vname pos	x y		set or get the rectangle position\n");
    bu_vls_printf(gedp->ged_result_str, " rect vname rt port		render the geometry within the rectangular area\n");
    bu_vls_printf(gedp->ged_result_str, " rect vname vars		print a list of all variables (i.e. var = val)\n");
    bu_vls_printf(gedp->ged_result_str, " rect vname zoom		zoom view to tangle position\n");
}


static void
rect_vls_print(struct ged *gedp)
{
    bu_vls_printf(gedp->ged_result_str, "bg = %d %d %d\n",
		  gedp->ged_gvp->gv_s->gv_rect.bg[0],
		  gedp->ged_gvp->gv_s->gv_rect.bg[1],
		  gedp->ged_gvp->gv_s->gv_rect.bg[2]);
    bu_vls_printf(gedp->ged_result_str, "cdim = %d %d\n",
		  gedp->ged_gvp->gv_s->gv_rect.cdim[X],
		  gedp->ged_gvp->gv_s->gv_rect.cdim[Y]);
    bu_vls_printf(gedp->ged_result_str, "color = %d %d %d\n",
		  gedp->ged_gvp->gv_s->gv_rect.color[0],
		  gedp->ged_gvp->gv_s->gv_rect.color[1],
		  gedp->ged_gvp->gv_s->gv_rect.color[2]);
    bu_vls_printf(gedp->ged_result_str, "dim = %d %d\n",
		  gedp->ged_gvp->gv_s->gv_rect.dim[X],
		  gedp->ged_gvp->gv_s->gv_rect.dim[Y]);
    bu_vls_printf(gedp->ged_result_str, "draw = %d\n", gedp->ged_gvp->gv_s->gv_rect.draw);
    bu_vls_printf(gedp->ged_result_str, "lstyle = %d\n", gedp->ged_gvp->gv_s->gv_rect.line_style);
    bu_vls_printf(gedp->ged_result_str, "lwidth = %d\n", gedp->ged_gvp->gv_s->gv_rect.line_width);
    bu_vls_printf(gedp->ged_result_str, "pos = %d %d\n",
		  gedp->ged_gvp->gv_s->gv_rect.pos[X],
		  gedp->ged_gvp->gv_s->gv_rect.pos[Y]);
}


/*
 * Given position and dimensions in image coordinates, calculate
 * position and dimensions in normalized view coordinates.
 */
static void
rect_image2view(struct bv_interactive_rect_state *grsp)
{
    grsp->x = (grsp->pos[X] / (fastf_t)grsp->cdim[X] - 0.5) * 2.0;
    grsp->y = ((0.5 - (grsp->cdim[Y] - grsp->pos[Y]) / (fastf_t)grsp->cdim[Y]) / grsp->aspect * 2.0);
    grsp->width = grsp->dim[X] * 2.0 / (fastf_t)grsp->cdim[X];
    grsp->height = grsp->dim[Y] * 2.0 / (fastf_t)grsp->cdim[X];
}


/*
 * Adjust the rubber band rectangle to have the same aspect ratio as the window.
 */
static void
rect_adjust_for_zoom(struct bv_interactive_rect_state *grsp)
{
    fastf_t width, height;

    if (grsp->width >= 0.0)
	width = grsp->width;
    else
	width = -grsp->width;

    if (grsp->height >= 0.0)
	height = grsp->height;
    else
	height = -grsp->height;

    if (width >= height) {
	if (grsp->height >= 0.0)
	    grsp->height = width / grsp->aspect;
	else
	    grsp->height = -width / grsp->aspect;
    } else {
	if (grsp->width >= 0.0)
	    grsp->width = height * grsp->aspect;
	else
	    grsp->width = -height * grsp->aspect;
    }
}


static int
rect_rt(struct ged *gedp, int port)
{
    int xmin, xmax;
    int ymin, ymax;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);

    /* initialize result in case we need to report something here */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (ZERO(gedp->ged_gvp->gv_s->gv_rect.width) &&
	ZERO(gedp->ged_gvp->gv_s->gv_rect.height))
	return BRLCAD_OK;

    if (port < 0) {
	bu_vls_printf(gedp->ged_result_str, "rect_rt: invalid port number - %d\n", port);
	return BRLCAD_ERROR;
    }

    xmin = gedp->ged_gvp->gv_s->gv_rect.pos[X];
    ymin = gedp->ged_gvp->gv_s->gv_rect.pos[Y];

    if (gedp->ged_gvp->gv_s->gv_rect.dim[X] >= 0) {
	xmax = xmin + gedp->ged_gvp->gv_s->gv_rect.dim[X];
    } else {
	xmax = xmin;
	xmin += gedp->ged_gvp->gv_s->gv_rect.dim[X];
    }

    if (gedp->ged_gvp->gv_s->gv_rect.dim[Y] >= 0) {
	ymax = ymin + gedp->ged_gvp->gv_s->gv_rect.dim[Y];
    } else {
	ymax = ymin;
	ymin += gedp->ged_gvp->gv_s->gv_rect.dim[Y];
    }

    {
	int ret;
	struct bu_vls wvls = BU_VLS_INIT_ZERO;
	struct bu_vls nvls = BU_VLS_INIT_ZERO;
	struct bu_vls vvls = BU_VLS_INIT_ZERO;
	struct bu_vls fvls = BU_VLS_INIT_ZERO;
	struct bu_vls jvls = BU_VLS_INIT_ZERO;
	struct bu_vls cvls = BU_VLS_INIT_ZERO;
	char *av[14];

	bu_vls_printf(&wvls, "%d", gedp->ged_gvp->gv_s->gv_rect.cdim[X]);
	bu_vls_printf(&nvls, "%d", gedp->ged_gvp->gv_s->gv_rect.cdim[Y]);
	bu_vls_printf(&vvls, "%lf", gedp->ged_gvp->gv_s->gv_rect.aspect);
	bu_vls_printf(&fvls, "%d", port);
	bu_vls_printf(&jvls, "%d, %d, %d, %d", xmin, ymin, xmax, ymax);
	bu_vls_printf(&cvls, "%d/%d/%d",
		      gedp->ged_gvp->gv_s->gv_rect.bg[0],
		      gedp->ged_gvp->gv_s->gv_rect.bg[1],
		      gedp->ged_gvp->gv_s->gv_rect.bg[2]);

	av[0] = "rt";
	av[1] = "-w";
	av[2] = bu_vls_addr(&wvls);
	av[3] = "-n";
	av[4] = bu_vls_addr(&nvls);
	av[5] = "-V";
	av[6] = bu_vls_addr(&vvls);
	av[7] = "-F";
	av[8] = bu_vls_addr(&fvls);
	av[9] = "-j";
	av[10] = bu_vls_addr(&jvls);
	av[11] = "-C";
	av[12] = bu_vls_addr(&cvls);
	av[13] = (char *)0;

	ret = ged_rt(gedp, 13, (const char **)av);

	bu_vls_free(&wvls);
	bu_vls_free(&nvls);
	bu_vls_free(&vvls);
	bu_vls_free(&fvls);
	bu_vls_free(&jvls);
	bu_vls_free(&cvls);

	return ret;
    }
}


static int
rect_zoom(struct ged *gedp)
{
    fastf_t width, height;
    fastf_t sf;
    point_t old_model_center;
    point_t new_model_center;
    point_t old_view_center;
    point_t new_view_center;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (ZERO(gedp->ged_gvp->gv_s->gv_rect.width) &&
	ZERO(gedp->ged_gvp->gv_s->gv_rect.height))
	return BRLCAD_OK;

    rect_adjust_for_zoom(&gedp->ged_gvp->gv_s->gv_rect);

    /* find old view center */
    MAT_DELTAS_GET_NEG(old_model_center, gedp->ged_gvp->gv_center);
    MAT4X3PNT(old_view_center, gedp->ged_gvp->gv_model2view, old_model_center);

    /* calculate new view center */
    VSET(new_view_center,
	 gedp->ged_gvp->gv_s->gv_rect.x + gedp->ged_gvp->gv_s->gv_rect.width / 2.0,
	 gedp->ged_gvp->gv_s->gv_rect.y + gedp->ged_gvp->gv_s->gv_rect.height / 2.0,
	 old_view_center[Z]);

    /* find new model center */
    MAT4X3PNT(new_model_center, gedp->ged_gvp->gv_view2model, new_view_center);

    /* zoom in to fill rectangle */
    if (gedp->ged_gvp->gv_s->gv_rect.width >= 0.0)
	width = gedp->ged_gvp->gv_s->gv_rect.width;
    else
	width = -gedp->ged_gvp->gv_s->gv_rect.width;

    if (gedp->ged_gvp->gv_s->gv_rect.height >= 0.0)
	height = gedp->ged_gvp->gv_s->gv_rect.height;
    else
	height = -gedp->ged_gvp->gv_s->gv_rect.height;

    if (width >= height)
	sf = width / 2.0;
    else
	sf = height / 2.0 * gedp->ged_gvp->gv_s->gv_rect.aspect;

    if (sf <= SMALL_FASTF || INFINITY < sf)
	return BRLCAD_OK;

    /* set the new model center */
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, new_model_center);
    gedp->ged_gvp->gv_scale *= sf;
    bv_update(gedp->ged_gvp);

    return BRLCAD_OK;
}


/*
 * Note - this needs to be rewritten to accept keyword/value pairs so
 * that multiple attributes can be set with a single command call.
 */
int
ged_rect_core(struct ged *gedp,
	 int argc,
	 const char *argv[])
{
    char *command;
    char *parameter;
    char **argp = (char **)argv;
    point_t user_pt;		/* Value(s) provided by user */
    int i;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 2 || 5 < argc) {
	usage(gedp, argv[0]);
	return BRLCAD_ERROR;
    }

#if 0
    // Archer has at least one selection mode (Modes->Comp Select Mode->List)
    // that uses rect - uncomment this to follow along with the commands it
    // issues as the GUI works.
    struct bu_vls rcmd = BU_VLS_INIT_ZERO;
    for (i = 0; i < argc; i++) {
	bu_vls_printf(&rcmd, "%s " , argv[i]);
    }
    bu_log("%s\n", bu_vls_cstr(&rcmd));
    bu_vls_free(&rcmd);
#endif

    command = (char *)argv[0];
    parameter = (char *)argv[1];
    argc -= 2;
    argp += 2;

    for (i = 0; i < argc; ++i) {
	double scan;

	if (sscanf(argp[i], "%lf", &scan) != 1) {
	    usage(gedp, argv[0]);
	    return BRLCAD_ERROR;
	}

	user_pt[i] = scan;
    }

    if (BU_STR_EQUAL(parameter, "draw")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_rect.draw);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_s->gv_rect.draw = 1;
	    else
		gedp->ged_gvp->gv_s->gv_rect.draw = 0;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s draw' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "cdim")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d",
			  gedp->ged_gvp->gv_s->gv_rect.cdim[X],
			  gedp->ged_gvp->gv_s->gv_rect.cdim[Y]);
	    return BRLCAD_OK;
	} else if (argc == 2) {
	    gedp->ged_gvp->gv_s->gv_rect.cdim[X] = user_pt[X];
	    gedp->ged_gvp->gv_s->gv_rect.cdim[Y] = user_pt[Y];
	    gedp->ged_gvp->gv_s->gv_rect.aspect = (fastf_t)gedp->ged_gvp->gv_s->gv_rect.cdim[X] / gedp->ged_gvp->gv_s->gv_rect.cdim[Y];

	    rect_image2view(&gedp->ged_gvp->gv_s->gv_rect);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s cdim' command requires 0 or 2 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dim")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d",
			  gedp->ged_gvp->gv_s->gv_rect.dim[X],
			  gedp->ged_gvp->gv_s->gv_rect.dim[Y]);
	    return BRLCAD_OK;
	} else if (argc == 2) {
	    gedp->ged_gvp->gv_s->gv_rect.dim[X] = user_pt[X];
	    gedp->ged_gvp->gv_s->gv_rect.dim[Y] = user_pt[Y];

	    rect_image2view(&gedp->ged_gvp->gv_s->gv_rect);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s dim' command requires 0 or 2 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "pos")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d",
			  gedp->ged_gvp->gv_s->gv_rect.pos[X],
			  gedp->ged_gvp->gv_s->gv_rect.pos[Y]);
	    return BRLCAD_OK;
	} else if (argc == 2) {
	    gedp->ged_gvp->gv_s->gv_rect.pos[X] = user_pt[X];
	    gedp->ged_gvp->gv_s->gv_rect.pos[Y] = user_pt[Y];

	    rect_image2view(&gedp->ged_gvp->gv_s->gv_rect);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s pos' command requires 0 or 2 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "bg")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  gedp->ged_gvp->gv_s->gv_rect.bg[X],
			  gedp->ged_gvp->gv_s->gv_rect.bg[Y],
			  gedp->ged_gvp->gv_s->gv_rect.bg[Z]);
	    return BRLCAD_OK;
	} else if (argc == 3) {
	    gedp->ged_gvp->gv_s->gv_rect.bg[0] = (int)user_pt[X];
	    gedp->ged_gvp->gv_s->gv_rect.bg[1] = (int)user_pt[Y];
	    gedp->ged_gvp->gv_s->gv_rect.bg[2] = (int)user_pt[Z];

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s bg' command requires 0 or 3 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "color")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d %d %d",
			  gedp->ged_gvp->gv_s->gv_rect.color[X],
			  gedp->ged_gvp->gv_s->gv_rect.color[Y],
			  gedp->ged_gvp->gv_s->gv_rect.color[Z]);
	    return BRLCAD_OK;
	} else if (argc == 3) {
	    gedp->ged_gvp->gv_s->gv_rect.color[0] = (int)user_pt[X];
	    gedp->ged_gvp->gv_s->gv_rect.color[1] = (int)user_pt[Y];
	    gedp->ged_gvp->gv_s->gv_rect.color[2] = (int)user_pt[Z];

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s color' command requires 0 or 3 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "lstyle")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_rect.line_style);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i <= 0)
		gedp->ged_gvp->gv_s->gv_rect.line_style = 0;
	    else
		gedp->ged_gvp->gv_s->gv_rect.line_style = 1;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s lstyle' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "lwidth")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_rect.line_width);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i <= 0)
		gedp->ged_gvp->gv_s->gv_rect.line_width = 0;
	    else
		gedp->ged_gvp->gv_s->gv_rect.line_width = i;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s lwidth' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "rt")) {
	if (argc == 1)
	    return rect_rt(gedp, (int)user_pt[X]);

	bu_vls_printf(gedp->ged_result_str, "The '%s rt' command accepts 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "zoom")) {
	if (argc == 0)
	    return rect_zoom(gedp);

	bu_vls_printf(gedp->ged_result_str, "The '%s zoom' command accepts no arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "vars")) {
	rect_vls_print(gedp);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(parameter, "help")) {
	usage(gedp, command);
	return BRLCAD_HELP;
    }

    bu_vls_printf(gedp->ged_result_str, "%s: unrecognized command '%s'\n", command, parameter);
    usage(gedp, command);

    return BRLCAD_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl rect_cmd_impl = {
    "rect",
    ged_rect_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd rect_cmd = { &rect_cmd_impl };
const struct ged_cmd *rect_cmds[] = { &rect_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  rect_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
