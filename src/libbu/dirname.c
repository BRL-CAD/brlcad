/*                      D I R N A M E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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

#include "bu.h"


char *
bu_dirname(const char *cp)
{
    char *ret;
    char *slash;
    size_t len;
    char SLASH = BU_DIR_SEPARATOR;
    const char *DOT = ".";
    const char *DOTDOT = "..";

    /* Special cases */
    if (cp == NULL)  return bu_strdup(".");
    if (BU_STR_EQUAL(cp, &SLASH))
	return bu_strdup(&SLASH);
    if (BU_STR_EQUAL(cp, DOT) ||
	BU_STR_EQUAL(cp, DOTDOT) ||
	strrchr(cp, SLASH) == NULL)
	return bu_strdup(DOT);

    /* Make a duplicate copy of the string, and shorten it in place */
    ret = bu_strdup(cp);

    /* A trailing slash doesn't count */
    len = strlen(ret);
    if (ret[len-1] == SLASH)  ret[len-1] = '\0';

    /* If no slashes remain, return "." */
    if ((slash = strrchr(ret, SLASH)) == NULL) {
	bu_free(ret, "bu_dirname");
	return bu_strdup(DOT);
    }

    /* Remove trailing slash, unless it's at front */
    if (slash == ret)
	ret[1] = '\0';		/* ret == "/" */
    else
	*slash = '\0';

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
