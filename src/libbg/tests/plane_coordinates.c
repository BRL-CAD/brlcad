/*              P L A N E _ C O O R D I N A T E S . C
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


int
main(int argc, char **argv)
{
    const point_t plane_point = {0.0, 0.0, 3.0};
    const vect_t original_normal = {0.0, 0.0, 2.0};
    const vect_t expected_normal = {0.0, 0.0, 1.0};
    vect_t normal;
    vect_t zero_normal = VINIT_ZERO;
    plane_t plane;
    plane_t unnormalized_plane = {0.0, 0.0, 2.0, 6.0};
    point_t query = {2.0, 2.0, 3.0};
    point_t off_plane_query = {2.0, 2.0, 5.0};
    const point_t expected_projection = {2.0, 2.0, 3.0};
    point_t reconstructed = VINIT_ZERO;
    fastf_t u, v;
    int failed = 0;

    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    VMOVE(normal, original_normal);
    if (bg_plane_pt_nrml(&plane, plane_point, normal) ||
	!VNEAR_EQUAL(plane, expected_normal, BN_TOL_DIST) ||
	!NEAR_EQUAL(plane[H], 3.0, BN_TOL_DIST) ||
	!VNEAR_EQUAL(normal, original_normal, BN_TOL_DIST)) {
	failed = 1;
    }

    if (bg_plane_pt_nrml(&plane, plane_point, zero_normal) != -1)
	failed = 1;

    if (bg_plane_closest_pt(&u, &v, &unnormalized_plane, &query) ||
	bg_plane_pt_at(&reconstructed, &unnormalized_plane, u, v) ||
	!VNEAR_EQUAL(reconstructed, query, BN_TOL_DIST)) {
	failed = 1;
    }

    if (bg_plane_closest_pt(&u, &v, &unnormalized_plane, &off_plane_query) ||
	bg_plane_pt_at(&reconstructed, &unnormalized_plane, u, v) ||
	!VNEAR_EQUAL(reconstructed, expected_projection, BN_TOL_DIST)) {
	failed = 1;
    }

    return failed;
}
