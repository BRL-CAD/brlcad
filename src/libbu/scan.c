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
#include <string.h>
#include "vmath.h"

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"

int
bu_scan_fastf_t(int *c, const char *src, const char *delim, size_t n, ...)
{
    va_list ap;
    int offset = 0;
    int current_n = 0, part_n = 0;
    int len = 0, delim_len;
    size_t i;

    if (c)
	*c = 0;

    if (UNLIKELY(!delim || n < 1)) {
	return 0;
    }

    if (src && *src == '\0') {
	return 0;
    }

    delim_len = (int)strlen(delim);
    va_start(ap, n);

    for (i = 0; i < n; i++) {
	/* Read in the next fastf_t */
	double scan = 0;
	fastf_t *arg;

	len = 0;
	if (src)
	    part_n = sscanf(src + offset, "%lf%n", &scan, &len);
	else
	    part_n = scanf("%lf%n", &scan, &len);

	if (part_n != 1 || len <= 0) {
	    break;
	}

	current_n += part_n;
	offset += len;

	arg = va_arg(ap, fastf_t *);
	if (arg) {
	    *arg = (fastf_t)scan;
	}
	/* Don't scan an extra delimiter at the end of the string */
	if (i == n - 1) {
	    break;
	}

	/* Make sure that a delimiter is present */
	if (src) {
	    if (bu_strncmp(src + offset, delim, (size_t)delim_len) != 0) {
		break;
	    }
	    offset += delim_len;
	} else {
	    int match = 1;
	    int d;
	    for (d = 0; d < delim_len; d++) {
		int ch = getchar();
		if (ch != (unsigned char)delim[d]) {
		    if (ch != EOF)
			ungetc(ch, stdin);
		    match = 0;
		    break;
		}
		offset++;
	    }
	    if (!match) {
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
