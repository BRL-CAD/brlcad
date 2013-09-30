/*                       G E T H O S T N A M E . C
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

#ifdef HAVE_WINSOCK_H
#  include <winsock.h>
#endif

#include "bio.h"
#include "bin.h"

#include "bu.h"


int
bu_gethostname(char *hostname, size_t hostlen)
{
    int status;

#ifdef HAVE_WINSOCK_H
    /**
     * Windows requires winsock networking library be initialized
     * before the hostname can be retrieved.
     */
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	bu_log("ERROR: unable to initialize networking\n");
#endif

#ifdef HAVE_GETHOSTNAME
    status = gethostname(hostname, hostlen);
#else

    /* gethostname is POSIX but not a C99.  fallback to nothing. */
    bu_strlcpy(hostname, "unknown", hostlen)
    status = 0; /* no error */

#endif

#ifdef HAVE_WINSOCK_H
    WSACleanup();
#endif

    return status;
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
