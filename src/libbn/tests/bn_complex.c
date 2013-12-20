/*                 B N _ C O M P L E X . C
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "bn.h"

static int
test_bn_cx_div(int argc, char **argv)
{
    /* We don't need an actual_result because bn_cx_div stores the
       result in ap */
    bn_complex_t expected_result = { 0.0, 0.0 };
    bn_complex_t ap = { 0.0, 0.0 };
    bn_complex_t bp = { 0.0, 0.0 };

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is APre,APim BPre,BPim expected_re,expected_im [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg,%lg", &bn_cx_real(&ap), &bn_cx_imag(&ap));
    sscanf(argv[3], "%lg,%lg", &bn_cx_real(&bp), &bn_cx_imag(&bp));
    sscanf(argv[4], "%lg,%lg", &bn_cx_real(&expected_result), &bn_cx_imag(&expected_result));

    bn_cx_div(&ap, &bp);

    bu_log("result: %g + %g * i\n", bn_cx_real(&ap), bn_cx_imag(&ap));

    return !(NEAR_EQUAL(bn_cx_real(&expected_result), bn_cx_real(&ap), BN_TOL_DIST)
	     && NEAR_EQUAL(bn_cx_imag(&expected_result), bn_cx_imag(&ap), BN_TOL_DIST));
}

static int
test_bn_cx_mul(int argc, char **argv)
{
    /* We need an actual_result even though bn_cx_mul stores the
       result in ap because we also test bn_cx_mul2 */
    bn_complex_t expected_result = { 0.0, 0.0 };
    bn_complex_t actual_result = { 0.0, 0.0 };
    bn_complex_t ap = { 0.0, 0.0 };
    bn_complex_t bp = { 0.0, 0.0 };

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is APre,APim BPre,BPim expected_re,expected_im [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg,%lg", &bn_cx_real(&ap), &bn_cx_imag(&ap));
    sscanf(argv[3], "%lg,%lg", &bn_cx_real(&bp), &bn_cx_imag(&bp));
    sscanf(argv[4], "%lg,%lg", &bn_cx_real(&expected_result), &bn_cx_imag(&expected_result));

    bn_cx_mul2(&actual_result, &ap, &bp)
    bn_cx_mul(&ap, &bp);

    bu_log("bn_cx_mul: result: %g + %g * i\n", bn_cx_real(&ap), bn_cx_imag(&ap));
    bu_log("bn_cx_mul2: result: %g + %g * i\n", bn_cx_real(&ap), bn_cx_imag(&ap));

    return !(NEAR_EQUAL(bn_cx_real(&expected_result), bn_cx_real(&actual_result), BN_TOL_DIST)
	     && NEAR_EQUAL(bn_cx_imag(&expected_result), bn_cx_imag(&actual_result), BN_TOL_DIST)
	     && NEAR_EQUAL(bn_cx_real(&expected_result), bn_cx_real(&ap), BN_TOL_DIST)
	     && NEAR_EQUAL(bn_cx_imag(&expected_result), bn_cx_imag(&ap), BN_TOL_DIST));
}

static int
test_bn_cx_sub(int argc, char **argv)
{
    /* We don't need an actual_result because bn_cx_sub stores the
       result in ap */
    bn_complex_t expected_result = { 0.0, 0.0 };
    bn_complex_t ap = { 0.0, 0.0 };
    bn_complex_t bp = { 0.0, 0.0 };

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is APre,APim BPre,BPim expected_re,expected_im [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg,%lg", &bn_cx_real(&ap), &bn_cx_imag(&ap));
    sscanf(argv[3], "%lg,%lg", &bn_cx_real(&bp), &bn_cx_imag(&bp));
    sscanf(argv[4], "%lg,%lg", &bn_cx_real(&expected_result), &bn_cx_imag(&expected_result));

    bn_cx_sub(&ap, &bp);

    bu_log("result: %g + %g * i\n", bn_cx_real(&ap), bn_cx_imag(&ap));

    return !(NEAR_EQUAL(bn_cx_real(&expected_result), bn_cx_real(&ap), BN_TOL_DIST)
	     && NEAR_EQUAL(bn_cx_imag(&expected_result), bn_cx_imag(&ap), BN_TOL_DIST));
}

static int
test_bn_cx_add(int argc, char **argv)
{
    /* We don't need an actual_result because bn_cx_add stores the
       result in ap */
    bn_complex_t expected_result = { 0.0, 0.0 };
    bn_complex_t ap = { 0.0, 0.0 };
    bn_complex_t bp = { 0.0, 0.0 };

    if (argc != 5) {
	bu_exit(1, "ERROR: input format is APre,APim BPre,BPim expected_re,expected_im [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg,%lg", &bn_cx_real(&ap), &bn_cx_imag(&ap));
    sscanf(argv[3], "%lg,%lg", &bn_cx_real(&bp), &bn_cx_imag(&bp));
    sscanf(argv[4], "%lg,%lg", &bn_cx_real(&expected_result), &bn_cx_imag(&expected_result));

    bn_cx_add(&ap, &bp);

    bu_log("result: %g + %g * i\n", bn_cx_real(&ap), bn_cx_imag(&ap));

    return !(NEAR_EQUAL(bn_cx_real(&expected_result), bn_cx_real(&ap), BN_TOL_DIST)
	     && NEAR_EQUAL(bn_cx_imag(&expected_result), bn_cx_imag(&ap), BN_TOL_DIST));
}

static int
test_bn_cx_sqrt(int argc, char **argv)
{
    bn_complex_t expected_result = { 0.0, 0.0 };
    bn_complex_t actual_result = { 0.0, 0.0 };
    bn_complex_t ip = { 0.0, 0.0 };

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is IPre,IPim expected_re,expected_im [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg,%lg", &bn_cx_real(&ip), &bn_cx_imag(&ip));
    sscanf(argv[3], "%lg,%lg", &bn_cx_real(&expected_result), &bn_cx_imag(&expected_result));

    bn_cx_sqrt(&actual_result, &ip);

    bu_log("result: %g + %g * i\n", bn_cx_real(&actual_result), bn_cx_imag(&actual_result));

    return !(NEAR_EQUAL(bn_cx_real(&expected_result), bn_cx_real(&actual_result), BN_TOL_DIST)
	     && NEAR_EQUAL(bn_cx_imag(&expected_result), bn_cx_imag(&actual_result), BN_TOL_DIST));
}

static int
test_bn_cx_neg(int argc, char **argv)
{
    /* We don't need an actual_result because bn_cx_neg stores the
       result in ip */
    bn_complex_t expected_result = { 0.0, 0.0 };
    bn_complex_t ip = { 0.0, 0.0 };

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is IPre,IPim expected_re,expected_im [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg,%lg", &bn_cx_real(&ip), &bn_cx_imag(&ip));
    sscanf(argv[3], "%lg,%lg", &bn_cx_real(&expected_result), &bn_cx_imag(&expected_result));

    bn_cx_neg(&ip);

    bu_log("result: %g + %g * i\n", bn_cx_real(&ip), bn_cx_imag(&ip));

    return !(NEAR_EQUAL(bn_cx_real(&expected_result), bn_cx_real(&ip), BN_TOL_DIST)
	     && NEAR_EQUAL(bn_cx_imag(&expected_result), bn_cx_imag(&ip), BN_TOL_DIST));
}

static int
test_bn_cx_conj(int argc, char **argv)
{
    /* We don't need an actual_result because bn_cx_conj stores the
       result in ip */
    bn_complex_t expected_result = { 0.0, 0.0 };
    bn_complex_t ip = { 0.0, 0.0 };

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is IPre,IPim expected_re,expected_im [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg,%lg", &bn_cx_real(&ip), &bn_cx_imag(&ip));
    sscanf(argv[3], "%lg,%lg", &bn_cx_real(&expected_result), &bn_cx_imag(&expected_result));

    bn_cx_conj(&ip);

    bu_log("result: %g + %g * i\n", bn_cx_real(&ip), bn_cx_imag(&ip));

    return !(NEAR_EQUAL(bn_cx_real(&expected_result), bn_cx_real(&ip), BN_TOL_DIST)
	     && NEAR_EQUAL(bn_cx_imag(&expected_result), bn_cx_imag(&ip), BN_TOL_DIST));
}

static int
test_bn_cx_parts(int argc, char **argv)
{
    fastf_t expected_re = 0;
    fastf_t actual_re = 0;
    fastf_t expected_im = 0;
    fastf_t actual_im = 0;
    bn_complex_t ip = { 0.0, 0.0 };

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is IPre,IPim expected_re,expected_im [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lg,%lg", &bn_cx_real(&ip), &bn_cx_imag(&ip));
    sscanf(argv[3], "%lg,%lg", &expected_re, &expected_im);

    actual_re = bn_cx_real(&ip);
    actual_im = bn_cx_imag(&ip);

    bu_log("real: %g", actual_re);
    bu_log("imaginary: %g", actual_im);

    return !(NEAR_EQUAL(expected_re, actual_re, BN_TOL_DIST)
	     && NEAR_EQUAL(expected_im, actual_im, BN_TOL_DIST));
}

int
main(int argc, char *argv[])
{
    int function_num = 0;

    if (argc < 3) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    sscanf(argv[1], "%d", &function_num);
    if (function_num < 1 || function_num > 8)
	function_num = 0;

    switch (function_num) {
	case 1:
	    return test_bn_cx_div(argc, argv);
	case 2:
	    return test_bn_cx_mul(argc, argv);
	case 3:
	    return test_bn_cx_sub(argc, argv);
	case 4:
	    return test_bn_cx_add(argc, argv);
	case 5:
	    return test_bn_cx_sqrt(argc, argv);
	case 6:
	    return test_bn_cx_neg(argc, argv);
	case 7:
	    return test_bn_cx_conj(argc, argv);
	case 8:
	    return test_bn_cx_parts(argc, argv);
    }
    bu_exit(1, "ERROR: function_num %d is not valid [%s]\n", function_num, argv[0]);
    /* should never get here */
    return 0;
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
