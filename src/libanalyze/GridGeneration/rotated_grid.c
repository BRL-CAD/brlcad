/*               R O T A T E D _ G R I D . C
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/**
 * @file rotated_grid.c
 *
 * Rotated-grid ray generator for libanalyze.
 *
 * A rotated grid fires parallel rays through the model along an arbitrary
 * user-specified direction rather than being locked to the X, Y or Z axes.
 * This eliminates the directional bias that the standard rectangular triple-
 * grid introduces when the geometry being analysed happens to be axis-aligned
 * (a very common situation in engineering models).
 *
 * The grid plane is always perpendicular to the ray direction; two orthogonal
 * basis vectors for that plane are computed automatically via bn_vec_perp().
 * Grid spacing and overall grid extent are derived from the model bounding box
 * projected onto the plane.
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "bn/mat.h"
#include "raytrace.h"

#include "analyze.h"


void
rotated_grid_setup(struct rotated_grid *g,
		   const point_t mdl_min, const point_t mdl_max,
		   const vect_t ray_dir, fastf_t grid_spacing)
{
    if (!g || !mdl_min || !mdl_max || !ray_dir || grid_spacing <= 0.0)
	return;

    /* Normalise the ray direction */
    VMOVE(g->ray_dir, ray_dir);
    VUNITIZE(g->ray_dir);

    /* Build two orthogonal basis vectors spanning the grid plane */
    bn_vec_perp(g->u_dir, g->ray_dir);
    VUNITIZE(g->u_dir);
    VCROSS(g->v_dir, g->ray_dir, g->u_dir);
    VUNITIZE(g->v_dir);

    g->grid_spacing = grid_spacing;

    /* Project all 8 corners of the model bounding box onto the grid plane
     * (which is anchored at the bbox center) to find the required grid extent.
     * The "plane anchor" is the bbox center; we find the extremes in u and v. */
    point_t center;
    VADD2SCALE(center, mdl_min, mdl_max, 0.5);

    double u_min =  INFINITY;
    double u_max = -INFINITY;
    double v_min =  INFINITY;
    double v_max = -INFINITY;

    int ix, iy, iz;
    for (ix = 0; ix < 2; ix++) {
	for (iy = 0; iy < 2; iy++) {
	    for (iz = 0; iz < 2; iz++) {
		point_t corner;
		corner[0] = (ix == 0) ? mdl_min[0] : mdl_max[0];
		corner[1] = (iy == 0) ? mdl_min[1] : mdl_max[1];
		corner[2] = (iz == 0) ? mdl_min[2] : mdl_max[2];

		vect_t delta;
		VSUB2(delta, corner, center);

		double u_proj = VDOT(delta, g->u_dir);
		double v_proj = VDOT(delta, g->v_dir);

		if (u_proj < u_min) u_min = u_proj;
		if (u_proj > u_max) u_max = u_proj;
		if (v_proj < v_min) v_min = v_proj;
		if (v_proj > v_max) v_max = v_proj;
	    }
	}
    }

    /* Pad slightly so rays cover the full bounding box face */
    double margin = grid_spacing;
    u_min -= margin;
    u_max += margin;
    v_min -= margin;
    v_max += margin;

    /* Grid dimensions */
    g->u_count = (size_t)((u_max - u_min) / grid_spacing) + 1;
    g->v_count = (size_t)((v_max - v_min) / grid_spacing) + 1;
    g->total   = g->u_count * g->v_count;
    g->current = 0;

    /* Step vectors */
    VSCALE(g->u_step, g->u_dir, grid_spacing);
    VSCALE(g->v_step, g->v_dir, grid_spacing);

    /* Grid start: move from center to (u_min, v_min) then pull back along
     * ray_dir so the ray origin is behind the model. */
    double pull_back = 0.0;
    {
	/* The pull-back distance must exceed the model extent along ray_dir */
	point_t span;
	VSUB2(span, mdl_max, mdl_min);
	pull_back = MAGNITUDE(span) + margin;
    }
    VJOIN2(g->start, center,
	   u_min, g->u_dir,
	   v_min, g->v_dir);
    VJOIN1(g->start, g->start, -pull_back, g->ray_dir);

    g->refine_flag = 0;
}


void
rotated_grid_setup_ae(struct rotated_grid *g,
		      const point_t mdl_min, const point_t mdl_max,
		      fastf_t azimuth_deg, fastf_t elevation_deg,
		      fastf_t grid_spacing)
{
    /* Compute ray direction from azimuth / elevation.
     * BRL-CAD convention: az is measured CCW from +X in the XY plane;
     * el is measured up from the XY plane.
     * The ray ENTERS the model from this direction, so the actual direction
     * vector points the opposite way (toward the model).
     */
    double az = azimuth_deg  * DEG2RAD;
    double el = elevation_deg * DEG2RAD;

    vect_t ray_dir;
    ray_dir[0] = cos(el) * cos(az);
    ray_dir[1] = cos(el) * sin(az);
    ray_dir[2] = sin(el);

    rotated_grid_setup(g, mdl_min, mdl_max, ray_dir, grid_spacing);
}


int
rotated_grid_generator(struct xray *ray, void *context)
{
    struct rotated_grid *g = (struct rotated_grid *)context;

    if (!g || !ray)
	return 1;

    if (g->current >= g->total)
	return 1;

    size_t u_idx = g->current % g->u_count;
    size_t v_idx = g->current / g->u_count;

    /* In refinement mode, skip points already shot in the previous
     * (coarser) pass: those are the points where BOTH u_idx and v_idx
     * are even (they were shot when the spacing was twice as large). */
    if (g->refine_flag) {
	while (g->current < g->total && !(u_idx & 1) && !(v_idx & 1)) {
	    g->current++;
	    u_idx = g->current % g->u_count;
	    v_idx = g->current / g->u_count;
	}
	if (g->current >= g->total)
	    return 1;
    }

    VJOIN2(ray->r_pt, g->start,
	   (fastf_t)u_idx, g->u_step,
	   (fastf_t)v_idx, g->v_step);
    VMOVE(ray->r_dir, g->ray_dir);

    g->current++;
    return 0;
}


double
rotated_grid_spacing(void *context)
{
    struct rotated_grid *g = (struct rotated_grid *)context;
    if (!g) return 0.0;
    return g->grid_spacing;
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
