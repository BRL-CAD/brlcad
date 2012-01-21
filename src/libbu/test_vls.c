/*                      T E S T _ V L S . C
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

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "bu.h"


/* Test against sprintf */
int
test_vls(const char *fmt, ...)
{
    int status = 0;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    char buffer[1024] = {0};
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    va_end(ap);

    va_start(ap, fmt);
    bu_vls_vprintf(&vls, fmt, ap);
    va_end(ap);

    if (BU_STR_EQUAL(buffer, bu_vls_addr(&vls))) {
        printf("%24s -> %28s [PASS]\n", fmt, bu_vls_addr(&vls));
    } else {
        printf("%24s -> %28s [FAIL]  (should be: %s)\n", fmt, bu_vls_addr(&vls), buffer);
        status = 1;
    }

    bu_vls_free(&vls);

    return status;
}


int
main(int ac, char *av[])
{
    int fails    = 0; /* track unexpected failures */
    int expfails = 0; /* track expected failures */
    int n;

    printf("Testing vls\n");

    /* various types */
    printf("An empty string (\"\"):\n");
    fails += test_vls("");

    printf("A newline (\"\\n\"):\n");
    fails += test_vls("\n");

    fails += test_vls("hello");
    fails += test_vls("%s", "hello");
    fails += test_vls("%d", 123);
    fails += test_vls("%u", -123);
    fails += test_vls("%e %E", 1.23, -3.21);
    fails += test_vls("%g %G", 1.23, -3.21);
    fails += test_vls("%x %X", 1.23, -3.21);
    fails += test_vls("%o", 1.23);
    fails += test_vls("%c%c%c", '1', '2', '3');
    fails += test_vls("%p", (void *)av);
    fails += test_vls("%%%d%%", ac);

    /* various lengths */
    fails += test_vls("%zu %zd", (size_t)123, (ssize_t)-123);
    fails += test_vls("%jd %td", (intmax_t)123, (ptrdiff_t)-123);

    /* various widths */
    fails += test_vls("he%*so", 2, "ll");

    /* various precisions */
    fails += test_vls("he%.*so", 2, "ll");
    fails += test_vls("he%.-1-o", 123);
    fails += test_vls("%6.-3f", 123);

    /* various flags */
    fails += test_vls("%010d", 123);
    fails += test_vls("%#-.10lx", 123);
    fails += test_vls("%#lf", 123.0);

    /* two-character length modifiers */
    fails += test_vls("he%10dllo", 123);
    fails += test_vls("he%-10ullo", 123);
    fails += test_vls("he%#-12.10tullo", (ptrdiff_t)0x1234);
    fails += test_vls("he%+-6.3ld%-+3.6dllo", 123, 321);
    fails += test_vls("he%.10dllo", 123);
    fails += test_vls("he%.-10ullo", 123);
    fails += test_vls("%hd %hhd", 123, -123);

    /* combinations, e.g., bug ID 3475562, fixed at rev 48958 */
    /* left justify, right justify, in wider fields than the strings */
    n = 2;
    fails += test_vls("|%-*.*s|%*.*s|", n, n, "t", n, n, "t");

    /* ======================================================== */
    /* KNOWN FAILURES BELOW HERE. NOTE: increment expfail vs. fails */
    /* FAILING TESTS (make vls-regress) ================ */
    printf("\nExpected failures (don't use in production code):\n");

    /* from "various types" */
    expfails += test_vls("%f %F", 1.23, -3.21);

    /* from "two-character length modifiers" */
    /* the last field ('%Lu') should result in zero because the number
       is a negative float and it is converted to a an unsigned int
       (it shouldn't be used with an integer) */
    expfails += test_vls("%ld %lld %Lu", 123, -123, -321.0);

    printf("%s: testing complete\n", av[0]);

    if (fails != 0) {
      /* as long as fails is < 127 the STATUS will be the number of unexped failures */
      return fails;
    }

    /* output expected failures to stderr to be used for the
       regression test script */
    fprintf(stderr, "%d", expfails);

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
