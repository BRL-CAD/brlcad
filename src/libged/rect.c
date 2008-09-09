/*                          R E C T . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2008 United States Government as represented by
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
/** @file rect.c
 *
 * Rubber band rectangle.
 *
 */

#include "common.h"

#include <math.h>
#include <stdio.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "fb.h"
#include "dm.h"
#include "ged_private.h"


void
ged_rect_vls_print(struct ged_rect_state *grsp, struct bu_vls *out_vp)
{
    bu_vls_printf(out_vp, "draw = %d\n", grsp->grs_draw);
    bu_vls_printf(out_vp, "dim = %d %d\n",
		  grsp->grs_dim[X],
		  grsp->grs_dim[Y]);
    bu_vls_printf(out_vp, "pos = %d %d\n",
		  grsp->grs_pos[X],
		  grsp->grs_pos[Y]);
    bu_vls_printf(out_vp, "color = %d %d %d\n",
		  grsp->grs_color[0],
		  grsp->grs_color[1],
		  grsp->grs_color[2]);
    bu_vls_printf(out_vp, "lstyle = %d\n", grsp->grs_linestyle);
    bu_vls_printf(out_vp, "lwidth = %d\n", grsp->grs_linewidth);
}

/*
 * Given position and dimensions in normalized view coordinates, calculate
 * position and dimensions in image coordinates.
 */
void
ged_rect_view2image(struct ged_rect_state *grsp, int width, int height, fastf_t aspect)
{
    grsp->grs_pos[X] = (grsp->grs_x * 0.5 + 0.5) * width;
    grsp->grs_pos[Y] = (grsp->grs_y * 0.5 + 0.5) * height * aspect;
    grsp->grs_dim[X] = grsp->grs_width * width * 0.5;
    grsp->grs_dim[Y] = grsp->grs_height * width * 0.5;
}

/*
 * Given position and dimensions in image coordinates, calculate
 * position and dimensions in normalized view coordinates.
 */
void
ged_rect_image2view(struct ged_rect_state *grsp, int width, int height, fastf_t aspect)
{
    grsp->grs_x = (grsp->grs_pos[X] / (fastf_t)width - 0.5) * 2.0;
    grsp->grs_y = (grsp->grs_pos[Y] / (fastf_t)height / aspect - 0.5) * 2.0;
    grsp->grs_width = grsp->grs_dim[X] * 2.0 / (fastf_t)width;
    grsp->grs_height = grsp->grs_dim[Y] * 2.0 / (fastf_t)width;
}

/*
 * Adjust the rubber band rectangle to have the same aspect ratio as the window.
 */
void
ged_adjust_rect_for_zoom(struct ged_rect_state *grsp, fastf_t aspect)
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
	    grsp->grs_height = width / aspect;
	else
	    grsp->grs_height = -width / aspect;
    } else {
	if (grsp->grs_width >= 0.0)
	    grsp->grs_width = height * aspect;
	else
	    grsp->grs_width = -height * aspect;
    }
}

int
ged_rt_rect_area(struct ged *gedp, struct ged_rect_state *grsp, int width, int height, fastf_t aspect, int port, int color[3])
{
    int xmin, xmax;
    int ymin, ymax;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);

    /* initialize result in case we need to report something here */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    if (NEAR_ZERO(grsp->grs_width, (fastf_t)SMALL_FASTF) &&
	NEAR_ZERO(grsp->grs_height, (fastf_t)SMALL_FASTF))
	return BRLCAD_OK;

    if (port < 0) {
	bu_vls_printf(&gedp->ged_result_str, "ged_rt_rect_area: invalid port number - %d\n", port);
	return BRLCAD_ERROR;
    }

    xmin = grsp->grs_pos[X];
    ymin = grsp->grs_pos[Y];

    if (grsp->grs_dim[X] >= 0) {
	xmax = xmin + grsp->grs_dim[X];
    } else {
	xmax = xmin;
	xmin += grsp->grs_dim[X];
    }

    if (grsp->grs_dim[Y] >= 0) {
	ymax = ymin + grsp->grs_dim[Y];
    } else {
	ymax = ymin;
	ymin += grsp->grs_dim[Y];
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

	bu_vls_printf(&wvls, "%d", width);
	bu_vls_printf(&nvls, "%d", height);
	bu_vls_printf(&vvls, "%lf", aspect);
	bu_vls_printf(&fvls, "%d", port);
	bu_vls_printf(&jvls, "%d,%d,%d,%d", xmin, ymin, xmax, ymax);
	bu_vls_printf(&cvls, "%d/%d/%d", color[0], color[1], color[2]);

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

	ret = ged_rt(gedp, 13, av);

	bu_vls_free(&wvls);
	bu_vls_free(&nvls);
	bu_vls_free(&vvls);
	bu_vls_free(&fvls);
	bu_vls_free(&jvls);
	bu_vls_free(&cvls);

	return ret;
    }
}

int
ged_zoom_rect_area(struct ged *gedp, struct ged_rect_state *grsp, int canvas_width, int canvas_height, fastf_t canvas_aspect)
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
    bu_vls_trunc(&gedp->ged_result_str, 0);

    if (NEAR_ZERO(grsp->grs_width, (fastf_t)SMALL_FASTF) &&
	NEAR_ZERO(grsp->grs_height, (fastf_t)SMALL_FASTF))
	return BRLCAD_OK;

    ged_adjust_rect_for_zoom(grsp, canvas_aspect);

    /* find old view center */
    MAT_DELTAS_GET_NEG(old_model_center, gedp->ged_gvp->gv_center);
    MAT4X3PNT(old_view_center, gedp->ged_gvp->gv_model2view, old_model_center);

    /* calculate new view center */
    VSET(new_view_center,
	 grsp->grs_x + grsp->grs_width / 2.0,
	 grsp->grs_y + grsp->grs_height / 2.0,
	 old_view_center[Z]);

    /* find new model center */
    MAT4X3PNT(new_model_center, gedp->ged_gvp->gv_view2model, new_view_center);

    /* zoom in to fill rectangle */
    if (grsp->grs_width >= 0.0)
	width = grsp->grs_width;
    else
	width = -grsp->grs_width;

    if (grsp->grs_height >= 0.0)
	height = grsp->grs_height;
    else
	height = -grsp->grs_height;

    if (width >= height)
	sf = width / 2.0;
    else
	sf = height / 2.0 * canvas_aspect;

    if (sf <= SMALL_FASTF || INFINITY < sf)
	return BRLCAD_OK;

    /* set the new model center */
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, new_model_center);
    gedp->ged_gvp->gv_scale *= sf;
    ged_view_update(gedp->ged_gvp);

#if 0
    grsp->grs_x = -1.0;
    grsp->grs_y = -1.0 / canvas_aspect;
    grsp->grs_width = 2.0;
    grsp->grs_height = 2.0 / canvas_aspect;

    ged_rect_view2image(grsp, canvas_width, canvas_height, canvas_aspect);
#endif

    return BRLCAD_OK;
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
