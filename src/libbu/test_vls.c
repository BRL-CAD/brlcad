/*                      T E S T _ V L S . C
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


/* Test against sprintf */
void
test_vls(const char *fmt, ...)
{
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
    }

    bu_vls_free(&vls);
}


int
main(int ac, char *av[])
{
    printf("Testing vls\n");

    /* various types */
    test_vls("");
    test_vls("hello");
    test_vls("%s", "hello");
    test_vls("%d", 123);
    test_vls("%u", -123);
    test_vls("%f %F", 1.23, -3.21);
    test_vls("%e %E", 1.23, -3.21);
    test_vls("%g %G", 1.23, -3.21);
    test_vls("%x %X", 1.23, -3.21);
    test_vls("%o", 1.23);
    test_vls("%c%c%c", '1', '2', '3');
    test_vls("%p", (void *)av);
    test_vls("%%%d%%", ac);

    /* various lengths */
    test_vls("%hd %hhd", 123, -123);
    test_vls("%ld %lld %Lu", 123, -123, -321.0);
    test_vls("%zu %zd", (size_t)123, (ssize_t)-123);
    test_vls("%jd %td", (intmax_t)123, (ptrdiff_t)-123);

    /* various widths */
    test_vls("he%10dllo", 123);
    test_vls("he%-10ullo", 123);
    test_vls("he%*so", 2, "ll");

    /* various precisions */
    test_vls("he%.10dllo", 123);
    test_vls("he%.-10ullo", 123);
    test_vls("he%.*so", 2, "ll");

    /* various flags */
    test_vls("%010d", 123);
    test_vls("%#-.10lx", 123);
    test_vls("he%+-6.3ld%-+3.6dllo", 123, 321);

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
