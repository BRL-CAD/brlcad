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

#include "bio.h"
#include "bin.h"

#include "bu.h"


/**
 * Windows requires some initializations before gethostname() can be used.
 */
int
bu_gethostname(char *hostname, size_t hostlen)
{
    int status;

#if defined(WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

#if defined(WIN32)
    status = gethostname(hostname, hostlen);
#else
  #if defined(__STD_VERSION__) && __STD_VERSION >= 199901L
    status = gethostname(hostname, hostlen);
  #else
    /* gethostname is POSIX but not a C99.  this is fallback behavior. */
    bu_strlcpy(hostname, "unknown", hostlen)
    status = 0; /* no error */
  #endif
#endif

#if defined(WIN32)
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
