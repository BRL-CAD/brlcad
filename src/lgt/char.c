/*                          C H A R . C
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
/** @file char.c
 *
 * routines for displaying a string on a frame buffer.

	Authors:	Paul R. Stay
			Gary S. Moss
			Doug A. Gwyn

			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "fb.h"
#include "vmath.h"
#include "raytrace.h"

#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"
#include "./font.h"

#define BUFFSIZ 200
static int	bitx(register char *bitstring, register int posn);
static void	do_char(int c, register int xpos, register int ypos);

void
do_line(int xpos, int ypos, register char *line)
{	register int    currx;
		register int    char_count, char_id;
		register int	len = strlen( line );
	if( ffdes == NULL )
		{
		bu_log( "ERROR: do_line() called before get_Font().\n" );
		return;
		}
	currx = xpos;

	for( char_count = 0; char_count < len; char_count++ )
		{
		char_id = (int) line[char_count] & 0377;

		/* Since there is no valid space in font, skip to the right
			using the width of the digit zero.
		 */
		if( char_id == ' ' )
			{
			currx += (SWABV(dir['0'].width) + 2) / ir_aperture;
			continue;
			}

		/* locate the bitmap for the character in the file */
		if( fseek( ffdes, (long)(SWABV(dir[char_id].addr)+offset), 0 )
			== EOF
			)
			{
			bu_log( "fseek() to %ld failed.\n",
				(long)(SWABV(dir[char_id].addr) + offset)
				);
			return;
			}

		/* Read in the dimensions for the character */
		width = dir[char_id].right + dir[char_id].left;
		height = dir[char_id].up + dir[char_id].down;

		if( currx + width > fb_getwidth( fbiop ) - 1 )
			break;		/* won't fit on screen */

		do_char( char_id, currx, ypos );
		currx += (SWABV(dir[char_id].width) + 2) / ir_aperture;
    		}
	return;
	}

/*	d o _ c h a r ( )
	Outputs pixel representation of a chararcter by reading a row of a
	bitmap from the character font file.  The file pointer is assumed
	to be in the correct position.
 */
static void
do_char(int c, register int xpos, register int ypos)
{	int     	up = dir[c].up / ir_aperture;
		int		left = dir[c].left / ir_aperture;
		static char	bitbuf[BUFFSIZ][BUFFSIZ];
		static RGBpixel	pixel;
		register int    h, i, j;
		int		k, x;
	for( k = 0; k < height; k++ )
		{
		/* Read row, rounding width up to nearest byte value. */
		if( fread( bitbuf[k], width/8+(width % 8 == 0 ? 0 : 1), 1, ffdes )
			!= 1 )
			{
			bu_log( "\"%s\" (%d) read of character from font failed.\n",
				__FILE__, __LINE__
				);
			return;
			}
		}
	for( k = 0; k < height; k += ir_aperture, ypos-- )
		{
		x = xpos - left;
		for( j = 0; j < width; j += ir_aperture, x++ )
			{	register int	sum;
				fastf_t		weight;
			/* The bitx routine extracts the bit value.
				Can't just use the j-th bit because
				the bytes are backwards. */
			sum = 0;
			for( i = 0; i < ir_aperture; i++ )
				for( h = 0; h < ir_aperture; h++ )
					sum += bitx(	bitbuf[k+i],
							((j+h)&~7) + (7-((j+h)&7))
							) != 0;
			weight = (fastf_t) sum / sample_sz;
			if( fb_seek( fbiop, x, ypos + up ) == -1 )
				continue;
			if( fb_rpixel( fbiop, (unsigned char *) pixel ) == -1 )
				{
				bu_log( "\"%s\" (%d) read of pixel from <%d,%d> failed.\n",
					__FILE__, __LINE__, x, ypos
					);
				return;
				}
			pixel[RED] = pixel[RED]*(1.0-weight) + FONTCOLOR_RED*weight;
			pixel[GRN] = pixel[GRN]*(1.0-weight) + FONTCOLOR_GRN*weight;
			pixel[BLU] = pixel[BLU]*(1.0-weight) + FONTCOLOR_BLU*weight;
			if( fb_seek( fbiop, x, ypos + up ) == -1 )
				continue;
			if( fb_wpixel( fbiop, (unsigned char *) pixel ) == -1 )
				{
				bu_log( "\"%s\" (%d) write of pixel to <%d,%d> failed.\n",
					__FILE__, __LINE__, x, ypos
					);
				return;
				}
			}
		}
	return;
	}

/*	b i t x ( )
	Extract a bit field from a bit string.
 */
/*ARGSUSED*/
static int
bitx(register char *bitstring, register int posn)
{
#if defined( vax )
   	register field;

   	asm("extzv	r10,$1,(r11),r8");
	return field;
#else
	for( ; posn >= 8; posn -= 8, bitstring++ )
		;
#if defined( CANT_DO_ZERO_SHIFT )
	if( posn == 0 )
		return	(int)(*bitstring) & 1;
	else
#endif
	return	(int)(*bitstring) & (1<<posn);
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
