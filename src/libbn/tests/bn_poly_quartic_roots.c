/*         B N _ P O L Y _ Q U A R T I C _ R O O T S. C
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
bn_poly_t input[3];
bn_complex_t rts[9];

struct bn_poly bn_Zero_poly = { BN_POLY_MAGIC, 0, {0.0} };


/**
 * Initialises quartic equations storing a negative, positive and zero coefficients.
 */
void
poly_init(void)
{

    /* initializes a zero equation */

    input[0] = bn_Zero_poly;
    input[0].dgr = 4;
    input[0].cf[0] = input[0].cf[1] = input[0].cf[2] = input[0].cf[3] = input[0].cf[4] = 0.0;

    rts[0].re = rts[0].im = 0.0;


    /* initializes a negative quadratic eqn. */
    input[1] = bn_Zero_poly;
    input[1].dgr = 4;


    input[1].cf[0] = -4, input[1].cf[1] = -3, input[1].cf[2] = -2, input[1].cf[3] = -25, input[1].cf[4] = -38;/* input coeff */

    /**
     * The known output values used for these tests were generated from
     * GNU Octave, version 3.4.3
     */
    rts[1].re = 0.2613656082942032, rts[1].im = 0.4284631324677022;
    rts[2].re = 0.2613656082942032, rts[2].im = -0.4284631324677022;
    rts[3].re = -0.5903129767152558, rts[3].im = 0.263475942656035;
    rts[4].re = -0.5903129767152558, rts[4].im = -0.263475942656035;

    /* initializes a positive quadratic equation */
    input[2] = bn_Zero_poly;
    input[2].dgr = 4;
    input[2].cf[0] = 5478, input[2].cf[1] = 5485, input[2].cf[2] = 458, input[2].cf[3] = 258564, input[2].cf[4] = 54785;/* input coeff */

    rts[5].re = 0.12889648467110737 , rts[5].im = 0.25711127015404556;
    rts[6].re = 0.12889648467110737, rts[6].im = -0.25711127015404556;
    rts[7].re = -0.25602234520349354, rts[7].im = 0.0;
    rts[8].re = -4.721383656903164, rts[8].im = 0.0;

    return;
}


/* tests the polynomials to make sure bn_poly_mul() works properly. */
int
test_bn_poly_qua_rts(void)
{
    int val, val1[4], val2[4];/* variables get results for comparisons */
    bn_complex_t r1, r2[4], r3[4];

    int i, j, ind1 = 1, ind2 = 5; /* creates array indexes for rts */

    bn_poly_quartic_roots(&r1, &input[0]);
    bn_poly_quartic_roots(r2, &input[1]);
    bn_poly_quartic_roots(r3, &input[2]);

    /* loop moves through arrays for roots and output comparing variables. */
    for (i = 0; i < 4; i++) {

	/*checks r1 */
	if (EQUAL(r1.re, rts[0].re) && EQUAL(r1.im, rts[0].im))
	    val = 0;

	/*checks r2 */
	for (j = ind1; j< ind1 + 4; j++) {
	    if (EQUAL(r2[i].re, rts[j].re) && EQUAL(r2[i].im, rts[j].im)) {
		val1[i] = 0;
		continue;
	    }
	}

	/*check r3 */
	for (j = ind2; j< ind2 + 4; j++) {
	    if (EQUAL(r3[i].re, rts[j].re) && EQUAL(r3[i].im, rts[j].im)) {
		val2[i] = 0;
		continue;
	    }
	}
    }

    if (!EQUAL(val, 0))
	return -1;

    for (i = 0; i < 4; i++) {
	if (!EQUAL(val1[i], val2[i]) && !EQUAL(val1[i], 0))
	    return -1;
    }

    return 0;
}


int
main(void)
{
    int ret;
    poly_init();
    ret = test_bn_poly_qua_rts();

    if (ret == 0) {
	bu_log("\nFunction computes correctly\n");
	return ret;

    } else
	exit(EXIT_FAILURE);

    return 0;

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
