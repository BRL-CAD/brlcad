/*                        T E S T _ S T R . C
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
test_str_roundtrip(void)
{
    int failures = 0;
    const char *test = "str_roundtrip";
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    vect_t vin = {1.25, -2.5, 3.75};
    vect_t vout = VINIT_ZERO;
    hvect_t hin = {7.0, -8.0, 9.5, 1.0};
    hvect_t hout = HINIT_ZERO;
    quat_t qin = {0.25, -0.5, 0.75, 1.0};
    quat_t qout = HINIT_ZERO;
    mat_t min = {
	1.0, 2.0, 3.0, 4.0,
	5.0, 6.0, 7.0, 8.0,
	9.0, 10.0, 11.0, 12.0,
	13.0, 14.0, 15.0, 16.0
    };
    mat_t mout = MAT_INIT_ZERO;
    vect_t vclamp = {1.8, -2.2, 3.49};
    vect_t vclamp_out = VINIT_ZERO;
    double ang = 0.0;
    int ret;

    bn_encode_vect(&vls, vin, 0);
    ret = bn_decode_vect(vout, bu_vls_cstr(&vls));
    if (ret != 3 || !vect_close(vin, vout, 1.0e-12)) {
	report_failure(test, "vector encode/decode roundtrip failed");
	failures++;
    }

    bu_vls_trunc(&vls, 0);
    bn_encode_hvect(&vls, hin, 0);
    ret = bn_decode_hvect(hout, bu_vls_cstr(&vls));
    if (ret != 4 || !hvect_close(hin, hout, 1.0e-12)) {
	report_failure(test, "homogeneous vector encode/decode roundtrip failed");
	failures++;
    }

    bu_vls_trunc(&vls, 0);
    bn_encode_quat(&vls, qin, 0);
    ret = bn_decode_quat(qout, bu_vls_cstr(&vls));
    if (ret != 4 || !hvect_close(qin, qout, 1.0e-12)) {
	report_failure(test, "quaternion encode/decode roundtrip failed");
	failures++;
    }

    bu_vls_trunc(&vls, 0);
    bn_encode_mat(&vls, min, 0);
    ret = bn_decode_mat(mout, bu_vls_cstr(&vls));
    if (ret != 16 || !mat_close(min, mout, 1.0e-12)) {
	report_failure(test, "matrix encode/decode roundtrip failed");
	failures++;
    }

    bu_vls_trunc(&vls, 0);
    bn_encode_vect(&vls, vclamp, 1);
    ret = bn_decode_vect(vclamp_out, bu_vls_cstr(&vls));
    if (ret != 3 ||
	!scalar_close(vclamp_out[X], INTCLAMP(vclamp[X]), SMALL_FASTF) ||
	!scalar_close(vclamp_out[Y], INTCLAMP(vclamp[Y]), SMALL_FASTF) ||
	!scalar_close(vclamp_out[Z], INTCLAMP(vclamp[Z]), SMALL_FASTF)) {
	report_failure(test, "clamped vector encoding did not round-trip to integer-clamped values");
	failures++;
    }

    ret = bn_decode_vect(vout, "{4 5 6}");
    {
	vect_t exp_vout = {4.0, 5.0, 6.0};
	if (ret != 3 || !vect_close(vout, exp_vout, SMALL_FASTF)) {
	    report_failure(test, "brace-tolerant vector decoding failed");
	    failures++;
	}
    }

    ret = bn_decode_hvect(hout, "{1 2 3 4}");
    {
	hvect_t exp_hout = {1.0, 2.0, 3.0, 4.0};
	if (ret != 4 || !hvect_close(hout, exp_hout, SMALL_FASTF)) {
	    report_failure(test, "brace-tolerant hvect decoding failed");
	    failures++;
	}
    }

    ret = bn_decode_mat(mout, "I");
    if (ret != 16 || !bn_mat_is_identity(mout)) {
	report_failure(test, "identity matrix decoding failed");
	failures++;
    }

    ret = bn_decode_angle(&ang, "1.5707963267948966rad");
    if (ret <= 0 || !scalar_close(ang, 90.0, 1.0e-12)) {
	report_failure(test, "radian angle decoding failed");
	failures++;
    }

    ret = bn_decode_angle(&ang, "45deg");
    if (ret <= 0 || !scalar_close(ang, 45.0, 1.0e-12)) {
	report_failure(test, "degree angle decoding failed");
	failures++;
    }

    ret = bn_decode_angle(&ang, "12foo");
    if (ret != 0) {
	report_failure(test, "invalid angle suffix should be rejected");
	failures++;
    }

    bu_vls_free(&vls);
    return failures;
}


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_single(argc, argv, "str", test_str_roundtrip);
}
