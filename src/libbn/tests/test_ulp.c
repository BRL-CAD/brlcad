/*                        T E S T _ U L P . C
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
test_ulp(void)
{
    int failures = 0;
    const char *test = "ulp";
    double dvals[8] = {0.0, -0.0, 1.0, -1.0, 123.456, -987.125, DBL_MIN * 8.0, DBL_MAX / 4.0};
    float fvals[8] = {0.0f, -0.0f, 1.0f, -1.0f, 12.5f, -9.75f, FLT_MIN * 8.0f, FLT_MAX / 4.0f};
    size_t i;

    if (!scalar_close(bn_dbl_epsilon(), DBL_EPSILON, 0.0)) {
	report_failure(test, "bn_dbl_epsilon disagrees with DBL_EPSILON");
	failures++;
    }
    if (!scalar_close(bn_flt_epsilon(), FLT_EPSILON, 0.0)) {
	report_failure(test, "bn_flt_epsilon disagrees with FLT_EPSILON");
	failures++;
    }
    if (!scalar_close(bn_dbl_min(), DBL_MIN, 0.0)) {
	report_failure(test, "bn_dbl_min disagrees with DBL_MIN");
	failures++;
    }
    if (!scalar_close(bn_dbl_max(), DBL_MAX, 0.0)) {
	report_failure(test, "bn_dbl_max disagrees with DBL_MAX");
	failures++;
    }
    if (!scalar_close(bn_flt_min(), FLT_MIN, 0.0)) {
	report_failure(test, "bn_flt_min disagrees with FLT_MIN");
	failures++;
    }
    if (!scalar_close(bn_flt_max(), FLT_MAX, 0.0)) {
	report_failure(test, "bn_flt_max disagrees with FLT_MAX");
	failures++;
    }

    if (!scalar_close(bn_dbl_min_sqrt(), sqrt(DBL_MIN), 0.0)) {
	report_failure(test, "bn_dbl_min_sqrt disagrees with sqrt(DBL_MIN)");
	failures++;
    }
    if (!scalar_close(bn_dbl_max_sqrt(), sqrt(DBL_MAX), 0.0)) {
	report_failure(test, "bn_dbl_max_sqrt disagrees with sqrt(DBL_MAX)");
	failures++;
    }
    if (!scalar_close(bn_flt_min_sqrt(), sqrtf(FLT_MIN), 0.0f)) {
	report_failure(test, "bn_flt_min_sqrt disagrees with sqrtf(FLT_MIN)");
	failures++;
    }
    if (!scalar_close(bn_flt_max_sqrt(), sqrtf(FLT_MAX), 0.0f)) {
	report_failure(test, "bn_flt_max_sqrt disagrees with sqrtf(FLT_MAX)");
	failures++;
    }

    for (i = 0; i < 8; i++) {
	double up = nextafter(dvals[i], INFINITY);
	double dn = nextafter(dvals[i], -INFINITY);
	double ulp = fabs(up - dvals[i]);

	if (!scalar_close(bn_nextafter_up(dvals[i]), up, 0.0)) {
	    report_failure(test, "bn_nextafter_up mismatch at index %zu", i);
	    failures++;
	}
	if (!scalar_close(bn_nextafter_dn(dvals[i]), dn, 0.0)) {
	    report_failure(test, "bn_nextafter_dn mismatch at index %zu", i);
	    failures++;
	}
	if (!scalar_close(bn_ulp(dvals[i]), ulp, 0.0)) {
	    report_failure(test, "bn_ulp mismatch at index %zu", i);
	    failures++;
	}
    }

    for (i = 0; i < 8; i++) {
	float up = nextafterf(fvals[i], INFINITY);
	float dn = nextafterf(fvals[i], -INFINITY);
	float ulp = fabsf(up - fvals[i]);

	if (!scalar_close((double)bn_nextafterf_up(fvals[i]), (double)up, 0.0)) {
	    report_failure(test, "bn_nextafterf_up mismatch at index %zu", i);
	    failures++;
	}
	if (!scalar_close((double)bn_nextafterf_dn(fvals[i]), (double)dn, 0.0)) {
	    report_failure(test, "bn_nextafterf_dn mismatch at index %zu", i);
	    failures++;
	}
	if (!scalar_close((double)bn_ulpf(fvals[i]), (double)ulp, 0.0)) {
	    report_failure(test, "bn_ulpf mismatch at index %zu", i);
	    failures++;
	}
    }

    if (!isnan(bn_ulp(NAN))) {
	report_failure(test, "bn_ulp(NAN) should be NAN");
	failures++;
    }
    if (!isinf(bn_ulp(INFINITY))) {
	report_failure(test, "bn_ulp(INFINITY) should be infinite");
	failures++;
    }
    if (!isinf(bn_nextafter_up(INFINITY))) {
	report_failure(test, "bn_nextafter_up(INFINITY) should stay infinite");
	failures++;
    }

    return failures;
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_single(argc, argv, "ulp", test_ulp);
}
