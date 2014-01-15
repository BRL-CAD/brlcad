/*                    T E S T _ V L S _ V P R I N T F. C
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
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
#include <string.h>
#include <ctype.h>

#include "bu.h"

#include "./vls_internals.h"


/* Test against sprintf */
int
test_vls(const char *fmt, ...)
{
    int status        = 0; /* okay */
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    char output[80]   = {0};
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

    snprintf(output, sizeof(output), "%-24s -> '%s'", fmt, bu_vls_addr(&vls));
    if (BU_STR_EQUAL(buffer, bu_vls_addr(&vls))
	&& strlen(buffer) == bu_vls_strlen(&vls)) {
	printf("%-*s[PASS]\n", 60, output);
    } else {
	printf("%-*s[FAIL]  (should be: '%s')\n", 60, output, buffer);
	status = 1;
    }

    bu_vls_free(&vls);

    return status;
}


int
check_format_chars(void)
{
  int status = 0; /* assume okay */
  int i, flags;
  vflags_t f;

  for (i = 0; i < 255; ++i) {
    unsigned char c = (unsigned char)i;
    if (!isprint(c))
      continue;
    flags = format_part_status(c);
    if (flags & VP_VALID) {
      /* we need a valid part handler */
      int vp_part = flags & VP_PARTS;

      /* for the moment we only have one such handler */
      if (vp_part ^ VP_LENGTH_MOD) /* same as !(vp_part & VP_LENGTH_MOD) */
	continue;

      if (!handle_format_part(vp_part, &f, c, VP_NOPRINT)) {
	/* tell user */
	printf("Unhandled valid char '%c'                                    [FAIL]\n", c);
	status = 1;
      }
    } else if (flags & VP_OBSOLETE) {
      /* we need an obsolete part handler */
      if (!handle_obsolete_format_char(c, VP_NOPRINT)) {
	/* tell user */
	printf("Unhandled obsolete char '%c'                                 [FAIL]\n", c);
	status = 1;
      }
    }
  }

  return status;
}


int
main(int argc, char *argv[])
{

    int test_num = 0;
    int f = 0;
    int p = 0;
    const char *word = "Lawyer";

    if (argc < 2) {
	fprintf(stderr,"Usage: %s test_num\n", argv[0]);
	return 1;
    }


    sscanf(argv[1], "%d", &test_num);


    switch (test_num) {
	case 1:
	    /* check that we handle all known format chars */
	    return check_format_chars();
	case 2:
	    return test_vls("");
	case 3:
	    return test_vls("\n");
	case 4:
	    return test_vls("hello");
	case 5:
	    return test_vls("%s", "hello");
	case 6:
	    return test_vls("%d", 123);
	case 7:
	    return test_vls("%u", -123);
	case 8:
	    return test_vls("%e %E", 1.23, -3.21);
	case 9:
	    return test_vls("%g %G", 1.23, -3.21);
	case 10:
	    return test_vls("%x %X", 1.23, -3.21);
	case 11:
	    return test_vls("%x %X", 123, -321);
	case 12:
	    return test_vls("%o", 1.23);
	case 13:
	    return test_vls("%c%c%c", '1', '2', '3');
	case 14:
	    return test_vls("%p", (void *)argv);
	case 15:
	    return test_vls("%%%d%%", argc);
	/* various lengths */
	case 16:
	    return test_vls("%zu %zd", (size_t)123, (ssize_t)-123);
	case 17:
	    return test_vls("%jd %td", (intmax_t)123, (ptrdiff_t)-123);
	/* various widths */
	case 18:
	    return test_vls("he%*so", 2, "ll");
	case 19:
	    return test_vls("he%*so", 2, "llll");
	case 20:
	    return test_vls("he%*so", 4, "ll");
	/* various precisions */
	case 21:
	    return test_vls("he%.*so", 2, "ll");
	case 22:
	    return test_vls("he%.-1-o", 123);
	case 23:
	    return test_vls("%6.-3f", 123);
	/* various flags */
	case 24:
	    return test_vls("%010d", 123);
	case 25:
	    return test_vls("%#-.10lx", 123);
	case 26:
	    return test_vls("%#lf", 123.0);
	/* two-character length modifiers */
	case 27:
	    return test_vls("he%10dllo", 123);
	case 28:
	    return test_vls("he%-10ullo", 123);
	case 29:
	    return test_vls("he%#-12.10tullo", (ptrdiff_t)0x1234);
	case 30:
	    return test_vls("he%+-6.3ld%-+3.6dllo", 123, 321);
	case 31:
	    return test_vls("he%.10dllo", 123);
	case 32:
	    return test_vls("he%.-10ullo", 123);
	case 33:
	    return test_vls("%hd %hhd", 123, -123);
	/* combinations, e.g., bug ID 3475562, fixed at rev 48958 */
	/* left justify, right justify, in wider fields than the strings */
	case 34:
	    f = p = 2;
	    return test_vls("|%-*.*s|%*.*s|", f, p, "t", f, p, "t");
	case 35:
	    f = p = 2;
	    return test_vls("|%*s|%-*s|", f, "test", f, "test");
	case 36:
	    f = p = 2;
	    return test_vls("|%*s|%-*s|", f, word, f, word);
	/* min field width; max string length ('precision'); string */
	case 37:
	    f = 2; p = 4;
	    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
	    return test_vls("%*.*s", f, p, word);
	case 38:
	    f = 4; p = 2;
	    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
	    return test_vls("%*.*s", f, p, word);
	case 39:
	    f = 4; p = 8;
	    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
	    return test_vls("%*.*s", f, p, word);
	case 40:
	    f = 0; p = 8;
	    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
	    return test_vls("%*.*s", f, p, word);
	case 41:
	    f = 8; p = 0;
	    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
	    return test_vls("%*.*s", f, p, word);
	case 42:
	    /* mged bug at rev 48989 */
	    f = 8; p = 0;
	    printf("fw=%d, '%s': '%%%ds'\n", f, word, f);
	    return test_vls("%*s", f, word);
	/* same but left justify */
	case 43:
	    f = 2; p = 4;
	    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
	    return test_vls("%-*.*s", f, p, word);
	case 44:
	    f = 4; p = 2;
	    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
	    return test_vls("%-*.*s", f, p, word);
	case 45:
	    f = 4; p = 8;
	    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
	    return test_vls("%-*.*s", f, p, word);
	case 46:
	    f = 0; p = 8;
	    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
	    return test_vls("%-*.*s", f, p, word);
	case 47:
	    f = 8; p = 0;
	    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
	    return test_vls("%-*.*s", f, p, word);
	/* from "various types" */
	case 48:
	    return test_vls("%f %F", 1.23, -3.21);
	/* from "two-character length modifiers" */
	case 49:
	    return test_vls("%ld %lld", 123, -123LL);
	/* unsigned variant */
	case 50:
	    return test_vls("%lu %llu", 123, 123ULL);
	/* from "two-character length modifiers" */
	case 51:
	    return test_vls("%ld %lld", 123, -123);
	/* unsigned variant */
	case 52:
	    return test_vls("%lu %llu", 123, 123);
	case 53:
	    return test_vls("%hd %hhd", 123, -123);
	/* misc */
	case 54:
	    return test_vls("% d % d", 123, -123);
	case 55:
	    return test_vls("% 05d % d", 123, -123);
	case 56:
	    return test_vls("%'d", 123000);
	case 57:
	    return test_vls("%c", 'r');
	case 58:
	    return test_vls("%20s", "right");
	case 59:
	    return test_vls("%-20s", "left");
	case 60:
	    return test_vls("%10.20s", "12345");
	case 61:
	    return test_vls("%-10.20s", "12345");
	case 62:
	    return test_vls("%10.20s", "123456789012345");
	case 63:
	    return test_vls("%-10.20s", "123456789012345");
	case 64:
	    return test_vls("%20.10s", "123456789012345");
	case 65:
	    return test_vls("%-20.10s", "123456789012345");

	/* this test needs a relook
	    return test_vls("%H", 123);
	 */

	/* obsolete but usable */
	/*
	   test_vls("%S", (wchar_t *)"hello");
	   test_vls("%qd %qd", 123, -123);
	 */

	/* EXPECTED FAILURES (don't use in production code):
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

	 /* obsolete - expected failures
	case 10000:
	    return !test_vls("%C", 'N');
	case 10001:
	    return !test_vls("%D %D", 123, -123);
	case 10002:
	    return !test_vls("%O %O", 123, -123);
	case 10003:
	    return !test_vls("%U %U", 123, -123);a
	*/
    }

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
