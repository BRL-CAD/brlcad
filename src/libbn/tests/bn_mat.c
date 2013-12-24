/*                      B N _ M A T . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

int
main(int argc, char *argv[])
{
    int function_num = 0;

    if (argc < 3) {
	bu_exit(1, "Argument format: <function_number> <args> [%s]\n", argv[0]);
    }

    sscanf(argv[1], "%d", &function_num);
    if (function_num < 1 || function_num > 10) function_num = 0;

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
