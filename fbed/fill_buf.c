/*
	SCCS id:	@(#) fill_buf.c	2.1
	Modified: 	12/9/86 at 15:55:48
	Retrieved: 	12/26/86 at 21:54:15
	SCCS archive:	/vld/moss/src/fbed/s.fill_buf.c

	Author:		Paul R. Stay
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6640 or DSN 298-6640
*/
/* 
 * fill_buf.c - Two routines for filling the buffers used in the filtering.
 */
#if ! defined( lint )
static
char sccsTag[] = "@(#) fill_buf.c 2.1, modified 12/9/86 at 15:55:48, archive /vld/moss/src/fbed/s.fill_buf.c";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"
#include "fb.h"

#include "./std.h"
#include "./ascii.h"
#include "./font.h"
#include "./try.h"
#include "./extern.h"

/*	f i l l _ b u f ( )
	Fills in the buffer by reading a row of a bitmap from the
	character font file.  The file pointer is assumed to be in the
	correct position.
 */
void
fill_buf( wid, buf )
register int wid;
register int *buf;
	{
	char    bitrow[FONTBUFSZ];
	register int     j;

	if( ffdes == NULL )
		return;
	/* Read the row, rounding width up to nearest byte value. */
	if( (int)fread( bitrow, (wid / 8) + ((wid % 8 == 0) ? 0 : 1), 1, ffdes)
		< 1
		)
		{
		(void) fprintf( stderr, "fill_buf() read failed!\n" );
		return;
		}

	/* For each bit in the row, set the array value to 1 if it's on.
		The bitx routine extracts the bit value.  Can't just use the
		j-th bit because the bytes are backwards.
	 */
	for (j = 0; j < wid; j++)
		if (bitx (bitrow, (j & ~7) + (7 - (j & 7))))
		    buf[j + 2] = 1;
		else
		    buf[j + 2] = 0;

	/* Need two samples worth of background on either end to make the
		filtering come out right without special casing the
		filtering.
	 */
	buf[0] = buf[1] = buf[wid + 2] = buf[wid + 3] = 0;
	return;
	}

/*	c l e a r _ b u f ( )
	Just sets all the buffer values to zero (this is used to
	"read" background areas of the character needed for filtering near
	the edges of the character definition.
 */
void
clear_buf( wid, buf )
int wid;
register int *buf;
	{
	register int     i, w = wid + 4;

	/* Clear each value in the row. */
	for( i = 0; i < w; i++ )
		buf[i] = 0;
	return;
	}
