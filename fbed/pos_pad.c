/*
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

#include "conf.h"

#include <stdio.h>
#ifdef SYSV
#include <termio.h>
#endif

#include "machine.h"
#include "externs.h"
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
	if( (pfd = open(padfile, 2)) < 0 )
		{
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
	(void) close( pfd );
	return;
	}

int
getpos(Point *pos)
{	static char str[1024];
		int buttons = -1;
		static int nread = 0;
		register int just_read = 0;
		register char *cp;
		register char *cend;
		char *last = NULL;
	while( nread < 9 )
		{
		if(	empty( pfd )
		     ||	(just_read = read(pfd, str+nread, (sizeof str) - nread))
		     ==	0
			)
			return -1; /* no input available */
		nread += just_read;
		}
	cend = str + nread - 4;
	nread = 0;
	for( cp = str; cp < cend; cp++ )
		{
		if (!(cp[0] & P_FLAG))
			continue;
		last = cp;
		if ( (buttons = ((cp[0]&P_BUTTONS) >> 2)))
			break;
		}
	if( last == NULL )
		return buttons;	/* no position parsed */
	last++;
	pos->p_x = (int)(((long)((last[0]&P_DATA) | ((last[1]&P_DATA)<<6)
			) * (long)npoints) / PADSIZE);
	pos->p_y = npoints -
			(int)(((long)((last[2]&P_DATA) | ((last[3]&P_DATA)<<6)
			) * (long)npoints) / PADSIZE);
	return buttons;
	}
