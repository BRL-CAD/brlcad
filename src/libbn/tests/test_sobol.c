/*                       T E S T _ S O B O L . C
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

#include "test_util.h"


static int
test_sobol_skip(void)
{
    int failures = 0;
    const char *test = "sobol_skip";
    struct bn_soboldata *a = NULL;
    struct bn_soboldata *b = NULL;
    double *pa;
    double *pb;
    int i;

    a = bn_sobol_create(3, 99UL);
    b = bn_sobol_create(3, 99UL);

    for (i = 0; i < 25; i++) {
	(void)bn_sobol_next(a, NULL, NULL);
    }
    bn_sobol_skip(b, 25);

    for (i = 0; i < 10; i++) {
	pa = bn_sobol_next(a, NULL, NULL);
	pb = bn_sobol_next(b, NULL, NULL);
	if (!scalar_close(pa[0], pb[0], 0.0) ||
	    !scalar_close(pa[1], pb[1], 0.0) ||
	    !scalar_close(pa[2], pb[2], 0.0)) {
	    report_failure(test, "bn_sobol_skip did not land on the same sequence point as repeated bn_sobol_next calls");
	    failures++;
	    break;
	}
    }

    bn_sobol_destroy(a);
    bn_sobol_destroy(b);

    return failures;
}


static int
test_sobol_bounds(void)
{
    int failures = 0;
    const char *test = "sobol_bounds";
    struct bn_soboldata *s1 = NULL;
    struct bn_soboldata *s2 = NULL;
    double lb[3] = {-1.0, 10.0, 100.0};
    double ub[3] = {1.0, 20.0, 120.0};
    double *p1;
    double *p2;
    int i;
    int j;

    s1 = bn_sobol_create(3, 777UL);
    s2 = bn_sobol_create(3, 777UL);

    for (i = 0; i < 128; i++) {
	p1 = bn_sobol_next(s1, lb, ub);
	p2 = bn_sobol_next(s2, lb, ub);
	for (j = 0; j < 3; j++) {
            if (!EQUAL(p1[j], p2[j])) {
		report_failure(test, "scaled Sobol sequence was not repeatable at step %d", i);
		failures++;
		goto cleanup;
	    }
	    if (!((p1[j] > lb[j] || EQUAL(p1[j], lb[j])) && (p1[j] < ub[j] || EQUAL(p1[j], ub[j])))) {
		report_failure(test, "scaled Sobol sample left the requested bounds");
		failures++;
	    }
	}
    }

cleanup:
    bn_sobol_destroy(s1);
    bn_sobol_destroy(s2);

    return failures;
}


static int
test_sobol_sphere(void)
{
    int failures = 0;
    const char *test = "sobol_sphere";
    struct bn_soboldata *s1 = NULL;
    struct bn_soboldata *s2 = NULL;
    point_t center = {1.0, 2.0, 3.0};
    fastf_t radius = 2.5;
    point_t first = VINIT_ZERO;
    point_t p1;
    point_t p2;
    point_t delta;
    int saw_variation = 0;
    int i;

    s1 = bn_sobol_create(2, 2024UL);
    s2 = bn_sobol_create(2, 2024UL);

    for (i = 0; i < 256; i++) {
	bn_sobol_sph_sample(p1, center, radius, s1);
	bn_sobol_sph_sample(p2, center, radius, s2);
	if (!vect_close(p1, p2, DBL_EPSILON * 8.0)) {
	    report_failure(test, "Sobol sphere sampling was not repeatable");
	    failures++;
	    break;
	}
	VSUB2(delta, p1, center);
	if (!scalar_close(MAGNITUDE(delta), radius, 1.0e-6)) {
	    report_failure(test, "Sobol sphere sample %d was not on the requested sphere", i);
	    failures++;
	    break;
	}
	if (i == 0) {
	    VMOVE(first, p1);
	} else if (!vect_close(p1, first, DBL_EPSILON * 8.0)) {
	    saw_variation = 1;
	}
    }

    if (!saw_variation) {
	report_failure(test, "Sobol sphere sampler produced a degenerate constant sequence");
	failures++;
    }

    bn_sobol_destroy(s1);
    bn_sobol_destroy(s2);

    return failures;
}


static const struct bn_api_case sobol_cases[] = {
    {"skip", test_sobol_skip},
    {"bounds", test_sobol_bounds},
    {"sphere", test_sobol_sphere},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    return bn_api_dispatch(argc, argv, sobol_cases);
}
