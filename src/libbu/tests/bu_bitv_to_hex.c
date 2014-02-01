/*                 B U _ B I T V _ T O _ H E X . C
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
test_bu_bitv_to_hex(int argc, char **argv)
{
    /*         argv[1]             argv[2]               argv[3]
     * inputs: <input char string> <expected hex string> <expected bitv length>
     */
    struct bu_vls  *result;
    struct bu_bitv *result_bitv;
    int length;
    const char *input, *expected;
    int test_results = FAIL;

    if (argc < 4) {
	bu_exit(1, "ERROR: input format: function_test_args [%s]\n", argv[0]);
    }

    length = atoi(argv[3]);
    if (length < (int)BITS_PER_BYTE || length % (int)BITS_PER_BYTE) {
	bu_exit(1, "ERROR: input format is function_num function_test_args [%s]\n", argv[0]);
    }

    input    = argv[1];
    expected = argv[2];

    result = bu_vls_vlsinit();
    result_bitv = bu_bitv_new(length);

    /* accessing the bits array directly as a char* is not safe since
     * there's no bounds checking and assumes implementation is
     * contiguous memory.
     */
    bu_strlcpy((char*)result_bitv->bits, input, length/BITS_PER_BYTE);

    bu_bitv_to_hex(result, result_bitv);

    test_results = bu_strcmp(result->vls_str, expected);

    if (!test_results) {
        printf("bu_bitv_to_hex PASSED Input: '%s' Output: '%s' Expected: '%s'\n",
               input, bu_vls_cstr(result), expected);
    } else {
        printf("bu_bitv_to_hex FAILED for Input: '%s' Output: '%s' Expected: '%s'\n",
               input, bu_vls_cstr(result), expected);
    }

    bu_bitv_free(result_bitv);
    bu_vls_free(result);

    /* a false return above is a PASS, so the value is the same as ctest's PASS/FAIL */
    return test_results;
}


int
main(int argc , char **argv)
{

    return test_bu_bitv_to_hex(argc, argv);
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
