/*                      D I R N A M E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

#include <string.h>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"

char *
bu_dirname(const char *cp)
{
    char *ret;
    char *found_dslash;
    char *found_fslash;
    size_t len;
    const char DSLASH[2] = {BU_DIR_SEPARATOR, '\0'};
    const char FSLASH[2] = {'/', '\0'};
    const char *DOT = ".";
    const char *DOTDOT = "..";

    /* Special cases */
    if (UNLIKELY(!cp))
	return bu_strdup(".");

    if (BU_STR_EQUAL(cp, DSLASH))
	return bu_strdup(DSLASH);
    if (BU_STR_EQUAL(cp, FSLASH))
	return bu_strdup(FSLASH);

    if (BU_STR_EQUAL(cp, DOT)
	|| BU_STR_EQUAL(cp, DOTDOT)
	|| (strrchr(cp, BU_DIR_SEPARATOR) == NULL
	    && strrchr(cp, '/') == NULL))
	return bu_strdup(DOT);

    /* Make a duplicate copy of the string, and shorten it in place */
    ret = bu_strdup(cp);

    /* A sequence of trailing slashes don't count */
    len = strlen(ret);
    while (len > 1
	   && (ret[len-1] == BU_DIR_SEPARATOR
	       || ret[len-1] == '/')) {
	ret[len-1] = '\0';
	len--;
    }

    /* If no slashes remain, return "." */
    found_dslash = strrchr(ret, BU_DIR_SEPARATOR);
    found_fslash = strrchr(ret, '/');
    if (!found_dslash && !found_fslash) {
	bu_free(ret, "bu_dirname");
	return bu_strdup(DOT);
    }

    /* Remove trailing slash, unless it's at front */
    if (found_dslash == ret || found_fslash == ret) {
	ret[1] = '\0';		/* ret == BU_DIR_SEPARATOR || "/" */
    } else {
	if (found_dslash)
	    *found_dslash = '\0';
	if (found_fslash)
	    *found_fslash = '\0';
    }

    return ret;
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
