/*                      T E S T _ N O I S E . C
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

#include "bu/parallel.h"

#include "test_util.h"

extern void bn_ensure_initialized(void);
extern int bn_noise_semaphore(void);


#define NOISE_NTHREADS 8
#define NOISE_ITERS 200

struct noise_concurrent_result {
    int failed;
};

static struct noise_concurrent_result noise_concurrent_results[NOISE_NTHREADS];


static void
noise_worker(int cpu, void *UNUSED(data))
{
    int i;
    int local_fail = 0;
    const double h = 1.0;
    const double lacunarity = 2.1753974;
    const double octaves = 4.0;

    for (i = 0; i < NOISE_ITERS; i++) {
	point_t pt_large;
	point_t pt_normal;
	double turb;
	double fbm;

	VSET(pt_large, (double)cpu * 1e36, (double)i * 1e36, 1e36);
	VSET(pt_normal, (double)cpu + (double)i * 0.01, (double)i * 0.1, 0.5);

	turb = bn_noise_turb(pt_large, h, lacunarity, octaves);
	fbm = bn_noise_fbm(pt_normal, h, lacunarity, octaves);

	if (!isfinite(turb) || !isfinite(fbm)) {
	    local_fail++;
	}
    }

    if (cpu >= 1 && cpu <= NOISE_NTHREADS) {
	noise_concurrent_results[cpu - 1].failed = local_fail;
    }
}


static int
test_noise_extended(void)
{
    int failures = 0;
    const char *test = "noise_extended";
    point_t pt = {0.125, -2.75, 99.5};
    point_t vec1 = VINIT_ZERO;
    point_t vec2 = VINIT_ZERO;
    double p1;
    double p2;
    double turb;
    double fbm;
    double mf;
    double ridged;

    bn_noise_init();

    p1 = bn_noise_perlin(pt);
    p2 = bn_noise_perlin(pt);
    if (!isfinite(p1) || !scalar_close(p1, p2, 0.0)) {
	report_failure(test, "bn_noise_perlin was not deterministic and finite");
	failures++;
    }

    bn_noise_vec(pt, vec1);
    bn_noise_vec(pt, vec2);
    if (!finite_vec(vec1) || !vect_close(vec1, vec2, DBL_EPSILON * 8.0)) {
	report_failure(test, "bn_noise_vec was not deterministic and finite");
	failures++;
    }

    turb = bn_noise_turb(pt, 1.0, 2.1753974, 4.0);
    fbm = bn_noise_fbm(pt, 1.0, 2.1753974, 4.0);
    mf = bn_noise_mf(pt, 1.0, 2.1753974, 4.0, 1.0);
    ridged = bn_noise_ridged(pt, 1.0, 2.1753974, 4.0, 1.0);

    if (!isfinite(turb) || !isfinite(fbm) || !isfinite(mf) || !isfinite(ridged)) {
	report_failure(test, "one or more advanced noise APIs returned non-finite results");
	failures++;
    }
    if (turb < 0.0) {
	report_failure(test, "bn_noise_turb should not return a negative turbulence magnitude");
	failures++;
    }

    return failures;
}


static int
test_noise_init(void)
{
    int failures = 0;
    const char *test = "noise_init";
    point_t pt = {0.125, -2.75, 99.5};
    double v1;
    double v2;
    int sem1;
    int sem2;

    sem1 = bn_noise_semaphore();
    sem2 = bn_noise_semaphore();
    if (sem1 != sem2) {
	report_failure(test, "bn_noise_semaphore returned inconsistent ids");
	failures++;
    }

    bn_ensure_initialized();
    bn_ensure_initialized();

    v1 = bn_noise_perlin(pt);
    v2 = bn_noise_perlin(pt);
    if (!isfinite(v1) || !scalar_close(v1, v2, 0.0)) {
	report_failure(test, "bn_ensure_initialized did not produce deterministic usable noise state");
	failures++;
    }

    return failures;
}


static int
test_noise_large_coords(void)
{
    int failures = 0;
    const char *test = "noise_large_coords";
    size_t i;

    static const struct {
	double x, y, z;
    } cases[] = {
	{1e19, 1e19, 1e19},
	{1e20, 1e20, 1e20},
	{1e36, 1e36, 1e36},
	{1e50, 1e50, 1e50},
	{1e100, 1e100, 1e100},
	{1e300, 1e300, 1e300},
	{1.0, 2.0, 3.0},
	{1000.0, 1000.0, 1000.0}
    };

    bn_noise_init();

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
	point_t pt;
	double turb;
	double fbm;

	VSET(pt, cases[i].x, cases[i].y, cases[i].z);
	turb = bn_noise_turb(pt, 1.0, 2.1753974, 4.0);
	fbm = bn_noise_fbm(pt, 1.0, 2.1753974, 4.0);

	if (!isfinite(turb) || !isfinite(fbm)) {
	    report_failure(test, "large-coordinate case %zu returned a non-finite result", i);
	    failures++;
	}
    }

    return failures;
}


static int
test_noise_concurrent_common(const char *test, int do_init)
{
    int failures = 0;
    int i;

    for (i = 0; i < NOISE_NTHREADS; i++) {
	noise_concurrent_results[i].failed = 0;
    }

    if (do_init) {
	bn_noise_init();
    }

    bu_parallel(noise_worker, NOISE_NTHREADS, NULL);

    for (i = 0; i < NOISE_NTHREADS; i++) {
	if (noise_concurrent_results[i].failed) {
	    report_failure(test, "thread %d produced %d non-finite results",
		i + 1, noise_concurrent_results[i].failed);
	    failures += noise_concurrent_results[i].failed;
	}
    }

    return failures;
}


static int
test_noise_concurrent(void)
{
    return test_noise_concurrent_common("noise_concurrent", 1);
}


static int
test_noise_first_use(void)
{
    return test_noise_concurrent_common("noise_first_use", 0);
}


static int
test_noise_perlin_basic(void)
{
    int failures = 0;
    const char *test = "noise_perlin_basic";
    point_t pt = {0.5, 0.5, 0.5};
    double v1;
    double v2;

    bn_noise_init();

    v1 = bn_noise_perlin(pt);
    v2 = bn_noise_perlin(pt);
    if (!isfinite(v1) || !scalar_close(v1, v2, 0.0)) {
	report_failure(test, "bn_noise_perlin was not deterministic at a non-lattice point");
	failures++;
    }

    VSETALL(pt, 0.0);
    v1 = bn_noise_perlin(pt);
    if (!isfinite(v1)) {
	report_failure(test, "bn_noise_perlin returned a non-finite value at the origin");
	failures++;
    }

    return failures;
}


static const struct bn_api_case noise_cases[] = {
    {"init", test_noise_init},
    {"extended", test_noise_extended},
    {"large_coords", test_noise_large_coords},
    {"concurrent", test_noise_concurrent},
    {"first_use", test_noise_first_use},
    {"perlin", test_noise_perlin_basic},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_dispatch(argc, argv, noise_cases);
}
