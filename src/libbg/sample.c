/*                        S A M P L E . C
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
/** @file sample.c
 *
 * Brief description
 *
 */

#include "common.h"

#include "vmath.h"
#include "bu/malloc.h"
#include "bg/sample.h"

static fastf_t
view_avg_size(const struct bview *gvp)
{
    fastf_t view_aspect, x_size, y_size;

    view_aspect = (fastf_t)gvp->gv_width / gvp->gv_height;
    x_size = gvp->gv_size;
    y_size = x_size / view_aspect;

    return (x_size + y_size) / 2.0;
}

static fastf_t
view_avg_sample_spacing(const struct bview *gvp)
{
    fastf_t avg_view_size, avg_view_samples;

    avg_view_size = view_avg_size(gvp);
    avg_view_samples = (gvp->gv_width + gvp->gv_height) / 2.0;

    return avg_view_size / avg_view_samples;
}

fastf_t
bg_sample_spacing(const struct bview *v, fastf_t w)
{
    fastf_t radius, avg_view_size, avg_sample_spacing;
    point2d_t p1, p2;

    if (w < SQRT_SMALL_FASTF)
	w = SQRT_SMALL_FASTF;

    avg_view_size = view_avg_size(v);

    /* Now, for the sake of simplicity we're going to make
     * several assumptions:
     *  - our samples represent a grid of square pixels
     *  - a circle with a diameter half the width of the solid is a
     *    good proxy for the kind of curve that will be plotted
     */
    radius = w / 4.0;
    if (avg_view_size < w) {
	/* If the solid is larger than the view, it is
	 * probably only partly visible and likely isn't the
	 * primary focus of the user. We'll cap the point
	 * spacing and avoid wasting effort.
	 */
	radius = avg_view_size / 4.0;
    }

    /* We imagine our representative circular curve lying in
     * the XY plane centered at the origin.
     *
     * Suppose we're viewing the circle head on, and that the
     * apex of the curve (0, radius) lies just inside the
     * top edge of a pixel. Here we place a plotted point p1.
     *
     * As we continue clockwise around the circle we pass
     * through neighboring pixels in the same row, until we
     * vertically drop a distance equal to the pixel spacing,
     * in which case we just barely enter a pixel in the next
     * row. Here we place a plotted point p2 (y = radius -
     * avg_sample_spacing).
     *
     * In theory, the line segment between p1 and p2 passes
     * through all the same pixels that the actual curve does,
     * and thus produces the exact same rasterization as if
     * the curve between p1 and p2 was approximated with an
     * infinite number of line segments.
     *
     * We assume that the distance between p1 and p2 is the
     * maximum point sampling distance we can use for the
     * curve which will give a perfect rasterization, i.e.
     * the same rasterization as if we chose a point distance
     * of 0.
     */
    p1[Y] = radius;
    p1[X] = 0.0;

    avg_sample_spacing = view_avg_sample_spacing(v);
    if (avg_sample_spacing < radius) {
	p2[Y] = radius - (avg_sample_spacing);
    } else {
	/* no particular reason other than symmetry, just need
	 * to prevent sqrt(negative).
	 */
	p2[Y] = radius;
    }
    p2[X] = sqrt((radius * radius) - (p2[Y] * p2[Y]));

    return DIST_PNT2_PNT2(p1, p2);
}



void
bg_ell_radian_pnt(
        point_t result,
        const vect_t center,
        const vect_t axis_a,
        const vect_t axis_b,
        fastf_t radian)
{
    fastf_t cos_rad, sin_rad;

    cos_rad = cos(radian);
    sin_rad = sin(radian);

    VJOIN2(result, center, cos_rad, axis_a, sin_rad, axis_b);
}

int
bg_ell_sample(
        point_t **pnts,
        const vect_t center,
        const vect_t axis_a,
        const vect_t axis_b,
        int num_points)
{

    if (!pnts || num_points < 0)
	return -1;

    if (!num_points)
	return 0;

    (*pnts) = (point_t *)bu_calloc(num_points, sizeof(point_t), "points array");
    if (!(*pnts))
	return -1;

    fastf_t radian = 0;
    fastf_t radian_step = M_2PI / num_points;
    point_t p;
    for (int i = 0; i < num_points; ++i) {
	bg_ell_radian_pnt(p, center, axis_a, axis_b, radian);
	VMOVE((*pnts)[i], p);
        radian += radian_step;
    }

    return num_points;
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
