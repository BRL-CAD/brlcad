/*                       P O S _ P A D . C
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
/** @file fbed/pos_pad.c
 *
 */

#include "common.h"

#ifdef HAVE_TERMIO_H
#  include <termio.h>
#endif
#include "bio.h"

#include "fb.h"

#include "./std.h"
#include "./ascii.h"
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
    if ( (pfd = open(padfile, 2)) < 0 ) {
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
    int just_read = 0;
    char *cp = (char *)NULL;
    char *cend = (char *)NULL;
    char *last = (char *)NULL;

    while ( nread < 9 ) {
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
    for ( cp = str; cp < cend; cp++ ) {
	if (!(cp[0] & P_FLAG)) {
	    continue;
	}
	last = cp;
	if ( (buttons = ((cp[0]&P_BUTTONS) >> 2))) {
	    break;
	}
    }

    if ( last == NULL ) {
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
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
