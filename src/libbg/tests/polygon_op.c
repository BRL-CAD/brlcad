/*                      P O L Y G O N _ O P . C
 *
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
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
/** @file polygon_op.c
 *
 * Test polygon boolean clipping routines
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "bg.h"
#include "bn/plot3.h"

static int
_bg_polygon_diff(struct bg_polygon *p1, struct bg_polygon *p2)
{
    if (!p1 && !p2) return 0;
    if ((!p1 && p2) || (p1 && !p2)) return 1;
    if (p1->num_contours != p2->num_contours) return 1;
    for (size_t i = 0; i < p1->num_contours; i++) {
	struct bg_poly_contour *c1 = &(p1->contour[i]);
	struct bg_poly_contour *c2 = &(p2->contour[i]);
	if (c1->num_points != c2->num_points) {
	    return 1;
	}
	for (size_t j = 0; j < c1->num_points; j++) {
	    if (DIST_PNT_PNT_SQ(c1->point[j], c2->point[j]) > VUNITIZE_TOL) {
		return 1;
	    }
	}
    }
    return 0;
}

int
main(int argc, const char **argv)
{
    int ret = 0;

    bu_setprogname(argv[0]);

    int plot_files = 0;
    if (argc > 1 && BU_STR_EQUAL(argv[1], "--plot")) {
	plot_files = 1;
    }

    /* Polygon 1 */
    struct bg_polygon p1 = BG_POLYGON_NULL;
    p1.num_contours = 1;
    p1.contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "c1 points");
    p1.contour[0].num_points = 4;
    p1.contour[0].point = (point_t *)bu_calloc(4, sizeof(point_t), "p1.contour[0] points");
    VSET(p1.contour[0].point[0], -3, -3, 0);
    VSET(p1.contour[0].point[1], 3, -3, 0);
    VSET(p1.contour[0].point[2], 3, 3, 0);
    VSET(p1.contour[0].point[3], -3, 3, 0);
    if (plot_files) {
	bg_polygon_plot("p1.plot3", (const point_t *)p1.contour[0].point, p1.contour[0].num_points, 255, 0, 0);
    }

    /* Polygon 2 */
    struct bg_polygon p2 = BG_POLYGON_NULL;
    p2.num_contours = 1;
    p2.contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "c1 points");
    p2.contour[0].num_points = 4;
    p2.contour[0].point = (point_t *)bu_calloc(4, sizeof(point_t), "p2.contour[0] points");
    VSET(p2.contour[0].point[0], 0, 0, 0);
    VSET(p2.contour[0].point[1], 4, 0, 0);
    VSET(p2.contour[0].point[2], 4, 5, 0);
    VSET(p2.contour[0].point[3], 0, 5, 0);
    if (plot_files) {
	bg_polygon_plot("p2.plot3", (const point_t *)p2.contour[0].point, p2.contour[0].num_points, 0, 255, 0);
    }

    /* Union expected result */
    struct bg_polygon union_expected = BG_POLYGON_NULL;
    union_expected.num_contours = 1;
    union_expected.contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "c1 points");
    union_expected.contour[0].num_points = 8;
    union_expected.contour[0].point = (point_t *)bu_calloc(8, sizeof(point_t), "union_expected.contour[0] points");
    VSET(union_expected.contour[0].point[0],  0,  5,  0);
    VSET(union_expected.contour[0].point[1],  0,  3,  0);
    VSET(union_expected.contour[0].point[2], -3,  3,  0);
    VSET(union_expected.contour[0].point[3], -3, -3,  0);
    VSET(union_expected.contour[0].point[4],  3, -3,  0);
    VSET(union_expected.contour[0].point[5],  3,  0,  0);
    VSET(union_expected.contour[0].point[6],  4,  0,  0);
    VSET(union_expected.contour[0].point[7],  4,  5,  0);

    /* Calculate union and compare it with the expected result */
    struct bg_polygon *ur = bg_clip_polygon(bg_Union, &p1, &p2, 1.0, NULL, NULL);
    if (plot_files) {
	bg_polygon_plot("ur.plot3", (const point_t *)ur->contour[0].point, ur->contour[0].num_points, 0, 0, 255);
    }
    ret += _bg_polygon_diff(ur, &union_expected);


    /* Difference expected result */
    struct bg_polygon difference_expected = BG_POLYGON_NULL;
    difference_expected.num_contours = 1;
    difference_expected.contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "c1 points");
    difference_expected.contour[0].num_points = 6;
    difference_expected.contour[0].point = (point_t *)bu_calloc(6, sizeof(point_t), "difference_expected.contour[0] points");
    VSET(difference_expected.contour[0].point[0], -3,  3,  0);
    VSET(difference_expected.contour[0].point[1], -3, -3,  0);
    VSET(difference_expected.contour[0].point[2],  3, -3,  0);
    VSET(difference_expected.contour[0].point[3],  3,  0,  0);
    VSET(difference_expected.contour[0].point[4],  0,  0,  0);
    VSET(difference_expected.contour[0].point[5],  0,  3,  0);

    /* Calculate difference and compare it with the expected result */
    struct bg_polygon *dr = bg_clip_polygon(bg_Difference, &p1, &p2, 1.0, NULL, NULL);
    if (plot_files) {
	bg_polygon_plot("dr.plot3", (const point_t *)dr->contour[0].point, dr->contour[0].num_points, 0, 0, 255);
    }
    ret += _bg_polygon_diff(dr, &difference_expected);

    /* Intersection expected result */
    struct bg_polygon intersection_expected = BG_POLYGON_NULL;
    intersection_expected.num_contours = 1;
    intersection_expected.contour = (struct bg_poly_contour *)bu_calloc(1, sizeof(struct bg_poly_contour), "c1 points");
    intersection_expected.contour[0].num_points = 4;
    intersection_expected.contour[0].point = (point_t *)bu_calloc(4, sizeof(point_t), "intersection_expected.contour[0] points");
    VSET(intersection_expected.contour[0].point[0],  0,  3,  0);
    VSET(intersection_expected.contour[0].point[1],  0,  0,  0);
    VSET(intersection_expected.contour[0].point[2],  3,  0,  0);
    VSET(intersection_expected.contour[0].point[3],  3,  3,  0);

    /* Calculate intersection and compare it with the expected result */
    struct bg_polygon *ir = bg_clip_polygon(bg_Intersection, &p1, &p2, 1.0, NULL, NULL);
    if (plot_files) {
	bg_polygon_plot("ir.plot3", (const point_t *)ir->contour[0].point, ir->contour[0].num_points, 0, 0, 255);
    }
    ret += _bg_polygon_diff(ir, &intersection_expected);

    /* Note - clipper doesn't yet support Xor */

    return ret;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
