/*                         T E S T _ M S R . C
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
test_msr_uniform(void)
{
    int failures = 0;
    const char *test = "msr_uniform";
    struct bn_unif *u1 = NULL;
    struct bn_unif *u2 = NULL;
    double mean = 0.0;
    int i;

    if (bn_unif_long_fill(NULL) != 1) {
	report_failure(test, "bn_unif_long_fill(NULL) should return 1");
	failures++;
    }
    if (!scalar_close(bn_unif_double_fill(NULL), 0.0, 0.0)) {
	report_failure(test, "bn_unif_double_fill(NULL) should return 0.0");
	failures++;
    }

    u1 = bn_unif_init(123L, 0);
    u2 = bn_unif_init(123L, 0);
    for (i = 0; i < 2048; i++) {
	long a = BN_UNIF_LONG(u1);
	long b = BN_UNIF_LONG(u2);
	if (a != b) {
	    report_failure(test, "uniform long sequence diverged at step %d", i);
	    failures++;
	    break;
	}
	if (!(a >= 1 && a <= 2147483647L)) {
	    report_failure(test, "uniform long result was outside the documented range");
	    failures++;
	}
    }
    bn_unif_free(u1);
    bn_unif_free(u2);

    u1 = bn_unif_init(123L, 0);
    u2 = bn_unif_init(123L, 0);
    for (i = 0; i < 4096; i++) {
	double a = BN_UNIF_DOUBLE(u1);
	double b = BN_UNIF_DOUBLE(u2);
	if (!scalar_close((double)a, (double)b, 0.0)) {
	    report_failure(test, "uniform double sequence diverged at step %d", i);
	    failures++;
	    break;
	}
	if (!(a >= -0.5 && a <= 0.5)) {
	    report_failure(test, "uniform double result was outside the documented range");
	    failures++;
	}
	mean += a;
    }
    mean /= 4096.0;
    if (fabs(mean) > 0.03) {
	report_failure(test, "uniform double sample mean (%g) drifted too far from zero", mean);
	failures++;
    }
    bn_unif_free(u1);
    bn_unif_free(u2);

    return failures;
}


static int
test_msr_gaussian(void)
{
    int failures = 0;
    const char *test = "msr_gaussian";
    struct bn_gauss *g1 = NULL;
    struct bn_gauss *g2 = NULL;
    double mean = 0.0;
    double sumsq = 0.0;
    int i;

    if (!scalar_close(bn_gauss_fill(NULL), 0.0, 0.0)) {
	report_failure(test, "bn_gauss_fill(NULL) should return 0.0");
	failures++;
    }

    g1 = bn_gauss_init(321L, 0);
    g2 = bn_gauss_init(321L, 0);
    for (i = 0; i < 8192; i++) {
	double a = BN_GAUSS_DOUBLE(g1);
	double b = BN_GAUSS_DOUBLE(g2);
	if (!scalar_close(a, b, 0.0)) {
	    report_failure(test, "gaussian sequence diverged at step %d", i);
	    failures++;
	    break;
	}
	mean += a;
	sumsq += a * a;
    }
    mean /= 8192.0;
    sumsq = sumsq / 8192.0 - mean * mean;

    if (fabs(mean) > 0.05) {
	report_failure(test, "gaussian sample mean (%g) drifted too far from zero", mean);
	failures++;
    }
    if (!(sumsq > 0.85 && sumsq < 1.15)) {
	report_failure(test, "gaussian sample variance (%g) drifted too far from one", sumsq);
	failures++;
    }

    bn_gauss_free(g1);
    bn_gauss_free(g2);

    return failures;
}


static const struct bn_api_case msr_cases[] = {
    {"uniform", test_msr_uniform},
    {"gaussian", test_msr_gaussian},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_dispatch(argc, argv, msr_cases);
}
