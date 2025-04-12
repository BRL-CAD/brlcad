/*                   B R L C A D _ P A T H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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

#ifdef HAVE_SYS_PARAM_H /* for MAXPATHLEN */
#  include <sys/param.h>
#endif

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "whereami.h"

#include "bu/app.h"
#include "bu/debug.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/vls.h"

#include "brlcad_version.h"


/* defaults for configure-less compiles */
#ifndef BRLCAD_ROOT
#  define BRLCAD_ROOT "/usr/brlcad"
#endif


/* arbitrary buffer size large enough to hold a couple paths and a label */
#define MAX_WHERE_SIZE (size_t)((MAXPATHLEN*2) + 64)


/**
 * put a left-hand and right-hand path together and test whether they
 * exist or not.
 *
 * @return boolean on whether a match was found.
 */
static int
join_path(char result[MAXPATHLEN], const char *lhs, const char *rhs, struct bu_vls *searched, const char *where)
{
    size_t llen, rlen;
    static const char *currdir=".";

    /* swap right with left if there is no left so logic is simplified
     * later on.
     */
    if (lhs == NULL && rhs != NULL) {
	lhs = rhs;
	rhs = NULL;
    }

    if (lhs == NULL) {
	if (searched && where) {
	    bu_vls_strcat(searched, where);
	}
	return 0;
    }

    rlen = 0;
    if (rhs) {
	rlen = strlen(rhs);
    }

    /* be safe */
    if (rlen + 2 > MAXPATHLEN) {
	bu_log("Warning: path is way too long (%zu characters > %d)\n", rlen+2, MAXPATHLEN);
	if (searched && where) {
	    bu_vls_strcat(searched, where);
	}
	return 0;
    }

    /* an empty left hand implies current directory (plus a slash is appended later) */
    if (lhs[0] == '\0') {
	lhs = currdir;
    }

    /* left-hand path should exist independent of right-hand path */
    if (!bu_file_exists(lhs, NULL)) {
	if (searched && where) {
	    bu_vls_strcat(searched, where);
	}
	return 0;
    }

    /* start fresh */
    memset(result, 0, (size_t)MAXPATHLEN);
    bu_strlcpy(result, lhs, (size_t)MAXPATHLEN);

    /* nothing to add, so just return what we have */
    if (!rhs || (rlen == 0)) {
	return 1;
    }

    /* be safe again */
    llen = strlen(result);
    if (llen + rlen + 3 > MAXPATHLEN) {
	bu_log("Warning: path is way too long (%zu characters > %d)\n", llen+rlen+3, MAXPATHLEN);
	if (searched && where) {
	    bu_vls_strcat(searched, where);
	}
	return 0;
    }

    if ((*(result+llen-1) != BU_DIR_SEPARATOR) && (rhs[0] != BU_DIR_SEPARATOR)) {
	/* let the caller give "/usr/brlcad" and "bin" and get "/usr/brlcad/bin" */
	*(result+llen) = BU_DIR_SEPARATOR;
	llen++;
    } else if ((*(result+llen-1) == BU_DIR_SEPARATOR) && (rhs[0] == BU_DIR_SEPARATOR)) {
	/* let the caller give "/usr/brlcad/" and "/bin" and get "/usr/brlcad/bin"*/
	rhs++;
	rlen--;
    }

    /* found a match */
    bu_strlcpy(result+llen, rhs, (size_t)(MAXPATHLEN - llen));
    if (bu_file_exists(result, NULL)) {
	return 1;
    }

    /* close, but no match */
    if (searched && where) {
	bu_vls_strcat(searched, where);
    }
    return 0;
}


/**
 * print out an error/warning message if we cannot find the specified
 * BRLCAD_ROOT (compile-time install path)
 */
static void
root_missing(const char *paths)
{
    bu_log("\
Unable to locate where BRL-CAD %s is installed while searching:\n\
%s\n\
This version of BRL-CAD was compiled to be installed at:\n\
	%s\n\n", brlcad_version(), paths, BRLCAD_ROOT);

    return;
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
