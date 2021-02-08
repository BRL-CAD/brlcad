/*                P O L Y _ M U L T I P L Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
#include <math.h>
#include <string.h>
#include <signal.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"


struct bn_poly bn_mZero_poly = { BN_POLY_MAGIC, 0, {0.0} };

/*holds three polynomials to be used in test.*/
bn_poly_t pmul_input[3], pmul_output[3];


/*
 *Initialises polynomial storing negative, positive and zero coefficients.
 */
void
mpoly_init(void)
{

    /*stores coefficients (zeros) to polynomial for pmul_input and pmul_output.*/
    pmul_output[0] = bn_mZero_poly;
    pmul_input[0] = bn_mZero_poly;
    pmul_input[0].dgr = 2;
    pmul_input[0].cf[0] = pmul_input[0].cf[1] = pmul_input[0].cf[2] = pmul_input[0].cf[3] = 0.0;

    pmul_output[0].dgr = 4;
    pmul_output[0].cf[0] = pmul_output[0].cf[1] = pmul_output[0].cf[2] = pmul_output[0].cf[3] = pmul_output[0].cf[4] = 0.0;

    /*stores negative coefficients to polynomial.*/
    pmul_output[1] = bn_mZero_poly;
    pmul_input[1] = bn_mZero_poly;
    pmul_input[1].dgr = 2;
    pmul_output[1].dgr = 4;

    pmul_input[1].cf[0] = -4;
    pmul_input[1].cf[1] = -3;
    pmul_input[1].cf[2] = -2;

    /**
     * The known pmul_output values used for these tests were generated from
     * GNU Octave, version 3.4.3
     */

    pmul_output[1].cf[0] = 16;
    pmul_output[1].cf[1] = 24;
    pmul_output[1].cf[2] = 25;
    pmul_output[1].cf[3] = 12;
    pmul_output[1].cf[4] = 4;

    /*stores positive coefficients to to polynomial pmul_input.*/
    pmul_output[2] = bn_mZero_poly;
    pmul_input[2] = bn_mZero_poly;
    pmul_input[2].dgr = 2;
    pmul_output[2].dgr = 4;

    pmul_input[2].cf[0] = 7854;
    pmul_input[2].cf[1] = 2136;
    pmul_input[2].cf[2] = 1450;

    pmul_output[2].cf[0] = 61685316;
    pmul_output[2].cf[1] = 33552288;
    pmul_output[2].cf[2] = 27339096;
    pmul_output[2].cf[3] = 6194400;
    pmul_output[2].cf[4] = 2102500;

}


/* compares the values of the array and returns 0. */
static size_t
m_check_results(fastf_t a[], fastf_t b[], size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
	if (!EQUAL(a[i], b[i]))
	    return 1;
    }

    return 0;
}


/*tests the polynomials to make sure bn_poly_mul() works properly.*/
static int
test_bn_mpoly(void)
{
    size_t val, val1, val2;
    bn_poly_t a, b, c;
    a = bn_mZero_poly, b = bn_mZero_poly, c = bn_mZero_poly;

    bn_poly_mul(&a, &pmul_input[0], &pmul_input[0]);
    bn_poly_mul(&b, &pmul_input[1], &pmul_input[1]);
    bn_poly_mul(&c, &pmul_input[2], &pmul_input[2]);

    val = m_check_results(a.cf, pmul_output[0].cf, pmul_output[0].dgr + 1);
    val1 = m_check_results(b.cf, pmul_output[1].cf, pmul_output[1].dgr + 1);
    val2 = m_check_results(c.cf, pmul_output[2].cf, pmul_output[2].dgr + 1);

    if (val == 0 && val1 == 0 && val2 == 0)
	return 0;

    return 1;
}


int
poly_multiply_main(int UNUSED(ac), char **UNUSED(av))
{
    int ret;

    mpoly_init();

    ret = test_bn_mpoly();

    if (ret != 0) {
	bu_log("\nInvalid pmul_output.\n");
	return ret;
    }

    bu_log("\nFunction computes correctly\n");
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
