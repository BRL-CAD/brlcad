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
test_mat_angles(void)
{
    int failures = 0;
    const char *test = "mat_angles";
    vect_t vec_ae = {1.0, 0.0, 0.0};
    vect_t vec_twist_zero = {0.0, 1.0, 0.0};
    vect_t vec_twist_ninety = {0.0, 0.0, 1.0};
    vect_t vec_pole = {0.0, 0.0, 1.0};
    fastf_t az = -1.0;
    fastf_t el = -1.0;
    fastf_t twist = -1.0;

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
    mat_t m = MAT_INIT_ZERO;

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
    point_t anchor = {1.0, 2.0, 3.0};
    vect_t axis = {0.0, 0.0, 1.0};
    point_t p = {2.0, 2.0, 3.0};
    point_t axis_point = {1.0, 2.0, 10.0};
    point_t expected = {1.0, 3.0, 3.0};
    point_t out = VINIT_ZERO;
    mat_t m = MAT_INIT_ZERO;

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
    {"angles", test_mat_angles},
    {"orientation", test_mat_orientation},
    {"transform", test_mat_transform},
    {"opt", test_mat_opt},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_dispatch(argc, argv, mat_cases);
}
