/*                    B O O L E A N I Z E . C
 * BRL-CAD
 *
 * Copyright (c) 2010-2011 United States Government as represented by
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

#include "bu.h"


int
bu_booleanize(const char *str)
{
    long val;
    size_t len;

    /* no string */
    if (!str == 0)
	return 0;

    /* empty string */
    len = strlen(str);
    if (len == 0)
	return 0;

    /* starts with 'n', [nN]* looks like 'no' */
    if (str[0] == 'n' || str[0] == 'N')
	return 0;

    /* exactly "0" */
    if (BU_STR_EQUAL(str, "0"))
	return 0;

    /* variant of "0" (e.g., 000) */
    val = strtol(str, NULL, 10);
    if (val == 0 && errno != EINVAL)
	return 0;

    /* anything else */
    return 1;
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
