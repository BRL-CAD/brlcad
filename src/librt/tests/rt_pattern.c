/*                   R T _ P A T T E R N . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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

#include "vmath.h"
#include "bu/app.h"
#include "bv/plot3.h"
#include "raytrace.h"

int validate_sph_qrand(struct rt_pattern_data* data) {
    /* Validate the quasi-random spherical sampling pattern: every ray must
     * share the seed origin and carry a unit-length direction, the mean of
     * the directions must be near zero (unbiased), and the directions must
     * be spread evenly across all eight octants (good coverage). */
    const size_t expected = data->n_p[0];
    /* arbitrary-ish tols */
    const fastf_t unit_tol = 1.0e-6;
    const fastf_t centroid_tol = 0.1;
    /* range for acceptable distribution (oct_lo, oct_hi) */
    const int oct_lo = 90;
    const int oct_hi = 165;
    vect_t centroid = VINIT_ZERO;
    int octants[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    fastf_t cmag;
    int oct;

    if (data->ray_cnt != expected) {
	bu_log("FAIL: expected %zu rays, got %zu\n", expected, data->ray_cnt);
	return 1;
    }

    for (int i = 0; i < data->ray_cnt; i++) {
	point_t base;
	vect_t dir;
	VSET(base, data->rays[6*i], data->rays[6*i+1], data->rays[6*i+2]);
	VSET(dir, data->rays[6*i+3], data->rays[6*i+4], data->rays[6*i+5]);

	if (!VNEAR_EQUAL(base, data->center_pt, unit_tol)) {
	    bu_log("FAIL: ray %zu origin (%g %g %g) is not at center_pt\n", i, V3ARGS(base));
	    return 1;
	}
	if (!NEAR_EQUAL(MAGNITUDE(dir), 1.0, unit_tol)) {
	    bu_log("FAIL: ray %zu direction not unit length (mag %g)\n", i, MAGNITUDE(dir));
	    return 1;
	}

	VADD2(centroid, centroid, dir);
	oct = (dir[X] >= 0 ? 1 : 0) + (dir[Y] >= 0 ? 2 : 0) + (dir[Z] >= 0 ? 4 : 0);
	octants[oct]++;
    }

    if (data->ray_cnt > 0) {
	VSCALE(centroid, centroid, 1.0 / (fastf_t)data->ray_cnt);
	cmag = MAGNITUDE(centroid);
	if (cmag > centroid_tol) {
	    bu_log("FAIL: direction centroid magnitude %g exceeds %g (biased sampling)\n", cmag, centroid_tol);
	    return 1;
	} else {
	    bu_log("direction centroid magnitude: %g (unbiased)\n", cmag);
	}

	for (oct = 0; oct < 8; oct++) {
	    bu_log("octant %d: %d rays\n", oct, octants[oct]);
	    if (octants[oct] < oct_lo || octants[oct] > oct_hi) {
		bu_log("FAIL: octant %d count %d outside expected coverage band [%d, %d]\n",
			oct, octants[oct], oct_lo, oct_hi);
		return 1;
	    }
	}
    }

    /* success */
    bu_log("SPH_QRAND validation PASSED\n");
    return 0;
}

int
main(int argc, char *argv[])
{
    FILE *fp = NULL;
    size_t i = 0;
    int test_case = 0;
    rt_pattern_t pattern_type = RT_PATTERN_UNKNOWN;
    struct rt_pattern_data data = RT_PATTERN_DATA_INIT_ZERO;
    vect_t *vects = (vect_t *)bu_calloc(5, sizeof(vect_t), "vects array");
    fastf_t *params = (fastf_t *)bu_calloc(5, sizeof(fastf_t), "params array");

    bu_setprogname(argv[0]);

    data.rays = NULL;
    VSET(data.center_pt, 0, 0, 2000);
    VSET(data.center_dir, 0, 0, -1);
    data.n_vec = vects;
    data.n_p = params;

    if (argc < 2)
	bu_exit(1, "Usage: %s <test number>\n", argv[0]);

    test_case = atoi(argv[1]);

    if (test_case < 0)
	bu_exit(1, "Usage: %s <test_number>\n", argv[0]);

    /* Set up the specifics of the test */
    switch (test_case) {
	case 0:
	    bu_log("Running test 0 (null inputs)\n");
	    break;
	case 1:
	    bu_log("Running test 1 (rectangular orthogonal grid)\n");
	    fp = fopen("rt_pattern_test_1.plot3", "wb");
	    pattern_type = RT_PATTERN_RECT_ORTHOGRID;
	    data.vn = 2;
	    data.pn = 2;
	    VSET(data.n_vec[0], 1000, 0, 0);
	    VSET(data.n_vec[1], 0, 1000, 0);
	    data.n_p[0] = 50;
	    data.n_p[1] = 50;
	    break;
	case 2:
	    bu_log("Running test 2 (rectangular perspective grid) - NOT YET WORKING\n");
	    fp = fopen("rt_pattern_test_2.plot3", "wb");
	    pattern_type = RT_PATTERN_RECT_PERSPGRID;
	    data.vn = 2;
	    data.pn = 4;
	    VSET(data.n_vec[0], 1000, 0, 0);
	    VSET(data.n_vec[1], 0, 1000, 0);
	    data.n_p[0] = M_PI/2;
	    data.n_p[1] = M_PI/2;
	    data.n_p[2] = 25;
	    data.n_p[3] = 45;
	    break;
	case 5:
	    bu_log("Running test 5 (circular skew)\n");
	    fp = fopen("rt_pattern_test_5.plot3", "wb");
	    pattern_type = RT_PATTERN_CIRC_SPIRAL;
	    data.vn = 0;
	    data.pn = 4;
	    data.n_p[0] = 1000.0;
	    data.n_p[1] = 50.0;
	    data.n_p[2] = 30.0;
	    data.n_p[3] = M_2PI / data.n_p[1]; /* to match rt_raybundle_maker */
	    break;
	case 6:
	    bu_log("Running test 6 (quasi-random unbiased spherical sampling)\n");
	    fp = fopen("rt_pattern_test_6.plot3", "wb");
	    pattern_type = RT_PATTERN_SPH_QRAND;
	    data.vn = 0;
	    data.pn = 1;
	    data.n_p[0] = 1000.0; /* number of rays */
	    break;
	default:
	    bu_exit(1, "Unknown test number %d\n", test_case);
	    break;
    }

    if (rt_pattern(&data, pattern_type) < 0) {
	bu_exit(1, "Error running rt_pattern\n");
    }

    if (fp) {
	for (i=0; i <= data.ray_cnt; i++) {
	    point_t base;
	    vect_t dir;
	    point_t tip;
	    VSET(base, data.rays[6*i], data.rays[6*i+1], data.rays[6*i+2]);
	    VSET(dir, data.rays[6*i+3], data.rays[6*i+4], data.rays[6*i+5]);
	    VJOIN1(tip, base, 3500, dir);
	    pdv_3line(fp, base, tip);
	}
	fclose(fp);
    }

    /* validate test case */
    /* TODO/FIXME: implement 1-5 :) */
    int ret = 0;
    switch (test_case) {
	case 6:
	    ret = validate_sph_qrand(&data);
	    break;
	default:
	    break;
    }

    bu_free(vects, "vects");
    bu_free(params, "params");
    if (data.rays) bu_free(data.rays, "rays");

    return ret;
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
