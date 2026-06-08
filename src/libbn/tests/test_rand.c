/*                        T E S T _ R A N D . C
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
test_rand_table(void)
{
    int failures = 0;
    const char *test = "rand_table";
    unsigned int i1 = 0;
    unsigned int i2 = 0;
    float *p = NULL;
    int i;
    int saw_nonzero = 0;

    BN_RANDSEED(i1, 1234);
    BN_RANDSEED(i2, 1234);
    for (i = 0; i < 64; i++) {
	float a = BN_RANDOM(i1);
	float b = BN_RANDOM(i2);
	if (!scalar_close((double)a, (double)b, 0.0)) {
	    report_failure(test, "BN_RANDOM sequence was not repeatable at step %d", i);
	    failures++;
	    break;
	}
	if (!(a > 0.0f && a < 1.0f)) {
	    report_failure(test, "BN_RANDOM produced a value outside the open unit interval");
	    failures++;
	}
    }

    bn_rand_init(p, 17);
    for (i = 0; i < 64; i++) {
	float v = bn_rand_half(p);
	if (!scalar_close((double)v, 0.0, 0.0)) {
	    saw_nonzero = 1;
	}
	if (!(v >= -0.5f && v <= 0.5f)) {
	    report_failure(test, "bn_rand_half produced a value outside [-0.5, 0.5]");
	    failures++;
	}
    }
    if (!saw_nonzero) {
	report_failure(test, "bn_rand_half unexpectedly produced only zeros before bn_mathtab_constant");
	failures++;
    }

    bn_mathtab_constant();

    bn_rand_init(p, 17);
    for (i = 0; i < 64; i++) {
	if (!scalar_close((double)bn_rand_half(p), 0.0, 0.0)) {
	    report_failure(test, "bn_mathtab_constant did not force bn_rand_half to zero");
	    failures++;
	    break;
	}
    }

    bn_rand_init(p, 17);
    for (i = 0; i < 64; i++) {
	if (!scalar_close((double)bn_rand0to1(p), 0.5, 0.0)) {
	    report_failure(test, "bn_mathtab_constant did not force bn_rand0to1 to 0.5");
	    failures++;
	    break;
	}
    }

    return failures;
}


static int
test_randmt(void)
{
    int failures = 0;
    const char *test = "randmt";
    double seq[32];
    double mean = 0.0;
    int i;
    int differs = 0;

    bn_randmt_seed(12345UL);
    for (i = 0; i < 32; i++) {
	seq[i] = bn_randmt();
	if (!(seq[i] >= 0.0 && seq[i] <= 1.0)) {
	    report_failure(test, "bn_randmt produced a value outside [0, 1]");
	    failures++;
	}
    }

    bn_randmt_seed(12345UL);
    for (i = 0; i < 32; i++) {
	double v = bn_randmt();
	if (!scalar_close(v, seq[i], 0.0)) {
	    report_failure(test, "bn_randmt sequence was not repeatable after reseeding");
	    failures++;
	    break;
	}
    }

    bn_randmt_seed(54321UL);
    for (i = 0; i < 8; i++) {
	if (!scalar_close(bn_randmt(), seq[i], 0.0)) {
	    differs = 1;
	    break;
	}
    }
    if (!differs) {
	report_failure(test, "different seeds produced the same initial sequence");
	failures++;
    }

    bn_randmt_seed(8675309UL);
    for (i = 0; i < 4096; i++) {
	mean += bn_randmt();
    }
    mean /= 4096.0;
    if (fabs(mean - 0.5) > 0.02) {
	report_failure(test, "bn_randmt sample mean (%g) drifted too far from 0.5", mean);
	failures++;
    }

    return failures;
}


static int
test_rand_sphere(void)
{
    int failures = 0;
    const char *test = "rand_sphere";
    point_t center = {5.0, -3.0, 2.0};
    fastf_t radius = 4.5;
    point_t saved[16];
    point_t first = VINIT_ZERO;
    point_t sample;
    point_t delta;
    int saw_variation = 0;
    int i;

    bn_randmt_seed(314159UL);
    for (i = 0; i < 1024; i++) {
	bn_rand_sph_sample(sample, center, radius);
	VSUB2(delta, sample, center);
	if (!scalar_close(MAGNITUDE(delta), radius, 1.0e-6)) {
	    report_failure(test, "sample %d was not on the requested sphere", i);
	    failures++;
	    break;
	}
	if (i == 0) {
	    VMOVE(first, sample);
	} else if (!vect_close(sample, first, DBL_EPSILON * 8.0)) {
	    saw_variation = 1;
	}
	if (i < 16) {
	    VMOVE(saved[i], sample);
	}
    }

    if (!saw_variation) {
	report_failure(test, "sphere sampler produced a degenerate constant sequence");
	failures++;
    }

    bn_randmt_seed(314159UL);
    for (i = 0; i < 16; i++) {
	bn_rand_sph_sample(sample, center, radius);
	if (!vect_close(sample, saved[i], DBL_EPSILON * 8.0)) {
	    report_failure(test, "sphere sampling sequence was not repeatable after reseeding");
	    failures++;
	    break;
	}
    }

    return failures;
}


static const struct bn_api_case rand_cases[] = {
    {"table", test_rand_table},
    {"randmt", test_randmt},
    {"sphere", test_rand_sphere},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_dispatch(argc, argv, rand_cases);
}
