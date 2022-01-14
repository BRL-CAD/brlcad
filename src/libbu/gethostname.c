/*                       G E T H O S T N A M E . C
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

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_UTSNAME_H
#  include <sys/utsname.h>
#endif
#include <string.h>

#include "bnetwork.h"
#include "bio.h"

#include "bu/log.h"
#include "bu/str.h"
#include "bu/endian.h"
#include "bu/file.h"

/* strict c89 doesn't declare gethostname() */
#if !defined(HAVE_DECL_GETHOSTNAME) && !defined(_WINSOCKAPI_)
extern int gethostname(char *, size_t);
#endif

int
bu_gethostname(char *result, size_t hostlen)
{
    char hostname[MAXPATHLEN] = {0};

#ifdef HAVE_WINSOCK_H
    /**
     * Windows requires winsock networking library be initialized
     * before the hostname can be retrieved.
     */
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	bu_log("ERROR: unable to initialize networking\n");
#endif

    /* METHOD 1: use gethostname() */
#ifdef HAVE_GETHOSTNAME
    if (BU_STR_EMPTY(hostname))
	gethostname(hostname, MAXPATHLEN);
#endif

    /* METHOD 2: use uname() */
#ifdef HAVE_UNAME
    if (BU_STR_EMPTY(hostname)) {
	struct utsname name;
	if (uname(&name) == 0) {
	    bu_strlcpy(hostname, name.nodename, MAXPATHLEN);
	}
    }
#endif

    /* METHOD 3: try procfs, typically on Linux */
    if (BU_STR_EMPTY(hostname) && bu_file_exists("/proc/sys/kernel/hostname", NULL)) {
	FILE *fp = fopen("/proc/sys/kernel/hostname", "r");
	bu_fgets(hostname, MAXPATHLEN, fp);
	fclose(fp);
    }

    /* METHOD 4: try GetComputerName, typically on Windows.  it's not
     * exactly what we're wanting, but close enough if gethostname()
     * failed for some reason.
     */
#ifdef HAVE_WINSOCK_H
    if (BU_STR_EMPTY(hostname)) {
	TCHAR buffer[MAXPATHLEN] = {0};
	if (GetComputerName(buffer, (LPDWORD)MAXPATHLEN) == 0) {
	    bu_strlcpy(hostname, buffer, hostlen);
	}
    }
#endif /* HAVE_WINSOCK_H */

    if (BU_STR_EMPTY(hostname)) {
	/* non-NULL fallback */
	bu_strlcpy(hostname, "unknown", hostlen);
    }

#ifdef HAVE_WINSOCK_H
    WSACleanup();
#endif

    if (!BU_STR_EMPTY(hostname))
	bu_strlcpy(result, hostname, hostlen);

    return (strlen(hostname) == 0) ? -1 : 0;
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
