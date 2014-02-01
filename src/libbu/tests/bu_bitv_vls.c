/*                 B U _ B I T V _ V L S . C
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
test_bu_bitv_vls(int argc, char **argv)
{
    /*         argv[1]             argv[2]
     * inputs: <input char string> <expected hex string>
     */
    int test_results = FAIL;
    struct bu_vls *a;
    struct bu_bitv *res_bitv;
    const char *input, *expected;

    if (argc < 3) {
	bu_exit(1, "ERROR: input format: function_test_args [%s]\n", argv[0]);
    }

    input    = argv[1];
    expected = argv[2];

    a = bu_vls_vlsinit();
    res_bitv = bu_hex_to_bitv(input);
    bu_bitv_vls(a, res_bitv);

    test_results = bu_strcmp(bu_vls_cstr(a), expected);

    if (!test_results) {
	printf("\nbu_bitv_vls test PASSED Input: '%s' Output: '%s' Expected: '%s'",
	       input, bu_vls_cstr(a), expected);
    } else {
	printf("\nbu_bitv_vls FAILED for Input: '%s' Expected: '%s' Expected: '%s'",
	       input, bu_vls_cstr(a), expected);
    }

    bu_vls_free(a);
    bu_bitv_free(res_bitv);

    /* a false return above is a pass, so the value is the same as ctest's PASS/FAIL */
    return test_results;
}


int
main(int argc , char **argv)
{
    return test_bu_bitv_vls(argc, argv);
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
