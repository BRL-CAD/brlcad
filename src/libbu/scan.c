/*                         S C A N . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2022 United States Government as represented by
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
    int len, delim_len;
    size_t i;
    char *delim_fmt;

    if (UNLIKELY(!delim || n < 1)) {
	return 0;
    }

    va_start(ap, n);

    /* TODO: we should just simply skip all occurrences of delim chars
     * after reading a fastf_t, avoid dynamic memory allocation and
     * avoid scanf/sscanf (we don't need to scan the delimiter, just
     * skip chars in a loop until none are found.
     */

    delim_len = (int)strlen(delim);
    /* + 3 here to make room for the two characters '%' and 'n' as
     * well as the terminating '\0'
     */
    delim_fmt = (char *)bu_malloc(delim_len + 3, "bu_scan_fastf_t");
    bu_strlcpy(delim_fmt, delim, delim_len + 1);
    bu_strlcat(delim_fmt, "%n", delim_len + 3);

    for (i = 0; i < n; i++) {
	/* Read in the next fastf_t */
	double scan = 0;
	fastf_t *arg;

	if (src)
	    part_n = sscanf(src + offset, "%lf%n", &scan, &len);
	else
	    part_n = scanf("%lf%n", &scan, &len);

	current_n += part_n;
	offset += len;
	if (part_n != 1) {
	    break;
	}

	arg = va_arg(ap, fastf_t *);
	if (arg) {
	    *arg = scan;
	}
	/* Don't scan an extra delimiter at the end of the string */
	if (i == n - 1) {
	    break;
	}

	/* Make sure that a delimiter is present */
	if (src)
	    part_n = sscanf(src + offset, delim_fmt, &len);
	else
	    part_n = scanf(delim_fmt, &len);

	offset += len;

	/* as delim_fmt should only have a %n and that doesn't get
	 * counted in the scanf return, make sure the return is 0 as
	 * no values should be scanned.
	 */
	if (part_n != 0 || len != delim_len) {
	    break;
	}
    }

    va_end(ap);
    bu_free(delim_fmt, "bu_scan_fastf_t");

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
