/*                 B U _ B I T V _ O R . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "bu.h"
#include "./test_internals.h"


static int
test_bu_bitv_or(int argc , char **argv)
{
    /*         argv[1]               argv[2]             argv[3]
     * inputs: <input hex string 1> <input hex string 2> <expected hex result>
     */
    struct bu_bitv *res_bitv , *res_bitv1 , *result;
    struct bu_vls *a , *b;
    int test_results = FAIL;
    const char *input1, *input2, *expected;

    if (argc < 4) {
	bu_exit(1, "ERROR: input format: function_test_args [%s]\n", argv[0]);
    }

    input1   = argv[1];
    input2   = argv[2];
    expected = argv[3];

    a = bu_vls_vlsinit();
    b = bu_vls_vlsinit();

    res_bitv1 = bu_hex_to_bitv(input1);
    res_bitv  = bu_hex_to_bitv(input2);
    result    = bu_hex_to_bitv(expected);

    bu_bitv_or(res_bitv1, res_bitv);
    bu_bitv_vls(a, res_bitv1);
    bu_bitv_vls(b, result);

    test_results = bu_strcmp(bu_vls_cstr(a), bu_vls_cstr(b));

    if (!test_results) {
	printf("\nbu_bitv_or test PASSED Input1: '%s' Input2: '%s' Output: '%s'",
	       input1, input2, expected);
    } else {
	printf("\nbu_bitv_or test FAILED Input1: '%s' Input2: '%s' Expected: '%s'",
	       input1, input2, expected);
    }

    bu_bitv_free(res_bitv);
    bu_bitv_free(res_bitv1);
    bu_bitv_free(result);

    /* a false return above is a PASS, so the value is the same as ctest's PASS/FAIL */
    return test_results;
}


int
main(int argc, char **argv)
{
    return test_bu_bitv_or(argc, argv);
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
