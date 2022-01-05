/*                    V L S _ V P R I N T F. C
 * BRL-CAD
 *
 * Copyright (c) 2011-2021 United States Government as represented by
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
#include "../vls_vprintf.h"


/* Test against sprintf */
int
vls_vs_sys(const char *fmt, ...)
{
    int status        = BRLCAD_OK; /* okay */
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
	status = BRLCAD_ERROR;
    }

    bu_vls_free(&vls);

    return status;
}

/* Test against a pre-defined string for formatting that
 * is not supported reliably across operating systems */
int
vls_vs_string(const char *correct, const char *fmt, ...)
{
    int status        = BRLCAD_OK; /* okay */
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    char output[80]   = {0};
    va_list ap;

    va_start(ap, fmt);
    /* use BRL-CAD bu_vls version for comparison */
    bu_vls_vprintf(&vls, fmt, ap);
    va_end(ap);

    snprintf(output, sizeof(output), "%-24s -> '%s'", fmt, bu_vls_addr(&vls));
    if (BU_STR_EQUAL(correct, bu_vls_addr(&vls))
	&& strlen(correct) == bu_vls_strlen(&vls)) {
	printf("%-*s[PASS]\n", 60, output);
    } else {
	printf("%-*s[FAIL]  (should be: '%s')\n", 60, output, correct);
	status = BRLCAD_ERROR;
    }

    bu_vls_free(&vls);

    return status;
}

int
check_format_chars(void)
{
    int status = BRLCAD_OK; /* assume okay */
    int i, cflags;
    vflags_t f = VFLAGS_INIT_ZERO;

    for (i = 0; i < 255; ++i) {
	unsigned char c = (unsigned char)i;
	if (!isprint(c))
	    continue;
	cflags = format_part_status(c);
	if (cflags & VP_VALID) {
	    /* we need a valid part handler */
	    int vp_part = cflags & VP_PARTS;

	    /* for the moment we only have one such handler */
	    if (vp_part ^ VP_LENGTH_MOD) /* same as !(vp_part & VP_LENGTH_MOD) */
		continue;

	    if (!handle_format_part(vp_part, &f, c, VP_NOPRINT)) {
		/* tell user */
		printf("Unhandled valid char '%c'                                    [FAIL]\n", c);
		status = BRLCAD_ERROR;
	    }
	} else if (cflags & VP_OBSOLETE) {
	    /* we need an obsolete part handler */
	    if (!handle_obsolete_format_char(c, VP_NOPRINT)) {
		/* tell user */
		printf("Unhandled obsolete char '%c'                                 [FAIL]\n", c);
		status = BRLCAD_ERROR;
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

    bu_setprogname(argv[0]);

    if (argc < 2) {
	fprintf(stderr, "Usage: %s {test_num}\n", argv[0]);
	return 1;
    }

    sscanf(argv[1], "%d", &test_num);

    switch (test_num) {
	case 1:
	    /* check that we handle all known format chars */
	    return check_format_chars();
	case 2:
	    return vls_vs_sys("");
	case 3:
	    return vls_vs_sys("\n");
	case 4:
	    return vls_vs_sys("hello");
	case 5:
	    return vls_vs_sys("%s", "hello");
	case 6:
	    return vls_vs_sys("%d", 123);
	case 7:
	    return vls_vs_sys("%u", -123);
	case 8:
	    return vls_vs_sys("%e %E", 1.23, -3.21);
	case 9:
	    return vls_vs_sys("%g %G", 1.23, -3.21);
	case 10:
	    return vls_vs_sys("%x %X", 1.23, -3.21);
	case 11:
	    return vls_vs_sys("%x %X", 123, -321);
	case 12:
	    return vls_vs_sys("%o", 1.23);
	case 13:
	    return vls_vs_sys("%c%c%c", '1', '2', '3');
	case 14:
	    return vls_vs_sys("%p", (void *)argv);
	case 15:
	    return vls_vs_sys("%%%d%%", argc);
	    /* various lengths */
	case 16:
	    return vls_vs_string("123 -123", "%zu %zd", (size_t)123, (ssize_t)-123);
	case 17:
	    return vls_vs_string("123 -123", "%jd %td", (intmax_t)123, (ptrdiff_t)-123);
	    /* various widths */
	case 18:
	    return vls_vs_sys("he%*so", 2, "ll");
	case 19:
	    return vls_vs_sys("he%*so", 2, "llll");
	case 20:
	    return vls_vs_sys("he%*so", 4, "ll");
	    /* various precisions */
	case 21:
	    return vls_vs_sys("he%.*so", 2, "ll");
	case 22:
	    return vls_vs_sys("he%.-1-o", 123);
	case 23:
	    return vls_vs_sys("%6.-3f", 123);
	    /* various flags */
	case 24:
	    return vls_vs_sys("%010d", 123);
	case 25:
	    return vls_vs_sys("%#-.10lx", 123);
	case 26:
	    return vls_vs_sys("%#lf", 123.0);
	    /* two-character length modifiers */
	case 27:
	    return vls_vs_sys("he%10dllo", 123);
	case 28:
	    return vls_vs_sys("he%-10ullo", 123);
	case 29:
	    return vls_vs_string("he0000004660  llo", "he%#-12.10tullo", (ptrdiff_t)0x1234);
	case 30:
	    return vls_vs_sys("he%+-6.3ld%-+3.6dllo", 123, 321);
	case 31:
	    return vls_vs_sys("he%.10dllo", 123);
	case 32:
	    return vls_vs_sys("he%.-10ullo", 123);
	case 33:
	    return vls_vs_sys("%hd %hhd", 123, -123);
	    /* combinations, e.g., bug ID 3475562, fixed at rev 48958 */
	    /* left justify, right justify, in wider fields than the strings */
	case 34:
	    f = p = 2;
	    return vls_vs_sys("|%-*.*s|%*.*s|", f, p, "t", f, p, "t");
	case 35:
	    f = 2;
	    return vls_vs_sys("|%*s|%-*s|", f, "test", f, "test");
	case 36:
	    f = 2;
	    return vls_vs_sys("|%*s|%-*s|", f, word, f, word);
	    /* min field width; max string length ('precision'); string */
	case 37:
	    f = 2; p = 4;
	    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
	    return vls_vs_sys("%*.*s", f, p, word);
	case 38:
	    f = 4; p = 2;
	    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
	    return vls_vs_sys("%*.*s", f, p, word);
	case 39:
	    f = 4; p = 8;
	    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
	    return vls_vs_sys("%*.*s", f, p, word);
	case 40:
	    f = 0; p = 8;
	    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
	    return vls_vs_sys("%*.*s", f, p, word);
	case 41:
	    f = 8; p = 0;
	    printf("fw=%d, prec=%d, '%s': '%%%d.%ds'\n", f, p, word, f, p);
	    return vls_vs_sys("%*.*s", f, p, word);
	case 42:
	    /* mged bug at rev 48989 */
	    f = 8;
	    printf("fw=%d, '%s': '%%%ds'\n", f, word, f);
	    return vls_vs_sys("%*s", f, word);
	    /* same but left justify */
	case 43:
	    f = 2; p = 4;
	    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
	    return vls_vs_sys("%-*.*s", f, p, word);
	case 44:
	    f = 4; p = 2;
	    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
	    return vls_vs_sys("%-*.*s", f, p, word);
	case 45:
	    f = 4; p = 8;
	    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
	    return vls_vs_sys("%-*.*s", f, p, word);
	case 46:
	    f = 0; p = 8;
	    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
	    return vls_vs_sys("%-*.*s", f, p, word);
	case 47:
	    f = 8; p = 0;
	    printf("fw=%d, prec=%d, '%s': '%%-%d.%ds'\n", f, p, word, f, p);
	    return vls_vs_sys("%-*.*s", f, p, word);
	    /* from "various types" */
	case 48:
	    return vls_vs_sys("%f %F", 1.23, -3.21);
	    /* from "two-character length modifiers" */
	case 49:
	    return vls_vs_sys("%ld %lld", 123, -123LL);
	    /* unsigned variant */
	case 50:
	    return vls_vs_sys("%lu %llu", 123, 123ULL);
	    /* from "two-character length modifiers" */
	case 51:
	    return vls_vs_sys("%ld %lld", 123, -123);
	    /* unsigned variant */
	case 52:
	    return vls_vs_sys("%lu %llu", 123, 123);
	case 53:
	    return vls_vs_sys("%hd %hhd", 123, -123);
	    /* misc */
	case 54:
	    return vls_vs_sys("% d % d", 123, -123);
	case 55:
	    return vls_vs_sys("% 05d % d", 123, -123);
	case 56:
	    return vls_vs_sys("%'d", 123000);
	case 57:
	    return vls_vs_sys("%c", 'r');
	case 58:
	    return vls_vs_sys("%20s", "right");
	case 59:
	    return vls_vs_sys("%-20s", "left");
	case 60:
	    return vls_vs_sys("%10.20s", "12345");
	case 61:
	    return vls_vs_sys("%-10.20s", "12345");
	case 62:
	    return vls_vs_sys("%10.20s", "123456789012345");
	case 63:
	    return vls_vs_sys("%-10.20s", "123456789012345");
	case 64:
	    return vls_vs_sys("%20.10s", "123456789012345");
	case 65:
	    return vls_vs_sys("%-20.10s", "123456789012345");
	case 66:
	    return vls_vs_sys("%010d", 1);
	case 67:
	    return vls_vs_sys("%.030s", "1234567890123456789012345678901234567890");

	    /* this test needs a relook
	       return vls_vs_sys("%H", 123);
	    */

	    /* obsolete but usable */
	    /*
	      vls_vs_sys("%S", (wchar_t *)"hello");
	      vls_vs_sys("%qd %qd", 123, -123);
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
	       return !vls_vs_sys("%C", 'N');
	       case 10001:
	       return !vls_vs_sys("%D %D", 123, -123);
	       case 10002:
	       return !vls_vs_sys("%O %O", 123, -123);
	       case 10003:
	       return !vls_vs_sys("%U %U", 123, -123);a
	    */
    }

    return BRLCAD_ERROR;
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
