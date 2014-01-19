/*                     B A S E N A M E . C
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

#include "bu.h"
#include <string.h>
#include <ctype.h>


void
bu_basename(char *basename, const char *path)
{
    const char *p;
    size_t len;

    if (UNLIKELY(!path)) {
	bu_strlcpy(basename, ".", strlen(".")+1);
	return;
    }

    /* skip the filesystem disk/drive name if we're on a DOS-capable
     * platform that uses '\' for paths, e.g., C:\ -> \
     */
    if (BU_DIR_SEPARATOR == '\\' && isalpha((int)(path[0])) && path[1] == ':') {
	path += 2;
    }

    /* Skip leading separators, e.g., ///foo/bar -> foo/bar */
    for (p = path; *p != '\0'; p++) {
	/* check native separator as well as / so we can use this
	 * routine for geometry paths too.
	 */
	if ((p[0] == BU_DIR_SEPARATOR && p[1] != BU_DIR_SEPARATOR && p[1] != '\0')
	    || (p[0] == '/' && p[1] != '/' && p[1] != '\0')) {
	    path = p+1;
	}
    }

    len = strlen(path);

    /* Remove trailing separators */
    while (len > 1 && (path[len - 1] == BU_DIR_SEPARATOR || path[len - 1] == '/'))
	len--;

    if (len > 0) {
	bu_strlcpy(basename, path, len+1);
    } else {
	basename[0] = '.';
    }
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
