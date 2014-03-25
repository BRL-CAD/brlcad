/*         B N _ P O L Y _ S Y N T H E T I C _ D I V . C
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


/* holds three polynomials to be used in test. */
bn_poly_t input[2] = { BN_POLY_INIT_ZERO, BN_POLY_INIT_ZERO };
bn_poly_t quo[1] = { BN_POLY_INIT_ZERO };
bn_poly_t rem[1] = { BN_POLY_INIT_ZERO };


/* Initializes polynomial storing a negative, positive and zero
 * coefficients.  Test polynomials will be of 4th, 3rd, and 2nd
 * degrees.
 */
void
poly_init(void)
{
    /* initializes a 3rd degree polynomial with negative
     * coefficients.
     */
    input[0].dgr = 3;
    quo[0].dgr = rem[0].dgr = 4;

    input[0].cf[0] = -4, input[0].cf[1] = -3, input[0].cf[2] = -2, input[0].cf[3] = -38;/* input coeff */

    /**
     * The known output values used for these tests were generated from
     * GNU Octave, version 3.4.3
     */
    quo[0].cf[0] = -1369.500000, quo[0].cf[1] = -344.125000, quo[0].cf[2] = quo[0].cf[3] = quo[0].cf[4] = 0.000000; /* quotient coeff */
    rem[0].cf[0] = -3313.375000, rem[0].cf[1] = 205834.750000, rem[0].cf[2] = 41708.250000, rem[0].cf[3] = 0.0; /* remainder coeff */

    /* initializes a 4th degree positive polynomial */
    input[1].dgr = 4;

    input[1].cf[0] = 5478, input[1].cf[1] = 5485, input[1].cf[2] = 458, input[1].cf[3] = 258564, input[1].cf[4] = 54785;/* input coeff */

    return;
}


/* compares the values of the array and returns 0 if they all match */
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
test_bn_poly_syn_div(void)
{
    /* variables to store results for comparison */
    int val1[2];
    bn_poly_t q2 = BN_POLY_INIT_ZERO;
    bn_poly_t r2 = BN_POLY_INIT_ZERO;

    bn_poly_synthetic_division(&q2, &r2, &input[1], &input[0]);

    /*checks the quotients */
    val1[0] = check_results(q2.cf, quo[0].cf, quo[0].dgr + 1);
    val1[1] = check_results(r2.cf, rem[0].cf, rem[0].dgr + 1);

    if (val1[0] == 0 && val1[1] == 0)
	return val1[0];

    return -1;
}


int
main(void)
{
    int ret;

    poly_init();
    ret = test_bn_poly_syn_div();

    if (ret)
	bu_log("[FAIL] %s test failed\n", __FILE__);
    else
	bu_log("[PASS] %s test passed\n", __FILE__);

    return ret;
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
