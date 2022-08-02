/*                   P O L Y _ A D D . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2022 United States Government as represented by
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
bn_poly_t padd_input[3], padd_output[3];

struct bn_poly bn_aZero_poly = { BN_POLY_MAGIC, 0, {0.0} };


/**
 *Initialises polynomial storing negative, positive and zero-value coefficients.
 */
void
a_poly_init(void)
{
    /* stores zero-value coefficients to polynomial for padd_input and padd_output. */
    padd_output[0] = bn_aZero_poly;
    padd_input[0] = bn_aZero_poly;
    padd_input[0].dgr = 2;
    padd_input[0].cf[0] = padd_input[0].cf[1] = padd_input[0].cf[2] = 0.0;

    padd_output[0].dgr = 2;
    padd_output[0].cf[0] = padd_output[0].cf[1] = padd_output[0].cf[2] = 0.0;

    /* stores negative coefficients to polynomial. */
    padd_output[1] = bn_aZero_poly;
    padd_input[1] = bn_aZero_poly;
    padd_input[1].dgr = 2;
    padd_output[1].dgr = 2;

    padd_input[1].cf[0] = -4853;
    padd_input[1].cf[1] = -324;
    padd_input[1].cf[2] = -275;

    /**
     * The known padd_output values used for these tests were generated from
     * GNU Octave, version 3.4.3
     */
    padd_output[1].cf[0] = -9706;
    padd_output[1].cf[1] = -648;
    padd_output[1].cf[2] = -550;

    /* stores positive coefficients to to polynomial padd_input. */
    padd_output[2] = bn_aZero_poly;
    padd_input[2] = bn_aZero_poly;
    padd_input[2].dgr = 2;
    padd_output[2].dgr = 2;

    padd_input[2].cf[0] = 61685316;
    padd_input[2].cf[1] = 33552288;
    padd_input[2].cf[2] = 27339096;

    padd_output[2].cf[0] = 123370632;
    padd_output[2].cf[1] = 67104576;
    padd_output[2].cf[2] = 54678192;

    return;
}


/* compares the values of the array and returns 0. */
static size_t
a_check_results(fastf_t a[], fastf_t b[], size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
	if (!EQUAL(a[i], b[i]))
	    return 1;
    }

    return 0;
}


/* tests the polynomials to make sure bn_poly_add() works properly. */
static int
test_bn_poly_add(void)
{
    size_t val, val1, val2;
    bn_poly_t a, b, c;
    a = bn_aZero_poly, b = bn_aZero_poly, c = bn_aZero_poly;

    bn_poly_add(&a, &padd_input[0], &padd_input[0]);
    bn_poly_add(&b, &padd_input[1], &padd_input[1]);
    bn_poly_add(&c, &padd_input[2], &padd_input[2]);

    val = a_check_results(a.cf, padd_output[0].cf, padd_output[0].dgr + 1);
    val1 = a_check_results(b.cf, padd_output[1].cf, padd_output[1].dgr + 1);
    val2 = a_check_results(c.cf, padd_output[2].cf, padd_output[2].dgr + 1);

    if (val == 0 && val1 == 0 && val2 == 0)
	return 0;

    return 1;
}


int
poly_add_main(int UNUSED(ac), char **UNUSED(av))
{
    int ret;
    a_poly_init();
    ret = test_bn_poly_add();

    if (ret != 0) {
	exit(EXIT_FAILURE);
    }

    bu_log("Function computes correctly");
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
