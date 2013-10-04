/*                       R E A L P A T H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
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

#include <limits.h>
#include <stdlib.h>

#include "bio.h"

#include "bu.h"

char *
bu_realpath(const char *path, char *resolved_path)
{
    if (!resolved_path)
	resolved_path = (char *) bu_calloc(MAXPATHLEN, sizeof(char), "resolved_path alloc");

#if defined(HAVE_WORKING_REALPATH_FUNCTION)
    {
	char *dirpath = NULL;
	dirpath = realpath(path, resolved_path);
	if (!dirpath) {
	    /* if path lookup failed, resort to simple copy */
	    bu_strlcpy(resolved_path, path, (size_t)MAXPATHLEN);
	}
    }
#elif defined(HAVE_GETFULLPATHNAME)
    /* Best solution currently available for Windows
     * See https://www.securecoding.cert.org/confluence/display/seccode/FIO02-C.+Canonicalize+path+names+originating+from+untrusted+sources */
    GetFullPathName(path, MAXPATHLEN, resolved_path, NULL);
#else
    /* Last resort - if NOTHING is defined, do a simple copy */
    bu_strlcpy(resolved_path, path, (size_t)MAXPATHLEN);
#endif

    return resolved_path;
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
