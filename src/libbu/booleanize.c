/*                    B O O L E A N I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2021 United States Government as represented by
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
#include <limits.h>
#include <string.h>
#include <errno.h>

#include "bu/parse.h"
#include "bu/str.h"

int
bu_str_true(const char *str)
{
    long val;
    size_t len;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    const char *newstr;
    char *endptr;

    /* no string */
    if (!str)
	return 0;

    bu_vls_strcpy(&vls, str);
    bu_vls_trimspace(&vls);
    newstr = bu_vls_addr(&vls);

    /* empty string */
    len = strlen(newstr);
    if (len == 0) {
	bu_vls_free(&vls);
	return 0;
    }

    /* case-insensitive "no" */
    if (BU_STR_EQUIV(newstr, "n") || BU_STR_EQUIV(newstr, "no")) {
	bu_vls_free(&vls);
	return 0;
    }

    /* case-insensitive "false" */
    if (BU_STR_EQUIV(newstr, "false")) {
	bu_vls_free(&vls);
	return 0;
    }

    /* case-insensitive "off" */
    if (BU_STR_EQUIV(newstr, "off")) {
	bu_vls_free(&vls);
	return 0;
    }

    /* any variant of "0" (e.g., 000) */
    errno = 0;
    val = strtol(newstr, &endptr, 10);
    if (val == 0 && !errno && *endptr == '\0') {
	bu_vls_free(&vls);
	return 0;
    }

    /* case-insensitive "(null)" */
    if (BU_STR_EQUIV(newstr, "(null)")) {
	bu_vls_free(&vls);
	return 0;
    }

    /* true value from here on out */

    /* case-insensitive "yes" */
    if (BU_STR_EQUIV(newstr, "y") || BU_STR_EQUIV(newstr, "yes")) {
	bu_vls_free(&vls);
	return 1;
    }

    /* case-insensitive "true" */
    if (BU_STR_EQUIV(newstr, "true")) {
	bu_vls_free(&vls);
	return 1;
    }

    /* case-insensitive "on" */
    if (BU_STR_EQUIV(newstr, "on")) {
	bu_vls_free(&vls);
	return 1;
    }

    /* variant of "1" (e.g., 001) */
    errno = 0;
    val = strtol(newstr, &endptr, 10);
    if (val == 1 && !errno && *endptr == '\0') {
	bu_vls_free(&vls);
	return 1;
    }

    /* done with our string */
    val = bu_vls_addr(&vls)[0];
    bu_vls_free(&vls);

    /* anything else */
    return (int)val;
}


int
bu_str_false(const char *str)
{
    return !bu_str_true(str);
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
