/*                        T E S T _ A N I M . C
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
test_anim(void)
{
    int failures = 0;
    const char *test = "anim";
    vect_t ypr = {0.7, -0.4, 1.2};
    vect_t ypr_out = VINIT_ZERO;
    vect_t zyx = {0.4, -0.5, 0.2};
    vect_t zyx_out = VINIT_ZERO;
    vect_t pre = {1.0, 2.0, 3.0};
    vect_t post = {4.0, 5.0, 6.0};
    vect_t rotv = {2.0, -1.0, 5.0};
    vect_t expect_rotv = VINIT_ZERO;
    mat_t m1 = MAT_INIT_ZERO;
    mat_t m2 = MAT_INIT_ZERO;
    mat_t m3 = MAT_INIT_ZERO;
    mat_t m4 = MAT_INIT_ZERO;
    mat_t arbitrary = {
	1.0, 2.0, 3.0, 4.0,
	5.0, 6.0, 7.0, 8.0,
	9.0, 10.0, 11.0, 12.0,
	13.0, 14.0, 15.0, 16.0
    };
    int ret;

    anim_ypr2mat(m1, ypr);
    anim_y_p_r2mat(m2, ypr[X], ypr[Y], ypr[Z]);
    if (!mat_close(m1, m2, 1.0e-12)) {
	report_failure(test, "anim_ypr2mat and anim_y_p_r2mat disagreed");
	failures++;
    }

    anim_dy_p_r2mat(m3, ypr[X] * RAD2DEG, ypr[Y] * RAD2DEG, ypr[Z] * RAD2DEG);
    if (!mat_close(m1, m3, 1.0e-12)) {
	report_failure(test, "anim_dy_p_r2mat disagreed with the radian API");
	failures++;
    }

    anim_ypr2vmat(m2, ypr);
    anim_dy_p_r2vmat(m3, ypr[X] * RAD2DEG, ypr[Y] * RAD2DEG, ypr[Z] * RAD2DEG);
    if (!mat_close(m2, m3, 1.0e-6)) {
	report_failure(test, "anim_ypr2vmat disagreed with anim_dy_p_r2vmat");
	failures++;
    }

    MAT_COPY(m3, m1);
    ret = anim_mat2ypr(m3, ypr_out);
    if (ret == 2) {
	report_failure(test, "anim_mat2ypr reported conversion failure");
	failures++;
    } else {
	anim_ypr2mat(m4, ypr_out);
	if (!mat_close(m1, m4, 1.0e-10)) {
	    report_failure(test, "anim_mat2ypr did not round-trip through anim_ypr2mat");
	    failures++;
	}
    }

    anim_zyx2mat(m1, zyx);
    MAT_COPY(m3, m1);
    ret = anim_mat2zyx(m3, zyx_out);
    if (ret == 2) {
	report_failure(test, "anim_mat2zyx reported conversion failure");
	failures++;
    } else {
	anim_zyx2mat(m4, zyx_out);
	if (!mat_close(m1, m4, 1.0e-10)) {
	    report_failure(test, "anim_mat2zyx did not round-trip through anim_zyx2mat");
	    failures++;
	}
    }

    anim_x_y_z2mat(m1, 0.2, -0.3, 0.4);
    anim_dx_y_z2mat(m2, 0.2 * RAD2DEG, -0.3 * RAD2DEG, 0.4 * RAD2DEG);
    if (!mat_close(m1, m2, 1.0e-12)) {
	report_failure(test, "anim_dx_y_z2mat disagreed with anim_x_y_z2mat");
	failures++;
    }

    anim_z_y_x2mat(m1, 0.3, -0.2, 0.5);
    anim_dz_y_x2mat(m2, 0.3 * RAD2DEG, -0.2 * RAD2DEG, 0.5 * RAD2DEG);
    if (!mat_close(m1, m2, 1.0e-12)) {
	report_failure(test, "anim_dz_y_x2mat disagreed with anim_z_y_x2mat");
	failures++;
    }

    MAT_COPY(m1, arbitrary);
    anim_tran(m1);
    bn_mat_trn(m2, arbitrary);
    if (!mat_close(m1, m2, 0.0)) {
	report_failure(test, "anim_tran disagreed with bn_mat_trn");
	failures++;
    }

    anim_ypr2mat(m1, ypr);
    MAT_COPY(m2, m1);
    anim_v_permute(m2);
    anim_v_unpermute(m2);
    if (!mat_close(m1, m2, 0.0)) {
	report_failure(test, "anim_v_permute and anim_v_unpermute were not inverses");
	failures++;
    }

    expect_rotv[X] = rotv[X] * cos(0.9) - rotv[Y] * sin(0.9);
    expect_rotv[Y] = rotv[X] * sin(0.9) + rotv[Y] * cos(0.9);
    expect_rotv[Z] = rotv[Z];
    anim_rotatez(0.9, rotv);
    if (!vect_close(rotv, expect_rotv, 1.0e-12)) {
	report_failure(test, "anim_rotatez did not match the analytical z-rotation");
	failures++;
    }

    MAT_IDN(m1);
    anim_add_trans(m1, post, pre);
    if (!scalar_close(m1[3], 5.0, 1.0e-12) ||
	!scalar_close(m1[7], 7.0, 1.0e-12) ||
	!scalar_close(m1[11], 9.0, 1.0e-12)) {
	report_failure(test, "anim_add_trans produced an unexpected translation for the identity matrix");
	failures++;
    }

    anim_ypr2mat(m1, ypr);
    MAT_COPY(m2, m1);
    anim_view_rev(m2);
    if (!scalar_close(m2[0], -m1[0], 0.0) ||
	!scalar_close(m2[1], -m1[1], 0.0) ||
	!scalar_close(m2[4], -m1[4], 0.0) ||
	!scalar_close(m2[5], -m1[5], 0.0) ||
	!scalar_close(m2[8], -m1[8], 0.0) ||
	!scalar_close(m2[9], -m1[9], 0.0) ||
	!scalar_close(m2[2], m1[2], 0.0) ||
	!scalar_close(m2[6], m1[6], 0.0) ||
	!scalar_close(m2[10], m1[10], 0.0)) {
	report_failure(test, "anim_view_rev changed unexpected matrix elements");
	failures++;
    }

    return failures;
}


static int
test_anim_quat(void)
{
    int failures = 0;
    const char *test = "anim_quat";
    quat_t q = {0.3, -0.5, 0.4, 0.7};
    quat_t qneg = HINIT_ZERO;
    quat_t qscaled = HINIT_ZERO;
    quat_t qout = HINIT_ZERO;
    mat_t m1 = MAT_INIT_ZERO;
    mat_t m2 = MAT_INIT_ZERO;
    mat_t m3 = MAT_INIT_ZERO;
    int i;

    normalize_quat(q);
    for (i = 0; i < 4; i++) {
	qneg[i] = -q[i];
	qscaled[i] = q[i] * 7.0;
    }

    anim_quat2mat(m1, q);
    if (!orthonormal_rotation(m1, 1.0e-12)) {
	report_failure(test, "anim_quat2mat did not produce a valid rotation matrix");
	failures++;
    }

    anim_quat2mat(m2, qneg);
    if (!mat_close(m1, m2, 1.0e-12)) {
	report_failure(test, "anim_quat2mat should map q and -q to the same rotation");
	failures++;
    }

    anim_quat2mat(m2, qscaled);
    if (!mat_close(m1, m2, 1.0e-12)) {
	report_failure(test, "anim_quat2mat should normalize quaternion magnitude");
	failures++;
    }

    if (!anim_mat2quat(qout, m1)) {
	report_failure(test, "anim_mat2quat reported failure");
	failures++;
    } else {
	anim_quat2mat(m3, qout);
	if (!mat_close(m1, m3, 1.0e-12)) {
	    report_failure(test, "anim_mat2quat did not round-trip through anim_quat2mat");
	    failures++;
	}
    }

    return failures;
}


static int
test_anim_dirs(void)
{
    int failures = 0;
    const char *test = "anim_dirs";
    vect_t d = {1.0, 2.0, 3.0};
    vect_t n = {0.0, 0.0, 1.0};
    mat_t m1 = MAT_INIT_ZERO;
    mat_t m2 = MAT_INIT_ZERO;
    vect_t c0 = VINIT_ZERO;

    VUNITIZE(d);

    anim_dir2mat(m1, d, n);
    VSET(c0, m1[0], m1[4], m1[8]);
    if (!vect_close(c0, d, 1.0e-12) || !orthonormal_rotation(m1, 1.0e-12)) {
	report_failure(test, "anim_dir2mat did not build a valid orientation matrix");
	failures++;
    }

    anim_dirn2mat(m2, d, n);
    VSET(c0, m2[0], m2[4], m2[8]);
    if (!vect_close(c0, d, 1.0e-12) || !orthonormal_rotation(m2, 1.0e-12)) {
	report_failure(test, "anim_dirn2mat did not build a valid orientation matrix");
	failures++;
    }

    return failures;
}


static const struct bn_api_case anim_cases[] = {
    {"basic", test_anim},
    {"quat", test_anim_quat},
    {"dirs", test_anim_dirs},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    return bn_api_dispatch(argc, argv, anim_cases);
}
