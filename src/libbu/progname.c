/*                      P R O G N A M E . C
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

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"

/* internal storage for bu_getprogname/bu_setprogname */
static char bu_progname[MAXPATHLEN] = {0};


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
    const char *pwd = NULL;

    /* already found the path before */
    if (ipwd) {
	return ipwd;
    }

    /* FIRST: try environment */
    pwd = getenv("PWD"); /* not our memory to free */
    if (pwd && strlen(pwd) > 0) {
#ifdef HAVE_REALPATH
	ipwd = realpath(pwd, buffer);
	if (ipwd) {
	    return ipwd;
	}
#endif
	ipwd = pwd;
	return ipwd;
    }

    /* SECOND: try to query path */
#ifdef HAVE_REALPATH
    ipwd = realpath(".", buffer);
    if (ipwd && strlen(ipwd) > 0) {
	return ipwd;
    }
#endif

    /* THIRD: try calling the 'pwd' command */
    ipwd = bu_which("pwd");
    if (ipwd) {
#if defined(HAVE_POPEN) && !defined(STRICT_FLAGS)
	FILE *fp = NULL;

	fp = popen(ipwd, "r");
	if (LIKELY(fp != NULL)) {
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
    }

    /* LAST: punt (but do not return NULL) */
    ipwd = ".";
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
    static char buffer[MAXPATHLEN] = {0};
    char *tmp_basename;

    if (bu_progname[0] != '\0') {
	tmp_basename = bu_basename(bu_progname);
	bu_strlcpy(buffer, tmp_basename, strlen(tmp_basename)+1);
	bu_free(tmp_basename, "tmp_basename free");
	return buffer;
    }

#ifdef HAVE_GETPROGNAME
    name = getprogname(); /* not malloc'd memory */
#endif

    if (!name) {
	name = _bu_argv0();
    }

    snprintf(bu_progname, MAXPATHLEN, "%s", name);

    tmp_basename = bu_basename(bu_progname);
    bu_strlcpy(buffer, tmp_basename, strlen(tmp_basename)+1);
    bu_free(tmp_basename, "tmp_basename free");
    return buffer;
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


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
