/*                         S C A N . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "vmath.h"

#include "bu/log.h"

int
bu_scan_fastf_t(int *c, const char *src, const char *delim, size_t n, ...)
{
    va_list ap;
    int offset = 0;
    int current_n = 0;
    size_t i;

    if (c) {
	*c = 0;
    }


    if (UNLIKELY(!delim || !*delim || n < 1)) {
	return 0;
    }

    va_start(ap, n);

    for (i = 0; i < n; i++) {
	double scan = 0.0;
	fastf_t *arg;

	if (src) {
	    const char *input = src + offset;
	    char *end;

	    scan = strtod(input, &end);
	    if (end == input) {
		break;
	    }
	    offset += (int)(end - input);
	} else {
	    int len = 0;

	    if (scanf("%lf%n", &scan, &len) != 1) {
		break;
	    }
	    offset += len;
	}

	current_n++;
	arg = va_arg(ap, fastf_t *);
	if (arg) {
	    *arg = scan;
	}
	/* Don't scan an extra delimiter at the end of the string */
	if (i == n - 1) {
	    break;
	}

	if (src) {
	    const char *input = src + offset;

	    while (*input && strchr(delim, *input)) {
		input++;
	    }
	    if (input == src + offset) {
		break;
	    }
	    offset += (int)(input - (src + offset));
	} else {
	    int character;
	    int delimiter_count = 0;

	    while ((character = fgetc(stdin)) != EOF) {
		if (!strchr(delim, character)) {
		    (void)ungetc(character, stdin);
		    break;
		}
		delimiter_count++;
		offset++;
	    }
	    if (!delimiter_count) {
		break;
	    }
	}
    }

    va_end(ap);

    if (c) {
	*c = offset;
    }
    return current_n;
}
/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
