/*
	SCCS id:	%Z% %M%	%I%
	Modified: 	%G% at %U% by Gary S. Moss
	Retrieved: 	%H% at %T%
	SCCS archive:	%P%

	Author:		Douglas P. Kingston III	
			Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#if ! defined( lint )
static
char	sccsTag[] = "%Z% %M% %I%, modified %G% at %U%, archive %P%";
#endif
#include <stdio.h>
#include <termio.h>
#include <fb.h>
#include "./popup.h"
#include "./extern.h"
#define	P_FLAG		0100
#define	P_DATA		0077
#define	P_BUTTONS	0074

#define	PADSIZE		2200

static	int	pfd;
static	char	*padfile = "/dev/pad";
static	int	npoints;

pad_open(n)
int	n;
	{
	if( (pfd = open(padfile, 2)) < 0 )
		{
		perror( padfile );
		return	-1;
		}
	save_Tty( pfd );
	set_HUPCL( pfd );
	set_Raw( pfd );
	npoints = n;
	return	pfd;
	}

void
pad_close()
	{
	reset_Tty( pfd );
	(void) close( pfd );
	return;
	}

getpos( pos )
Point	*pos;
	{	static char	str[1024];
		int		buttons;
		static int	nread = 0;
		register int	just_read;
		register char	*cp;
		register char	*cend;
		char	*last;
	while( nread < 9 )
		{
		if(	empty( pfd )
		     ||	(just_read = read (pfd, str+nread, (sizeof str) - nread))
		     ==	0
			)
			return	-1;
		nread += just_read;
		}
	cend = str + nread - 4;
	nread = 0;
	for( cp = str; cp < cend; cp++ )
		{
		if (!(cp[0] & P_FLAG))
			continue;
		last = cp;
		if (buttons = (cp[0]&P_BUTTONS) >> 2)
			break;
		}
	last++;
	pos->p_x = (int)(((long)((last[0]&P_DATA) | ((last[1]&P_DATA)<<6)
			) * (long)npoints) / PADSIZE);
	pos->p_y = npoints - (int)(((long)((last[2]&P_DATA) | ((last[3]&P_DATA)<<6)
			) * (long)npoints) / PADSIZE);
	return	buttons;
	}
