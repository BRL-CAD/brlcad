/*                    P O L Y _ S U B . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2021 United States Government as represented by
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <signal.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"

/* holds three polynomials to be used in test. */
bn_poly_t psub_input[3], psub_output[3];

struct bn_poly bn_Zero_poly = { BN_POLY_MAGIC, 0, {0.0} };

/**
 * Initialises polynomial storing negative, positive and zero coefficients.
 */
void
psub_poly_init(void)
{
    /* stores zero-value coefficients to polynomial for psub_input and psub_output. */
    psub_output[0] = bn_Zero_poly;
    psub_input[0] = bn_Zero_poly;
    psub_input[0].dgr = 2;
    psub_input[0].cf[0] = psub_input[0].cf[1] = psub_input[0].cf[2] = 0.0;

    psub_output[0].dgr = 2;
    psub_output[0].cf[0] = psub_output[0].cf[1] = psub_output[0].cf[2] = 0.0;

    /* stores negative coefficients to polynomial. */
    psub_output[1] = bn_Zero_poly;
    psub_input[1] = bn_Zero_poly;
    psub_input[1].dgr = 2;
    psub_output[1].dgr = 2;

    psub_input[1].cf[0] = -4853;
    psub_input[1].cf[1] = -324;
    psub_input[1].cf[2] = -275;

    /**
     * The known values used for these tests are generated from
     * GNU Octave, version 3.4.3
     */
    psub_output[1].cf[0] = -9706;
    psub_output[1].cf[1] = -648;
    psub_output[1].cf[2] = -550;

    /* stores positive coefficients to to polynomial psub_input. */
    psub_output[2] = bn_Zero_poly;
    psub_input[2] = bn_Zero_poly;
    psub_input[2].dgr = 2;
    psub_output[2].dgr = 2;

    psub_input[2].cf[0] = 61685316;
    psub_input[2].cf[1] = 33552288;
    psub_input[2].cf[2] = 27339096;

    psub_output[2].cf[0] = 61695022;
    psub_output[2].cf[1] = 33552936;
    psub_output[2].cf[2] = 27339646;

    return;
}


/* compares the values of the array and returns 0. */
size_t
psub_check_results(fastf_t a[], fastf_t b[], size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
	if (!EQUAL(a[i], b[i]))
	    return -1;
    }

    return 0;
}


/* tests the polynomials to make sure bn_poly_add() works properly. */
int
test_bn_poly_sub(void)
{
    size_t val, val1, val2;
    bn_poly_t a, b, c;

    a = bn_Zero_poly, b = bn_Zero_poly, c = bn_Zero_poly;

    bn_poly_sub(&a, &psub_input[0], &psub_input[0]);
    bn_poly_sub(&b, &psub_input[1], &psub_input[0]);
    bn_poly_sub(&c, &psub_input[2], &psub_input[1]);

    val = psub_check_results(a.cf, psub_output[0].cf, psub_output[0].dgr + 1);
    val1 = psub_check_results(b.cf, psub_output[1].cf, psub_output[1].dgr + 1);
    val2 = psub_check_results(c.cf, psub_output[2].cf, psub_output[2].dgr + 1);

    if (val == 0 && val1 == 0 && val2 == 0)
	return 0;

    return 1;
}


int
poly_sub_main(int UNUSED(ac), char **UNUSED(av))
{
    int ret;

    psub_poly_init();

    ret = test_bn_poly_sub();
    if (ret != 0) {
	exit(EXIT_FAILURE);
    }

    bu_log("Function computes correctly\n");
    return EXIT_SUCCESS;
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
