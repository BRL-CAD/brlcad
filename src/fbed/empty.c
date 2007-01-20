/*                         E M P T Y . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file empty.c
	SCCS id:	@(#) empty.c	2.3
	Modified: 	1/5/87 at 16:52:54
	Retrieved: 	1/5/87 at 16:58:01
	SCCS archive:	/vld/moss/src/fbed/s.empty.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#if ! defined( lint )
static const char RCSid[] = "@(#) empty.c 2.3, modified 1/5/87 at 16:52:54, archive /vld/moss/src/fbed/s.empty.c";
#endif

#include "common.h"



#include <stdio.h>
#include <sys/time.h>

#include "machine.h"
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
	if( isSGI )
		return sgi_Empty();
	else
#endif
#if defined( sgi )
		{	static struct timeval	timeout = { 0L, 600L };
			fd_set		readfds;
			register int nfound;
		FD_ZERO( &readfds );
		FD_SET( fd, &readfds );
		nfound = select( fd+1, &readfds, (fd_set *)0, (fd_set *)0, &timeout );
		return nfound == -1 ? 1 : (nfound == 0);
		}
#else
	/* On most machines we aren't supporting the mouse, so no need to
		not block on keyboard input. */
	return 0;
#endif
	}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
