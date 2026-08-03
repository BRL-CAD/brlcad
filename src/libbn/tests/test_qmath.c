/*                      T E S T _ Q M A T H . C
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
quat_close_or_neg(const quat_t a, const quat_t b, double tol)
{
    quat_t negb = HINIT_ZERO;
    int i;

    for (i = 0; i < 4; i++) {
	negb[i] = -b[i];
    }

    return hvect_close(a, b, tol) || hvect_close(a, negb, tol);
}


static int
quat_is_unit_finite(const quat_t q, double tol)
{
    return isfinite(q[X]) && isfinite(q[Y]) && isfinite(q[Z]) && isfinite(q[W]) &&
	scalar_close(QMAGNITUDE(q), 1.0, tol);
}


static int
test_qmath_roundtrip(void)
{
    int failures = 0;
    const char *test = "qmath_roundtrip";
    quat_t q = HINIT_ZERO;
    quat_t q2 = HINIT_ZERO;
    quat_t qscaled = HINIT_ZERO;
    mat_t mats[5];
    mat_t roundtrip = MAT_INIT_ZERO;
    int i;

    quat_t qid = {0.0, 0.0, 0.0, 1.0};
    quat_t qx = {1.0, 0.0, 0.0, 0.0};
    quat_t qy = {0.0, 1.0, 0.0, 0.0};
    quat_t qz = {0.0, 0.0, 1.0, 0.0};
    quat_t qarb = {0.2, -0.3, 0.4, 0.5};

    normalize_quat(qarb);
    for (i = 0; i < 4; i++) {
	qscaled[i] = qarb[i] * 7.0;
    }

    quat_quat2mat(mats[0], qid);
    quat_quat2mat(mats[1], qx);
    quat_quat2mat(mats[2], qy);
    quat_quat2mat(mats[3], qz);
    quat_quat2mat(mats[4], qarb);

    for (i = 0; i < 5; i++) {
	quat_mat2quat(q, mats[i]);
	quat_quat2mat(roundtrip, q);
	if (!mat_close(mats[i], roundtrip, 1.0e-12)) {
	    report_failure(test, "matrix/quaternion round-trip failed for case %d", i);
	    failures++;
	}
    }

    quat_quat2mat(mats[0], qarb);
    quat_quat2mat(mats[1], qscaled);
    if (!mat_close(mats[0], mats[1], 1.0e-12)) {
	report_failure(test, "quat_quat2mat should normalize quaternion magnitude");
	failures++;
    }

    quat_mat2quat(q2, mats[0]);
    if (!quat_close_or_neg(q2, qarb, 1.0e-12)) {
	report_failure(test, "quat_mat2quat did not recover the original quaternion orientation");
	failures++;
    }

    return failures;
}


static int
test_qmath_interp(void)
{
    int failures = 0;
    const char *test = "qmath_interp";
    quat_t qid = {0.0, 0.0, 0.0, 1.0};
    quat_t qx = {1.0, 0.0, 0.0, 0.0};
    quat_t qanti = {0.0, 0.0, 0.0, -1.0};
    quat_t qneg = {-1.0, 0.0, 0.0, 0.0};
    quat_t out = HINIT_ZERO;

    quat_slerp(out, qid, qx, 0.0);
    if (!quat_close_or_neg(out, qid, 1.0e-12)) {
	report_failure(test, "quat_slerp failed at interpolation factor 0");
	failures++;
    }

    quat_slerp(out, qid, qx, 1.0);
    if (!quat_close_or_neg(out, qx, 1.0e-12)) {
	report_failure(test, "quat_slerp failed at interpolation factor 1");
	failures++;
    }

    quat_slerp(out, qid, qx, 0.5);
    if (!quat_is_unit_finite(out, 1.0e-12)) {
	report_failure(test, "quat_slerp failed to return a finite unit quaternion");
	failures++;
    }

    quat_slerp(out, qid, qanti, 0.5);
    if (!quat_is_unit_finite(out, 1.0e-12)) {
	report_failure(test, "quat_slerp antipodal branch failed to return a finite unit quaternion");
	failures++;
    }

    quat_sberp(out, qid, qid, qx, qx, 0.5);
    if (!quat_is_unit_finite(out, 1.0e-12)) {
	report_failure(test, "quat_sberp failed to return a finite unit quaternion");
	failures++;
    }

    quat_make_nearest(qneg, qx);
    if (QDOT(qneg, qx) < 0.0) {
	report_failure(test, "quat_make_nearest failed to choose the nearest sign");
	failures++;
    }

    quat_double(out, qid, qx);
    if (!quat_is_unit_finite(out, 1.0e-12)) {
	report_failure(test, "quat_double failed to return a finite unit quaternion");
	failures++;
    }

    quat_bisect(out, qid, qx);
    if (!quat_is_unit_finite(out, 1.0e-12)) {
	report_failure(test, "quat_bisect failed to return a finite unit quaternion");
	failures++;
    }

    return failures;
}


static int
test_qmath_logexp(void)
{
    int failures = 0;
    const char *test = "qmath_logexp";
    quat_t v = {0.2, -0.1, 0.3, 0.0};
    quat_t q = HINIT_ZERO;
    quat_t out = HINIT_ZERO;
    quat_t qin = {0.3, 0.4, 0.1, 0.85};

    quat_exp(q, v);
    quat_log(out, q);
    if (!hvect_close(out, v, 1.0e-12)) {
	report_failure(test, "quat_log(quat_exp(v)) did not recover the original pure-vector quaternion");
	failures++;
    }

    normalize_quat(qin);
    if (qin[W] < 0.0) {
	QSCALE(qin, qin, -1.0);
    }
    quat_log(out, qin);
    quat_exp(q, out);
    if (!quat_close_or_neg(q, qin, 1.0e-12)) {
	report_failure(test, "quat_exp(quat_log(q)) did not recover the original unit quaternion");
	failures++;
    }

    return failures;
}


static const struct bn_api_case qmath_cases[] = {
    {"roundtrip", test_qmath_roundtrip},
    {"interp", test_qmath_interp},
    {"logexp", test_qmath_logexp},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_dispatch(argc, argv, qmath_cases);
}
