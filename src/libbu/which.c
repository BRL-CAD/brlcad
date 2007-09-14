/*                         W H I C H . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2007 United States Government as represented by
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
/** @file which.c
 *
 * Routine to provide BSD "which" functionality, locating binaries of
 * specified programs from the user's PATH. This is useful to locate
 * binaries and resources at run-time.
 *
 * Author -
 *   Christopher Sean Morrison
 *
 * Source -
 *   BRL-CAD Open Source
 */
#include "common.h"

/* system headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* common headers */
#include "bu.h"


/* how big should PATH from getenv ever be */
#define MAXPATHENV 32767

/** container for path match results */
static char bu_which_result[MAXPATHLEN] = {0};


/**
 * b u _ w h i c h
 *
 * returns the first USER path match to a given executable cmd name.
 *
 * caller should not free the result, though it will not be preserved
 * between calls either.  the caller should strdup the result if they
 * need to keep it around.
 */
const char *
bu_which(const char *cmd)
{
    static const char *gotpath = NULL;

    char PATH[MAXPATHENV];

    char *directory = NULL;
    char *position = NULL;

    if (bu_debug & BU_DEBUG_PATHS) {
	bu_log("bu_which: [%s]\n", cmd);
    }

    if (!cmd || (strlen(cmd) == 0)) {
	return 0;
    }

    /* start fresh */
    memset(PATH, 0, MAXPATHENV);
    memset(bu_which_result, 0, MAXPATHLEN);

    /* check for full/relative path match */
    strncpy(bu_which_result, cmd, MAXPATHLEN);
    if (strcmp(bu_which_result, cmd) != 0) {
	if (bu_debug & BU_DEBUG_PATHS) {
	    bu_log("command [%s] is too long\n", cmd);
	}
	return NULL;
    }

    if (bu_file_exists(bu_which_result) && strchr(bu_which_result, BU_DIR_SEPARATOR)) {
	return bu_which_result;
    }

    /* load up the PATH from the caller's user environment */
    gotpath = getenv("PATH");
    if (gotpath) {
	strncpy(PATH, gotpath, MAXPATHENV);

	/* make sure it fit, we have a problem if it did not */
	if (strcmp(PATH, gotpath) != 0) {
	    position = strrchr(PATH, BU_PATH_SEPARATOR);
	    if (position) {
		position = '\0';
	    } else {
		/* too much and no separator? wtf. */
		if (bu_debug & BU_DEBUG_PATHS) {
		    bu_log("path contains invalid data?\n");
		}
		return NULL;
	    }
	}

	if (bu_debug & BU_DEBUG_PATHS) {
	    bu_log("PATH is %s\n", PATH);
	}
    } else {
	if (bu_debug & BU_DEBUG_PATHS) {
	    bu_log("PATH is NULL\n");
	}
	return NULL;
    }

    /* search for the executable */
    directory = PATH;
    do {
	position = strchr(directory, BU_PATH_SEPARATOR);
	if (position) {
	    *position = '\0';
	}

	/* empty means use current dir */
	if (strlen(directory) == 0) {
	    directory = ".";
	}

	snprintf(bu_which_result, MAXPATHLEN, "%s/%s", directory, cmd);
	if (bu_file_exists(bu_which_result)) {
	    return bu_which_result;
	}

	if (position) {
	    directory = position + 1;
	} else {
	    directory = NULL;
	}
    } while (directory); /* iterate over PATH directories */

    /* no path or no match */
    if (bu_debug & BU_DEBUG_PATHS) {
	bu_log("no %s in %s\n", cmd, gotpath ? gotpath : "(no path)");
    }

    return NULL;
}
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
