/*                           P C A . C
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
orthonormal_axes(const vect_t xaxis, const vect_t yaxis, const vect_t zaxis)
{
    return !NEAR_EQUAL(MAGSQ(xaxis), 1.0, BN_TOL_DIST)
	|| !NEAR_EQUAL(MAGSQ(yaxis), 1.0, BN_TOL_DIST)
	|| !NEAR_EQUAL(MAGSQ(zaxis), 1.0, BN_TOL_DIST)
	|| !NEAR_ZERO(VDOT(xaxis, yaxis), BN_TOL_DIST)
	|| !NEAR_ZERO(VDOT(xaxis, zaxis), BN_TOL_DIST)
	|| !NEAR_ZERO(VDOT(yaxis, zaxis), BN_TOL_DIST);
}


int
main(int argc, char **argv)
{
    const point_t one_point[] = {{1.0, 2.0, 3.0}};
    const point_t two_points[] = {{1.0, 2.0, 3.0}, {5.0, 2.0, 3.0}};
    const point_t one_center = {1.0, 2.0, 3.0};
    const point_t two_center = {3.0, 2.0, 3.0};
    const vect_t xaxis_expected = {1.0, 0.0, 0.0};
    point_t center;
    vect_t xaxis, yaxis, zaxis;
    fastf_t alignment;
    int failed = 0;

    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    if (bg_pca(&center, &xaxis, &yaxis, &zaxis, 1, one_point) != BRLCAD_OK ||
	!VNEAR_EQUAL(center, one_center, BN_TOL_DIST) ||
	orthonormal_axes(xaxis, yaxis, zaxis)) {
	failed = 1;
    }

    if (bg_pca(&center, &xaxis, &yaxis, &zaxis, 2, two_points) != BRLCAD_OK ||
	!VNEAR_EQUAL(center, two_center, BN_TOL_DIST) ||
	orthonormal_axes(xaxis, yaxis, zaxis)) {
	failed = 1;
    }

    alignment = VDOT(xaxis, xaxis_expected);
    if (!NEAR_EQUAL(alignment * alignment, 1.0, BN_TOL_DIST))
	failed = 1;

    return failed;
}
