/*                    D S P _ S A _ V O L . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @addtogroup primitives */
/** @{ */
/** @file primitives/dsp/dsp_sa_vol.c
 *
 * Surface area and volume for the DSP primitive.
 * 
 * Each cell is split into two triangles along one diagonal; the resulting
 * surface is the top of the solid.  The solid is closed by a flat base at
 * z=0 and by vertical walls around the outer perimeter.
 *
 * - Volume is the integral of the (per-triangle linear) height over the
 * footprint, which reduces to a simple prism, scaled by |det| of the linear 
 * part of dsp_stom.
 * - Surface area must be computed in model space (area does not scale
 * uniformly): every triangle vertex is pushed through dsp_stom and the 
 * triangle areas are summed.
 * These mirror the geometry that rt_dsp_shot() actually intersects, so the
 * values agree with the ray-traced solid.
 */

#include "common.h"

#include <math.h>

#include "vmath.h"
#include "raytrace.h"
#include "rt/geom.h"

#include "./dsp.h"

/**
 * Area, in model space, of the triangle whose vertices are given in solid
 * space.  Each vertex is transformed by stom before the area is taken.
 */
static fastf_t
tri_area_model(const mat_t stom,
	       fastf_t x0, fastf_t y0, fastf_t z0,
	       fastf_t x1, fastf_t y1, fastf_t z1,
	       fastf_t x2, fastf_t y2, fastf_t z2)
{
    point_t s, m0, m1, m2;
    vect_t v01, v02, cross;

    VSET(s, x0, y0, z0); MAT4X3PNT(m0, stom, s);
    VSET(s, x1, y1, z1); MAT4X3PNT(m1, stom, s);
    VSET(s, x2, y2, z2); MAT4X3PNT(m2, stom, s);

    VSUB2(v01, m1, m0);
    VSUB2(v02, m2, m0);
    VCROSS(cross, v01, v02);

    return 0.5 * MAGNITUDE(cross);
}


/**
 * |det| of the linear (3x3) part of stom, obtained as the scalar triple
 * product of the transformed unit axes.  Translation is irrelevant; this
 * is the factor by which solid-space volume scales into model space.
 */
static fastf_t
stom_volume_scale(const mat_t stom)
{
    vect_t e, xv, yv, zv, cross;

    VSET(e, 1.0, 0.0, 0.0); MAT4X3VEC(xv, stom, e);
    VSET(e, 0.0, 1.0, 0.0); MAT4X3VEC(yv, stom, e);
    VSET(e, 0.0, 0.0, 1.0); MAT4X3VEC(zv, stom, e);

    VCROSS(cross, yv, zv);
    return fabs(VDOT(xv, cross));
}


/**
 * Volume of a DSP primitive, in model-space
 *
 * Sums, over every cell, the prism volume of the height above z=0,
 * then scales by |det| of the solid-to-model transform
 */
RT_EXPORT void
rt_dsp_volume(fastf_t *vol, const struct rt_db_internal *ip)
{
    struct rt_dsp_internal *dsp;
    size_t x, y, xsiz, ysiz;
    fastf_t solid_vol = 0.0;

    if (!vol)
	return;
    *vol = 0.0;

    dsp = rt_dsp_internal(ip);
    if (!dsp)
	return;

    xsiz = (size_t)dsp->dsp_xcnt - 1;
    ysiz = (size_t)dsp->dsp_ycnt - 1;

    for (y = 0; y < ysiz; y++) {
	for (x = 0; x < xsiz; x++) {
	    fastf_t A = DSP(dsp, x,     y);
	    fastf_t B = DSP(dsp, x + 1, y);
	    fastf_t C = DSP(dsp, x,     y + 1);
	    fastf_t D = DSP(dsp, x + 1, y + 1);

	    /* Each unit cell splits into two triangles; the volume under a
	     * triangle is its area (1/2) times the mean of its three corner
	     * heights, i.e. (sum of corners)/6.  The shared diagonal corners
	     * therefore appear in both triangles and are doubled.
	     */
	    if (rt_dsp_cell_cut(dsp, x, y, xsiz, ysiz) == DSP_CUT_DIR_llUR)
		solid_vol += (2.0 * A + B + C + 2.0 * D) / 6.0;	/* A-D */
	    else
		solid_vol += (A + 2.0 * B + 2.0 * C + D) / 6.0;	/* B-C */
	}
    }

    *vol = solid_vol * stom_volume_scale(dsp->dsp_stom);
}


/**
 * Surface area of a DSP primitive, in model-space
 *
 * Sums the model-space area of the triangulated top surface, 
 * the flat base at z=0, and the vertical walls around the
 * outer perimeter.  There are no interior walls: the top surface is
 * continuous across shared cell edges.
 */
RT_EXPORT void
rt_dsp_surf_area(fastf_t *area, const struct rt_db_internal *ip)
{
    struct rt_dsp_internal *dsp;
    const fastf_t *stom;
    size_t x, y, xsiz, ysiz;
    fastf_t total = 0.0;

    if (!area)
	return;
    *area = 0.0;

    dsp = rt_dsp_internal(ip);
    if (!dsp)
	return;

    stom = dsp->dsp_stom;
    xsiz = (size_t)dsp->dsp_xcnt - 1;
    ysiz = (size_t)dsp->dsp_ycnt - 1;

    /* top surface + flat base, per cell */
    for (y = 0; y < ysiz; y++) {
	for (x = 0; x < xsiz; x++) {
	    fastf_t A = DSP(dsp, x,     y);
	    fastf_t B = DSP(dsp, x + 1, y);
	    fastf_t C = DSP(dsp, x,     y + 1);
	    fastf_t D = DSP(dsp, x + 1, y + 1);
	    fastf_t x0 = (fastf_t)x,     y0 = (fastf_t)y;
	    fastf_t x1 = (fastf_t)x + 1, y1 = (fastf_t)y + 1;

	    if (rt_dsp_cell_cut(dsp, x, y, xsiz, ysiz) == DSP_CUT_DIR_llUR) {
		/* A-D diagonal: triangles {A,B,D} and {A,C,D} */
		total += tri_area_model(stom, x0, y0, A, x1, y0, B, x1, y1, D);
		total += tri_area_model(stom, x0, y0, A, x0, y1, C, x1, y1, D);

		total += tri_area_model(stom, x0, y0, 0, x1, y0, 0, x1, y1, 0);
		total += tri_area_model(stom, x0, y0, 0, x0, y1, 0, x1, y1, 0);
	    } else {
		/* B-C diagonal: triangles {A,B,C} and {B,C,D} */
		total += tri_area_model(stom, x0, y0, A, x1, y0, B, x0, y1, C);
		total += tri_area_model(stom, x1, y0, B, x0, y1, C, x1, y1, D);

		total += tri_area_model(stom, x0, y0, 0, x1, y0, 0, x0, y1, 0);
		total += tri_area_model(stom, x1, y0, 0, x0, y1, 0, x1, y1, 0);
	    }
	}
    }

    /* perimeter walls: one planar quad per boundary grid edge, from z=0 up
     * to the post heights, split into two triangles.  All four sides.
     */
    for (x = 0; x < xsiz; x++) {
	fastf_t x0 = (fastf_t)x, x1 = (fastf_t)x + 1;

	/* south edge (y = 0) */
	{
	    fastf_t h0 = DSP(dsp, x, 0);
	    fastf_t h1 = DSP(dsp, x + 1, 0);
	    total += tri_area_model(stom, x0, 0, 0, x1, 0, 0, x1, 0, h1);
	    total += tri_area_model(stom, x0, 0, 0, x1, 0, h1, x0, 0, h0);
	}
	/* north edge (y = ysiz) */
	{
	    fastf_t yn = (fastf_t)ysiz;
	    fastf_t h0 = DSP(dsp, x, ysiz);
	    fastf_t h1 = DSP(dsp, x + 1, ysiz);
	    total += tri_area_model(stom, x0, yn, 0, x1, yn, 0, x1, yn, h1);
	    total += tri_area_model(stom, x0, yn, 0, x1, yn, h1, x0, yn, h0);
	}
    }
    for (y = 0; y < ysiz; y++) {
	fastf_t y0 = (fastf_t)y, y1 = (fastf_t)y + 1;

	/* west edge (x = 0) */
	{
	    fastf_t h0 = DSP(dsp, 0, y);
	    fastf_t h1 = DSP(dsp, 0, y + 1);
	    total += tri_area_model(stom, 0, y0, 0, 0, y1, 0, 0, y1, h1);
	    total += tri_area_model(stom, 0, y0, 0, 0, y1, h1, 0, y0, h0);
	}
	/* east edge (x = xsiz) */
	{
	    fastf_t xe = (fastf_t)xsiz;
	    fastf_t h0 = DSP(dsp, xsiz, y);
	    fastf_t h1 = DSP(dsp, xsiz, y + 1);
	    total += tri_area_model(stom, xe, y0, 0, xe, y1, 0, xe, y1, h1);
	    total += tri_area_model(stom, xe, y0, 0, xe, y1, h1, xe, y0, h0);
	}
    }

    *area = total;
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
