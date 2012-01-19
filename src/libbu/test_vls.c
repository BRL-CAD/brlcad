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
    int status = 0; /* no falures */
    int n;

    printf("Testing vls\n");

    /* various types */
    status += test_vls("");
    status += test_vls("\n");
    status += test_vls("hello");
    status += test_vls("%s", "hello");
    status += test_vls("%d", 123);
    status += test_vls("%u", -123);
    status += test_vls("%f %F", 1.23, -3.21);
    status += test_vls("%e %E", 1.23, -3.21);
    status += test_vls("%g %G", 1.23, -3.21);
    status += test_vls("%x %X", 1.23, -3.21);
    status += test_vls("%o", 1.23);
    status += test_vls("%c%c%c", '1', '2', '3');
    status += test_vls("%p", (void *)av);
    status += test_vls("%%%d%%", ac);

    /* various lengths */
    status += test_vls("%hd %hhd", 123, -123);
    status += test_vls("%ld %lld %Lu", 123, -123, -321.0);
    status += test_vls("%zu %zd", (size_t)123, (ssize_t)-123);
    status += test_vls("%jd %td", (intmax_t)123, (ptrdiff_t)-123);

    /* various widths */
    status += test_vls("he%10dllo", 123);
    status += test_vls("he%-10ullo", 123);
    status += test_vls("he%*so", 2, "ll");

    /* various precisions */
    status += test_vls("he%.10dllo", 123);
    status += test_vls("he%.-10ullo", 123);
    status += test_vls("he%.*so", 2, "ll");

    /* various flags */
    status += test_vls("%010d", 123);
    status += test_vls("%#-.10lx", 123);
    status += test_vls("%#lf", 123.0);
    status += test_vls("he%#-12.10tullo", (ptrdiff_t)0x1234);
    status += test_vls("he%+-6.3ld%-+3.6dllo", 123, 321);

    /* combinations, e.g., bug ID 3475562 */
    /* left justify, right justify, in wider fields than the strings */
    n = 2;
    status += test_vls("|%-*.*s|%*.*s|", n, n, "t", n, n, "t");

    printf("%s: testing complete\n", av[0]);

    return status;
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
