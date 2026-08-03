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
    matp_t dup = NULL;

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
