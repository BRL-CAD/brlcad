/*               P R I M I T I V E _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
/** @file primitive_util.c
 *
 * General utility routines for librt primitives that are not part of
 * the librt API. Prototypes for these routines are located in
 * librt_private.h.
 */

#include "../librt_private.h"

/**
 * If only the absolute tolerance is valid (positive), it is returned.
 *
 * If only the relative tolerance is valid (in (0.0, 1.0)), the return value
 * is the relative tolerance multiplied by rel_to_abs.
 *
 * If both tolerances are valid, the smaller is used.
 * If neither is valid, a default of .1 * rel_to_abs is used.
 */
#define DEFAULT_REL_TOL .1
fastf_t
primitive_get_absolute_tolerance(
	const struct rt_tess_tol *ttol,
	fastf_t rel_to_abs)
{
    fastf_t tol, rel_tol, abs_tol;
    int rel_tol_is_valid;

    rel_tol = ttol->rel;
    abs_tol = ttol->abs;

    rel_tol_is_valid = 0;
    if (rel_tol > 0.0 && rel_tol < 1.0) {
	rel_tol_is_valid = 1;
    }

    if (abs_tol > 0.0 && (!rel_tol_is_valid || rel_tol > abs_tol)) {
	tol = abs_tol;
    } else {
	if (!rel_tol_is_valid) {
	    rel_tol = DEFAULT_REL_TOL;
	}
	tol = rel_tol * rel_to_abs;
    }

    return tol;
}

/**
 * Gives a rough estimate of the maximum number of times a primitive's bounding
 * box diagonal will be sampled based on the sample density of the view.
 *
 * Practically, it is an estimate of the maximum number of pixels that would be
 * used if the diagonal line were drawn in the current view window.
 *
 * It is currently used in adaptive plot routines to help choose how many
 * sample points should be used for plotted curves.
 */
fastf_t
primitive_diagonal_samples(
	struct rt_db_internal *ip,
	const struct rt_view_info *info)
{
    point_t bbox_min, bbox_max;
    fastf_t primitive_diagonal_mm, samples_per_mm;
    fastf_t diagonal_samples;

    ip->idb_meth->ft_bbox(ip, &bbox_min, &bbox_max, info->tol);
    primitive_diagonal_mm = DIST_PT_PT(bbox_min, bbox_max);

    samples_per_mm = 1.0 / info->point_spacing;
    diagonal_samples = samples_per_mm * primitive_diagonal_mm;

    return diagonal_samples;
}

/**
 * Estimate the number of evenly spaced cross-section curves needed to meet a
 * target curve spacing.
 */
fastf_t
primitive_curve_count(
	struct rt_db_internal *ip,
	const struct rt_view_info *info)
{
    point_t bbox_min, bbox_max;
    fastf_t x_len, y_len, z_len, avg_len;

    if (!ip->idb_meth->ft_bbox) {
	return 0.0;
    }

    ip->idb_meth->ft_bbox(ip, &bbox_min, &bbox_max, info->tol);

    x_len = fabs(bbox_max[X] - bbox_min[X]);
    y_len = fabs(bbox_max[Y] - bbox_min[Y]);
    z_len = fabs(bbox_max[Z] - bbox_min[Z]);

    avg_len = (x_len + y_len + z_len) / 3.0;

    return avg_len / info->curve_spacing;
}

/* Calculate the length of the shortest distance between a point and a line in
 * the y-z plane.
 */
static fastf_t
distance_point_to_line(point_t p, fastf_t slope, fastf_t intercept)
{
    /* input line:
     *   z = slope * y + intercept
     *
     * implicit form is:
     *   az + by + c = 0,  where a = -slope, b = 1, c = -intercept
     *
     * standard 2D point-line distance calculation:
     *   d = |aPy + bPz + c| / sqrt(a^2 + b^2)
     */
    return fabs(-slope * p[Y] + p[Z] - intercept) / sqrt(slope * slope + 1);
}

/* Assume the existence of a line and a parabola at the origin, both in the y-z
 * plane (x = 0.0). The parabola and line are described by arguments p and m,
 * from the respective equations (z = y^2 / 4p) and (z = my + b).
 *
 * The line is assumed to intersect the parabola at two points.
 *
 * The portion of the parabola between these two intersection points takes on a
 * single maximum value with respect to the intersecting line when its slope is
 * the same as the line's (i.e. when the tangent line and intersecting line are
 * parallel).
 *
 * The first derivate slope equation z' = y / 2p implies that the parabola has
 * slope m when y = 2pm. So we calculate y from p and m, and then z from y.
 */
static void
parabola_point_farthest_from_line(point_t max_point, fastf_t p, fastf_t m)
{
    fastf_t y = 2.0 * p * m;

    max_point[X] = 0.0;
    max_point[Y] = y;
    max_point[Z] = (y * y) / ( 4.0 * p);
}

/* Approximate part of the parabola (y - h)^2 = 4p(z - k) lying in the y-z
 * plane.
 *
 * pts->p: the vertex (0, h, k)
 * pts->next->p: another point on the parabola
 * pts->next->next: NULL
 * p: the constant from the above equation
 *
 * This routine inserts num_new_points points between the two input points to
 * better approximate the parabolic curve passing between them.
 *
 * Returns number of points successfully added.
 */
int
approximate_parabolic_curve(struct rt_pt_node *pts, fastf_t p, int num_new_points)
{
    fastf_t error, max_error, seg_slope, seg_intercept;
    point_t v, point, p0, p1, new_point = VINIT_ZERO;
    struct rt_pt_node *node, *worst_node, *new_node;
    int i;

    if (pts == NULL || pts->next == NULL || num_new_points < 1) {
	return 0;
    }

    VMOVE(v, pts->p);

    for (i = 0; i < num_new_points; ++i) {
	worst_node = node = pts;
	max_error = 0.0;

	/* Find the least accurate line segment, and calculate a new parabola
	 * point to insert between its endpoints.
	 */
	while (node->next != NULL) {
	    VMOVE(p0, node->p);
	    VMOVE(p1, node->next->p);

	    seg_slope = (p1[Z] - p0[Z]) / (p1[Y] - p0[Y]);
	    seg_intercept = p0[Z] - seg_slope * p0[Y];

	    /* get farthest point on the equivalent parabola at the origin */
	    parabola_point_farthest_from_line(point, p, seg_slope);

	    /* translate result to actual parabola position */
	    point[Y] += v[Y];
	    point[Z] += v[Z];

	    /* see if the maximum distance between the parabola and the line
	     * (the error) is larger than the largest recorded error
	     * */
	    error = distance_point_to_line(point, seg_slope, seg_intercept);

	    if (error > max_error) {
		max_error = error;
		worst_node = node;
		VMOVE(new_point, point);
	    }

	    node = node->next;
	}

	/* insert new point between endpoints of the least accurate segment */
	BU_ALLOC(new_node, struct rt_pt_node);
	VMOVE(new_node->p, new_point);
	new_node->next = worst_node->next;
	worst_node->next = new_node;
    }

    return num_new_points;
}

/* Assume the existence of a line and a hyperbola with asymptote origin at the
 * origin, both in the y-z plane (x = 0.0). The hyperbola and line are
 * described by arguments a, b, and m, from the respective equations
 * (z = +-(a/b) * sqrt(y^2 + b^2)) and (z = my + b).
 *
 * The line is assumed to intersect the positive half of the hyperbola at two
 * points.
 *
 * The portion of the hyperbola between these two intersection points takes on
 * a single maximum value with respect to the intersecting line when its slope
 * is the same as the line's (i.e. when the tangent line and intersecting line
 * are parallel).
 *
 * The first derivate slope equation z' = ay / (b * sqrt(y^2 + b^2)) implies
 * that the hyperbola has slope m when y = mb^2 / sqrt(a^2 - b^2m^2).
 *
 * We calculate y from a, b, and m, and then z from y.
 */
static void
hyperbola_point_farthest_from_line(point_t max_point, fastf_t a, fastf_t b, fastf_t m)
{
    fastf_t y = (m * b * b) / sqrt((a * a) - (b * b * m * m));

    max_point[X] = 0.0;
    max_point[Y] = y;
    max_point[Z] = (a / b) * sqrt(y * y + b * b);
}

/* Approximate part of the hyperbola lying in the Y-Z plane described by:
 *  ((z - k)^2 / a^2) - ((y - h)^2 / b^2) = 1
 *
 * pts->p: the asymptote origin (0, h, k)
 * pts->next->p: another point on the hyperbola
 * pts->next->next: NULL
 * a, b: the constants from the above equation
 *
 * This routine inserts num_new_points points between the two input points to
 * better approximate the hyperbolic curve passing between them.
 *
 * Returns number of points successfully added.
 */
int
approximate_hyperbolic_curve(struct rt_pt_node *pts, fastf_t a, fastf_t b, int num_new_points)
{
    fastf_t error, max_error, seg_slope, seg_intercept;
    point_t v, point, p0, p1, new_point = VINIT_ZERO;
    struct rt_pt_node *node, *worst_node, *new_node;
    int i;

    if (pts == NULL || pts->next == NULL || num_new_points < 1) {
	return 0;
    }

    VMOVE(v, pts->p);

    for (i = 0; i < num_new_points; ++i) {
	worst_node = node = pts;
	max_error = 0.0;

	/* Find the least accurate line segment, and calculate a new hyperbola
	 * point to insert between its endpoints.
	 */
	while (node->next != NULL) {
	    VMOVE(p0, node->p);
	    VMOVE(p1, node->next->p);

	    seg_slope = (p1[Z] - p0[Z]) / (p1[Y] - p0[Y]);
	    seg_intercept = p0[Z] - seg_slope * p0[Y];

	    /* get farthest point on the equivalent hyperbola with asymptote origin at
	     * the origin
	     */
	    hyperbola_point_farthest_from_line(point, a, b, seg_slope);

	    /* translate result to actual hyperbola position */
	    point[Y] += v[Y];
	    point[Z] += v[Z] - a;

	    /* see if the maximum distance between the hyperbola and the line
	     * (the error) is larger than the largest recorded error
	     * */
	    error = distance_point_to_line(point, seg_slope, seg_intercept);

	    if (error > max_error) {
		max_error = error;
		worst_node = node;
		VMOVE(new_point, point);
	    }

	    node = node->next;
	}

	/* insert new point between endpoints of the least accurate segment */
	BU_ALLOC(new_node, struct rt_pt_node);
	VMOVE(new_node->p, new_point);
	new_node->next = worst_node->next;
	worst_node->next = new_node;
    }

    return num_new_points;
}

void
ellipse_point_at_radian(
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

void
plot_ellipse(
	struct bu_list *vhead,
	const vect_t center,
	const vect_t axis_a,
	const vect_t axis_b,
	int num_points)
{
    int i;
    point_t p;
    fastf_t radian, radian_step;

    radian_step = M_2PI / num_points;

    ellipse_point_at_radian(p, center, axis_a, axis_b,
	    radian_step * (num_points - 1));
    RT_ADD_VLIST(vhead, p, BN_VLIST_LINE_MOVE);

    radian = 0;
    for (i = 0; i < num_points; ++i) {
	ellipse_point_at_radian(p, center, axis_a, axis_b, radian);
	RT_ADD_VLIST(vhead, p, BN_VLIST_LINE_DRAW);

	radian += radian_step;
    }
}
