/*                       R E A L P A T H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"

/* c89 strict doesn't declare realpath */
#if defined(HAVE_WORKING_REALPATH) && !defined(HAVE_DECL_REALPATH)
extern char *realpath(const char *, char *);
#endif

char *
bu_file_realpath(const char *path, char *resolved_path)
{
    if (!resolved_path)
	resolved_path = (char *) bu_calloc(MAXPATHLEN, sizeof(char), "resolved_path alloc");

#if defined(HAVE_WORKING_REALPATH)
    {
	char *dirpath = NULL;
	/* NOTE: realpath() has a critical but well-reported buffer-overrun bug
	 * (linux glibc pre 5.4.13), so we intentionally avoid it if we have
	 * alternatives. */
#if defined(HAVE_WORKING_REALPATH)
	dirpath = realpath(path, resolved_path);
#endif
	if (!dirpath) {
	    /* If path lookup failed, resort to simple copy */
	    bu_strlcpy(resolved_path, path, MAXPATHLEN);
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
