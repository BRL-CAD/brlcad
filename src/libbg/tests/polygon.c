/*                       P O L Y G O N . C
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
 */

#include "common.h"

#include "bn.h"
#include "bu.h"
#include "bg.h"


static int
test_view_bbox(void)
{
    point_t outer[] = {
	{0.0, 0.0, 0.0},
	{2.0, 0.0, 0.0},
	{2.0, 2.0, 0.0},
	{0.0, 2.0, 0.0}
    };
    point_t hole[] = {
	{100.0, 100.0, 0.0},
	{101.0, 100.0, 0.0},
	{101.0, 101.0, 0.0},
	{100.0, 101.0, 0.0}
    };
    struct bg_poly_contour contours[] = {
	{4, outer, 0},
	{4, hole, 0}
    };
    int holes[] = {0, 1};
    struct bg_polygon polygon = BG_POLYGON_NULL;
    mat_t model2view;
    point2d_t bmin, bmax;

    polygon.num_contours = 2;
    polygon.hole = holes;
    polygon.contour = contours;
    MAT_IDN(model2view);
    bg_polygon_view_bbox(&bmin, &bmax, &polygon, model2view);

    return !NEAR_EQUAL(bmin[X], 0.0, BN_TOL_DIST)
	|| !NEAR_EQUAL(bmin[Y], 0.0, BN_TOL_DIST)
	|| !NEAR_EQUAL(bmax[X], 2.0, BN_TOL_DIST)
	|| !NEAR_EQUAL(bmax[Y], 2.0, BN_TOL_DIST);
}


static int
test_centroid(void)
{
    const point_t rectangle[] = {
	{5.0, 0.0, 0.0},
	{5.0, 4.0, 0.0},
	{5.0, 4.0, 2.0},
	{5.0, 0.0, 2.0}
    };
    const point_t expected = {5.0, 2.0, 1.0};
    point_t output[] = {
	{11.0, 12.0, 13.0},
	{0.0, 0.0, 0.0},
	{17.0, 18.0, 19.0},
	{23.0, 24.0, 25.0}
    };
    const point_t before = {11.0, 12.0, 13.0};
    const point_t after = {17.0, 18.0, 19.0};
    const point_t farther_after = {23.0, 24.0, 25.0};

    return bg_3d_polygon_centroid(&output[1], 4, rectangle)
	|| !VNEAR_EQUAL(output[1], expected, BN_TOL_DIST)
	|| !VNEAR_EQUAL(output[0], before, BN_TOL_DIST)
	|| !VNEAR_EQUAL(output[2], after, BN_TOL_DIST)
	|| !VNEAR_EQUAL(output[3], farther_after, BN_TOL_DIST);
}


static int
test_ccw_sort(void)
{
    point_t points[] = {
	{10.0, 10.0, 0.0},
	{10.1, 10.1, 0.0},
	{10.1, 10.0, 0.0},
	{10.0, 10.1, 0.0}
    };
    plane_t plane = {0.0, 0.0, 1.0, 0.0};
    vect_t total = VINIT_ZERO;
    vect_t cross;

    if (bg_3d_polygon_sort_ccw(4, points, plane))
	return 1;

    for (size_t i = 0; i < 4; i++) {
	size_t next = i + 1 == 4 ? 0 : i + 1;
	VCROSS(cross, points[i], points[next]);
	VADD2(total, total, cross);
    }

    return total[Z] <= SMALL_FASTF;
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    return test_view_bbox() || test_centroid() || test_ccw_sort();
}
