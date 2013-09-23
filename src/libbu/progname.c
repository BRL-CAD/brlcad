/*                      P R O G N A M E . C
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

#ifdef HAVE_SYS_PARAM_H /* for MAXPATHLEN */
#  include <sys/param.h>
#endif

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"

/* internal storage for bu_getprogname/bu_setprogname */
static char bu_argv0_buffer[MAXPATHLEN] = {0};
static char bu_progname[MAXPATHLEN] = {0};


HIDDEN const char *
progname_argv0(void)
{
    /* private stash */
    static const char *argv0 = NULL;

    if (bu_argv0_buffer[0] != '\0') {
	argv0 = bu_argv0_buffer;
    }

#ifdef HAVE_GETPROGNAME
    if (!argv0) {
	/* do not call bu_getgrogname() */
	argv0 = getprogname(); /* not malloc'd memory */
    }
#endif

    if (!argv0 || *argv0 == '\0') {
	argv0 = "(BRL-CAD)";
    }

    return argv0;
}


const char *
bu_argv0_full_path(void)
{
    static char buffer[MAXPATHLEN] = {0};

    const char *argv0 = progname_argv0();

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
    bu_getcwd(buffer, MAXPATHLEN);
    snprintf(buffer+strlen(buffer), MAXPATHLEN-strlen(buffer), "%c%s", BU_DIR_SEPARATOR, argv0);
    if (bu_file_exists(buffer, NULL)) {
	return buffer;
    }

    /* give up */
    snprintf(buffer, MAXPATHLEN, "%s", argv0);
    return buffer;
}


const char *
bu_getprogname(void)
{
    const char *name = NULL;
    char *tmp_basename = NULL;

#ifdef HAVE_GETPROGNAME
    name = getprogname(); /* not malloc'd memory */
#endif

    if (!name || *name == '\0') {
	name = progname_argv0();
    }

    tmp_basename = bu_basename(name);
    if (!BU_STR_EQUAL(tmp_basename, ".") && !BU_STR_EQUAL(tmp_basename, "/")) {
	bu_strlcpy(bu_progname, tmp_basename, MAXPATHLEN);
	bu_free(tmp_basename, "tmp_basename free");
    } else {
	bu_strlcpy(bu_progname, name, MAXPATHLEN);
    }

    return bu_progname;
}


void
bu_setprogname(const char *argv0)
{
#ifdef HAVE_SETPROGNAME
    setprogname(argv0 ? argv0 : "");
#endif

    if (argv0) {
	memset(&bu_progname[0], '\0', sizeof(bu_progname));
	snprintf(bu_progname, MAXPATHLEN, "%s", argv0);
	if (strlen(bu_argv0_buffer) == 0)
	    snprintf(bu_argv0_buffer, MAXPATHLEN, "%s", argv0);
    } else {
	bu_progname[0] = '\0';
    }

    return;
}


/* DEPRECATED: Do not use. */
const char *
bu_argv0(void)
{
    return progname_argv0();
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
