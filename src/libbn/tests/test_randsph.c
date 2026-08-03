/*                    T E S T _ R A N D S P H . C
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
test_randsph_invalid(void)
{
    int failures = 0;
    const char *test = "randsph_invalid";
    point_t center = {1.0, 2.0, 3.0};
    point_t saved = {7.0, 8.0, 9.0};
    point_t sample = {7.0, 8.0, 9.0};
    struct bn_soboldata *s = NULL;

    bn_rand_sph_sample(sample, center, 0.0);
    if (!vect_close(sample, saved, 0.0)) {
	report_failure(test, "zero-radius bn_rand_sph_sample should leave the sample unchanged");
	failures++;
    }

    bn_rand_sph_sample(sample, NULL, 1.0);
    if (!vect_close(sample, saved, 0.0)) {
	report_failure(test, "NULL-center bn_rand_sph_sample should leave the sample unchanged");
	failures++;
    }

    s = bn_sobol_create(2, 99UL);
    bn_sobol_sph_sample(sample, center, 0.0, s);
    if (!vect_close(sample, saved, 0.0)) {
	report_failure(test, "zero-radius bn_sobol_sph_sample should leave the sample unchanged");
	failures++;
    }

    bn_sobol_sph_sample(sample, center, 1.0, NULL);
    if (!vect_close(sample, saved, 0.0)) {
	report_failure(test, "NULL-state bn_sobol_sph_sample should leave the sample unchanged");
	failures++;
    }

    bn_sobol_destroy(s);
    return failures;
}


static int
test_randsph_rand_geometry(void)
{
    int failures = 0;
    const char *test = "randsph_rand_geometry";
    point_t center = {5.0, -3.0, 2.0};
    fastf_t radius = 4.5;
    point_t sample;
    point_t delta;
    point_t saved[32];
    vect_t mean = VINIT_ZERO;
    int saw_variation = 0;
    int i;

    bn_randmt_seed(314159UL);
    for (i = 0; i < 4096; i++) {
	bn_rand_sph_sample(sample, center, radius);
	VSUB2(delta, sample, center);
	if (!scalar_close(MAGNITUDE(delta), radius, 1.0e-6)) {
	    report_failure(test, "sample %d was not on the requested sphere", i);
	    failures++;
	    break;
	}
	mean[X] += delta[X];
	mean[Y] += delta[Y];
	mean[Z] += delta[Z];
	if (i == 0) {
	    VMOVE(saved[0], sample);
	} else if (!vect_close(sample, saved[0], DBL_EPSILON * 8.0)) {
	    saw_variation = 1;
	}
	if (i < 32) {
	    VMOVE(saved[i], sample);
	}
    }

    if (!saw_variation) {
	report_failure(test, "bn_rand_sph_sample produced a degenerate constant sequence");
	failures++;
    }

    VSCALE(mean, mean, 1.0 / 4096.0);
    if (MAGNITUDE(mean) > radius * 0.15) {
	report_failure(test, "sample mean drifted too far from the sphere center");
	failures++;
    }

    bn_randmt_seed(314159UL);
    for (i = 0; i < 32; i++) {
	bn_rand_sph_sample(sample, center, radius);
	if (!vect_close(sample, saved[i], DBL_EPSILON * 8.0)) {
	    report_failure(test, "bn_rand_sph_sample was not repeatable after reseeding at step %d", i);
	    failures++;
	    break;
	}
    }

    return failures;
}


static int
test_randsph_sobol_geometry(void)
{
    int failures = 0;
    const char *test = "randsph_sobol_geometry";
    struct bn_soboldata *s1 = NULL;
    struct bn_soboldata *s2 = NULL;
    point_t center = {1.0, 2.0, 3.0};
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

    for (i = 0; i < 256; i++) {
	bn_sobol_sph_sample(p1, center, radius, s1);
	bn_sobol_sph_sample(p2, center, radius, s2);
	if (!vect_close(p1, p2, DBL_EPSILON * 8.0)) {
	    report_failure(test, "bn_sobol_sph_sample was not repeatable");
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
	report_failure(test, "bn_sobol_sph_sample produced a degenerate constant sequence");
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


static const struct bn_api_case randsph_cases[] = {
    {"invalid", test_randsph_invalid},
    {"rand", test_randsph_rand_geometry},
    {"sobol", test_randsph_sobol_geometry},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_dispatch(argc, argv, randsph_cases);
}
