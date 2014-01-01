/*              T E S T _ B N _ P O L Y _ S C A L E . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
#include "magic.h"


/* holds three polynomials to be used in test. */
static bn_poly_t input[3], output[3];


/**
 * Initialises polynomial storing a negative, positive and zero coefficients.
 */
void
poly_init(void)
{
    struct bn_poly bn_Zero_poly = { BN_POLY_MAGIC, 0, {0.0} };

    /* stores 0 coefficients to polynomial for input and output. */
    output[0] = bn_Zero_poly;
    input[0] = bn_Zero_poly;
    input[0].dgr = 2;
    input[0].cf[0] = input[0].cf[1] = input[0].cf[2] = 0.0;

    output[0].dgr = 2;
    output[0].cf[0] = output[0].cf[1] = output[0].cf[2] = 0.0;

    /* stores negative coefficients to polynomial. */
    output[1] = bn_Zero_poly;
    input[1] = bn_Zero_poly;
    input[1].dgr = 2;
    output[1].dgr = 2;

    input[1].cf[0] = -4;
    input[1].cf[1] = -3;
    input[1].cf[2] = -2;

    /**
     * The known output values used for these tests were generated from
     * GNU Octave, version 3.4.3
     */
    output[1].cf[0] = -8000;
    output[1].cf[1] = -6000;
    output[1].cf[2] = -4000;


    /* stores positive coefficients to to polynomial input. */
    output[2] = bn_Zero_poly;
    input[2] = bn_Zero_poly;
    input[2].dgr = 2;
    output[2].dgr = 2;

    input[2].cf[0] = 7854;
    input[2].cf[1] = 2136;
    input[2].cf[2] = 1450;

    output[2].cf[0] = -3141600;
    output[2].cf[1] = -854400;
    output[2].cf[2] = -580000;

    return;
}


/* compares the values of the array and returns 0. */
int
check_results(fastf_t a[], fastf_t b[], int n)
{
    int i;

    for (i = 0; i < n; i++) {
	if (!EQUAL(a[i], b[i]))
	    return -1;
    }

    return 0;
}


/* tests the polynomials to make sure bn_poly_mul() works properly. */
int
test_bn_poly_scale(void)
{
    int val, val1, val2;

    bn_poly_scale(&input[0], 0);
    bn_poly_scale(&input[1], 2000);
    bn_poly_scale(&input[2], -400);

    val = check_results(input[0].cf, output[0].cf, output[0].dgr + 1);
    val1 = check_results(input[1].cf, output[1].cf, output[1].dgr + 1);
    val2 = check_results(input[2].cf, output[2].cf, output[2].dgr + 1);

    if (val == 0 && val1 == 0 && val2 == 0)
	return val;

    return -1;
}


int
main(void)
{
    int ret;
    poly_init();

    ret = test_bn_poly_scale();

    if (ret == 0) {
	bu_log("\nFunction computes correctly\n");
	return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
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
