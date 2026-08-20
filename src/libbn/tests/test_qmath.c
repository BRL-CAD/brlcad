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
test_qmath_distance(void)
{
    int failures = 0;
    const char *test = "qmath_distance";
    quat_t q1 = {1.0, 2.0, 0.0, 3.0};
    quat_t q2 = {-1.0, -2.0, 0.0, -3.0};
    quat_t q3 = {4.0, 2.0, 3.0, 1.0};
    quat_t q4 = {0.0, 0.0, 2.0, 3.0};

    if (!scalar_close(quat_distance(q1, q1), 0.0, 0.0)) {
	report_failure(test, "quat_distance failed for identical quaternions");
	failures++;
    }

    if (!scalar_close(quat_distance(q1, q2), 7.483314773547883, 1.0e-12)) {
	report_failure(test, "quat_distance failed for opposite-sign test case");
	failures++;
    }

    if (!scalar_close(quat_distance(q3, q4), 5.0, 1.0e-12)) {
	report_failure(test, "quat_distance failed for mixed quaternion inputs");
	failures++;
    }

    return failures;
}


static int
test_qmath_roundtrip(void)
{
    int failures = 0;
    const char *test = "qmath_roundtrip";
    static const struct {
	quat_t quat;
	mat_t expected;
    } quat2mat_cases[] = {
	{{0.0, 0.0, 0.0, 1.0}, {
		1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }},
	{{1.0, 0.0, 0.0, 0.0}, {
		1.0, 0.0, 0.0, 0.0,
		0.0, -1.0, 0.0, 0.0,
		0.0, 0.0, -1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }},
	{{0.0, 1.0, 0.0, 0.0}, {
		-1.0, 0.0, 0.0, 0.0,
		0.0, 1.0, 0.0, 0.0,
		0.0, 0.0, -1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }},
	{{0.0, 0.0, 1.0, 0.0}, {
		-1.0, 0.0, 0.0, 0.0,
		0.0, -1.0, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	    }}
    };
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

    for (i = 0; i < (int)(sizeof(quat2mat_cases) / sizeof(quat2mat_cases[0])); i++) {
	quat_quat2mat(roundtrip, quat2mat_cases[i].quat);
	if (!mat_close(roundtrip, quat2mat_cases[i].expected, 1.0e-12)) {
	    report_failure(test, "quat_quat2mat reference case %d failed", i);
	    failures++;
	}
	quat_mat2quat(q, quat2mat_cases[i].expected);
	if (!quat_close_or_neg(q, quat2mat_cases[i].quat, 1.0e-12)) {
	    report_failure(test, "quat_mat2quat reference case %d failed", i);
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
    quat_t qdouble_same_in = {0.0, 2.0, 0.0, 1.0};
    quat_t qdouble_same_expected = {0.0, 0.894, 0.0, 0.447};
    quat_t qdouble_expected = {0.900, 0.177, 0.355, 0.179};
    quat_t qbisect_expected = {0.657, 0.327, 0.653, 0.187};
    quat_t qslerp_expected = {0.657, 0.327, 0.653, 0.187};
    quat_t qsberp_same_expected = {0.723, 0.529, 0.230, 0.380};
    quat_t qsberp_expected = {0.724, 0.358, 0.501, 0.310};
    quat_t qsame = {0.0, 0.894, 0.0, 0.447};
    quat_t qsame_neg = {0.0, -0.894, 0.0, -0.447};
    quat_t q1 = {0.548, 0.365, 0.730, 0.183};
    quat_t q2 = {0.753, 0.282, 0.564, 0.188};
    quat_t qa = {1.0, 0.0, 0.0, 0.0};
    quat_t qb = {0.5, 0.5, 0.5, 0.5};
    quat_t exact = HINIT_ZERO;
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

    quat_double(exact, qdouble_same_in, qdouble_same_in);
    if (!hvect_close(exact, qdouble_same_expected, 1.0e-3)) {
	report_failure(test, "quat_double legacy same-input case failed");
	failures++;
    }

    quat_double(exact, q1, q2);
    if (!quat_close_or_neg(exact, qdouble_expected, 1.0e-3)) {
	report_failure(test, "quat_double legacy mixed-input case failed");
	failures++;
    }

    quat_bisect(exact, qsame, qsame);
    if (!hvect_close(exact, qsame, 1.0e-3)) {
	report_failure(test, "quat_bisect legacy same-input case failed");
	failures++;
    }

    quat_bisect(exact, q1, q2);
    if (!quat_close_or_neg(exact, qbisect_expected, 1.0e-3)) {
	report_failure(test, "quat_bisect legacy mixed-input case failed");
	failures++;
    }

    quat_slerp(exact, qsame, qsame, 0.783);
    if (!quat_close_or_neg(exact, qsame, 1.0e-3)) {
	report_failure(test, "quat_slerp legacy equal-input case failed");
	failures++;
    }

    quat_slerp(exact, q1, q2, 0.5);
    if (!quat_close_or_neg(exact, qslerp_expected, 1.0e-3)) {
	report_failure(test, "quat_slerp legacy mixed-input midpoint case failed");
	failures++;
    }

    quat_sberp(exact, qsame, qa, qb, qsame, 0.5);
    if (!quat_close_or_neg(exact, qsberp_same_expected, 1.0e-3)) {
	report_failure(test, "quat_sberp legacy equal-endpoint case failed");
	failures++;
    }

    quat_sberp(exact, q1, qa, qb, q2, 0.783);
    if (!quat_close_or_neg(exact, qsberp_expected, 1.0e-3)) {
	report_failure(test, "quat_sberp legacy mixed-input case failed");
	failures++;
    }

    quat_make_nearest(qsame_neg, qsame);
    if (!hvect_close(qsame_neg, qsame, 1.0e-3)) {
	report_failure(test, "quat_make_nearest legacy sign-flip case failed");
	failures++;
    }

    return failures;
}


static int
test_qmath_logexp(void)
{
    int failures = 0;
    const char *test = "qmath_logexp";
    static const struct {
	quat_t in;
	quat_t expected;
    } exp_cases[] = {
	{{0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 1.0}},
	{{0.577, 0.577, 0.577, 0.0}, {0.486, 0.486, 0.486, 0.541}},
	{{0.365, 0.730, 0.183, 0.0}, {0.324, 0.648, 0.162, 0.670}}
    };
    static const struct {
	quat_t in;
	quat_t expected;
    } log_cases[] = {
	{{1.0, 0.0, 0.0, 0.0}, {1.571, 0.0, 0.0, 0.0}},
	{{0.5, 0.5, 0.5, 0.5}, {0.605, 0.605, 0.605, 0.0}},
	{{0.548, 0.365, 0.730, 0.183}, {0.773, 0.515, 1.030, 0.0}}
    };
    quat_t v = {0.2, -0.1, 0.3, 0.0};
    quat_t q = HINIT_ZERO;
    quat_t out = HINIT_ZERO;
    quat_t qin = {0.3, 0.4, 0.1, 0.85};
    int i;

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

    for (i = 0; i < (int)(sizeof(exp_cases) / sizeof(exp_cases[0])); i++) {
	quat_exp(q, exp_cases[i].in);
	if (!hvect_close(q, exp_cases[i].expected, 1.0e-3)) {
	    report_failure(test, "quat_exp reference case %d failed", i);
	    failures++;
	}
    }

    for (i = 0; i < (int)(sizeof(log_cases) / sizeof(log_cases[0])); i++) {
	quat_log(out, log_cases[i].in);
	if (!hvect_close(out, log_cases[i].expected, 1.0e-3)) {
	    report_failure(test, "quat_log reference case %d failed", i);
	    failures++;
	}
    }

    return failures;
}


static const struct bn_api_case qmath_cases[] = {
    {"distance", test_qmath_distance},
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
