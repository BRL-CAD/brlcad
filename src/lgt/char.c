/*                          C H A R . C
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
/** @file lgt/char.c
 *
 * routines for displaying a string on a frame buffer.
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "fb.h"
#include "vmath.h"
#include "raytrace.h"

#include "../vfont/vfont.h"

#include "./hmenu.h"
#include "./lgt.h"
#include "./extern.h"

#define FONTCOLOR_RED  0.0
#define FONTCOLOR_GRN  0.0
#define FONTCOLOR_BLU  0.0

#define BUFFSIZ 200
static int	bitx(char *bitstring, int posn);
static void	do_char(int c, int xpos, int ypos);

void
do_line(int xpos, int ypos, char *line)
{
    int    currx;
    int    char_count, char_id;
    int	len = strlen( line );
    if ( font.ffdes == NULL )
    {
	bu_log( "ERROR: do_line() called before get_font().\n" );
	return;
    }
    currx = xpos;

    for ( char_count = 0; char_count < len; char_count++ )
    {
	char_id = (int) line[char_count] & 0377;

	/* Since there is no valid space in font, skip to the right
	   using the width of the digit zero.
	*/
	if ( char_id == ' ' )
	{
	    currx += (SWABV(font.dir['0'].width) + 2) / ir_aperture;
	    continue;
	}

	/* locate the bitmap for the character in the file */
	if ( fseek( font.ffdes, (long)(SWABV(font.dir[char_id].addr)+font.offset), 0 )
	     == EOF
	    )
	{
	    bu_log( "fseek() to %ld failed.\n",
		    (long)(SWABV(font.dir[char_id].addr) + font.offset)
		);
	    return;
	}

	/* Read in the dimensions for the character */
	font.width = font.dir[char_id].right + font.dir[char_id].left;
	font.height = font.dir[char_id].up + font.dir[char_id].down;

	if ( currx + font.width > fb_getwidth( fbiop ) - 1 )
	    break;		/* won't fit on screen */

	do_char( char_id, currx, ypos );
	currx += (SWABV(font.dir[char_id].width) + 2) / ir_aperture;
    }
    return;
}

/*	d o _ c h a r ( )
	Outputs pixel representation of a chararcter by reading a row of a
	bitmap from the character font file.  The file pointer is assumed
	to be in the correct position.
*/
static void
do_char(int c, int xpos, int ypos)
{
    int     	up = font.dir[c].up / ir_aperture;
    int		left = font.dir[c].left / ir_aperture;
    static char	bitbuf[BUFFSIZ][BUFFSIZ];
    static RGBpixel	pixel;
    int    h, i, j;
    int		k, x;
    for ( k = 0; k < font.height; k++ )
    {
	/* Read row, rounding width up to nearest byte value. */
	if ( fread( bitbuf[k], (size_t)font.width/8+(font.width % 8 == 0 ? 0 : 1), 1, font.ffdes )
	     != 1 )
	{
	    bu_log( "\"%s\" (%d) read of character from font failed.\n",
		    __FILE__, __LINE__
		);
	    return;
	}
    }
    for ( k = 0; k < font.height; k += ir_aperture, ypos-- )
    {
	x = xpos - left;
	for ( j = 0; j < font.width; j += ir_aperture, x++ )
	{
	    int	sum;
	    fastf_t		weight;
	    /* The bitx routine extracts the bit value.
	       Can't just use the j-th bit because
	       the bytes are backwards. */
	    sum = 0;
	    for ( i = 0; i < ir_aperture; i++ )
		for ( h = 0; h < ir_aperture; h++ )
		    sum += bitx(	bitbuf[k+i],
					((j+h)&~7) + (7-((j+h)&7))
			) != 0;
	    weight = (fastf_t) sum / sample_sz;
	    if ( fb_seek( fbiop, x, ypos + up ) == -1 )
		continue;
	    if ( fb_rpixel( fbiop, (unsigned char *) pixel ) == -1 )
	    {
		bu_log( "\"%s\" (%d) read of pixel from <%d,%d> failed.\n",
			__FILE__, __LINE__, x, ypos
		    );
		return;
	    }
	    pixel[RED] = pixel[RED]*(1.0-weight) + FONTCOLOR_RED*weight;
	    pixel[GRN] = pixel[GRN]*(1.0-weight) + FONTCOLOR_GRN*weight;
	    pixel[BLU] = pixel[BLU]*(1.0-weight) + FONTCOLOR_BLU*weight;
	    if ( fb_seek( fbiop, x, ypos + up ) == -1 )
		continue;
	    if ( fb_wpixel( fbiop, (unsigned char *) pixel ) == -1 )
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
bitx(char *bitstring, int posn)
{
#if defined( vax )
    field;

    asm("extzv	r10,$1,(r11), r8");
    return field;
#else
    for (; posn >= 8; posn -= 8, bitstring++ )
	;
#if defined( CANT_DO_ZERO_SHIFT )
    if ( posn == 0 )
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
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
