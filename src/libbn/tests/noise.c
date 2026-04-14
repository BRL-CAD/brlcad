/*                       N O I S E . C
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file libbn/tests/noise.c
 *
 * Tests for bn_noise_turb, bn_noise_fbm, and bn_noise_perlin.
 *
 * Exercises two previously-broken paths:
 *
 * 1. Large-coordinate hang (filter_args infinite loop)
 *    bn_noise_turb / bn_noise_fbm with very large point coordinates
 *    (> ~1.66e35) used to enter an infinite loop because the
 *    floating-point folding formula `max2x - dst[i]` equalled -dst[i]
 *    exactly, causing the value to never converge.  The fix replaces
 *    the while loop with an fmod-based O(1) reduction.
 *
 * 2. Concurrent spectral-table race (find_spec_wgt data race)
 *    Multiple threads calling bn_noise_turb / bn_noise_fbm
 *    simultaneously would race on the global etbl pointer: one thread
 *    could be reading from old (freed) memory while another called
 *    bu_realloc inside build_spec_tbl.  The fix holds sem_noise for
 *    the entire table lookup, eliminating the unsynchronized first scan.
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bu/log.h"
#include "bu/parallel.h"
#include "vmath.h"
#include "bn/noise.h"

/* --------------------------------------------------------------------------
 * 1.  Single-thread large-coordinate test
 *     Demonstrates that filter_args() terminates for coordinates well above
 *     the historical threshold (~1.66e35) that triggered the infinite loop.
 * -------------------------------------------------------------------------- */

struct large_coord_case {
    double x, y, z;
    const char *label;
};

static int
test_noise_large_coords(int UNUSED(argc), char **UNUSED(argv))
{
    int failures = 0;
    size_t i;

    /* These values exercised the infinite-loop path in the old code:
     *   values > ~1.66e35 caused fabs(max2x - v) == fabs(v) due to
     *   double precision loss, so the fold never converged.            */
    static const struct large_coord_case cases[] = {
	{ 1e36,  1e36,  1e36,  "1e36"  },
	{ 1e50,  1e50,  1e50,  "1e50"  },
	{ 1e100, 1e100, 1e100, "1e100" },
	{ 1e300, 1e300, 1e300, "1e300" },
	/* boundary region: just above max (~9.2e18) */
	{ 1e19,  1e19,  1e19,  "1e19"  },
	{ 2e19,  2e19,  2e19,  "2e19"  },
	{ 1e20,  1e20,  1e20,  "1e20"  },
	/* normal coordinates (regression guard) */
	{ 1.0,   2.0,   3.0,   "normal"},
	{ 1000.0, 1000.0, 1000.0, "1000" },
    };

    bn_noise_init();

    for (i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
	point_t pt;
	double turb_val, fbm_val;

	VSET(pt, cases[i].x, cases[i].y, cases[i].z);

	turb_val = bn_noise_turb(pt, 1.0, 2.1753974, 4.0);
	fbm_val  = bn_noise_fbm (pt, 1.0, 2.1753974, 4.0);

	if (!isfinite(turb_val)) {
	    bu_log("FAIL [noise_large_coords] bn_noise_turb(%s): non-finite result %g\n",
		   cases[i].label, turb_val);
	    failures++;
	} else if (!isfinite(fbm_val)) {
	    bu_log("FAIL [noise_large_coords] bn_noise_fbm(%s): non-finite result %g\n",
		   cases[i].label, fbm_val);
	    failures++;
	} else {
	    bu_log("  PASS %-8s  turb=%g fbm=%g\n", cases[i].label, turb_val, fbm_val);
	}
    }

    return failures;
}


/* --------------------------------------------------------------------------
 * 2.  Concurrent noise test
 *     Multiple threads all call bn_noise_turb / bn_noise_fbm with the same
 *     parameters (forcing spectral-table construction under contention) and
 *     with large coordinates (exercising the fixed filter_args path).
 *     If the data race or semaphore logic is broken, this produces crashes
 *     or hangs (visible as a test timeout in CTest).
 * -------------------------------------------------------------------------- */

#define NOISE_NTHREADS 8
#define NOISE_ITERS    200

struct concurrent_result {
    int failed;
};

static struct concurrent_result concurrent_results[NOISE_NTHREADS];

static void
noise_worker(int cpu, void *UNUSED(data))
{
    int i;
    int local_fail = 0;
    /* Use the same h/lacunarity/octaves so all threads race to build
     * exactly the same spectral-weight entry.                        */
    const double h         = 1.0;
    const double lacunarity = 2.1753974;
    const double octaves   = 4.0;

    /* Mix a large coordinate (regression for filter_args hang)
     * with a normal one to exercise both code paths.                 */
    for (i = 0; i < NOISE_ITERS; i++) {
	point_t pt_large, pt_normal;
	double v1, v2;

	VSET(pt_large,  (double)(cpu) * 1e36,
			(double)(i)   * 1e36,
			1e36);
	VSET(pt_normal, (double)cpu + (double)i * 0.01,
			(double)i * 0.1,
			0.5);

	v1 = bn_noise_turb(pt_large,  h, lacunarity, octaves);
	v2 = bn_noise_fbm (pt_normal, h, lacunarity, octaves);

	if (!isfinite(v1) || !isfinite(v2)) {
	    local_fail++;
	}
    }

    /* cpu index is 1-based in bu_parallel.  Guard against unexpected range. */
    if (cpu >= 1 && cpu <= NOISE_NTHREADS) {
	concurrent_results[cpu - 1].failed = local_fail;
    } else {
	bu_log("WARNING [noise_concurrent] worker cpu=%d out of range [1,%d]\n",
	       cpu, NOISE_NTHREADS);
    }
}

static int
test_noise_concurrent(int UNUSED(argc), char **UNUSED(argv))
{
    int i;
    int failures = 0;

    for (i = 0; i < NOISE_NTHREADS; i++)
	concurrent_results[i].failed = 0;

    bn_noise_init();

    bu_parallel(noise_worker, NOISE_NTHREADS, NULL);

    for (i = 0; i < NOISE_NTHREADS; i++) {
	if (concurrent_results[i].failed) {
	    bu_log("FAIL [noise_concurrent] thread %d had %d non-finite results\n",
		   i + 1, concurrent_results[i].failed);
	    failures += concurrent_results[i].failed;
	}
    }

    if (!failures)
	bu_log("  PASS concurrent noise (%d threads x %d iters)\n",
	       NOISE_NTHREADS, NOISE_ITERS);

    return failures;
}


/* --------------------------------------------------------------------------
 * 3.  Basic perlin noise sanity check
 *     Values should be in a reasonable range for unit-scale inputs.
 * -------------------------------------------------------------------------- */
static int
test_noise_perlin_basic(int UNUSED(argc), char **UNUSED(argv))
{
    point_t pt;
    double v;
    int failures = 0;

    bn_noise_init();

    VSET(pt, 0.5, 0.5, 0.5);
    v = bn_noise_perlin(pt);
    if (!isfinite(v)) {
	bu_log("FAIL [noise_perlin_basic] non-finite result at (0.5, 0.5, 0.5): %g\n", v);
	failures++;
    } else {
	bu_log("  PASS bn_noise_perlin(0.5,0.5,0.5) = %g\n", v);
    }

    /* Zero point */
    VSETALL(pt, 0.0);
    v = bn_noise_perlin(pt);
    if (!isfinite(v)) {
	bu_log("FAIL [noise_perlin_basic] non-finite result at origin\n");
	failures++;
    } else {
	bu_log("  PASS bn_noise_perlin(0,0,0) = %g\n", v);
    }

    return failures;
}


/* --------------------------------------------------------------------------
 * Main dispatcher
 * -------------------------------------------------------------------------- */
int
noise_main(int argc, char *argv[])
{
    int function_num = 0;

    if (argc < 2) {
	bu_exit(1, "Usage: bn_test noise <function_num>\n"
		"  1 = large coordinate single-thread\n"
		"  2 = concurrent multi-thread\n"
		"  3 = basic perlin sanity\n");
    }

    sscanf(argv[1], "%d", &function_num);

    switch (function_num) {
	case 1:
	    return test_noise_large_coords(argc, argv);
	case 2:
	    return test_noise_concurrent(argc, argv);
	case 3:
	    return test_noise_perlin_basic(argc, argv);
    }

    bu_log("ERROR: function_num %d is not valid (expected 1-3) [%s]\n", function_num, argv[0]);
    return 1;
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
