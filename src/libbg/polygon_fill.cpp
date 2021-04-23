/*                 P O L Y G O N _ F I L L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2011-2021 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/polyclip.cpp
 *
 * Add internal lines to polygons to indicate "filled" areas.
 */
/** @} */

#include "common.h"

#include "bu/malloc.h"
#include "bu/sort.h"
#include "bu/vls.h"
#include "bn/plane.h" /* bn_fit_plane */
#include "bg/polygon.h"

/* Note - line_slope encodes the fill line slope as a vector.  Doing it as a
 * this way instead of a single number allows us to handle vertical lines (i.e.
 * infinite slope) cleanly */
struct bg_polygon *
bg_polygon_fill_segments(struct bg_polygon *poly, vect2d_t line_slope, fastf_t line_spacing)
{
    struct bg_polygon poly_2d;

    if (poly->num_contours < 1 || poly->contour[0].num_points < 3)
	return NULL;

    // Fit the outer contour to get a 2D plane (bg_polygon is in principle a 3D data structure)
    point_t pcenter;
    vect_t  pnorm;
    plane_t pl;
    if (bn_fit_plane(&pcenter, &pnorm, poly->contour[0].num_points, poly->contour[0].point)) {
	return NULL;
    }
    bn_plane_pt_nrml(&pl, pcenter, pnorm);

    /* Project poly onto the fit plane. While we're at it, build the 2D AABB */
    vect2d_t b2d_min = {MAX_FASTF, MAX_FASTF};
    vect2d_t b2d_max = {-MAX_FASTF, -MAX_FASTF};

    poly_2d.num_contours = poly->num_contours;
    poly_2d.hole = (int *)bu_calloc(poly->num_contours, sizeof(int), "p_hole");
    poly_2d.contour = (struct bg_poly_contour *)bu_calloc(poly->num_contours, sizeof(struct bg_poly_contour), "p_contour");
    for (size_t i = 0; i < poly->num_contours; ++i) {
	poly_2d.hole[i] = poly->hole[i];
	poly_2d.contour[i].num_points = poly->contour[i].num_points;
	poly_2d.contour[i].point = (point_t *)bu_calloc(poly->contour[i].num_points, sizeof(point_t), "pc_point");
	for (size_t j = 0; j < poly->contour[i].num_points; ++j) {
	    vect2d_t p2d;
	    bn_plane_closest_pt(&p2d[0], &p2d[1], pl, poly->contour[i].point[j]);
	    VSET(poly_2d.contour[i].point[j], p2d[0], p2d[1], 0);
	    // bounding box
	    V2MINMAX(b2d_min, b2d_max, p2d);
	}
    }

    bg_polygon_plot("fill_mask.plot3", poly_2d.contour[0].point, poly_2d.contour[0].num_points, 255, 255, 0);

    /* Generate lines with desired slope - enough to cover the bounding box with the
     * desired pattern.  Add these lines as non-closed contours into a bg_polygon.
     *
     * Starting from the center of the bbox, construct line segments parallel
     * to line_slope that span the bbox and step perpendicular to line_slope in both
     * directions until the segments no longer intersect the bbox.  If we step
     * beyond the length of 0.5 of the bbox diagonal in each direction, we'll
     * be far enough.
     */
    vect2d_t bcenter, lseg, per;
    bcenter[0] = (b2d_max[0] - b2d_min[0]) * 0.5 + b2d_min[0];
    bcenter[1] = (b2d_max[1] - b2d_min[1]) * 0.5 + b2d_min[1];
    fastf_t ldiag = DIST_PNT2_PNT2(b2d_max, b2d_min);
    V2MOVE(lseg, line_slope);
    lseg[0] = -1 * lseg[0]; // Looks like the projection flips this...
    V2UNITIZE(lseg);
    int dir_step_cnt = (int)(0.5*ldiag / fabs(line_spacing) + 1);
    int step_cnt = 2*dir_step_cnt - 1; // center line is not repeated

    struct bg_polygon poly_lines;
    poly_lines.num_contours = step_cnt;
    poly_lines.hole = (int *)bu_calloc(poly_lines.num_contours, sizeof(int), "l_hole");
    poly_lines.contour = (struct bg_poly_contour *)bu_calloc(poly_lines.num_contours, sizeof(struct bg_poly_contour), "p_contour");

    // Construct the first contour (center line) first as it is not mirrored
    struct bg_poly_contour *c = &poly_lines.contour[0];
    vect2d_t p2d1, p2d2;
    poly_lines.hole[0] = 0;
    c->num_points = 2;
    c->open = 1;
    c->point = (point_t *)bu_calloc(c->num_points, sizeof(point_t), "l_point");
    V2JOIN1(p2d1, bcenter, ldiag*0.51, lseg);
    V2JOIN1(p2d2, bcenter, -ldiag*0.51, lseg);
    VSET(c->point[0], p2d1[0], p2d1[1], 0);
    VSET(c->point[1], p2d2[0], p2d2[1], 0);

    // step 1
    V2SET(per, -lseg[1], lseg[0]);
    V2UNITIZE(per);
    for (int i = 1; i < dir_step_cnt+1; ++i) {
	c = &poly_lines.contour[i];
	c->num_points = 2;
	c->open = 1;
	c->point = (point_t *)bu_calloc(c->num_points, sizeof(point_t), "l_point");
	V2JOIN2(p2d1, bcenter, fabs(line_spacing) * i, per, ldiag*0.51, lseg);
	V2JOIN2(p2d2, bcenter, fabs(line_spacing) * i, per, -ldiag*0.51, lseg);
	VSET(c->point[0], p2d1[0], p2d1[1], 0);
	VSET(c->point[1], p2d2[0], p2d2[1], 0);
    }

    // step 2
    V2SET(per, lseg[1], -lseg[0]);
    V2UNITIZE(per);
    for (int i = 1+dir_step_cnt; i < step_cnt; ++i) {
	c = &poly_lines.contour[i];
	c->num_points = 2;
	c->open = 1;
	c->point = (point_t *)bu_calloc(c->num_points, sizeof(point_t), "l_point");
	V2JOIN2(p2d1, bcenter, fabs(line_spacing) * (i - dir_step_cnt), per, ldiag*0.51, lseg);
	V2JOIN2(p2d2, bcenter, fabs(line_spacing) * (i - dir_step_cnt), per, -ldiag*0.51, lseg);
	VSET(c->point[0], p2d1[0], p2d1[1], 0);
	VSET(c->point[1], p2d2[0], p2d2[1], 0);
    }

    /* Take the generated lines and apply a clipper intersect using the 2D
     * polygon projection as a mask.  The resulting polygon should define the
     * fill lines */
    mat_t m;
    MAT_IDN(m);
    struct bg_polygon *fpoly = bg_clip_polygon(bg_Intersection, &poly_lines, &poly_2d, CLIPPER_MAX, m, m);
    if (!fpoly || !fpoly->num_contours) {
	bg_polygon_free(&poly_lines);
	bg_polygon_free(&poly_2d);
	return NULL;
    }

    for (size_t i = 0; i < fpoly->num_contours; i++) {
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "fpoly%ld.plot3", i);
	bg_polygon_plot(bu_vls_cstr(&fname), fpoly->contour[i].point, fpoly->contour[i].num_points, 0, 0, 255);
	bu_vls_free(&fname);
    }

    /* Use bn_plane_pt_at to produce the final 3d fill line polygon */
    struct bg_polygon *poly_fill;
    BU_GET(poly_fill, struct bg_polygon);
    poly_fill->num_contours = fpoly->num_contours;
    poly_fill->hole = (int *)bu_calloc(fpoly->num_contours, sizeof(int), "hole");
    poly_fill->contour = (struct bg_poly_contour *)bu_calloc(fpoly->num_contours, sizeof(struct bg_poly_contour), "f_contour");
    for (size_t i = 0; i < fpoly->num_contours; ++i) {
	poly_fill->hole[i] = fpoly->hole[i];
	poly_fill->contour[i].open = 1;
	poly_fill->contour[i].num_points = fpoly->contour[i].num_points;
	poly_fill->contour[i].point = (point_t *)bu_calloc(fpoly->contour[i].num_points, sizeof(point_t), "f_point");
	for (size_t j = 0; j < fpoly->contour[i].num_points; ++j) {
	    bn_plane_pt_at(&poly_fill->contour[i].point[j], pl, fpoly->contour[i].point[j][0], fpoly->contour[i].point[j][1]);
	}
    }

    for (size_t i = 0; i < poly_fill->num_contours; i++) {
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "poly3d%ld.plot3", i);
	bg_polygon_plot(bu_vls_cstr(&fname), poly_fill->contour[i].point, poly_fill->contour[i].num_points, 0, 0, 255);
	bu_vls_free(&fname);
    }



    bg_polygon_free(&poly_lines);
    bg_polygon_free(&poly_2d);
    bg_polygon_free(fpoly);
    BU_PUT(fpoly, struct bg_polygon);

    return poly_fill;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

