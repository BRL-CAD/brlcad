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
#include <ctype.h>


char *
bu_basename(const char *str)
{
    const char *p;
    char *base_str;
    size_t len;

    if (UNLIKELY(!str)) {
	base_str = bu_malloc(2, "bu_basename empty");;
	base_str[0] = '.';
	base_str[1] = '\0';
	return base_str;
    }

    /* skip the filesystem disk/drive name if we're on a DOS-capable
     * platform that uses '\' for paths, e.g., C:\ -> \
     */
    if (BU_DIR_SEPARATOR == '\\' && isalpha(str[0]) && str[1] == ':') {
	str += 2;
    }

    /* Skip leading separators, e.g., ///foo/bar -> foo/bar */
    for (p = str; *p != '\0'; p++) {
	/* check native separator as well as / so we can use this
	 * routine for geometry paths too.
	 */
	if ((p[0] == BU_DIR_SEPARATOR && p[1] != BU_DIR_SEPARATOR && p[1] != '\0')
	    || (p[0] == '/' && p[1] != '/' && p[1] != '\0')) {
	    str = p+1;
	}
    }

    len = strlen(str);

    /* Remove trailing separators */
    while (len > 1 && (str[len - 1] == BU_DIR_SEPARATOR || str[len - 1] == '/'))
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
