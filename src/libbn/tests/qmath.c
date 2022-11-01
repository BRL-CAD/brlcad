/*                      Q M A T H . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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

#include "bu/log.h"
#include "vmath.h"
#include "bn/tol.h"
#include "bn/qmath.h"


int
test_quat_mat2quat(int argc, const char *argv[])
{
    mat_t mat;
    quat_t expected_quat, actual_quat;

    if (argc != 2) {
	bu_exit(1, "ERROR: input format is: mat expected_quat\n");
    }
    bu_scan_fastf_t(NULL, argv[0], ",", 16, &(mat[0]), &(mat[1]), &(mat[2]), &(mat[3]), &(mat[4]), &(mat[5]), &(mat[6]), &(mat[7]), &(mat[8]), &(mat[9]), &(mat[10]), &(mat[11]), &(mat[12]), &(mat[13]), &(mat[14]), &(mat[15]));
    bu_scan_fastf_t(NULL, argv[1], "," , 4, &(expected_quat[X]), &(expected_quat[Y]), &(expected_quat[Z]), &(expected_quat[W]));

    quat_mat2quat(actual_quat, mat);

    return !HNEAR_EQUAL(expected_quat, actual_quat, BN_TOL_DIST);
}

int
test_quat_quat2mat(int argc, const char *argv[])
{
    quat_t quat;
    mat_t expected_mat, actual_mat;
    int i;

    if (argc != 2) {
	bu_exit(1, "ERROR: input format is: quat expected_mat\n");
    }
    bu_scan_fastf_t(NULL, argv[0], "," , 4, &(quat[X]), &(quat[Y]), &quat[Z], &(quat[W]));
    bu_scan_fastf_t(NULL, argv[1], ",", 16, &(expected_mat[0]), &(expected_mat[1]), &(expected_mat[2]), &(expected_mat[3]), &(expected_mat[4]), &(expected_mat[5]), &(expected_mat[6]), &(expected_mat[7]), &(expected_mat[8]), &(expected_mat[9]), &(expected_mat[10]), &(expected_mat[11]), &(expected_mat[12]), &(expected_mat[13]), &(expected_mat[14]), &(expected_mat[15]));

    quat_quat2mat(actual_mat, quat);

    for (i = 0; i < 16; i++) {
	if (!NEAR_EQUAL(expected_mat[i], actual_mat[i], BN_TOL_DIST)) { return 1; }
    }
    return 0;
}

int
test_quat_distance(int argc, const char *argv[])
{
    quat_t q1, q2;
    double expected_dist, actual_dist;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is q1 q2 expected_dist\n");
    }
    bu_scan_fastf_t(NULL, argv[0], ",", 4, &(q1[X]), &(q1[Y]), &(q1[Z]), &(q1[W]));
    bu_scan_fastf_t(NULL, argv[1], ",", 4, &(q2[X]), &(q2[Y]), &(q2[Z]), &(q2[W]));
    bu_sscanf(argv[2], "%lf", &expected_dist);

    actual_dist = quat_distance(q1, q2);
    return !(NEAR_EQUAL(actual_dist, expected_dist, BN_TOL_DIST));
}

int
test_quat_double(int argc, const char *argv[])
{
    quat_t q1, q2;
    quat_t expected_quat, actual_quat;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is q1 q2 expected_quat\n");
    }
    bu_scan_fastf_t(NULL, argv[0], ",", 4, &(q1[X]), &(q1[Y]), &(q1[Z]), &(q1[W]));
    bu_scan_fastf_t(NULL, argv[1], ",", 4, &(q2[X]), &(q2[Y]), &(q2[Z]), &(q2[W]));
    bu_scan_fastf_t(NULL, argv[2], ",", 4, &(expected_quat[X]), &(expected_quat[Y]), &(expected_quat[Z]), &(expected_quat[W]));

    quat_double(actual_quat, q1, q2);

    return !HNEAR_EQUAL(expected_quat, actual_quat, BN_TOL_DIST);
}

int test_quat_bisect(int argc, const char *argv[])
{
    quat_t q1, q2;
    quat_t expected_quat, actual_quat;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is q1 q2 expected_quat\n");
    }
    bu_scan_fastf_t(NULL, argv[0], ",", 4, &(q1[X]), &(q1[Y]), &(q1[Z]), &(q1[W]));
    bu_scan_fastf_t(NULL, argv[1], ",", 4, &(q2[X]), &(q2[Y]), &(q2[Z]), &(q2[W]));
    bu_scan_fastf_t(NULL, argv[2], ",", 4, &(expected_quat[X]), &(expected_quat[Y]), &(expected_quat[Z]), &(expected_quat[W]));

    quat_bisect(actual_quat, q1, q2);

    return !HNEAR_EQUAL(expected_quat, actual_quat, BN_TOL_DIST);
}

int test_quat_slerp(int argc, const char *argv[])
{
    quat_t q1, q2;
    double f;
    quat_t expected_quat, actual_quat;

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is q1 q2 f expected_quat\n");
    }
    bu_scan_fastf_t(NULL, argv[0], ",", 4, &(q1[X]), &(q1[Y]), &(q1[Z]), &(q1[W]));
    bu_scan_fastf_t(NULL, argv[1], ",", 4, &(q2[X]), &(q2[Y]), &(q2[Z]), &(q2[W]));
    bu_sscanf(argv[2], "%lf", &f);
    bu_scan_fastf_t(NULL, argv[3], ",", 4, &(expected_quat[X]), &(expected_quat[Y]), &(expected_quat[Z]), &(expected_quat[W]));

    quat_slerp(actual_quat, q1, q2, f);

    return !HNEAR_EQUAL(expected_quat, actual_quat, BN_TOL_DIST);
}

int test_quat_sberp(int argc, const char *argv[])
{
    quat_t q1, qa, qb, q2;
    double f;
    quat_t expected_quat, actual_quat;

    if (argc != 6) {
	bu_exit(1, "ERROR: input format is q1 qa qb q2 f expected_quat\n");
    }
    bu_scan_fastf_t(NULL, argv[0], ",", 4, &(q1[X]), &(q1[Y]), &(q1[Z]), &(q1[W]));
    bu_scan_fastf_t(NULL, argv[1], ",", 4, &(qa[X]), &(qa[Y]), &(qa[Z]), &(qa[W]));
    bu_scan_fastf_t(NULL, argv[2], ",", 4, &(qb[X]), &(qb[Y]), &(qb[Z]), &(qb[W]));
    bu_scan_fastf_t(NULL, argv[3], ",", 4, &(q2[X]), &(q2[Y]), &(q2[Z]), &(q2[W]));
    bu_sscanf(argv[4], "%lf", &f);
    bu_scan_fastf_t(NULL, argv[5], ",", 4, &(expected_quat[X]), &(expected_quat[Y]), &(expected_quat[Z]), &(expected_quat[W]));

    quat_sberp(actual_quat, q1, qa, qb, q2, f);

    return !HNEAR_EQUAL(expected_quat, actual_quat, BN_TOL_DIST);
}

int test_quat_make_nearest(int argc, const char *argv[])
{
    quat_t q1, q2, expected_result;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is q1 q2 expected_result\n");
    }
    bu_scan_fastf_t(NULL, argv[0], ",", 4, &(q1[X]), &(q1[Y]), &(q1[Z]), &(q1[W]));
    bu_scan_fastf_t(NULL, argv[0], ",", 4, &(q2[X]), &(q2[Y]), &(q2[Z]), &(q2[W]));
    bu_scan_fastf_t(NULL, argv[0], ",", 4, &(expected_result[X]), &(expected_result[Y]), &(expected_result[Z]), &(expected_result[W]));

    quat_make_nearest(q1, q2);

    return !HNEAR_EQUAL(expected_result, q1, BN_TOL_DIST);
}

int test_quat_exp(int argc, const char *argv[])
{
    quat_t in;
    quat_t expected_out, actual_out;

    if (argc != 2) {
	bu_exit(1, "ERROR: input format is in expected_out\n");
    }
    bu_scan_fastf_t(NULL, argv[0], ",", 4, &(in[X]), &(in[Y]), &(in[Z]), &(in[W]));
    bu_scan_fastf_t(NULL, argv[1], ",", 4, &(expected_out[X]), &(expected_out[Y]), &(expected_out[Z]), &(expected_out[W]));

    quat_exp(actual_out, in);

    return !HNEAR_EQUAL(expected_out, actual_out, BN_TOL_DIST);
}

int test_quat_log(int argc, const char *argv[])
{
    quat_t in;
    quat_t expected_out, actual_out;

    if (argc != 2) {
	bu_exit(1, "ERROR: input format is in expected_out\n");
    }
    bu_scan_fastf_t(NULL, argv[0], ",", 4, &(in[X]), &(in[Y]), &(in[Z]), &(in[W]));
    bu_scan_fastf_t(NULL, argv[1], ",", 4, &(expected_out[X]), &(expected_out[Y]), &(expected_out[Z]), &(expected_out[W]));

    quat_log(actual_out, in);

    return !HNEAR_EQUAL(expected_out, actual_out, BN_TOL_DIST);
}

int
qmath_main(int argc, char *argv[])
{
    int function_num = 0;

    const char **av = (const char **)argv;

    if (argc < 3) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    sscanf(argv[1], "%d", &function_num);

    switch (function_num) {
	case 1:
	    return test_quat_mat2quat(argc - 2, av + 2);
	case 2:
	    return test_quat_quat2mat(argc - 2, av + 2);
	case 3:
	    return test_quat_distance(argc - 2, av + 2);
	case 4:
	    return test_quat_double(argc - 2, av + 2);
	case 5:
	    return test_quat_bisect(argc - 2, av + 2);
	case 6:
	    return test_quat_slerp(argc - 2, av + 2);
	case 7:
	    return test_quat_sberp(argc - 2, av + 2);
	case 8:
	    return test_quat_make_nearest(argc - 2, av + 2);
	case 9:
	    return test_quat_exp(argc - 2, av + 2);
	case 10:
	    return test_quat_log(argc - 2, av + 2);
	default:
	    bu_exit(1, "ERROR: Unrecognized function_num\n");
    }

    return 1;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
