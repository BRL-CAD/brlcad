/*                    T E S T _ Q U O T E . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2012 United States Government as represented by
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

#include "bu.h"


/* Test for reversibility:
 *
 *   1. encode the input string
 *   2. decode the encoded string
 *   3. decoded string should be the same as input
 *
 */
int
test_quote(const char *str)
{
    int status = 0;
    struct bu_vls encoded = BU_VLS_INIT_ZERO;
    struct bu_vls decoded = BU_VLS_INIT_ZERO;

    bu_vls_encode(&encoded, str);
    bu_vls_encode(&decoded, bu_vls_addr(&encoded));


    if (BU_STR_EQUAL(str, bu_vls_addr(&decoded))
        && !BU_STR_EQUAL(str, bu_vls_addr(&encoded))) {
	printf("%24s -> %28s [PASS]\n", str, bu_vls_addr(&encoded));
    } else {
	printf("%24s -> %28s [FAIL]  (should be: %s)\n", str, bu_vls_addr(&encoded), str);
        status = 1;
    }

    bu_vls_free(&encoded);
    bu_vls_free(&decoded);

    return status;
}


int
main(int ac, char *av[])
{
    int fails    = 0; /* track unexpected failures */
    int expfails = 0; /* track expected failures */

    printf("Testing quote\n");

    if (ac > 1)
	printf("Usage: %s\n", av[0]);

    fails += test_quote(NULL);
    fails += test_quote("");
    fails += test_quote(" ");
    fails += test_quote("hello");
    fails += test_quote("\"");
    fails += test_quote("\'");
    fails += test_quote("\\");
    fails += test_quote("\\\"");
    fails += test_quote("\\\\");
    fails += test_quote("\"hello\"");
    fails += test_quote("\'hello\'");
    fails += test_quote("\\hello");
    fails += test_quote("\\hello\"");
    fails += test_quote("hello\\\\");
    fails += test_quote("\"hello\'\"");
    fails += test_quote("\"hello\'");
    fails += test_quote("\'hello\'");
    fails += test_quote("\'hello\"");
    fails += test_quote("\"\"hello\"");
    fails += test_quote("\'\'hello\'\'");
    fails += test_quote("\'\"hello\"\'");
    fails += test_quote("\"\"hello\"\"");
    fails += test_quote("\"\"\"hello\"\"\"");

    /* ======================================================== */
    /* EXPECTED FAILURES ONLY BELOW HERE                           */
    /* ======================================================== */
    /* EXPECTED FAILURES:
     *
     * Notes:
     *
     *   1. For these tests have the return value increment 'expfails'.
     *   2. Test with both 'make vsl-regress' and 'make regress' because
     *        some other tests use this function in unpredictable ways.
     *   3. After a test is fixed, change the return value to increment
     *        'fails', move it to the EXPECTED PASS group above, and add
     *        some info about it as necessary to help those who may be
     *        forced to revisit this.
     *
     */

    printf("\nExpected failures (don't use in production code):\n");

    printf("  NONE AT THIS TIME\n");

    /* report results */
    fprintf(stderr, "%d", expfails);

    printf("%s: testing complete\n", av[0]);

    if (fails != 0) {
      /* as long as fails is < 127 the STATUS will be the number of unexpected failures */
      return fails;
    }

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
