/*                         E M P T Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file empty.c
 *
 */

#include "common.h"

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#include "bio.h"

#include "fb.h"

/*	e m p t y ( )
	Examine file descriptor for input with no time delay.
	Return 1 if input is pending on file descriptor 'fd'.
	Return 0 if no input or error.
*/
int
empty(int fd)
{
#if 0
    if ( isSGI )
	return sgi_Empty();
    else
#endif
#if defined( sgi )
    {
	static struct timeval	timeout = { 0L, 600L };
	fd_set		readfds;
	int nfound;
	FD_ZERO( &readfds );
	FD_SET( fd, &readfds );
	nfound = select( fd+1, &readfds, (fd_set *)0, (fd_set *)0, &timeout );
	return nfound == -1 ? 1 : (nfound == 0);
    }
#else
    /* On most machines we aren't supporting the mouse, so no need to
       not block on keyboard input. */
    fd = fd; /* quell warning */
    return 0;
#endif
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
