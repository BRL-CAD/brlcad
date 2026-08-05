/*                         T E S T _ M A T . C
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
vect_close_or_neg(const vect_t a, const vect_t b, double tol)
{
    vect_t negb = VINIT_ZERO;

    VSCALE(negb, b, -1.0);
    return vect_close(a, b, tol) || vect_close(a, negb, tol);
}


static int
test_mat_basic(void)
{
    int failures = 0;
    const char *test = "mat_basic";
    static const struct {
	mat_t a;
	mat_t b;
	mat_t expected;
    } mul_cases[] = {
	{{
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }, {
		1.0, 2.1, 3.3, 4.4,
		5.5, 6.6, 7.7, 8.8,
		9.9, 10.1, 11.11, 12.12,
		13.13, 14.14, 15.15, 16.16
	    }, {
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }},
	{{
		2.8, 1.7, 3205.555, 87.76,
		55.0, 66.0, 77.0, 0.0,
		0.0, 10.1, 46.8, 537.999,
		13.0, 14.0, 15.0, 24382.5373
	    }, {
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }, {
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }},
	{{
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }, {
		33.34, 28.7135672, 44.84, 55.85,
		1.0, 1.0, 0.0, 0.0,
		0.0, 7843.4444, 11.0, 12.0,
		473.232, 83.17, 75.0, 8.417
	    }, {
		33.34, 28.7135672, 44.84, 55.85,
		1.0, 1.0, 0.0, 0.0,
		0.0, 7843.4444, 11.0, 12.0,
		473.232, 83.17, 75.0, 8.417
	    }},
	{{
		3688.701794246889, 5712.945961512324, 8172.282142910184, 5109.8260742578495,
		2697.196099611537, 2586.742536023153, 5067.2882336104285, 1149.9923643570387,
		6827.968448547921, 9548.849169087358, 5418.682604729545, 9117.6798488191,
		5478.474626204755, 7525.19019682893, 3172.945417883698, 8609.907811093646
	    }, {
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }, {
		3688.701794246889, 5712.945961512324, 8172.282142910184, 5109.8260742578495,
		2697.196099611537, 2586.742536023153, 5067.2882336104285, 1149.9923643570387,
		6827.968448547921, 9548.849169087358, 5418.682604729545, 9117.6798488191,
		5478.474626204755, 7525.19019682893, 3172.945417883698, 8609.907811093646
	    }},
	{{
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }, {
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }, {
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }},
	{{
		1.0, 2.0, 3.0, 4.0,
		5.0, 6.0, 7.0, 8.0,
		9.0, 10.0, 11.0, 12.0,
		13.0, 14.0, 15.0, 16.0
	    }, {
		16.0, 15.0, 14.0, 13.0,
		12.0, 11.0, 10.0, 9.0,
		8.0, 7.0, 6.0, 5.0,
		4.0, 3.0, 2.0, 1.0
	    }, {
		80.0, 70.0, 60.0, 50.0,
		240.0, 214.0, 188.0, 162.0,
		400.0, 358.0, 316.0, 274.0,
		560.0, 502.0, 444.0, 386.0
	    }},
	{{
		3.2, 62.9, 53.0, 71.5,
		98.6, 75.4, 9.6, 89.9,
		37.7, 49.0, 60.2, 55.2,
		27.8, 40.2, 0.5, 78.1
	    }, {
		37.1, 73.1, 4.9, 72.1,
		68.2, 64.7, 75.7, 87.4,
		85.8, 45.0, 94.5, 30.8,
		95.9, 45.9, 45.4, 49.1
	    }, {
		15812.75, 9970.4, 13031.81, 10871.23,
		18245.43, 16644.45, 11179.58, 18408.79,
		15199.31, 11168.85, 12089.01, 11565.25,
		11305.71, 8240.41, 6772.35, 9367.97
	    }}
    };
    static const struct {
	mat_t a;
	mat_t b;
	mat_t c;
	mat_t expected;
    } mul3_cases[] = {
	{{
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }, {
		1.0, 2.0, 3.0, 4.0,
		5.0, 6.0, 7.0, 8.0,
		9.0, 10.0, 11.0, 12.0,
		13.0, 14.0, 15.0, 16.0
	    }, {
		16.0, 15.0, 14.0, 13.0,
		12.0, 11.0, 10.0, 9.0,
		8.0, 7.0, 6.0, 5.0,
		4.0, 3.0, 2.0, 1.0
	    }, {
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }},
	{{
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }, {
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }, {
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }, {
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }},
	{{
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }, {
		1.0, 2.0, 3.0, 4.0,
		5.0, 6.0, 7.0, 8.0,
		9.0, 10.0, 11.0, 12.0,
		13.0, 14.0, 15.0, 16.0
	    }, {
		16.0, 15.0, 14.0, 13.0,
		12.0, 11.0, 10.0, 9.0,
		8.0, 7.0, 6.0, 5.0,
		4.0, 3.0, 2.0, 1.0
	    }, {
		80.0, 70.0, 60.0, 50.0,
		240.0, 214.0, 188.0, 162.0,
		400.0, 358.0, 316.0, 274.0,
		560.0, 502.0, 444.0, 386.0
	    }},
	{{
		3.1, 4.2, 1.9, 1.8,
		2.9, 0.3, 4.1, 4.5,
		2.9, 2.7, 0.7, 1.7,
		1.6, 0.8, 4.5, 5.0
	    }, {
		1.1, 2.1, 2.3, 3.5,
		2.7, 1.8, 2.6, 3.8,
		1.5, 0.7, 3.0, 4.4,
		2.3, 0.3, 4.5, 4.3
	    }, {
		3.2, 2.0, 2.6, 0.0,
		2.1, 4.0, 0.5, 1.6,
		4.3, 3.1, 2.7, 4.7,
		0.9, 3.3, 2.4, 4.7
	    }, {
		278.616, 347.578, 253.473, 376.876,
		304.197, 369.044, 283.557, 434.156,
		203.015, 252.984, 183.327, 274.048,
		315.303, 376.398, 294.975, 446.11
	    }}
    };
    static const struct {
	mat_t a;
	mat_t b;
	mat_t c;
	mat_t d;
	mat_t expected;
    } mul4_cases[] = {
	{{
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }, {
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }, {
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }, {
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }, {
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }},
	{{
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }, {
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }, {
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }, {
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }, {
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }},
	{{
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }, {
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }, {
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }, {
		1.0, 2.0, 3.0, 4.0,
		5.0, 6.0, 7.0, 8.0,
		9.0, 10.0, 11.0, 12.0,
		13.0, 14.0, 15.0, 16.0
	    }, {
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	    }},
	{{
		1.0, 2.0, 3.0, 4.0,
		5.0, 6.0, 7.0, 8.0,
		9.0, 10.0, 11.0, 12.0,
		13.0, 14.0, 15.0, 16.0
	    }, {
		1.0, 2.0, 3.0, 4.0,
		5.0, 6.0, 7.0, 8.0,
		9.0, 10.0, 11.0, 12.0,
		13.0, 14.0, 15.0, 16.0
	    }, {
		1.0, 2.0, 3.0, 4.0,
		5.0, 6.0, 7.0, 8.0,
		9.0, 10.0, 11.0, 12.0,
		13.0, 14.0, 15.0, 16.0
	    }, {
		1.0, 2.0, 3.0, 4.0,
		5.0, 6.0, 7.0, 8.0,
		9.0, 10.0, 11.0, 12.0,
		13.0, 14.0, 15.0, 16.0
	    }, {
		113960.0, 129040.0, 144120.0, 159200.0,
		263272.0, 298128.0, 332984.0, 367840.0,
		412584.0, 467216.0, 521848.0, 576480.0,
		561896.0, 636304.0, 710712.0, 785120.0
	    }}
    };
    mat_t a = {
	1.0, 2.0, 3.0, 4.0,
	5.0, 6.0, 7.0, 8.0,
	9.0, 10.0, 11.0, 12.0,
	13.0, 14.0, 15.0, 16.0
    };
    mat_t b = {
	16.0, 15.0, 14.0, 13.0,
	12.0, 11.0, 10.0, 9.0,
	8.0, 7.0, 6.0, 5.0,
	4.0, 3.0, 2.0, 1.0
    };
    mat_t expected = MAT_INIT_ZERO;
    mat_t aliased = MAT_INIT_ZERO;
    mat_t actual = MAT_INIT_ZERO;
    matp_t dup = NULL;
    int i;

    bn_mat_mul(expected, a, b);
    MAT_COPY(aliased, b);
    bn_mat_mul2(a, aliased);
    if (!mat_close(expected, aliased, 0.0)) {
	report_failure(test, "bn_mat_mul2 failed the in-place multiply contract");
	failures++;
    }

    dup = bn_mat_dup(a);
    if (!dup || !mat_close(a, dup, 0.0)) {
	report_failure(test, "bn_mat_dup failed to produce an exact copy");
	failures++;
    }
    if (dup) {
	bu_free((void *)dup, "matrix duplicate");
    }

    for (i = 0; i < (int)(sizeof(mul_cases) / sizeof(mul_cases[0])); i++) {
	bn_mat_mul(actual, mul_cases[i].a, mul_cases[i].b);
	if (!mat_close(actual, mul_cases[i].expected, 1.0e-9)) {
	    report_failure(test, "bn_mat_mul legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(mul3_cases) / sizeof(mul3_cases[0])); i++) {
	bn_mat_mul3(actual, mul3_cases[i].a, mul3_cases[i].b, mul3_cases[i].c);
	if (!mat_close(actual, mul3_cases[i].expected, 1.0e-9)) {
	    report_failure(test, "bn_mat_mul3 legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(mul4_cases) / sizeof(mul4_cases[0])); i++) {
	bn_mat_mul4(actual, mul4_cases[i].a, mul4_cases[i].b, mul4_cases[i].c, mul4_cases[i].d);
	if (!mat_close(actual, mul4_cases[i].expected, 1.0e-9)) {
	    report_failure(test, "bn_mat_mul4 legacy case %d failed", i);
	    failures++;
	}
    }

    return failures;
}


static int
test_mat_exact(void)
{
    int failures = 0;
    const char *test = "mat_exact";
    const double legacy_tol = BN_TOL_DIST;
    static const struct {
	mat_t m;
	hvect_t v;
	hvect_t expected;
    } matxvec_cases[] = {
	{{1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0},
	 {2.78, 3.45, 16.7, 38.22},
	 {2.78, 3.45, 16.7, 38.22}},
	{{18.59, 25.82, 46.39, 16.56, 46.13, 22.16, 29.66, 92.89, 67.58, 79.96, 59.47, 51.4, 55.83, 48.5, 82.51, 73.52},
	 {0.0, 0.0, 0.0, 0.0},
	 {0.0, 0.0, 0.0, 0.0}},
	{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
	 {1.1, 2.2, 3.3, 4.4},
	 {0.0, 0.0, 0.0, 0.0}},
	{{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0},
	 {1.0, 2.0, 3.0, 4.0},
	 {30.0, 70.0, 110.0, 150.0}},
	{{0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0},
	 {0.0, 1.0, 2.0, 3.0},
	 {14.0, 38.0, 62.0, 86.0}},
	{{18.59, 25.82, 46.39, 16.56, 46.13, 22.16, 29.66, 92.89, 67.58, 79.96, 59.47, 51.4, 55.83, 48.5, 82.51, 73.52},
	 {7.31, 68.38, 65.97, 3.26},
	 {5015.7984, 4112.0027, 10052.4745, 9407.4072}}
    };
    static const struct {
	int singular;
	mat_t m;
	mat_t expected;
    } inverse_cases[] = {
	{1, {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, MAT_INIT_ZERO},
	{0, {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0},
	    {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0}},
	{1, {1.0, 2.0, 2.0, 0.0, 1.0, 1.0, 3.0, 4.0, 2.0, 2.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0}, MAT_INIT_ZERO},
	{1, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0}, MAT_INIT_ZERO},
	{0, {14.16, 71.27, 95.53, 77.27, 91.51, 26.83, 17.56, 6.94, 30.11, 40.47, 38.1, 39.27, 56.52, 90.12, 87.96, 84.3},
	    {0.00836179, 0.0114599, 0.0844829, -0.0479631, -0.0600379, -0.0107874, -0.349984, 0.218954,
	     0.0425546, 0.0207413, -0.0527631, -0.0161344, 0.0141745, -0.0177931, 0.372557, -0.173216}}
    };
    static const struct {
	mat_t in;
	mat_t expected;
    } trn_cases[] = {
	{{1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0},
	 {1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0}},
	{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
	 {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
	{{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0},
	 {1.0, 5.0, 9.0, 13.0, 2.0, 6.0, 10.0, 14.0, 3.0, 7.0, 11.0, 15.0, 4.0, 8.0, 12.0, 16.0}},
	{{99.01, 41.88, 17.07, 37.47, 38.8, 42.89, 34.48, 82.84, 59.97, 23.74, 6.98, 27.81, 1.64, 86.35, 43.57, 13.87},
	 {99.01, 38.8, 59.97, 1.64, 41.88, 42.89, 23.74, 86.35, 17.07, 34.48, 6.98, 43.57, 37.47, 82.84, 27.81, 13.87}}
    };
    static const struct {
	mat_t in;
	int expected;
    } identity_cases[] = {
	{{1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0}, 1},
	{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, 0},
	{{15.58, 76.15, 18.45, 80.55, 52.36, 28.49, 85.75, 56.75, 76.44, 66.57, 32.39, 52.18, 39.21, 96.09, 98.66, 30.95}, 0},
	{{1.001, 0.0, 0.0, 0.0, 0.0, 1.001, 0.0, 0.0, 0.0, 0.0, 1.001, 0.0, 0.0, 0.0, 0.0, 1.001}, 0}
    };
    static const struct {
	mat_t in;
	double expected;
    } det3_cases[] = {
	{{1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0}, 1.0},
	{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, 0.0},
	{{77.64, 50.22, 46.68, 16.35, 41.25, 71.79, 24.01, 23.88, 37.37, 94.83, 34.25, 4.43, 95.01, 81.8, 1.45, 45.57}, 45601.558488},
	{{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0}, 0.0}
    };
    static const struct {
	mat_t in;
	double expected;
    } determinant_cases[] = {
	{{1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0}, 1.0},
	{{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, 0.0},
	{{80.22, 28.45, 95.46, 74.05, 53.38, 74.98, 71.55, 24.86, 65.01, 46.42, 64.24, 49.13, 75.74, 79.22, 64.61, 17.33}, 2.1004520202541295e6},
	{{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0}, 0.0}
    };
    static const struct {
	mat_t a;
	mat_t b;
	int expected;
    } equal_cases[] = {
	{{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0},
	 {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0}, 1},
	{{70.46, 38.7, 1.57, 84.19, 38.53, 52.52, 89.28, 8.58, 89.96, 61.36, 5.45, 63.62, 99.98, 82.15, 65.44, 67.46},
	 {70.46, 38.7, 1.57, 84.19, 38.53, 52.52, 89.28, 8.58, 89.96, 61.36, 5.45, 63.62, 99.98, 82.15, 65.44, 67.46}, 1},
	{{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0},
	 {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, 0},
	{{1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0},
	 {71.53, 18.85, 47.48, 24.56, 42.34, 98.18, 42.1, 74.61, 30.47, 49.18, 63.06, 2.11, 58.31, 60.61, 0.62, 34.23}, 0}
    };
    static const struct {
	double y;
	double x;
	double expected;
    } atan2_cases[] = {
	{0.0, 0.0, 0.0},
	{2.8, 0.0, 1.570796326794896},
	{-2.8, 0.0, -1.570796326794896},
	{2.55, 8.76, 0.283268001141651},
	{-2.8, -7.2, -2.770701364777130}
    };
    hvect_t hv_expected = {2.6, -5.5, 8.12, 1.0};
    vect_t ht_expected = {0.14864864864864866, 0.2972972972972973, 0.4459459459459459};
    hvect_t hv_in = {1.1, 2.2, 3.3, 7.4};
    hvect_t hv_out = HINIT_ZERO;
    vect_t v_in = {2.6, -5.5, 8.12};
    vect_t v_out = VINIT_ZERO;
    mat_t actual_mat = MAT_INIT_ZERO;
    hvect_t actual_h = HINIT_ZERO;
    int i;
    struct bn_tol tol = BN_TOL_INIT_TOL;

    for (i = 0; i < (int)(sizeof(matxvec_cases) / sizeof(matxvec_cases[0])); i++) {
	bn_matXvec(actual_h, matxvec_cases[i].m, matxvec_cases[i].v);
	if (!hvect_close(actual_h, matxvec_cases[i].expected, legacy_tol)) {
	    report_failure(test, "bn_matXvec legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(inverse_cases) / sizeof(inverse_cases[0])); i++) {
	if (!bn_mat_inverse(actual_mat, inverse_cases[i].m)) {
	    if (!inverse_cases[i].singular) {
		report_failure(test, "bn_mat_inverse failed for non-singular case %d", i);
		failures++;
	    }
	    continue;
	}
	if (inverse_cases[i].singular || !mat_close(actual_mat, inverse_cases[i].expected, 1.0e-5)) {
	    report_failure(test, "bn_mat_inverse legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(trn_cases) / sizeof(trn_cases[0])); i++) {
	bn_mat_trn(actual_mat, trn_cases[i].in);
	if (!mat_close(actual_mat, trn_cases[i].expected, legacy_tol)) {
	    report_failure(test, "bn_mat_trn legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(identity_cases) / sizeof(identity_cases[0])); i++) {
	if (bn_mat_is_identity(identity_cases[i].in) != identity_cases[i].expected) {
	    report_failure(test, "bn_mat_is_identity legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(det3_cases) / sizeof(det3_cases[0])); i++) {
	if (!NEAR_EQUAL(bn_mat_det3(det3_cases[i].in), det3_cases[i].expected, legacy_tol)) {
	    report_failure(test, "bn_mat_det3 legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(determinant_cases) / sizeof(determinant_cases[0])); i++) {
	if (!NEAR_EQUAL(bn_mat_determinant(determinant_cases[i].in), determinant_cases[i].expected, legacy_tol)) {
	    report_failure(test, "bn_mat_determinant legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(equal_cases) / sizeof(equal_cases[0])); i++) {
	if (bn_mat_is_equal(equal_cases[i].a, equal_cases[i].b, &tol) != equal_cases[i].expected) {
	    report_failure(test, "bn_mat_is_equal legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(atan2_cases) / sizeof(atan2_cases[0])); i++) {
	if (!NEAR_EQUAL(bn_atan2(atan2_cases[i].y, atan2_cases[i].x), atan2_cases[i].expected, 1.0e-5)) {
	    report_failure(test, "bn_atan2 legacy case %d failed", i);
	    failures++;
	}
    }

    bn_vtoh_move(hv_out, v_in);
    if (!hvect_close(hv_out, hv_expected, 0.0)) {
	report_failure(test, "bn_vtoh_move failed the legacy exact case");
	failures++;
    }

    bn_htov_move(v_out, hv_in);
    if (!vect_close(v_out, ht_expected, 1.0e-12)) {
	report_failure(test, "bn_htov_move failed the legacy exact case");
	failures++;
    }

    return failures;
}


static int
test_mat_angles(void)
{
    int failures = 0;
    const char *test = "mat_angles";
    const double legacy_tol = BN_TOL_DIST;
    static const struct {
	vect_t in;
	double az;
	double el;
    } ae_vec_cases[] = {
	{{0.0, 1.0, 0.0}, 90.0, 0.0},
	{{5.7, -2.34, 19.61}, 337.68055960702, 72.556852698595},
	{{-23.78, 42.0, 3.141}, 119.518125952119, 3.7234738236524}
    };
    static const struct {
	double az;
	double el;
	vect_t expected;
    } vec_ae_cases[] = {
	{0.0, 0.0, {1.0, 0.0, 0.0}},
	{0.0, 1.57079632679, {0.0, 0.0, 1.0}},
	{0.0, -1.57079632679, {0.0, 0.0, -1.0}},
	{0.0, 3.14159265359, {-1.0, 0.0, 0.0}},
	{0.0, -3.14159265359, {-1.0, 0.0, 0.0}},
	{1.57079632679, 0.0, {0.0, 1.0, 0.0}},
	{1.57079632679, 1.57079632679, {0.0, 0.0, 1.0}},
	{1.57079632679, -1.57079632679, {0.0, 0.0, -1.0}},
	{1.57079632679, 3.14159265359, {0.0, -1.0, 0.0}},
	{1.57079632679, -3.14159265359, {0.0, -1.0, 0.0}},
	{-1.57079632679, 0.0, {0.0, -1.0, 0.0}},
	{-1.57079632679, 1.57079632679, {0.0, 0.0, 1.0}},
	{-1.57079632679, -1.57079632679, {0.0, 0.0, -1.0}},
	{-1.57079632679, 3.14159265359, {0.0, 1.0, 0.0}},
	{-1.57079632679, -3.14159265359, {0.0, 1.0, 0.0}},
	{3.14159265359, 0.0, {-1.0, 0.0, 0.0}},
	{3.14159265359, 1.57079632679, {0.0, 0.0, 1.0}},
	{3.14159265359, -1.57079632679, {0.0, 0.0, -1.0}},
	{3.14159265359, 3.14159265359, {1.0, 0.0, 0.0}},
	{3.14159265359, -3.14159265359, {1.0, 0.0, 0.0}},
	{-3.14159265359, 0.0, {-1.0, 0.0, 0.0}},
	{-3.14159265359, 1.5707963267, {0.0, 0.0, 1.0}},
	{-3.14159265359, -1.5707963267, {0.0, 0.0, -1.0}},
	{-3.14159265359, 3.14159265359, {1.0, 0.0, 0.0}},
	{-3.14159265359, -3.14159265359, {1.0, 0.0, 0.0}},
	{1.0, 1.0, {0.291926581820, 0.454648713558, 0.841470984697}},
	{0.5, 1.2, {0.317998846662, 0.173723561699, 0.932039085893}}
    };
    static const struct {
	double az;
	double el;
	double d;
	vect_t expected;
    } vec_aed_cases[] = {
	{1.0, 1.0, 5.0, {1.45963290910, 2.27324356779, 4.20735492349}},
	{0.3845, 0.286, 18.3354, {16.3062567701, 6.59816438267, 5.17272752864}}
    };
    static const struct {
	double az;
	double el;
	mat_t expected;
    } mat_ae_cases[] = {
	{0.0, 90.0, {0.0, 0.0, -1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0}}
    };
    static const struct {
	double x;
	double y;
	double z;
	mat_t expected;
    } angle_cases[] = {
	{30.0, 40.0, 50.0, {0.4924038765, -0.5868240888, 0.6427876097, 0.0, 0.8700019038, 0.310468461, -0.3830222216, 0.0, 0.0252013863, 0.7478280708, 0.6634139482, 0.0, 0.0, 0.0, 0.0, 1.0}},
	{84.23, 19.5, 38.9, {0.733604282, -0.591944033, 0.3338068592, 0.0, 0.3215992025, -0.1303153824, -0.9378655842, 0.0, 0.5986641049, 0.7953742282, 0.0947688064, 0.0, 0.0, 0.0, 0.0, 1.0}}
    };
    static const struct {
	double x;
	double y;
	double z;
	mat_t expected;
    } angle_rad_cases[] = {
	{1.14, 0.856, 0.321, {0.6219827767, -0.2068090198, 0.7552267572, 0.0, 0.7829298453, 0.1797494268, -0.5955761923, 0.0, -0.0125810482, 0.9617277021, 0.2737180014, 0.0, 0.0, 0.0, 0.0, 1.0}},
	{0.6, 1.0, 0.92, {0.3273260276, -0.4298653899, 0.8414709848, 0.0, 0.9444818397, 0.1219905484, -0.3050776304, 0.0, 0.0284908076, 0.8946139126, 0.4459307359, 0.0, 0.0, 0.0, 0.0, 1.0}}
    };
    vect_t vec_ae = {1.0, 0.0, 0.0};
    vect_t vec_twist_zero = {0.0, 1.0, 0.0};
    vect_t vec_twist_ninety = {0.0, 0.0, 1.0};
    vect_t vec_pole = {0.0, 0.0, 1.0};
    fastf_t az = -1.0;
    fastf_t el = -1.0;
    fastf_t twist = -1.0;
    vect_t actual_vec = VINIT_ZERO;
    mat_t actual_mat = MAT_INIT_ZERO;
    int i;

    bn_aet_vec(&az, &el, &twist, vec_ae, vec_twist_zero, 1.0e-12);
    if (!scalar_close(az, 0.0, 1.0e-12) ||
	!scalar_close(el, 0.0, 1.0e-12) ||
	!scalar_close(twist, 0.0, 1.0e-12)) {
	report_failure(test, "bn_aet_vec failed for the zero-twist +X orientation");
	failures++;
    }

    bn_aet_vec(&az, &el, &twist, vec_ae, vec_twist_ninety, 1.0e-12);
    if (!scalar_close(az, 0.0, 1.0e-12) ||
	!scalar_close(el, 0.0, 1.0e-12) ||
	!scalar_close(twist, 90.0, 1.0e-9)) {
	report_failure(test, "bn_aet_vec failed for a +90 degree twist");
	failures++;
    }

    bn_aet_vec(&az, &el, &twist, vec_pole, vec_twist_zero, 1.0e-12);
    if (!scalar_close(az, 0.0, 1.0e-12) ||
	!scalar_close(el, 90.0, 1.0e-12) ||
	!scalar_close(twist, 0.0, 1.0e-12)) {
	report_failure(test, "bn_aet_vec failed at the elevation pole special case");
	failures++;
    }

    for (i = 0; i < (int)(sizeof(mat_ae_cases) / sizeof(mat_ae_cases[0])); i++) {
	bn_mat_ae(actual_mat, mat_ae_cases[i].az, mat_ae_cases[i].el);
	if (!mat_close(actual_mat, mat_ae_cases[i].expected, legacy_tol)) {
	    report_failure(test, "bn_mat_ae legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(ae_vec_cases) / sizeof(ae_vec_cases[0])); i++) {
	bn_ae_vec(&az, &el, ae_vec_cases[i].in);
	if (!NEAR_EQUAL(az, ae_vec_cases[i].az, legacy_tol) ||
	    !NEAR_EQUAL(el, ae_vec_cases[i].el, legacy_tol)) {
	    report_failure(test, "bn_ae_vec legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(vec_ae_cases) / sizeof(vec_ae_cases[0])); i++) {
	bn_vec_ae(actual_vec, vec_ae_cases[i].az, vec_ae_cases[i].el);
	if (!vect_close(actual_vec, vec_ae_cases[i].expected, legacy_tol)) {
	    report_failure(test, "bn_vec_ae legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(vec_aed_cases) / sizeof(vec_aed_cases[0])); i++) {
	bn_vec_aed(actual_vec, vec_aed_cases[i].az, vec_aed_cases[i].el, vec_aed_cases[i].d);
	if (!vect_close(actual_vec, vec_aed_cases[i].expected, legacy_tol)) {
	    report_failure(test, "bn_vec_aed legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(angle_cases) / sizeof(angle_cases[0])); i++) {
	bn_mat_angles(actual_mat, angle_cases[i].x, angle_cases[i].y, angle_cases[i].z);
	if (!mat_close(actual_mat, angle_cases[i].expected, legacy_tol)) {
	    report_failure(test, "bn_mat_angles legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(angle_rad_cases) / sizeof(angle_rad_cases[0])); i++) {
	bn_mat_angles_rad(actual_mat, angle_rad_cases[i].x, angle_rad_cases[i].y, angle_rad_cases[i].z);
	if (!mat_close(actual_mat, angle_rad_cases[i].expected, legacy_tol)) {
	    report_failure(test, "bn_mat_angles_rad legacy case %d failed", i);
	    failures++;
	}
    }

    return failures;
}


static int
test_mat_orientation(void)
{
    int failures = 0;
    const char *test = "mat_orientation";
    struct bn_tol tol = BN_TOL_INIT_TOL;
    vect_t input = {1.0, 2.0, 3.0};
    vect_t perp = VINIT_ZERO;
    vect_t expected_perp = {0.0, 0.0, 1.0};
    vect_t from = {1.0, 0.0, 0.0};
    vect_t to = {0.0, 1.0, 0.0};
    vect_t opposite = {-1.0, 0.0, 0.0};
    vect_t dir = {1.0, 1.0, -2.0};
    vect_t out = VINIT_ZERO;
    hvect_t homogeneous = HINIT_ZERO;
    mat_t m = MAT_INIT_ZERO;

    if (!HZERO(homogeneous)) {
	report_failure(test, "HZERO failed for a zero homogeneous vector");
	failures++;
    }

    bn_vec_perp(perp, input);
    if (!NEAR_ZERO(VDOT(perp, input), 1.0e-12) || NEAR_ZERO(MAGSQ(perp), 1.0e-12)) {
	report_failure(test, "bn_vec_perp failed for a non-zero input vector");
	failures++;
    }

    VSETALL(input, 0.0);
    bn_vec_perp(perp, input);
    if (!vect_close(perp, expected_perp, 0.0)) {
	report_failure(test, "bn_vec_perp failed for the zero-vector special case");
	failures++;
    }

    bn_mat_fromto(m, from, to, &tol);
    MAT4X3VEC(out, m, from);
    VUNITIZE(out);
    if (VDOT(out, to) < 1.0 - 1.0e-12 || !orthonormal_rotation(m, 1.0e-12)) {
	report_failure(test, "bn_mat_fromto failed for perpendicular vectors");
	failures++;
    }

    bn_mat_fromto(m, from, opposite, &tol);
    MAT4X3VEC(out, m, from);
    VUNITIZE(out);
    if (VDOT(out, opposite) < 1.0 - 1.0e-12 || !orthonormal_rotation(m, 1.0e-12)) {
	report_failure(test, "bn_mat_fromto failed for opposite vectors");
	failures++;
    }

    VUNITIZE(dir);
    bn_mat_lookat(m, dir, 0);
    MAT4X3VEC(out, m, dir);
    VUNITIZE(out);
    if (fabs(out[X]) > 1.0e-9 || fabs(out[Y]) > 1.0e-9 || out[Z] > -0.999 ||
	!orthonormal_rotation(m, 1.0e-12)) {
	report_failure(test, "bn_mat_lookat failed to align the direction vector with -Z");
	failures++;
    }

    return failures;
}


static int
test_mat_transform(void)
{
    int failures = 0;
    const char *test = "mat_transform";
    const double legacy_tol = BN_TOL_DIST;
    static const struct {
	double sinv;
	double cosv;
	mat_t expected;
    } xrot_cases[] = {
	{0.342020143325668, 0.939692620785908, {1.0, 0.0, 0.0, 0.0, 0.0, 0.9396926208, -0.3420201433, 0.0, 0.0, 0.3420201433, 0.9396926208, 0.0, 0.0, 0.0, 0.0, 1.0}},
	{-0.78260815685241, 0.622514636637619, {1.0, 0.0, 0.0, 0.0, 0.0, 0.6225146366, 0.7826081569, 0.0, 0.0, -0.7826081569, 0.6225146366, 0.0, 0.0, 0.0, 0.0, 1.0}}
    };
    static const struct {
	double sinv;
	double cosv;
	mat_t expected;
    } yrot_cases[] = {
	{0.30901699437494, 0.951056516295153, {0.9510565163, 0.0, -0.3090169944, 0.0, 0.0, 1.0, 0.0, 0.0, 0.3090169944, 0.0, 0.9510565163, 0.0, 0.0, 0.0, 0.0, 1.0}},
	{0.17364817766693, 0.984807753012208, {0.9848077530122, 0.0, -0.1736481777, 0.0, 0.0, 1.0, 0.0, 0.0, 0.1736481777, 0.0, 0.984807753, 0.0, 0.0, 0.0, 0.0, 1.0}}
    };
    static const struct {
	double sinv;
	double cosv;
	mat_t expected;
    } zrot_cases[] = {
	{0.669130606358858, 0.743144825477394, {0.743144825477394, -0.669130606358858, 0.0, 0.0, 0.669130606358858, 0.743144825477394, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0}},
	{-0.99619469809174, -0.087155742747658, {-0.087155742747658, 0.996194698091745, 0.0, 0.0, -0.996194698091746, -0.087155742747658, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0}}
    };
    static const struct {
	point_t p;
	double s;
	int expected_error;
	mat_t expected;
    } scale_cases[] = {
	{{3.0, 4.0, 5.0}, 1.5, 0, {1.0, 0.0, 0.0, -1.0, 0.0, 1.0, 0.0, -1.333333333333333, 0.0, 0.0, 1.0, -1.666666666666667, 0.0, 0.0, 0.0, 0.666666666666667}},
	{{2.18, -4.55, -17.4}, 0.31, 0, {1.0, 0.0, 0.0, 4.852258064516120, 0.0, 1.0, 0.0, -10.127419354838700, 0.0, 0.0, 1.0, -38.729032258064500, 0.0, 0.0, 0.0, 3.225806451612900}}
    };
    static const struct {
	mat_t xform;
	point_t p;
	mat_t expected;
    } xform_cases[] = {
	{{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0},
	 {-2.0, 4.5, 6.7},
	 {-25.0, -26.0, -27.0, 219.9, 63.5, 69.0, 74.5, -602.65, 96.1, 103.8, 111.5, -902.75, 13.0, 14.0, 15.0, -121.5}}
    };
    static const struct {
	mat_t in;
	int expected;
    } ck_cases[] = {
	{{-0.064769339233561, -0.740316935054896, -0.669130606358858, 0.0, -0.920939044198179, -0.213876288688268, 0.325773249374894, 0.0, -0.384286624235859, 0.637328619165857, -0.667934144677115, 0.0, 0.0, 0.0, 0.0, 1.0}, 0},
	{{-0.384795044679446, -0.906520316365329, 0.17364817766693, 23.835, -0.772197474581702, 0.419235049422787, 0.477444272753497, -1.23, -0.505612335529687, 0.049627506006009, -0.861332268528144, -46.321, 0.0, 0.0, 0.0, 1.0}, 0},
	{{-0.38457345, -0.944, 0.46483, 23.835, 0.27474, 0.4192787, 0.4272753497, -1.23, -0.87, 0.06009, 0.999144, -46.321, 0.0, 0.0, 0.0, 1.0}, -1}
    };
    point_t anchor = {1.0, 2.0, 3.0};
    vect_t axis = {0.0, 0.0, 1.0};
    point_t p = {2.0, 2.0, 3.0};
    point_t axis_point = {1.0, 2.0, 10.0};
    point_t expected = {1.0, 3.0, 3.0};
    point_t out = VINIT_ZERO;
    mat_t m = MAT_INIT_ZERO;
    int i;

    bn_mat_arb_rot(m, anchor, axis, M_PI_2);
    MAT4X3PNT(out, m, p);
    if (!vect_close(out, expected, 1.0e-12)) {
	report_failure(test, "bn_mat_arb_rot failed to rotate a point about the requested axis");
	failures++;
    }

    MAT4X3PNT(out, m, axis_point);
    if (!vect_close(out, axis_point, 1.0e-12)) {
	report_failure(test, "bn_mat_arb_rot moved a point lying on the rotation axis");
	failures++;
    }

    MAT_IDN(m);
    if (bn_mat_is_non_unif(m) != 0) {
	report_failure(test, "bn_mat_is_non_unif misclassified the identity matrix");
	failures++;
    }

    MAT_IDN(m);
    m[0] = 2.0;
    if (bn_mat_is_non_unif(m) != 1) {
	report_failure(test, "bn_mat_is_non_unif failed to detect non-uniform scale");
	failures++;
    }

    MAT_IDN(m);
    m[12] = 0.5;
    if (bn_mat_is_non_unif(m) != 2) {
	report_failure(test, "bn_mat_is_non_unif failed to detect a non-affine bottom row");
	failures++;
    }

    for (i = 0; i < (int)(sizeof(xrot_cases) / sizeof(xrot_cases[0])); i++) {
	bn_mat_xrot(m, xrot_cases[i].sinv, xrot_cases[i].cosv);
	if (!mat_close(m, xrot_cases[i].expected, legacy_tol)) {
	    report_failure(test, "bn_mat_xrot legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(yrot_cases) / sizeof(yrot_cases[0])); i++) {
	bn_mat_yrot(m, yrot_cases[i].sinv, yrot_cases[i].cosv);
	if (!mat_close(m, yrot_cases[i].expected, legacy_tol)) {
	    report_failure(test, "bn_mat_yrot legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(zrot_cases) / sizeof(zrot_cases[0])); i++) {
	bn_mat_zrot(m, zrot_cases[i].sinv, zrot_cases[i].cosv);
	if (!mat_close(m, zrot_cases[i].expected, legacy_tol)) {
	    report_failure(test, "bn_mat_zrot legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(scale_cases) / sizeof(scale_cases[0])); i++) {
	int err = bn_mat_scale_about_pnt(m, scale_cases[i].p, scale_cases[i].s);
	if (err != scale_cases[i].expected_error || !mat_close(m, scale_cases[i].expected, legacy_tol)) {
	    report_failure(test, "bn_mat_scale_about_pnt legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(xform_cases) / sizeof(xform_cases[0])); i++) {
	bn_mat_xform_about_pnt(m, xform_cases[i].xform, xform_cases[i].p);
	if (!mat_close(m, xform_cases[i].expected, legacy_tol)) {
	    report_failure(test, "bn_mat_xform_about_pnt legacy case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(ck_cases) / sizeof(ck_cases[0])); i++) {
	if (bn_mat_ck(test, ck_cases[i].in) != ck_cases[i].expected) {
	    report_failure(test, "bn_mat_ck legacy case %d failed", i);
	    failures++;
	}
    }

    return failures;
}


static int
test_mat_eigen(void)
{
    int failures = 0;
    const char *test = "mat_eigen";
    static const struct {
	double a;
	double b;
	double c;
	double val1;
	double val2;
	vect_t vec1;
	vect_t vec2;
    } cases[] = {
	{2.18, 13.9, 6.6, -9.68459058019096, 18.464590580191,
	 {0.760598630446286, -0.649222398999938, 0.0},
	 {0.649222398999938, 0.760598630446286, 0.0}},
	{1.0, 2.0, 3.0, -0.23606797749979, 4.23606797749979,
	 {0.85065080835204, -0.525731112119133, 0.0},
	 {0.525731112119133, 0.85065080835204, 0.0}}
    };
    double actual_val1;
    double actual_val2;
    vect_t actual_vec1 = VINIT_ZERO;
    vect_t actual_vec2 = VINIT_ZERO;
    int i;

    for (i = 0; i < (int)(sizeof(cases) / sizeof(cases[0])); i++) {
	bn_eigen2x2(&actual_val1, &actual_val2, actual_vec1, actual_vec2, cases[i].a, cases[i].b, cases[i].c);
	if (!NEAR_EQUAL(actual_val1, cases[i].val1, BN_TOL_DIST) ||
	    !NEAR_EQUAL(actual_val2, cases[i].val2, BN_TOL_DIST) ||
	    !vect_close_or_neg(actual_vec1, cases[i].vec1, BN_TOL_DIST) ||
	    !vect_close_or_neg(actual_vec2, cases[i].vec2, BN_TOL_DIST)) {
	    report_failure(test, "bn_eigen2x2 legacy case %d failed", i);
	    failures++;
	}
    }

    return failures;
}


static int
test_mat_opt(void)
{
    int failures = 0;
    const char *test = "mat_opt";
    const char *idn_argv[] = {"IDN"};
    const char *str_argv[] = {"{1,0,0,4,0,1,0,5,0,0,1,6,0,0,0,1}"};
    const char *argv16[] = {
	"1", "0", "0", "0",
	"0", "1", "0", "0",
	"0", "0", "1", "0",
	"0", "0", "0", "1"
    };
    const char *nonid_argv[] = {
	"27.7", "7", "7", "7",
	"7", "7", "7", "7",
	"7", "7", "7", "7",
	"7", "7", "7", "3.3"
    };
    mat_t expected = {
	27.7, 7.0, 7.0, 7.0,
	7.0, 7.0, 7.0, 7.0,
	7.0, 7.0, 7.0, 7.0,
	7.0, 7.0, 7.0, 3.3
    };
    mat_t m = MAT_INIT_ZERO;

    if (bn_opt_mat(NULL, 1, idn_argv, m) != 1 || !bn_mat_is_identity(m)) {
	report_failure(test, "bn_opt_mat failed to parse the IDN special case");
	failures++;
    }

    MAT_ZERO(m);
    if (bn_opt_mat(NULL, 1, str_argv, m) != 1 ||
	!scalar_close(m[3], 4.0, 0.0) ||
	!scalar_close(m[7], 5.0, 0.0) ||
	!scalar_close(m[11], 6.0, 0.0) ||
	!scalar_close(m[15], 1.0, 0.0)) {
	report_failure(test, "bn_opt_mat failed to parse a brace/comma formatted matrix");
	failures++;
    }

    MAT_ZERO(m);
    if (bn_opt_mat(NULL, 16, argv16, m) != 16 || !bn_mat_is_identity(m)) {
	report_failure(test, "bn_opt_mat failed to parse 16 individual matrix arguments");
	failures++;
    }

    MAT_ZERO(m);
    if (bn_opt_mat(NULL, 16, nonid_argv, m) != 16 || !mat_close(m, expected, 0.0)) {
	report_failure(test, "bn_opt_mat failed to parse a non-identity matrix from individual arguments");
	failures++;
    }

    return failures;
}


static const struct bn_api_case mat_cases[] = {
    {"basic", test_mat_basic},
    {"exact", test_mat_exact},
    {"angles", test_mat_angles},
    {"orientation", test_mat_orientation},
    {"transform", test_mat_transform},
    {"eigen", test_mat_eigen},
    {"opt", test_mat_opt},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_dispatch(argc, argv, mat_cases);
}
