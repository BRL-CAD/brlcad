/*                       P O S _ P A D . C
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
/** @file pos_pad.c
	SCCS id:	@(#) pos_pad.c	2.1
	Modified: 	12/9/86 at 15:54:52 by Gary S. Moss
	Retrieved: 	12/26/86 at 21:54:35
	SCCS archive:	/vld/moss/src/fbed/s.pos_pad.c

	Author:		Douglas P. Kingston III
			Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#if ! defined( lint )
static const char RCSid[] = "@(#) pos_pad.c 2.1, modified 12/9/86 at 15:54:52, archive /vld/moss/src/fbed/s.pos_pad.c";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_TERMIO_H
#  include <termio.h>
#endif
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "fb.h"

#include "./std.h"
#include "./ascii.h"
#include "./font.h"
#include "./try.h"
#include "./extern.h"

#define	P_FLAG		0100
#define	P_DATA		0077
#define	P_BUTTONS	0074

#define	PADSIZE		2200

static	int pfd;
static	char *padfile = "/dev/pad";
static	int npoints;

int
pad_open(int n)
{
    if( (pfd = open(padfile, 2)) < 0 ) {
	perror( padfile );
	return -1;
    }
    save_Tty( pfd );
    set_HUPCL( pfd );
    set_Raw( pfd );
    npoints = n;
    return pfd;
}

void
pad_close(void)
{
    reset_Tty( pfd );
    (void)close( pfd );
    return;
}

int
getpos(Point *pos)
{	
    static char str[1024] = {0};
    int buttons = -1;
    static int nread = 0;
    register int just_read = 0;
    register char *cp = (char *)NULL;
    register char *cend = (char *)NULL;
    char *last = (char *)NULL;

    while( nread < 9 ) {
	if (empty(pfd)) {
	    return -1;
	}
	just_read = read(pfd, str+nread, (sizeof str) - nread);
	if (just_read == 0) {
	    /* no input available */
	    return -1;
	} else if (just_read < 0) {
	    perror("READ ERROR");
	    return -1;
	}
	nread += just_read;
    }

    cend = str + nread - 4;
    nread = 0;
    for( cp = str; cp < cend; cp++ ) {
	if (!(cp[0] & P_FLAG)) {
	    continue;
	}
	last = cp;
	if ( (buttons = ((cp[0]&P_BUTTONS) >> 2))) {
	    break;
	}
    }

    if( last == NULL ) {
	return buttons;	/* no position parsed */
    }
    last++;

    pos->p_x = (int)(((long)((last[0]&P_DATA) | ((last[1]&P_DATA)<<6)) * (long)npoints) / PADSIZE);
    pos->p_y = npoints - (int)(((long)((last[2]&P_DATA) | ((last[3]&P_DATA)<<6)) * (long)npoints) / PADSIZE);

    return buttons;
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
