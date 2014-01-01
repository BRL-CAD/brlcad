/*                      B N _ M A T . C
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

#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"

static int
mat_equal(const mat_t a, const mat_t b)
{
    int i;
    for (i = 0; i < 16; i++) {
	if (!NEAR_EQUAL(a[i], b[i], BN_TOL_DIST)) return 0;
    }
    return 1;
}

static void
scan_mat_args(char *argv[], int argnum, mat_t *mat)
{
    sscanf(argv[argnum], "%lg,%lg,%lg,%lg,%lg,%lg,%lg,%lg,%lg,%lg,%lg,%lg,%lg,%lg,%lg,%lg", &(*mat)[0], &(*mat)[1], &(*mat)[2], &(*mat)[3], &(*mat)[4], &(*mat)[5], &(*mat)[6], &(*mat)[7], &(*mat)[8], &(*mat)[9], &(*mat)[10], &(*mat)[11], &(*mat)[12], &(*mat)[13], &(*mat)[14], &(*mat)[15]);
}

static int
test_bn_mat_mul(int argc, char *argv[])
{
    mat_t m1, m2, expected, actual;

    if (argc != 5) {
	bu_exit(1, "<args> format: M1 M2 <expected_result> [%s]\n", argv[0]);
    }

    scan_mat_args(argv, 2, &m1);
    scan_mat_args(argv, 3, &m2);
    scan_mat_args(argv, 4, &expected);

    bn_mat_mul(actual, m1, m2);
    return !mat_equal(expected, actual);
}

static int
test_bn_mat_mul3(int argc, char *argv[])
{
    mat_t m1, m2, m3, expected, actual;

    if (argc != 6) {
	bu_exit(1, "<args> format: M1 M2 M3 <expected_result> [%s]\n", argv[0]);
    }

    scan_mat_args(argv, 2, &m1);
    scan_mat_args(argv, 3, &m2);
    scan_mat_args(argv, 4, &m3);
    scan_mat_args(argv, 5, &expected);

    bn_mat_mul3(actual, m1, m2, m3);
    return !mat_equal(expected, actual);
}

static int
test_bn_mat_mul4(int argc, char *argv[])
{
    mat_t m1, m2, m3, m4, expected, actual;

    if (argc != 7) {
	bu_exit(1, "<args> format: M1 M2 M3 M4 <expected_result> [%s]\n", argv[0]);
    }

    scan_mat_args(argv, 2, &m1);
    scan_mat_args(argv, 3, &m2);
    scan_mat_args(argv, 4, &m3);
    scan_mat_args(argv, 5, &m4);
    scan_mat_args(argv, 6, &expected);

    bn_mat_mul4(actual, m1, m2, m3, m4);
    return !mat_equal(expected, actual);
}

static int
test_bn_matXvec(int argc, char *argv[])
{
    mat_t m;
    hvect_t v, expected, actual;

    if (argc != 5) {
	bu_exit(1, "<args> format: M V <expected_result> [%s]\n", argv[0]);
    }

    scan_mat_args(argv, 2, &m);
    sscanf(argv[3], "%lg,%lg,%lg,%lg", &v[0], &v[1], &v[2], &v[3]);
    sscanf(argv[4], "%lg,%lg,%lg,%lg", &expected[0], &expected[1], &expected[2], &expected[3]);

    bn_matXvec(actual, m, v);
    return !HNEAR_EQUAL(expected, actual, BN_TOL_DIST);
}

static int
test_bn_mat_inverse(int argc, char *argv[])
{
    mat_t m, expected, actual;
    int singular;

    sscanf(argv[2], "%d", &singular);
    if ((argc == 4 && !singular) || (argc != 4 && argc != 5)) {
	bu_exit(1, "<args> format: 0|1 M [expected_result] [%s]\n", argv[0]);
    }

    scan_mat_args(argv, 3, &m);
    if (!singular) {
	scan_mat_args(argv, 4, &expected);
    }

    if (!bn_mat_inverse(actual, m)) {
	return !singular;
    }

    return !mat_equal(expected, actual);
}

static int
test_bn_mat_trn(int argc, char *argv[])
{
    mat_t m, expected, actual;

    if (argc != 4) {
	bu_exit(1, "<args> format: M <expected_result> [%s]\n", argv[0]);
    }

    scan_mat_args(argv, 2, &m);
    scan_mat_args(argv, 3, &expected);

    bn_mat_trn(actual, m);
    return !mat_equal(expected, actual);
}

static int
test_bn_mat_is_identity(int argc, char *argv[])
{
    mat_t m;
    int expected;

    if (argc != 4) {
	bu_exit(1, "<args> format: M <expected_result> [%s]\n", argv[0]);
    }

    scan_mat_args(argv, 2, &m);
    sscanf(argv[3], "%d", &expected);

    return !(bn_mat_is_identity(m) == expected);
}

static int
test_bn_mat_det3(int argc, char *argv[])
{
    mat_t m;
    fastf_t expected, actual;

    if (argc != 4) {
	bu_exit(1, "<args> format: M <expected_result> [%s]\n", argv[0]);
    }

    scan_mat_args(argv, 2, &m);
    sscanf(argv[3], "%lg", &expected);

    actual = bn_mat_det3(m);
    return !NEAR_EQUAL(expected, actual, BN_TOL_DIST);
}

static int
test_bn_mat_determinant(int argc, char *argv[])
{
    mat_t m = MAT_INIT_ZERO;
    fastf_t expected = 0.0, actual = 0.0;

    if (argc != 4) {
	bu_exit(1, "<args> format: M <expected_result> [%s]\n", argv[0]);
    }

    scan_mat_args(argv, 2, &m);
    sscanf(argv[3], "%lg", &expected);

    actual = bn_mat_determinant(m);
    return !NEAR_EQUAL(expected, actual, BN_TOL_DIST);
}

static int
test_bn_mat_is_equal(int argc, char *argv[])
{
    mat_t m1, m2;
    int expected;
    struct bn_tol tol = BN_TOL_INIT_ZERO;
    tol.dist = BN_TOL_DIST;
    tol.dist = BN_TOL_DIST * BN_TOL_DIST;
    tol.perp = BN_TOL_DIST;

    if (argc != 5) {
	bu_exit(1, "<args> format: M1 M2 <expected_result> [%s]\n", argv[0]);
    }

    scan_mat_args(argv, 2, &m1);
    scan_mat_args(argv, 3, &m2);
    sscanf(argv[4], "%d", &expected);

    return !(bn_mat_is_equal(m1, m2, &tol) == expected);
}

static int
test_bn_atan2(int argc, char *argv[])
{
    double x, y, expected, actual;

    if (argc != 5) {
	bu_exit(1, "<args> format: X Y <expected_result> [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg", &y);
    sscanf(argv[3], "%lg", &x);
    sscanf(argv[4], "%lg", &expected);

    actual =  bn_atan2(y, x);
    return !NEAR_EQUAL(expected, actual, 0.00001);
}

static int
test_bn_vtoh_move(int argc, char *argv[])
{
    hvect_t expected, actual;
    vect_t v;

    if (argc != 4) {
	bu_exit(1, "<args> format: V <expected_result> [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg,%lg,%lg", &v[0], &v[1], &v[2]);
    sscanf(argv[3], "%lg,%lg,%lg,%lg", &expected[0], &expected[1], &expected[2], &expected[3]);

    bn_vtoh_move(actual, v);
    return !HNEAR_EQUAL(expected, actual, BN_TOL_DIST);
}

static int
test_bn_htov_move(int argc, char *argv[])
{
    hvect_t h;
    vect_t expected, actual;

    if (argc != 4) {
	bu_exit(1, "<args> format: H <expected_result> [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg,%lg,%lg,%lg", &h[0], &h[1], &h[2], &h[3]);
    sscanf(argv[3], "%lg,%lg,%lg", &expected[0], &expected[1], &expected[2]);

    bn_htov_move(actual, h);
    return !VNEAR_EQUAL(expected, actual, BN_TOL_DIST);
}

static int
test_bn_mat_ae(int argc, char *argv[])
{
    double az, el;
    mat_t expected, actual;

    if (argc != 5) {
	bu_exit(1, "<args> format: az el <expected_result> [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg", &az);
    sscanf(argv[3], "%lg", &el);
    scan_mat_args(argv, 4, &expected);

    bn_mat_ae(actual, az, el);
    return !mat_equal(expected, actual);
}

static int
test_bn_ae_vec(int argc, char *argv[])
{
    fastf_t expected_az, expected_el, actual_az, actual_el;
    vect_t v;

    if (argc != 5) {
	bu_exit(1, "<args> format: V <expected_az> <expected_el> [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg,%lg,%lg", &v[0], &v[1], &v[2]);
    sscanf(argv[3], "%lg", &expected_az);
    sscanf(argv[4], "%lg", &expected_el);

    bn_ae_vec(&actual_az, &actual_el, v);
    return !(NEAR_EQUAL(expected_az, actual_az, BN_TOL_DIST) && NEAR_EQUAL(expected_el, actual_el, BN_TOL_DIST));
}

static int
test_bn_vec_ae(int argc, char *argv[])
{
    vect_t expected, actual;
    fastf_t az, el;

    if (argc != 5) {
	bu_exit(1, "<args> format: az el <expected_result> [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg", &az);
    sscanf(argv[3], "%lg", &el);
    sscanf(argv[4], "%lg,%lg,%lg", &expected[0], &expected[1], &expected[2]);

    bn_vec_ae(actual, az, el);
    return !VNEAR_EQUAL(expected, actual, BN_TOL_DIST);
}

static int
test_bn_vec_aed(int argc, char *argv[])
{
    vect_t expected, actual;
    fastf_t az, el, d;

    if (argc != 6) {
	bu_exit(1, "<args> format: az el d <expected_result> [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg", &az);
    sscanf(argv[3], "%lg", &el);
    sscanf(argv[4], "%lg", &d);
    sscanf(argv[5], "%lg,%lg,%lg", &expected[0], &expected[1], &expected[2]);

    bn_vec_aed(actual, az, el, d);
    return !VNEAR_EQUAL(expected, actual, BN_TOL_DIST);
}

static int
test_bn_mat_angles(int argc, char *argv[])
{
    mat_t expected, actual;
    double x, y, z;

    if (argc != 6) {
	bu_exit(1, "<args> format: x y z <expected_result> [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg", &x);
    sscanf(argv[3], "%lg", &y);
    sscanf(argv[4], "%lg", &z);
    scan_mat_args(argv, 5, &expected);

    bn_mat_angles(actual, x, y, z);
    return !mat_equal(expected, actual);
}

static int
test_bn_mat_angles_rad(int argc, char *argv[])
{
    mat_t expected, actual;
    double x, y, z;

    if (argc != 6) {
	bu_exit(1, "<args> format: x y z <expected_result> [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg", &x);
    sscanf(argv[3], "%lg", &y);
    sscanf(argv[4], "%lg", &z);
    scan_mat_args(argv, 5, &expected);

    bn_mat_angles_rad(actual, x, y, z);
    return !mat_equal(expected, actual);
}

static int
test_bn_eigen2x2(int argc, char *argv[])
{
    fastf_t expected_val1, expected_val2, actual_val1, actual_val2;
    fastf_t a, b, c;
    vect_t expected_vec1, expected_vec2, actual_vec1, actual_vec2;

    if (argc != 9) {
	bu_exit(1, "<args> format: a b c <expected_val1> <expected_val2> <expected_vec1> <expected_vec2> [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg", &a);
    sscanf(argv[3], "%lg", &b);
    sscanf(argv[4], "%lg", &c);
    sscanf(argv[5], "%lg", &expected_val1);
    sscanf(argv[6], "%lg", &expected_val2);
    sscanf(argv[7], "%lg,%lg,%lg", &expected_vec1[0], &expected_vec1[1], &expected_vec1[2]);
    sscanf(argv[8], "%lg,%lg,%lg", &expected_vec2[0], &expected_vec2[1], &expected_vec2[2]);

    bn_eigen2x2(&actual_val1, &actual_val2, actual_vec1, actual_vec2, a, b, c);
    return !(VNEAR_EQUAL(expected_vec1, actual_vec1, BN_TOL_DIST) && VNEAR_EQUAL(expected_vec2, actual_vec2, BN_TOL_DIST) && NEAR_EQUAL(expected_val1, actual_val1, BN_TOL_DIST) && NEAR_EQUAL(expected_val2, actual_val2, BN_TOL_DIST));
}

static int
test_bn_mat_xrot(int argc, char *argv[])
{
    mat_t expected, actual;
    double sinx, cosx;

    if (argc != 5) {
	bu_exit(1, "<args> format: sinx cosx <expected_result> [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg", &sinx);
    sscanf(argv[3], "%lg", &cosx);
    scan_mat_args(argv, 4, &expected);

    bn_mat_xrot(actual, sinx, cosx);
    return !mat_equal(expected, actual);
}

static int
test_bn_mat_yrot(int argc, char *argv[])
{
    mat_t expected, actual;
    double siny, cosy;

    if (argc != 5) {
	bu_exit(1, "<args> format: siny cosy <expected_result> [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg", &siny);
    sscanf(argv[3], "%lg", &cosy);
    scan_mat_args(argv, 4, &expected);

    bn_mat_yrot(actual, siny, cosy);
    return !mat_equal(expected, actual);
}

static int
test_bn_mat_zrot(int argc, char *argv[])
{
    mat_t expected, actual;
    double sinz, cosz;

    if (argc != 5) {
	bu_exit(1, "<args> format: sinz cosz <expected_result> [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg", &sinz);
    sscanf(argv[3], "%lg", &cosz);
    scan_mat_args(argv, 4, &expected);

    bn_mat_zrot(actual, sinz, cosz);
    return !mat_equal(expected, actual);
}

static int
test_bn_mat_scale_about_pt(int argc, char *argv[])
{
    mat_t expected, actual;
    point_t p;
    double s;
    int error, expected_error;

    if (argc != 6) {
	bu_exit(1, "<args> format: P s error <expected_result> [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg,%lg,%lg", &p[0], &p[1], &p[2]);
    sscanf(argv[3], "%lg", &s);
    sscanf(argv[4], "%d", &expected_error);
    scan_mat_args(argv, 5, &expected);

    error = bn_mat_scale_about_pt(actual, p, s);
    return !(mat_equal(expected, actual) && error == expected_error);
}

static int
test_bn_mat_xform_about_pt(int argc, char *argv[])
{
    mat_t xform, expected, actual;
    point_t p;

    if (argc != 5) {
	bu_exit(1, "<args> format: M P <expected_result> [%s]\n", argv[0]);
    }

    scan_mat_args(argv, 2, &xform);
    sscanf(argv[3], "%lg,%lg,%lg", &p[0], &p[1], &p[2]);
    scan_mat_args(argv, 4, &expected);

    bn_mat_xform_about_pt(actual, xform, p);
    return !mat_equal(expected, actual);
}

static int
test_bn_mat_ck(int argc, char *argv[])
{
    mat_t m;
    int expected;
    char *title = "test_bn_mat_ck";

    if (argc != 4) {
	bu_exit(1, "<args> format: M <expected_result> [%s]\n", argv[0]);
    }

    scan_mat_args(argv, 2, &m);
    sscanf(argv[3], "%d", &expected);

    return !(bn_mat_ck(title, m) == expected);
}

static int
test_bn_mat_dup()
{
    mat_t m = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9, 10.1, 11.11, 12.12, 13.13, 14.14, 15.15, 16.16};
    matp_t expected = m, actual;

    actual = bn_mat_dup(m);
    return !mat_equal(expected, actual);
}

int
main(int argc, char *argv[])
{
    int function_num = 0;

    if (argc < 3) {
	bu_exit(1, "Argument format: <function_number> <args> [%s]\n", argv[0]);
    }

    sscanf(argv[1], "%d", &function_num);
    if (function_num < 1 || function_num > 27) function_num = 0;

    switch (function_num) {
    case 1:
	return test_bn_mat_mul(argc, argv);
    case 2:
	return test_bn_mat_mul3(argc, argv);
    case 3:
	return test_bn_mat_mul4(argc, argv);
    case 4:
	return test_bn_matXvec(argc, argv);
    case 5:
	return test_bn_mat_inverse(argc, argv);
    case 6:
	return test_bn_mat_trn(argc, argv);
    case 7:
	return test_bn_mat_is_identity(argc, argv);
    case 8:
	return test_bn_mat_det3(argc, argv);
    case 9:
	return test_bn_mat_determinant(argc, argv);
    case 10:
	return test_bn_mat_is_equal(argc, argv);
    case 11:
	return test_bn_atan2(argc, argv);
    case 12:
	return test_bn_vtoh_move(argc, argv);
    case 13:
	return test_bn_htov_move(argc, argv);
    case 14:
	return test_bn_mat_ae(argc, argv);
    case 15:
	return test_bn_ae_vec(argc, argv);
    case 16:
	return test_bn_vec_ae(argc, argv);
    case 17:
	return test_bn_vec_aed(argc, argv);
    case 18:
	return test_bn_mat_angles(argc, argv);
    case 19:
	return test_bn_mat_angles_rad(argc, argv);
    case 20:
	return test_bn_eigen2x2(argc, argv);
    case 21:
	return test_bn_mat_xrot(argc, argv);
    case 22:
	return test_bn_mat_yrot(argc, argv);
    case 23:
	return test_bn_mat_zrot(argc, argv);
    case 24:
	return test_bn_mat_scale_about_pt(argc, argv);
    case 25:
	return test_bn_mat_xform_about_pt(argc, argv);
    case 26:
	return test_bn_mat_ck(argc, argv);
    case 27:
	return test_bn_mat_dup();
    }

    bu_log("ERROR: function_num %d is not valid [%s]\n", function_num, argv[0]);
    return 1;
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
