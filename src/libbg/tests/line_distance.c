/*             L I N E _ D I S T A N C E . C
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

#include "common.h"

#include <math.h>

#include "bu.h"
#include "bg.h"


static int
check_value(const char *name, fastf_t expected, fastf_t actual)
{
    if (NEAR_EQUAL(expected, actual, BN_TOL_DIST))
	return 0;

    bu_log("%s: expected %g, got %g\n", name, expected, actual);
    return 1;
}


int
main(int argc, char **argv)
{
    point_t line_pt = VINIT_ZERO;
    point_t sample_pt = VINIT_ZERO;
    point_t projection_pt = VINIT_ZERO;
    point_t ray_pt = VINIT_ZERO;
    point_t seg_pt = VINIT_ZERO;
    point_t first_segment_pt = VINIT_ZERO;
    point_t second_segment_pt = VINIT_ZERO;
    point_t legacy_pca = VINIT_ZERO;
    point_t first_line_pt = VINIT_ZERO;
    point_t second_line_pt = VINIT_ZERO;
    vect_t zero_dir = VINIT_ZERO;
    vect_t nonunit_dir = VINIT_ZERO;
    vect_t seg_dir = VINIT_ZERO;
    vect_t first_segment_dir = VINIT_ZERO;
    vect_t second_segment_dir = VINIT_ZERO;
    vect_t first_line_dir = VINIT_ZERO;
    vect_t second_line_dir = VINIT_ZERO;
    fastf_t isect_dist[2] = {0.0, 0.0};
    fastf_t segment_dist[2] = {0.0, 0.0};
    fastf_t line_dist[2] = {0.0, 0.0};
    fastf_t legacy_dist = 0.0;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    fastf_t long_segment_length;
    fastf_t near_endpoint_offset;
    int failures = 0;

    bu_setprogname(argv[0]);

    if (argc != 1)
	bu_exit(1, "ERROR: [%s] takes no arguments\n", argv[0]);

    VSET(line_pt, 1.0, 2.0, 3.0);
    VSET(sample_pt, 4.0, 6.0, 3.0);
    failures += check_value("3D zero-direction distance", 5.0,
	bg_dist_line3_pnt3(line_pt, zero_dir, sample_pt));
    failures += check_value("3D zero-direction squared distance", 25.0,
	bg_distsq_line3_pnt3(line_pt, zero_dir, sample_pt));
    failures += check_value("3D zero-direction origin distance", sqrt(14.0),
	bg_dist_line_origin(line_pt, zero_dir));
    if (bg_dist_pnt3_line3(&legacy_dist, legacy_pca, line_pt, zero_dir,
	    sample_pt, &tol) != 2
	|| !NEAR_EQUAL(legacy_dist, 5.0, BN_TOL_DIST)
	|| !NEAR_EQUAL(legacy_pca[X], line_pt[X], BN_TOL_DIST)
	|| !NEAR_EQUAL(legacy_pca[Y], line_pt[Y], BN_TOL_DIST)
	|| !NEAR_EQUAL(legacy_pca[Z], line_pt[Z], BN_TOL_DIST)) {
	bu_log("legacy 3D zero-direction point-to-line distance failed\n");
	failures++;
    }

    VSET(line_pt, 1.0, 2.0, 0.0);
    VSET(sample_pt, 4.0, 6.0, 0.0);
    failures += check_value("2D zero-direction distance", 5.0,
	bg_dist_line2_point2(line_pt, zero_dir, sample_pt));
    failures += check_value("2D zero-direction squared distance", 25.0,
	bg_distsq_line2_point2(line_pt, zero_dir, sample_pt));

    VSET(line_pt, 1.0, 2.0, 3.0);
    VSET(nonunit_dir, 2.0, 0.0, 0.0);
    VSET(projection_pt, 5.0, 7.0, 3.0);
    failures += check_value("3D nonunit line parameter", 2.0,
	bg_dist_pnt3_along_line3(line_pt, nonunit_dir, projection_pt));

    VSET(line_pt, 1.0, 2.0, 0.0);
    VSET(projection_pt, 5.0, 7.0, 0.0);
    failures += check_value("2D nonunit line parameter", 2.0,
	bg_dist_pnt2_along_line2(line_pt, nonunit_dir, projection_pt));

    VSET(ray_pt, 0.0, 0.0, 0.0);
    VSET(seg_pt, 4.0, 0.0, 0.0);
    VSET(seg_dir, 2.0, 0.0, 0.0);
    if (bg_isect_line2_lseg2(isect_dist, ray_pt, nonunit_dir, seg_pt, seg_dir, &tol) != 0
	|| !NEAR_EQUAL(isect_dist[0], 2.0, BN_TOL_DIST)
	|| !NEAR_EQUAL(isect_dist[1], 3.0, BN_TOL_DIST)) {
	bu_log("2D collinear line/segment parameters: expected 2,3, got %g,%g\n",
	       isect_dist[0], isect_dist[1]);
	failures++;
    }

    VSET(ray_pt, 0.0, 0.0, 0.0);
    VSET(nonunit_dir, 1.0, 0.0, 0.0);
    VSET(seg_pt, -1.0, -1.0, 0.0);
    VSET(seg_dir, 0.0, 2.0, 0.0);
    if (bg_isect_line2_lseg2(isect_dist, ray_pt, nonunit_dir, seg_pt, seg_dir, &tol) != 3
	|| !NEAR_EQUAL(isect_dist[0], -1.0, BN_TOL_DIST)
	|| !NEAR_EQUAL(isect_dist[1], 0.5, BN_TOL_DIST)) {
	bu_log("2D infinite line/segment parameters: expected -1,0.5, got %g,%g\n",
	       isect_dist[0], isect_dist[1]);
	failures++;
    }

    /* The second segment's endpoint tolerance must use its own length. */
    long_segment_length = 1.0 / tol.dist;
    near_endpoint_offset = tol.dist * 0.5;
    VSET(first_segment_pt, 0.0, 0.0, 0.0);
    VSET(first_segment_dir, long_segment_length, 0.0, 0.0);
    VSET(second_segment_pt, long_segment_length * 0.5,
	-near_endpoint_offset, 0.0);
    VSET(second_segment_dir, 0.0, 1.0, 0.0);
    if (bg_isect_lseg2_lseg2(segment_dist, first_segment_pt,
	    first_segment_dir, second_segment_pt, second_segment_dir, &tol) != 1
	|| !NEAR_EQUAL(segment_dist[0], 0.5, BN_TOL_DIST)
	|| !NEAR_ZERO(segment_dist[1], BN_TOL_DIST)) {
	bu_log("2D unequal-segment endpoint tolerance: expected 0.5,0, got %g,%g\n",
	       segment_dist[0], segment_dist[1]);
	failures++;
    }

    VSET(first_segment_pt, 0.0, 0.0, 0.0);
    VSET(first_segment_dir, 1.0, 0.0, 0.0);
    VSET(second_segment_pt, 0.0, 1.0, 0.0);
    VSET(second_segment_dir, 1.0, 0.0, 0.0);
    if (bg_isect_lseg2_lseg2(segment_dist, first_segment_pt,
	    first_segment_dir, second_segment_pt, second_segment_dir, &tol) != -2) {
	bu_log("2D parallel segment intersection did not return -2\n");
	failures++;
    }

    VSET(first_line_pt, 0.0, 0.0, 0.0);
    VSET(first_line_dir, 1.0, 0.0, 0.0);
    VSET(second_line_pt, 0.0, 1.0, 0.0);
    VSET(second_line_dir, 0.0, -1.0, 0.0);
    if (bg_isect_line3_line3(&line_dist[0], &line_dist[1], first_line_pt,
	    first_line_dir, second_line_pt, second_line_dir, &tol) != 1
	|| !NEAR_EQUAL(line_dist[0], 0.0, BN_TOL_DIST)
	|| !NEAR_EQUAL(line_dist[1], 1.0, BN_TOL_DIST)) {
	bu_log("3D unit-vector line intersection: expected 0,1, got %g,%g\n",
	       line_dist[0], line_dist[1]);
	failures++;
    }

    return failures;
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
