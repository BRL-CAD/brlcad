/*                     B A S E N A M E . C
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

#include "bu.h"
#include <string.h>

char *
bu_basename(const char *str)
{
    register const char	*p = str;
    char *base_str;
    int len;

    if (UNLIKELY(!str)) {
	base_str = bu_malloc(MAXPATHLEN, "bu_basename empty");;
	base_str[0] = '.';
	return base_str;
    }

    /* Skip leading separators */
    while (*p != '\0')
	if (*p++ == BU_DIR_SEPARATOR && *p != BU_DIR_SEPARATOR && *p != '\0')
	    str = p;

    len = strlen(str);
    
    /* Remove trailing separators */
    while (len > 1 && str[len - 1] == BU_DIR_SEPARATOR)
	len--;
    
    /* Create a new string */
    base_str = bu_calloc(len + 2, sizeof(char), "bu_basename alloc");
    if (len > 0) {
	bu_strlcpy(base_str, str, len+1);
    } else {
	base_str[0] = '.';
    }

    return base_str;
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
