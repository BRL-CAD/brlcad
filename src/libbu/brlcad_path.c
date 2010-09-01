/*                   B R L C A D _ P A T H . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
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
/** @addtogroup bu_log */
/** @{ */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#include "bu.h"
#include "sysv.h"

/* private headers */
#include "brlcad_version.h"


/* defaults for configure-less compiles */
#ifndef BRLCAD_ROOT
#  define BRLCAD_ROOT "/usr/brlcad"
#endif


/* arbitrary buffer size large enough to hold a couple paths and a label */
#define MAX_WHERE_SIZE (size_t)((MAXPATHLEN*2) + 64)


/* internal storage for bu_getprogname/bu_setprogname */
static char bu_progname[MAXPATHLEN] = {0};


HIDDEN const char *
_brlcad_data()
{
    static char path[MAXPATHLEN] = {0};
#ifndef BRLCAD_DATA
    snprintf(path, MAXPATHLEN, "%s/share/brlcad/%s", BRLCAD_ROOT, brlcad_version());
#else
    /* do this instead of just returning BRLCAD_DATA to keep compiler
     * quiet about unreachable code.
     */
    snprintf(path, MAXPATHLEN, "%s", BRLCAD_DATA);
#endif
    return path;
}


/**
 * b u _ i p w d
 *
 * set/return the path to the initial working directory.
 * bu_setprogname() must be called on app startup for the correct pwd to
 * be acquired/set.
 */
HIDDEN const char *
_bu_ipwd()
{
    /* private stash */
    static const char *ipwd = NULL;
    static char buffer[MAXPATHLEN] = {0};

    if (ipwd) {
	return ipwd;
    }

    ipwd = getenv("PWD"); /* not our memory to free */
    if (!ipwd)
	ipwd = bu_which("pwd");

    if (ipwd) {
#if defined(HAVE_POPEN) && !defined(STRICT_FLAGS)
	FILE *fp = NULL;

	fp = popen(ipwd, "r");
	if (fp) {
	    if (bu_fgets(buffer, MAXPATHLEN, fp)) {
		ipwd = buffer;
	    } else {
		ipwd = ".";
	    }
	} else {
	    ipwd = ".";
	}
#else
	memset(buffer, 0, MAXPATHLEN); /* quellage */
	ipwd = ".";
#endif
    } else {
	ipwd = ".";
    }

    return ipwd;
}


HIDDEN const char *
_bu_argv0(void)
{
    /* private stash */
    static const char *argv0 = NULL;

    /* set initial pwd if we have not already */
    (void)_bu_ipwd();

    if (bu_progname[0] != '\0') {
	argv0 = bu_progname;
    }

#ifdef HAVE_GETPROGNAME
    if (!argv0) {
	/* do not call bu_getgrogname() */
	argv0 = getprogname(); /* not malloc'd memory */
    }
#endif

    if (!argv0) {
	argv0 = "(unknown)";
    }

    return argv0;
}


const char *
bu_argv0_full_path(void)
{
    static char buffer[MAXPATHLEN] = {0};

    const char *argv0 = _bu_argv0();
    const char *ipwd = _bu_ipwd();

    const char *which = bu_which(argv0);

    if (argv0[0] == BU_DIR_SEPARATOR) {
	/* seems to already be a full path */
	snprintf(buffer, MAXPATHLEN, "%s", argv0);
	return buffer;
    }

    /* running from PATH */
    if (which) {
	snprintf(buffer, MAXPATHLEN, "%s", which);
	return buffer;
    }

    while (argv0[0] == '.' && argv0[1] == BU_DIR_SEPARATOR) {
	/* remove ./ if present, relative paths are appended to pwd */
	argv0 += 2;
    }

    /* running from relative dir */
    snprintf(buffer, MAXPATHLEN, "%s%c%s", ipwd, BU_DIR_SEPARATOR, argv0);
    if (bu_file_exists(buffer)) {
	return buffer;
    }

    /* give up */
    snprintf(buffer, MAXPATHLEN, "%s", argv0);
    return buffer;
}


const char *
bu_getprogname(void) {
    const char *name = NULL;

    if (bu_progname[0] != '\0') {
	return bu_basename(bu_progname);
    }

#ifdef HAVE_GETPROGNAME
    name = getprogname(); /* not malloc'd memory */
#endif

    if (!name) {
	name = _bu_argv0();
    }

    snprintf(bu_progname, MAXPATHLEN, "%s", name);

    return bu_basename(bu_progname);
}


void
bu_setprogname(const char *argv0)
{
#ifdef HAVE_SETPROGNAME
    setprogname(argv0);
#endif

    if (argv0) {
	snprintf(bu_progname, MAXPATHLEN, "%s", argv0);
    }

    (void)_bu_ipwd();

    return;
}


/* DEPRECATED: Do not use. */
const char *
bu_argv0(void)
{
    return _bu_argv0();
}


/**
 * b u _ r o o t _ m i s s i n g
 *
 *print out an error/warning message if we cannot find the specified
 * BRLCAD_ROOT (compile-time install path)
 */
HIDDEN void
_bu_root_missing(const char *paths)
{
    bu_log("\
Unable to locate where BRL-CAD %s is installed while searching:\n\
%s\n\
This version of BRL-CAD was compiled to be installed at:\n\
	%s\n\n", brlcad_version(), paths, BRLCAD_ROOT);

#ifndef _WIN32
    bu_log("\
You may specify where to locate BRL-CAD by setting the BRLCAD_ROOT\n\
environment variable.  For example:\n\
\n\
for csh/tcsh users:\n\
	setenv BRLCAD_ROOT /path/to/brlcad\n\
for sh/bash users:\n\
	BRLCAD_ROOT=/path/to/brlcad ; export BRLCAD_ROOT\n\n");
#endif

    return;
}


/**
 * b u _ d a t a _ m i s s i n g
 *
 * print out an error/warning message if we cannot find the specified
 * BRLCAD_DATA (compile-time install path)
 */
HIDDEN void
_bu_data_missing(const char *paths)
{
    bu_log("\
Unable to locate where BRL-CAD %s data resources are installed\n\
while searching:\n\
%s\n\
This release of BRL-CAD expects data resources to be at:\n\
	%s\n\n", brlcad_version(), paths, _brlcad_data());

#ifndef _WIN32
    bu_log("\
You may specify where to locate BRL-CAD data resources by setting\n\
the BRLCAD_DATA environment variable.  For example:\n\
\n\
for csh/tcsh users:\n\
	setenv BRLCAD_DATA /path/to/brlcad/data\n\
for sh/bash users:\n\
	BRLCAD_DATA=/path/to/brlcad/data ; export BRLCAD_DATA\n\n");
#endif

    return;
}


/**
 * put a left-hand and right-hand path together and test whether they
 * exist or not.
 *
 * @return boolean on whether a match was found.
 */
HIDDEN int
_bu_find_path(char result[MAXPATHLEN], const char *lhs, const char *rhs, struct bu_vls *searched, const char *where)
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

    rlen = llen = 0;
    if (rhs) {
	rlen = strlen(rhs);
    }

    /* be safe */
    if (rlen + 2 > MAXPATHLEN) {
	bu_log("Warning: path is way too long (%d characters > %d)\n", rlen+2, MAXPATHLEN);
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
    if (!bu_file_exists(lhs)) {
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
	bu_log("Warning: path is way too long (%d characters > %d)\n", llen+rlen+3, MAXPATHLEN);
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
    if (bu_file_exists(result)) {
	return 1;
    }

    /* close, but no match */
    if (searched && where) {
	bu_vls_strcat(searched, where);
    }
    return 0;
}


const char *
bu_brlcad_root(const char *rhs, int fail_quietly)
{
    static char result[MAXPATHLEN] = {0};
    const char *lhs;
    struct bu_vls searched;
    char where[MAX_WHERE_SIZE] = {0};

    bu_vls_init(&searched);

    if (bu_debug & BU_DEBUG_PATHS) {
	bu_log("bu_brlcad_root: searching for [%s]\n", rhs?rhs:"");
    }

    /* BRLCAD_ROOT environment variable if set */
    lhs = getenv("BRLCAD_ROOT");
    if (lhs) {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT environment variable [%s]\n", lhs);
	if (_bu_find_path(result, lhs, rhs, &searched, where)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: BRLCAD_ROOT environment variable [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    } else {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT environment variable\n");
	bu_vls_strcat(&searched, where);
    }

    /* BRLCAD_ROOT compile-time path */
#ifdef BRLCAD_ROOT
    lhs = BRLCAD_ROOT;
    if (lhs) {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT compile-time path [%s]\n", lhs);
	if (_bu_find_path(result, lhs, rhs, &searched, where)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
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

    /* run-time path identification */
    lhs = bu_getprogname();
    if (lhs) {
	char argv0[MAX_WHERE_SIZE] = {0};
	size_t len = strlen(lhs);
	snprintf(argv0, MAX_WHERE_SIZE, "%s", lhs);

	/* need to trim off the trailing binary */
	while (len-1 > 0) {
	    if (argv0[len-1] == BU_DIR_SEPARATOR) {
		argv0[len] = '.';
		argv0[len+1] = '.';
		argv0[len+2] = '\0';
		break;
	    }
	    len--;
	}

	snprintf(where, MAX_WHERE_SIZE, "\trun-time path identification [%s]\n", argv0);
	if (_bu_find_path(result, argv0, rhs, &searched, where)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: Run-time path identification [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    } else {
	snprintf(where, MAX_WHERE_SIZE, "\trun-time path identification [UNKNOWN]\n");
	bu_vls_strcat(&searched, where);
    }

    /* /usr/brlcad static path */
    {
	const char *root = BRLCAD_ROOT;
	/* only check /usr/brlcad if not already tested earlier via BRLCAD_ROOT */

	if (root[0] != '/' || root[1] != 'u' || root[ 2] != 's' || root[ 3] != 'r' ||
	    root[4] != '/' || root[5] != 'b' || root[ 6] != 'r' || root[ 7] != 'l' ||
	    root[8] != 'c' || root[9] != 'a' || root[10] != 'd' || root[11] != '\0') {

	    if (_bu_find_path(result, "/usr/brlcad", rhs, &searched, "\t/usr/brlcad default path\n")) {
		if (bu_debug & BU_DEBUG_PATHS) {
		    bu_log("Found: /usr/brlcad default path [%s]\n", result);
		}
		bu_vls_free(&searched);
		return result;
	    }
	}
    }

    /* current directory */
    if (_bu_find_path(result, ".", rhs, &searched, "\tcurrent directory\n")) {
	if (bu_debug & BU_DEBUG_PATHS) {
	    bu_log("Found: current directory [%s]\n", result);
	}
	bu_vls_free(&searched);
	return result;
    }

    if (!fail_quietly) {
	_bu_root_missing(bu_vls_addr(&searched));
	if (rhs) {
	    bu_log("Unable to find '%s' within the BRL-CAD software installation.\nThis copy of BRL-CAD may not be properly installed.\n\n", rhs);
	} else {
	    bu_log("Unable to find the BRL-CAD software installation.\nThis copy of BRL-CAD may not be properly installed.\n\n");
	}
    }

    bu_vls_free(&searched);
    return NULL;
}


const char *
bu_brlcad_data(const char *rhs, int fail_quietly)
{
    static char result[MAXPATHLEN] = {0};
    const char *lhs;
    struct bu_vls searched;
    char where[MAX_WHERE_SIZE] = {0};
    char path[MAXPATHLEN] = {0};

    bu_vls_init(&searched);

    if (bu_debug & BU_DEBUG_PATHS) {
	bu_log("bu_brlcad_data: looking for [%s]\n", rhs?rhs:"");
    }

    /* BRLCAD_DATA environment variable if set */
    lhs = getenv("BRLCAD_DATA");
    if (lhs) {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_DATA environment variable [%s]\n", lhs);
	if (_bu_find_path(result, lhs, rhs, &searched, where)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: BRLCAD_DATA environment variable [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    } else {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_DATA environment variable\n");
	bu_vls_strcat(&searched, where);
    }

    /* BRLCAD_DATA compile-time path */
    lhs = _brlcad_data();
    snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_DATA compile-time path [%s]\n", lhs);
    if (_bu_find_path(result, lhs, rhs, &searched, where)) {
	if (bu_debug & BU_DEBUG_PATHS) {
	    bu_log("Found: BRLCAD_DATA compile-time path [%s]\n", result);
	}
	bu_vls_free(&searched);
	return result;
    }

    /* bu_brlcad_root/share/brlcad/VERSION path */
    snprintf(path, (size_t)MAXPATHLEN, "share/brlcad/%s", brlcad_version());
    lhs = bu_brlcad_root(path, 1);
    if (lhs) {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT common data path  [%s]\n", path);
	if (_bu_find_path(result, lhs, rhs, &searched, where)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: BRLCAD_ROOT common data path [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    }

    /* bu_brlcad_root/share/brlcad path */
    lhs = bu_brlcad_root("share/brlcad", 1);
    if (lhs) {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT common data path  [%s]\n", lhs);
	if (_bu_find_path(result, lhs, rhs, &searched, where)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: BRLCAD_ROOT common data path [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    }

    /* bu_brlcad_root/share path */
    lhs = bu_brlcad_root("share", 1);
    if (lhs) {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT common data path  [%s]\n", lhs);
	if (_bu_find_path(result, lhs, rhs, &searched, where)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: BRLCAD_ROOT common data path [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    }

    /* bu_brlcad_root path */
    lhs = bu_brlcad_root("", 1);
    if (lhs) {
	snprintf(where, MAX_WHERE_SIZE, "\tBRLCAD_ROOT common data path  [%s]\n", lhs);
	if (_bu_find_path(result, lhs, rhs, &searched, where)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: BRLCAD_ROOT common data path [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    }

    /* current directory (running from uninstalled source distribution) */
    if (_bu_find_path(result, ".", rhs, &searched, "\tcurrent directory\n")) {
	if (bu_debug & BU_DEBUG_PATHS) {
	    bu_log("Found: current directory [%s]\n", result);
	}
	bu_vls_free(&searched);
	return result;
    }

    /* running from uninstalled source distribution, look for THIS file */
#define BPC "/src/libbu/" __FILE__
    if (bu_file_exists("." BPC)) {
	if (_bu_find_path(result, ".", rhs, NULL, NULL)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: source directory [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	} else if (_bu_find_path(result, "./src", rhs, NULL, NULL)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: source directory [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    }
    if (bu_file_exists(".." BPC)) {
	if (_bu_find_path(result, "..", rhs, NULL, NULL)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: source directory [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	} else if (_bu_find_path(result, "../src", rhs, NULL, NULL)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: source directory [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    }
    if (bu_file_exists("../.." BPC)) {
	if (_bu_find_path(result, "../..", rhs, NULL, NULL)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: source directory [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	} else if (_bu_find_path(result, "../../src", rhs, NULL, NULL)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: source directory [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    }
    if (bu_file_exists("../../.." BPC)) {
	if (_bu_find_path(result, "../../..", rhs, NULL, NULL)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: source directory [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	} else if (_bu_find_path(result, "../../../src", rhs, NULL, NULL)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: source directory [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    }
    if (bu_file_exists("../../../.." BPC)) {
	if (_bu_find_path(result, "../../../..", rhs, NULL, NULL)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: source directory [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	} else if (_bu_find_path(result, "../../../../src", rhs, NULL, NULL)) {
	    if (bu_debug & BU_DEBUG_PATHS) {
		bu_log("Found: source directory [%s]\n", result);
	    }
	    bu_vls_free(&searched);
	    return result;
	}
    }

    if (!fail_quietly) {
	_bu_data_missing(bu_vls_addr(&searched));
	if (rhs) {
	    bu_log("Unable to find '%s' within the BRL-CAD software installation.\nThis copy of BRL-CAD may not be properly installed.\n\n", rhs);
	} else {
	    bu_log("Unable to find the BRL-CAD software installation.\nThis copy of BRL-CAD may not be properly installed.\n\n");
	}
    }

    bu_vls_free(&searched);
    return NULL;
}


/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
