/*                       W H E R I S . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2011 United States Government as represented by
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

/* system headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_SYSCTL_H
#  ifdef HAVE_SYS_PARAM_H
#    include <sys/param.h>
#  endif
#  include <sys/sysctl.h>
#endif

/* common headers */
#include "bu.h"


/* how big should PATH from getenv ever be */
#define MAXPATHENV 32767

/* container for path match results */
static char bu_whereis_result[MAXPATHLEN] = {0};


const char *
bu_whereis(const char *cmd)
{
    static const char *gotpath = NULL;

    char PATH[MAXPATHENV];

    char *directory = NULL;
    char *position = NULL;

    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	bu_log("bu_whereis: [%s]\n", cmd);
    }

    if (UNLIKELY(!cmd)) {
	return NULL;
    }

    /* start fresh */
    memset(PATH, 0, MAXPATHENV);
    memset(bu_whereis_result, 0, MAXPATHLEN);

    /* check for full/relative path match */
    bu_strlcpy(bu_whereis_result, cmd, MAXPATHLEN);
    if (!BU_STR_EQUAL(bu_whereis_result, cmd)) {
	if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	    bu_log("command [%s] is too long\n", cmd);
	}
	return NULL;
    }
    if (bu_file_exists(bu_whereis_result) && strchr(bu_whereis_result, BU_DIR_SEPARATOR)) {
	if (bu_whereis_result[0] == '\0')
	    return NULL; /* never return empty */
	return bu_whereis_result;
    }

#if defined(HAVE_SYSCTL) && defined(CTL_USER) && defined(USER_CS_PATH)
    {
	int mib[2] = { CTL_USER, USER_CS_PATH };
	size_t len = MAXPATHENV;

	/* use sysctl() to get the PATH */
	if (sysctl(mib, 2, PATH, &len, NULL, 0) != 0) {
	    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
		perror("sysctl of user.cs_path");
		bu_log("user.cs_path is unusable\n");
	    }
	    return NULL;
	}

	if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	    bu_log("PATH is %s\n", PATH);
	}
    }
#endif  /* HAVE_SYSCTL */

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

	snprintf(bu_whereis_result, MAXPATHLEN, "%s/%s", directory, cmd);
	if (bu_file_exists(bu_whereis_result)) {
	    if (bu_whereis_result[0] == '\0')
		return NULL; /* never return empty */
	    return bu_whereis_result;
	}

	if (position) {
	    directory = position + 1;
	} else {
	    directory = NULL;
	}
    } while (directory); /* iterate over PATH directories */

    /* no path or no match */
    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	bu_log("no %s in %s\n", cmd, gotpath ? gotpath : "(no path)");
    }

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
