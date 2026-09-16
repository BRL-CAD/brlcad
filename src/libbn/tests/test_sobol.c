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


/* Not public libbn API, but needed to preserve the old Joe/Kuo integrand
 * comparison between Sobol and pseudorandom sampling. */
extern double _sobol_urand(struct bn_soboldata *, double a, double b);


static double
sobol_testfunc(unsigned n, const double *x)
{
    double f = 1.0;
    unsigned j;

    for (j = 1; j <= n; ++j) {
	double cj = pow((double)j, 1.0 / 3.0);
	f *= (fabs(4.0 * x[j - 1] - 2.0) + cj) / (1.0 + cj);
    }

    return f;
}


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
	    if (!((p1[j] > lb[j] || EQUAL(p1[j], lb[j])) &&
		  (p1[j] < ub[j] || EQUAL(p1[j], ub[j])))) {
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
    point_t saved = {7.0, 8.0, 9.0};
    point_t sample = {7.0, 8.0, 9.0};
    fastf_t radius = 2.5;
    point_t first = VINIT_ZERO;
    point_t p1;
    point_t p2;
    point_t delta;
    vect_t mean = VINIT_ZERO;
    int saw_variation = 0;
    int i;

    s1 = bn_sobol_create(2, 2024UL);
    s2 = bn_sobol_create(2, 2024UL);

    bn_sobol_sph_sample(sample, center, 0.0, s1);
    if (!vect_close(sample, saved, 0.0)) {
	report_failure(test, "zero-radius Sobol sampling should leave the output unchanged");
	failures++;
    }

    bn_sobol_sph_sample(sample, center, 1.0, NULL);
    if (!vect_close(sample, saved, 0.0)) {
	report_failure(test, "NULL-state Sobol sampling should leave the output unchanged");
	failures++;
    }

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
	mean[X] += delta[X];
	mean[Y] += delta[Y];
	mean[Z] += delta[Z];
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

    VSCALE(mean, mean, 1.0 / 256.0);
    if (MAGNITUDE(mean) > radius * 0.15) {
	report_failure(test, "Sobol sample mean drifted too far from the sphere center");
	failures++;
    }

    bn_sobol_destroy(s1);
    bn_sobol_destroy(s2);

    return failures;
}


static int
test_sobol_integrand(void)
{
    int failures = 0;
    const char *test = "sobol_integrand";
    const unsigned sdim = 3;
    const unsigned n = 1000;
    double testint_sobol = 0.0;
    double testint_rand = 0.0;
    struct bn_soboldata *s = NULL;
    unsigned j;
    unsigned i;

    s = bn_sobol_create(sdim, 2026UL);
    bn_sobol_skip(s, n);

    for (j = 1; j <= n; ++j) {
	double *x = bn_sobol_next(s, NULL, NULL);
	testint_sobol += sobol_testfunc(sdim, x);
	for (i = 0; i < sdim; ++i) {
	    x[i] = _sobol_urand(s, 0.0, 1.0);
	}
	testint_rand += sobol_testfunc(sdim, x);
    }

    bn_sobol_destroy(s);

    testint_sobol /= (double)n;
    testint_rand /= (double)n;

    if (!isfinite(testint_sobol) || !isfinite(testint_rand)) {
	report_failure(test, "integrand estimates were not finite");
	failures++;
    }

    if (fabs(testint_sobol - 1.0) > 0.01) {
	report_failure(test, "Sobol integration estimate drifted too far from 1.0: got %.12g", testint_sobol);
	failures++;
    }

    if (fabs(testint_sobol - 1.0) > fabs(testint_rand - 1.0) + 0.01) {
	report_failure(test, "Sobol integration was unexpectedly worse than pseudorandom: sobol=%.12g rand=%.12g",
	    testint_sobol, testint_rand);
	failures++;
    }

    return failures;
}


static const struct bn_api_case sobol_cases[] = {
    {"skip", test_sobol_skip},
    {"bounds", test_sobol_bounds},
    {"sphere", test_sobol_sphere},
    {"integrand", test_sobol_integrand},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_dispatch(argc, argv, sobol_cases);
}
