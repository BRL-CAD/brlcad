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
#include <signal.h>

#include "bu.h"

#if 0
void
termination_handler(const int signum)
{
  fprintf(stderr, "DEBUG: received signal %d (but exiting with status 0\n",
          signum);
  /* restore the signal */
}
#endif

/* Test against sprintf */
int
test_vls(const char *fmt, ...)
{
    int status = 0;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    char buffer[1024] = {0};
    va_list ap;

    va_start(ap, fmt);
    /* use the libc version */
    vsprintf(buffer, fmt, ap);
    va_end(ap);

    va_start(ap, fmt);
    /* use BRL-CAD bu_vls version for comparison */
    bu_vls_vprintf(&vls, fmt, ap);

    va_end(ap);

    if (BU_STR_EQUAL(buffer, bu_vls_addr(&vls))) {
        printf("%-24s -> '%s' [PASS]\n", fmt, bu_vls_addr(&vls));
    } else {
        printf("%-24s -> '%s' [FAIL]  (should be: '%s')\n", fmt, bu_vls_addr(&vls), buffer);
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
    int f = 0;
    int p = 0;
    const char *word = "Lawyer";

    printf("Testing vls...\n");

    /* ======================================================== */
    /* TESTS EXPECTED TO PASS
     *
     *   (see expected failures section below)
     */
    /* ======================================================== */
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
    f = p = 2;
    fails += test_vls("|%-*.*s|%*.*s|", f, p, "t", f, p, "t");
    fails += test_vls("|%*s|%-*s|", f, "test", f, "test");
    fails += test_vls("|%*s|%-*s|", f, word, f, word);

    /* min field width; max string length ('precision'); string */
    f = 2; p = 4;
    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
    fails += test_vls("%*.*s", f, p, word);

    f = 4; p = 2;
    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
    fails += test_vls("%*.*s", f, p, word);

    f = 4; p = 8;
    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
    fails += test_vls("%*.*s", f, p, word);

    f = 0; p = 8;
    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
    fails += test_vls("%*.*s", f, p, word);

    f = 8; p = 0;
    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
    fails += test_vls("%*.*s", f, p, word);

    /* same but left justify */

    f = 2; p = 4;
    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
    fails += test_vls("%-*.*s", f, p, word);

    f = 4; p = 2;
    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
    fails += test_vls("%-*.*s", f, p, word);

    f = 4; p = 8;
    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
    fails += test_vls("%-*.*s", f, p, word);

    f = 0; p = 8;
    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
    fails += test_vls("%-*.*s", f, p, word);

    f = 8; p = 0;
    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
    fails += test_vls("%-*.*s", f, p, word);

    /* from "various types" */
    fails += test_vls("%f %F", 1.23, -3.21);

    /* from "two-character length modifiers" */
    /* the last field ('%Lu') should result in zero because the number
       is a negative float and it is converted to a an unsigned int
       (it shouldn't be used with an integer) */
    fails += test_vls("%ld %lld %Lu", 123, -123, -321.0);

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

    printf("\n%s: testing complete\n", av[0]);

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
