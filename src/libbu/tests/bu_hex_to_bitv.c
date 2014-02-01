/*                 B U _ H E X _ T O  _ B I T V . C
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
test_bu_hex_to_bitv(int argc, char **argv)
{
    /*         argv[1]             argv[2]
     * inputs: <input hex string> <expected char string>
     */
    struct bu_bitv *res_bitv ;
    int test_results = FAIL;
    const char *input, *expected;

    if (argc < 3) {
	bu_exit(1, "ERROR: input format: function_test_args [%s]\n", argv[0]);
    }

    input    = argv[1];
    expected = argv[2];

    res_bitv = bu_hex_to_bitv(input);

    if (res_bitv == NULL) {
	printf("\nbu_hex_to_bitv FAILED Input: '%s' Output: 'NULL' Expected: '%s'",
	       input, expected);
	test_results = FAIL;
    } else {
	test_results = bu_strcmp((char*)res_bitv->bits, expected);

	if (!test_results) {
	    printf("\nbu_hex_to_bitv PASSED Input: '%s' Output: '%s' Expected: '%s'",
		   input, (char *)res_bitv->bits, expected);
	} else {
	    printf("\nbu_hex_to_bitv FAILED Input: '%s' Output: '%s' Expected: '%s'",
		   input, (char *)res_bitv->bits, expected);
	}
    }

    if (res_bitv != NULL)
	bu_bitv_free(res_bitv);

    /* a false return above is a PASS, so the value is the same as ctest's PASS/FAIL */
    return test_results;
}

int
main(int argc , char **argv)
{
    return test_bu_hex_to_bitv(argc, argv);
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
