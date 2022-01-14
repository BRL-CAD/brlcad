/*                   B R L C A D _ P A T H . C
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
HIDDEN int
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
HIDDEN void
root_missing(const char *paths)
{
    bu_log("\
Unable to locate where BRL-CAD %s is installed while searching:\n\
%s\n\
This version of BRL-CAD was compiled to be installed at:\n\
	%s\n\n", brlcad_version(), paths, BRLCAD_ROOT);

    return;
}


const char *
bu_brlcad_dir(const char *dirkey, int fail_quietly)
{
    static char result[MAXPATHLEN] = {0};
    if (BU_STR_EQUAL(dirkey, "bin")) {
	snprintf(result, MAXPATHLEN, "%s", "bin");
	return result;
    }
    if (BU_STR_EQUAL(dirkey, "lib")) {
	snprintf(result, MAXPATHLEN, "%s", "lib");
	return result;
    }
    if (BU_STR_EQUAL(dirkey, "include")) {
	snprintf(result, MAXPATHLEN, "%s", "include");
	return result;
    }
    if (BU_STR_EQUAL(dirkey, "data") || BU_STR_EQUAL(dirkey, "share")) {
	snprintf(result, MAXPATHLEN, "%s", "share");
	return result;
    }
    if (BU_STR_EQUAL(dirkey, "doc")) {
	snprintf(result, MAXPATHLEN, "%s", "doc");
	return result;
    }
    if (BU_STR_EQUAL(dirkey, "man")) {
	snprintf(result, MAXPATHLEN, "%s", "share/man");
	return result;
    }

    if (!fail_quietly) {
	snprintf(result, MAXPATHLEN, "Unknown directory key %s", dirkey);
	return result;
    }
    return NULL;
}

extern int _bu_dir_join_path(char result[MAXPATHLEN], const char *lhs, const char *rhs, struct bu_vls *searched, const char *where);

const char *
bu_brlcad_root(const char *rhs, int fail_quietly)
{
    static char result[MAXPATHLEN] = {0};
    const char *lhs;
    int length, dirname_length;
    struct bu_vls searched = BU_VLS_INIT_ZERO;
    char where[MAX_WHERE_SIZE] = {0};

    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	bu_log("bu_brlcad_root: searching for [%s]\n", rhs?rhs:"");
    }

    /* BRLCAD_ROOT environment variable if set */
    lhs = getenv("BRLCAD_ROOT");
    if (lhs) {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT environment variable [%s]\n", lhs);
	if (join_path(result, lhs, rhs, &searched, where)) {
	    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
		bu_log("Found: BRLCAD_ROOT environment variable [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    } else {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT environment variable\n");
	bu_vls_strcat(&searched, where);
    }

    /* run-time path identification */
    length = wai_getExecutablePath(NULL, 0, &dirname_length);
    if (length > 0) {
	char *plhs  = (char *)bu_calloc(length+1, sizeof(char), "program path");
	wai_getExecutablePath(plhs, length, &dirname_length);
	plhs[length] = '\0';
	snprintf(where, MAX_WHERE_SIZE, "\trun-time path identification [UNKNOWN]\n");
	if (strlen(plhs)) {
	    char *dirpath;
	    char *real_path = bu_file_realpath(plhs, NULL);
	    dirpath = bu_path_dirname(real_path);
	    snprintf(real_path, MAXPATHLEN, "%s", dirpath);
	    bu_free(dirpath, "free bu_path_dirname");
	    dirpath = bu_path_dirname(real_path);
	    snprintf(real_path, MAXPATHLEN, "%s", dirpath);
	    bu_free(dirpath, "free bu_path_dirname");
	    if (_bu_dir_join_path(result, real_path, rhs, &searched, where)) {
		if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
		    bu_log("Found: Run-time path identification [%s]\n", result);
		}
		bu_vls_free(&searched);
		bu_free(real_path, "free real_path");
		bu_free(plhs, "free plhs");
		return result;
	    }
	    bu_free(real_path, "free real_path");
	} else {
	    bu_vls_strcat(&searched, where);
	}
	bu_free(plhs, "free plhs");
    }

    /* BRLCAD_ROOT compile-time path */
#ifdef BRLCAD_ROOT
    lhs = BRLCAD_ROOT;
    if (lhs) {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT compile-time path [%s]\n", lhs);
	if (join_path(result, lhs, rhs, &searched, where)) {
	    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
		bu_log("Found: BRLCAD_ROOT compile-time path [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    }
#else
    snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT compile-time path [UNKNOWN]\n");
    bu_vls_strcat(&searched, where);
#endif

    /* current directory */
    if (join_path(result, ".", rhs, &searched, "\tcurrent directory\n")) {
	if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	    bu_log("Found: current directory [%s]\n", result);
	}
	bu_vls_free(&searched);
	return result;
    }

    if (!fail_quietly) {
	root_missing(bu_vls_addr(&searched));
	if (rhs) {
	    bu_log("Unable to find '%s' within the BRL-CAD software installation.\nThis copy of BRL-CAD may not be properly installed.\n\n", rhs);
	} else {
	    bu_log("Unable to find the BRL-CAD software installation.\nThis copy of BRL-CAD may not be properly installed.\n\n");
	}
    }

    bu_vls_free(&searched);
    return NULL;
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
