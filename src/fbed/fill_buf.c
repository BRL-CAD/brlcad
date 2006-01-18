/*                      F I L L _ B U F . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file fill_buf.c
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
static const char RCSid[] = "@(#) fill_buf.c 2.1, modified 12/9/86 at 15:55:48, archive /vld/moss/src/fbed/s.fill_buf.c";
#endif

#include "common.h"



#include <stdio.h>

#include "machine.h"
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
fill_buf(register int wid, register int *buf)
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
clear_buf(int wid, register int *buf)
{
	register int     i, w = wid + 4;

	/* Clear each value in the row. */
	for( i = 0; i < w; i++ )
		buf[i] = 0;
	return;
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
