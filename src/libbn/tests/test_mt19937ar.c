/*                 T E S T _ M T 1 9 9 3 7 A R . C
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


static const double mt_reference[] = {
    0.8147236920927473,
    0.13547700413863104,
    0.9057919343248456,
    0.8350085899780990,
    0.12698681189841285
};


static int
check_reference_sequence(const char *test)
{
    int failures = 0;
    size_t i;

    for (i = 0; i < sizeof(mt_reference) / sizeof(mt_reference[0]); i++) {
	double v = bn_randmt();
	if (!scalar_close(v, mt_reference[i], 1.0e-15)) {
	    report_failure(test, "reference value %zu mismatch: got %.17g expected %.17g",
		i, v, mt_reference[i]);
	    failures++;
	}
    }

    return failures;
}


static int
test_mt19937ar_default_seed(void)
{
    return check_reference_sequence("mt19937ar_default_seed");
}


static int
test_mt19937ar_seeded_reference(void)
{
    bn_randmt_seed(5489UL);
    return check_reference_sequence("mt19937ar_seeded_reference");
}


static int
test_mt19937ar_stats(void)
{
    int failures = 0;
    const char *test = "mt19937ar_stats";
    double seq[32];
    double mean = 0.0;
    int differs = 0;
    int i;

    bn_randmt_seed(12345UL);
    for (i = 0; i < 32; i++) {
	seq[i] = bn_randmt();
	if (!(seq[i] >= 0.0 && seq[i] <= 1.0)) {
	    report_failure(test, "bn_randmt produced a value outside [0, 1] at step %d", i);
	    failures++;
	}
    }

    bn_randmt_seed(12345UL);
    for (i = 0; i < 32; i++) {
	double v = bn_randmt();
	if (!scalar_close(v, seq[i], 0.0)) {
	    report_failure(test, "bn_randmt was not repeatable after reseeding at step %d", i);
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
	report_failure(test, "sample mean (%g) drifted too far from 0.5", mean);
	failures++;
    }

    return failures;
}


static const struct bn_api_case mt19937ar_cases[] = {
    {"default_seed", test_mt19937ar_default_seed},
    {"reference", test_mt19937ar_seeded_reference},
    {"stats", test_mt19937ar_stats},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_dispatch(argc, argv, mt19937ar_cases);
}
