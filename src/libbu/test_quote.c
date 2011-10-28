/*                    T E S T _ Q U O T E . C
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
#include <stdarg.h>
#include <stdio.h>

#include "bu.h"


/* Test for reversibility */
void
test_quote(const char *str)
{
    struct bu_vls quoted = BU_VLS_INIT_ZERO;
    const char *dequoted = NULL;

    if (BU_STR_EQUAL(str, dequoted) && !BU_STR_EQUAL(str, bu_vls_addr(&quoted))) {
	printf("%24s -> %28s [PASS]\n", str, bu_vls_addr(&quoted));
    } else {
	printf("%24s -> %28s [FAIL]  (should be: %s)\n", str, bu_vls_addr(&quoted), str);
    }

    bu_vls_free(&quoted);
}


int
main(int ac, char *av[])
{
    printf("Testing quote\n");

    if (ac > 1)
	printf("Usage: %s\n", av[0]);

    test_quote(NULL);
    test_quote("");
    test_quote(" ");
    test_quote("hello");
    test_quote("\"");
    test_quote("\'");
    test_quote("\\");
    test_quote("\\\"");
    test_quote("\\\\");
    test_quote("\"hello\"");
    test_quote("\'hello\'");
    test_quote("\\hello");
    test_quote("\\hello\"");
    test_quote("hello\\\\");
    test_quote("\"hello\'\"");
    test_quote("\"hello\'");
    test_quote("\'hello\'");
    test_quote("\'hello\"");
    test_quote("\"\"hello\"");
    test_quote("\'\'hello\'\'");
    test_quote("\'\"hello\"\'");
    test_quote("\"\"hello\"\"");
    test_quote("\"\"\"hello\"\"\"");

    printf("%s: testing complete\n", av[0]);
    return 0;
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
