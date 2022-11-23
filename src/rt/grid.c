/*                          G R I D . C
 * BRL-CAD
 *
 * Copyright (c) 2022 United States Government as represented by
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
/** @file grid.c
 *
 * Routines that help set up the view grid
 *
 */

#include "common.h"

#include "rt/application.h"
#include "bn/mat.h"
#include "bu/log.h"
#include "bu/app.h"
#include "bu/assert.h"
#include "vmath.h"

#include "./ext.h"

extern int cell_newsize;
extern fastf_t cell_width;
extern fastf_t cell_height;
extern size_t width;
extern size_t height;
extern fastf_t aspect;
extern fastf_t viewsize;
extern mat_t Viewrotscale;
extern mat_t model2view;
extern mat_t view2model;
extern fastf_t rt_perspective;
extern int stereo;
extern unsigned int jitter;
extern point_t eye_model;
extern point_t viewbase_model;
extern vect_t dx_model;
extern vect_t dy_model;
extern vect_t dx_unit;
extern vect_t dy_unit;
extern point_t viewbase_model;
extern struct application APP;

/* globals */
mat_t	view2model = MAT_INIT_IDN;
mat_t	model2view = MAT_INIT_IDN;
point_t viewbase_model = VINIT_ZERO; /* model-space location of viewplane corner */
fastf_t gift_grid_rounding = 0;      /* set to 25.4 for inches */


/*
 * sets cell_width+cell_height variables and width+height to match.
 */
void
grid_sync_dimensions(double vsize)
{
    BU_ASSERT(!ZERO(width) || !ZERO(cell_width));
    BU_ASSERT(!ZERO(height) || !ZERO(cell_height));

    if (vsize <= 0.0)
	return;

    if (cell_newsize) {
	if (cell_width <= 0.0 && cell_height > 0.0)
	    cell_width = cell_height;
	if (cell_height <= 0.0 && cell_width > 0.0)
	    cell_height = cell_width;

	/* sanity, should be positive by now */
	if (cell_width <= 0.0)
	    cell_width = 1.0;
	if (cell_height <= 0.0)
	    cell_height = 1.0;

	/* adjust grid specification to match viewsize */
	width = (vsize / cell_width) + 0.99;
	if (!ZERO(aspect)) {
	    height = (vsize / (cell_height*aspect)) + 0.99;
	} else {
	    height = (vsize / cell_height) + 0.99;
	}

	cell_newsize = 0;
    } else {
	/* Chop -1.0..+1.0 range into parts */
	cell_width = vsize / width;
	if (!ZERO(aspect)) {
	    cell_height = vsize / (height*aspect);
	} else {
	    cell_height = vsize / height;
	}
    }
}


/**
 * In theory, the grid can be specified by providing any two of
 * these sets of parameters:
 *
 * number of pixels (width, height)
 * viewsize (in model units, mm)
 * number of grid cells (cell_width, cell_height)
 *
 * however, for now, it is required that the view size always be
 * specified, and one or the other parameter be provided.
 */
int
grid_setup(struct bu_vls *err)
{
    vect_t temp;
    mat_t toEye;

    if (viewsize <= 0.0) {
	bu_vls_printf(err, "viewsize <= 0");
	return 1;
    }

    /* model2view takes us to eye_model location & orientation */
    MAT_IDN(toEye);
    MAT_DELTAS_VEC_NEG(toEye, eye_model);
    Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
    bn_mat_mul(model2view, Viewrotscale, toEye);
    bn_mat_inv(view2model, model2view);

    /* Determine grid cell size and number of pixels */
    grid_sync_dimensions(viewsize);

    /*
     * Optional GIFT compatibility, mostly for RTG3.  Round coordinates
     * of lower left corner to fall on integer- valued coordinates, in
     * "gift_grid_rounding" units.
     */
    if (gift_grid_rounding > 0.0) {
	point_t v_ll;		/* view, lower left */
	point_t m_ll;		/* model, lower left */
	point_t hv_ll;		/* hv, lower left*/
	point_t hv_wanted;
	vect_t hv_delta;
	vect_t m_delta;
	mat_t model2hv;
	mat_t hv2model;

	/* Build model2hv matrix, including mm2inches conversion */
	MAT_COPY(model2hv, Viewrotscale);
	model2hv[15] = gift_grid_rounding;
	bn_mat_inv(hv2model, model2hv);

	VSET(v_ll, -1, -1, 0);
	MAT4X3PNT(m_ll, view2model, v_ll);
	MAT4X3PNT(hv_ll, model2hv, m_ll);
	VSET(hv_wanted, floor(hv_ll[X]), floor(hv_ll[Y]), floor(hv_ll[Z]));
	VSUB2(hv_delta, hv_ll, hv_wanted);

	MAT4X3PNT(m_delta, hv2model, hv_delta);
	VSUB2(eye_model, eye_model, m_delta);
	MAT_DELTAS_VEC_NEG(toEye, eye_model);
	bn_mat_mul(model2view, Viewrotscale, toEye);
	bn_mat_inv(view2model, model2view);
    }

    /* Create basis vectors dx and dy for emanation plane (grid) */
    VSET(temp, 1, 0, 0);
    MAT3X3VEC(dx_unit, view2model, temp);	/* rotate only */
    VSCALE(dx_model, dx_unit, cell_width);

    VSET(temp, 0, 1, 0);
    MAT3X3VEC(dy_unit, view2model, temp);	/* rotate only */
    VSCALE(dy_model, dy_unit, cell_height);

    /* "Lower left" corner of viewing plane */
    if (rt_perspective > 0.0) {
	fastf_t zoomout;
	zoomout = 1.0 / tan(DEG2RAD * rt_perspective / 2.0);
	VSET(temp, -1, -1/aspect, -zoomout);	/* viewing plane */

	/*
	 * divergence is perspective angle divided by the number of
	 * pixels in that angle. Extra factor of 0.5 is because
	 * perspective is a full angle while divergence is the tangent
	 * (slope) of a half angle.
	 */
	APP.a_diverge = tan(DEG2RAD * rt_perspective * 0.5 / width);
	APP.a_rbeam = 0;
    } else {
	/* all rays go this direction */
	VSET(temp, 0, 0, -1);
	MAT4X3VEC(APP.a_ray.r_dir, view2model, temp);
	VUNITIZE(APP.a_ray.r_dir);

	VSET(temp, -1, -1/aspect, 0);	/* eye plane */
	APP.a_rbeam = 0.5 * viewsize / width;
	APP.a_diverge = 0;
    }

    if (ZERO(APP.a_rbeam) && ZERO(APP.a_diverge)) {
	bu_vls_printf(err, "zero-radius beam");
	return 1;
    }

    MAT4X3PNT(viewbase_model, view2model, temp);

    if (jitter & JITTER_FRAME) {
	/* Move the frame in a smooth circular rotation in the plane */
	fastf_t ang;	/* radians */
	fastf_t dx, dy;

	ang = curframe * M_2PI / 100.0;	/* semi-abitrary 100 frame period */
	dx = cos(ang) * 0.5;	/* +/- 1/4 pixel width in amplitude */
	dy = sin(ang) * 0.5;
	VJOIN2(viewbase_model, viewbase_model,
	       dx, dx_model,
	       dy, dy_model);
    }

    if (cell_width <= 0 || cell_width >= INFINITY ||
	cell_height <= 0 || cell_height >= INFINITY) {
	bu_vls_printf(err, "bad cell size (%g, %g) mm",
		      cell_width, cell_height);
	return 1;
    }
    if (width <= 0 || height <= 0) {
	bu_vls_printf(err, "bad image size (%zu, %zu)",
		      width, height);
	return 1;
    }

    return 0;
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
