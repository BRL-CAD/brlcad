/*                   B O O L E A N I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2021 United States Government as represented by
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

#include "bu.h"


int
main(int argc, char *argv[])
{
    int result_true = 0;
    int result_false = 0;
    int expect_true = 0;

    const char *input = NULL;

    bu_setprogname(argv[0]);

    if (argc > 3) {
	fprintf(stderr, "Usage: %s {test_string} [expect_true]\n", argv[0]);
	return 1;
    }
    if (argc > 2) {
	expect_true = atol(argv[2]);
    }
    if (argc > 1) {
	input = argv[1];
    }

    result_true = bu_str_true(input);
    result_false = bu_str_false(input);

    if ((expect_true > 1)
	?
	(result_true > 1 && result_false == 0)
	:
	(expect_true == result_true && result_true != result_false))
    {
	printf("%24s -> true:%d, false:%d [PASSED]\n", input, result_true, result_false);
	return 0;
    }

    printf("%24s -> true:%d, false:%d (expecting true:%d) [FAIL]\n", input, result_true, result_false, expect_true);
    return 1;
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
