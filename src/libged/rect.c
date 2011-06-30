/*                          R E C T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "fb.h"
#include "dm.h"

#include "./ged_private.h"


static void
usage(struct ged *gedp, const char *argv0)
{
    bu_vls_printf(&gedp->ged_result_str, "Usage: %s\n", argv0);
    bu_vls_printf(&gedp->ged_result_str, " rect vname bg [r g b]		set or get the background color\n");
    bu_vls_printf(&gedp->ged_result_str, " rect vname color [r g b]	set or get the color\n");
    bu_vls_printf(&gedp->ged_result_str, " rect vname cdim w h		set or get the canvas dimension\n");
    bu_vls_printf(&gedp->ged_result_str, " rect vname dim	w h		set or get the rectangle dimension\n");
    bu_vls_printf(&gedp->ged_result_str, " rect vname draw [0|1]		set or get the draw parameter\n");
    bu_vls_printf(&gedp->ged_result_str, " rect vname help		prints this help message\n");
    bu_vls_printf(&gedp->ged_result_str, " rect vname lstyle [0|1]	set or get the line style, 0 - solid, 1 - dashed\n");
    bu_vls_printf(&gedp->ged_result_str, " rect vname lwidth w		set or get the line width\n");
    bu_vls_printf(&gedp->ged_result_str, " rect vname pos	x y		set or get the rectangle position\n");
    bu_vls_printf(&gedp->ged_result_str, " rect vname rt port		render the geometry within the rectangular area\n");
    bu_vls_printf(&gedp->ged_result_str, " rect vname vars		print a list of all variables (i.e. var = val)\n");
    bu_vls_printf(&gedp->ged_result_str, " rect vname zoom		zoom view to tangle position\n");
}


static void
rect_vls_print(struct ged *gedp)
{
    bu_vls_printf(&gedp->ged_result_str, "bg = %d %d %d\n",
		  gedp->ged_gvp->gv_rect.grs_bg[0],
		  gedp->ged_gvp->gv_rect.grs_bg[1],
		  gedp->ged_gvp->gv_rect.grs_bg[2]);
    bu_vls_printf(&gedp->ged_result_str, "cdim = %d %d\n",
		  gedp->ged_gvp->gv_rect.grs_cdim[X],
		  gedp->ged_gvp->gv_rect.grs_cdim[Y]);
    bu_vls_printf(&gedp->ged_result_str, "color = %d %d %d\n",
		  gedp->ged_gvp->gv_rect.grs_color[0],
		  gedp->ged_gvp->gv_rect.grs_color[1],
		  gedp->ged_gvp->gv_rect.grs_color[2]);
    bu_vls_printf(&gedp->ged_result_str, "dim = %d %d\n",
		  gedp->ged_gvp->gv_rect.grs_dim[X],
		  gedp->ged_gvp->gv_rect.grs_dim[Y]);
    bu_vls_printf(&gedp->ged_result_str, "draw = %d\n", gedp->ged_gvp->gv_rect.grs_draw);
    bu_vls_printf(&gedp->ged_result_str, "lstyle = %d\n", gedp->ged_gvp->gv_rect.grs_line_style);
    bu_vls_printf(&gedp->ged_result_str, "lwidth = %d\n", gedp->ged_gvp->gv_rect.grs_line_width);
    bu_vls_printf(&gedp->ged_result_str, "pos = %d %d\n",
		  gedp->ged_gvp->gv_rect.grs_pos[X],
		  gedp->ged_gvp->gv_rect.grs_pos[Y]);
}


/*
 * Given position and dimensions in image coordinates, calculate
 * position and dimensions in normalized view coordinates.
 */
static void
rect_image2view(struct ged_rect_state *grsp)
{
    grsp->grs_x = (grsp->grs_pos[X] / (fastf_t)grsp->grs_cdim[X] - 0.5) * 2.0;
    grsp->grs_y = ((0.5 - (grsp->grs_cdim[Y] - grsp->grs_pos[Y]) / (fastf_t)grsp->grs_cdim[Y]) / grsp->grs_aspect * 2.0);
    grsp->grs_width = grsp->grs_dim[X] * 2.0 / (fastf_t)grsp->grs_cdim[X];
    grsp->grs_height = grsp->grs_dim[Y] * 2.0 / (fastf_t)grsp->grs_cdim[X];
}


/*
 * Adjust the rubber band rectangle to have the same aspect ratio as the window.
 */
static void
rect_adjust_for_zoom(struct ged_rect_state *grsp)
{
    fastf_t width, height;

    if (grsp->grs_width >= 0.0)
	width = grsp->grs_width;
    else
	width = -grsp->grs_width;

    if (grsp->grs_height >= 0.0)
	height = grsp->grs_height;
    else
	height = -grsp->grs_height;

    if (width >= height) {
	if (grsp->grs_height >= 0.0)
	    grsp->grs_height = width / grsp->grs_aspect;
	else
	    grsp->grs_height = -width / grsp->grs_aspect;
    } else {
	if (grsp->grs_width >= 0.0)
	    grsp->grs_width = height * grsp->grs_aspect;
	else
	    grsp->grs_width = -height * grsp->grs_aspect;
    }
}


static int
rect_rt(struct ged *gedp, int port)
{
    int xmin, xmax;
    int ymin, ymax;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);

    /* initialize result in case we need to report something here */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    if (ZERO(gedp->ged_gvp->gv_rect.grs_width) &&
	ZERO(gedp->ged_gvp->gv_rect.grs_height))
	return GED_OK;

    if (port < 0) {
	bu_vls_printf(&gedp->ged_result_str, "rect_rt: invalid port number - %d\n", port);
	return GED_ERROR;
    }

    xmin = gedp->ged_gvp->gv_rect.grs_pos[X];
    ymin = gedp->ged_gvp->gv_rect.grs_pos[Y];

    if (gedp->ged_gvp->gv_rect.grs_dim[X] >= 0) {
	xmax = xmin + gedp->ged_gvp->gv_rect.grs_dim[X];
    } else {
	xmax = xmin;
	xmin += gedp->ged_gvp->gv_rect.grs_dim[X];
    }

    if (gedp->ged_gvp->gv_rect.grs_dim[Y] >= 0) {
	ymax = ymin + gedp->ged_gvp->gv_rect.grs_dim[Y];
    } else {
	ymax = ymin;
	ymin += gedp->ged_gvp->gv_rect.grs_dim[Y];
    }

    {
	int ret;
	struct bu_vls wvls;
	struct bu_vls nvls;
	struct bu_vls vvls;
	struct bu_vls fvls;
	struct bu_vls jvls;
	struct bu_vls cvls;
	char *av[14];

	bu_vls_init(&wvls);
	bu_vls_init(&nvls);
	bu_vls_init(&vvls);
	bu_vls_init(&fvls);
	bu_vls_init(&jvls);
	bu_vls_init(&cvls);

	bu_vls_printf(&wvls, "%d", gedp->ged_gvp->gv_rect.grs_cdim[X]);
	bu_vls_printf(&nvls, "%d", gedp->ged_gvp->gv_rect.grs_cdim[Y]);
	bu_vls_printf(&vvls, "%lf", gedp->ged_gvp->gv_rect.grs_aspect);
	bu_vls_printf(&fvls, "%d", port);
	bu_vls_printf(&jvls, "%d, %d, %d, %d", xmin, ymin, xmax, ymax);
	bu_vls_printf(&cvls, "%d/%d/%d",
		      gedp->ged_gvp->gv_rect.grs_bg[0],
		      gedp->ged_gvp->gv_rect.grs_bg[1],
		      gedp->ged_gvp->gv_rect.grs_bg[2]);

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

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    if (ZERO(gedp->ged_gvp->gv_rect.grs_width) &&
	ZERO(gedp->ged_gvp->gv_rect.grs_height))
	return GED_OK;

    rect_adjust_for_zoom(&gedp->ged_gvp->gv_rect);

    /* find old view center */
    MAT_DELTAS_GET_NEG(old_model_center, gedp->ged_gvp->gv_center);
    MAT4X3PNT(old_view_center, gedp->ged_gvp->gv_model2view, old_model_center);

    /* calculate new view center */
    VSET(new_view_center,
	 gedp->ged_gvp->gv_rect.grs_x + gedp->ged_gvp->gv_rect.grs_width / 2.0,
	 gedp->ged_gvp->gv_rect.grs_y + gedp->ged_gvp->gv_rect.grs_height / 2.0,
	 old_view_center[Z]);

    /* find new model center */
    MAT4X3PNT(new_model_center, gedp->ged_gvp->gv_view2model, new_view_center);

    /* zoom in to fill rectangle */
    if (gedp->ged_gvp->gv_rect.grs_width >= 0.0)
	width = gedp->ged_gvp->gv_rect.grs_width;
    else
	width = -gedp->ged_gvp->gv_rect.grs_width;

    if (gedp->ged_gvp->gv_rect.grs_height >= 0.0)
	height = gedp->ged_gvp->gv_rect.grs_height;
    else
	height = -gedp->ged_gvp->gv_rect.grs_height;

    if (width >= height)
	sf = width / 2.0;
    else
	sf = height / 2.0 * gedp->ged_gvp->gv_rect.grs_aspect;

    if (sf <= SMALL_FASTF || INFINITY < sf)
	return GED_OK;

    /* set the new model center */
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, new_model_center);
    gedp->ged_gvp->gv_scale *= sf;
    ged_view_update(gedp->ged_gvp);

    return GED_OK;
}


/*
 * Note - this needs to be rewritten to accept keyword/value pairs so
 * that multiple attributes can be set with a single command call.
 */
int
ged_rect(struct ged *gedp,
	 int argc,
	 const char *argv[])
{
    char *command;
    char *parameter;
    char **argp = (char **)argv;
    point_t user_pt;		/* Value(s) provided by user */
    int i;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    if (argc < 2 || 5 < argc) {
	usage(gedp, argv[0]);
	return GED_ERROR;
    }

    command = (char *)argv[0];
    parameter = (char *)argv[1];
    argc -= 2;
    argp += 2;

    for (i = 0; i < argc; ++i)
	if (sscanf(argp[i], "%lf", &user_pt[i]) != 1) {
	    usage(gedp, argv[0]);
	    return GED_ERROR;
	}

    if (BU_STR_EQUAL(parameter, "draw")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_rect.grs_draw);
	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_rect.grs_draw = 1;
	    else
		gedp->ged_gvp->gv_rect.grs_draw = 0;

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s draw' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "cdim")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d %d",
			  gedp->ged_gvp->gv_rect.grs_cdim[X],
			  gedp->ged_gvp->gv_rect.grs_cdim[Y]);
	    return GED_OK;
	} else if (argc == 2) {
	    gedp->ged_gvp->gv_rect.grs_cdim[X] = user_pt[X];
	    gedp->ged_gvp->gv_rect.grs_cdim[Y] = user_pt[Y];
	    gedp->ged_gvp->gv_rect.grs_aspect = (fastf_t)gedp->ged_gvp->gv_rect.grs_cdim[X] / gedp->ged_gvp->gv_rect.grs_cdim[Y];

	    rect_image2view(&gedp->ged_gvp->gv_rect);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s cdim' command requires 0 or 2 arguments\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dim")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d %d",
			  gedp->ged_gvp->gv_rect.grs_dim[X],
			  gedp->ged_gvp->gv_rect.grs_dim[Y]);
	    return GED_OK;
	} else if (argc == 2) {
	    gedp->ged_gvp->gv_rect.grs_dim[X] = user_pt[X];
	    gedp->ged_gvp->gv_rect.grs_dim[Y] = user_pt[Y];

	    rect_image2view(&gedp->ged_gvp->gv_rect);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s dim' command requires 0 or 2 arguments\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "pos")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d %d",
			  gedp->ged_gvp->gv_rect.grs_pos[X],
			  gedp->ged_gvp->gv_rect.grs_pos[Y]);
	    return GED_OK;
	} else if (argc == 2) {
	    gedp->ged_gvp->gv_rect.grs_pos[X] = user_pt[X];
	    gedp->ged_gvp->gv_rect.grs_pos[Y] = user_pt[Y];

	    rect_image2view(&gedp->ged_gvp->gv_rect);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s pos' command requires 0 or 2 arguments\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "bg")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d %d %d",
			  gedp->ged_gvp->gv_rect.grs_bg[X],
			  gedp->ged_gvp->gv_rect.grs_bg[Y],
			  gedp->ged_gvp->gv_rect.grs_bg[Z]);
	    return GED_OK;
	} else if (argc == 3) {
	    gedp->ged_gvp->gv_rect.grs_bg[0] = (int)user_pt[X];
	    gedp->ged_gvp->gv_rect.grs_bg[1] = (int)user_pt[Y];
	    gedp->ged_gvp->gv_rect.grs_bg[2] = (int)user_pt[Z];

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s bg' command requires 0 or 3 arguments\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "color")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d %d %d",
			  gedp->ged_gvp->gv_rect.grs_color[X],
			  gedp->ged_gvp->gv_rect.grs_color[Y],
			  gedp->ged_gvp->gv_rect.grs_color[Z]);
	    return GED_OK;
	} else if (argc == 3) {
	    gedp->ged_gvp->gv_rect.grs_color[0] = (int)user_pt[X];
	    gedp->ged_gvp->gv_rect.grs_color[1] = (int)user_pt[Y];
	    gedp->ged_gvp->gv_rect.grs_color[2] = (int)user_pt[Z];

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s color' command requires 0 or 3 arguments\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "lstyle")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_rect.grs_line_style);
	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i <= 0)
		gedp->ged_gvp->gv_rect.grs_line_style = 0;
	    else
		gedp->ged_gvp->gv_rect.grs_line_style = 1;

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s lstyle' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "lwidth")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_rect.grs_line_width);
	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i <= 0)
		gedp->ged_gvp->gv_rect.grs_line_width = 0;
	    else
		gedp->ged_gvp->gv_rect.grs_line_width = i;

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s lwidth' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "rt")) {
	if (argc == 1)
	    return rect_rt(gedp, (int)user_pt[X]);

	bu_vls_printf(&gedp->ged_result_str, "The '%s rt' command accepts 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "zoom")) {
	if (argc == 0)
	    return rect_zoom(gedp);

	bu_vls_printf(&gedp->ged_result_str, "The '%s zoom' command accepts no arguments\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "vars")) {
	rect_vls_print(gedp);
	return GED_OK;
    }

    if (BU_STR_EQUAL(parameter, "help")) {
	usage(gedp, command);
	return GED_HELP;
    }

    bu_vls_printf(&gedp->ged_result_str, "%s: unrecognized command '%s'\n", command, parameter);
    usage(gedp, command);

    return GED_ERROR;
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
