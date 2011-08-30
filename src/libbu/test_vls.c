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
        printf("%24s -> %24s [PASS]\n", fmt, bu_vls_addr(&vls));
    } else {
        printf("%24s -> %24s [FAIL]  (should be: %s)\n", fmt, bu_vls_addr(&vls), buffer);
    }

    bu_vls_free(&vls);
}


int
main(int ac, char *av[])
{
    printf("Testing vls\n");

    test_vls("");
    test_vls("hello");
    test_vls("%s", "hello");
    test_vls("%d", 123);

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
