/*                          C H A R . C
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
/** @file char.c
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6651 or DSN 298-6651
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "fb.h"

#include "./std.h"
#include "./font.h"
#include "./ascii.h"
#include "./try.h"
#include "./extern.h"

#define DEBUG_STRINGS	false

/*
	char.c - routines for displaying a string on a frame buffer.
 */
extern void fudge_Pixel();
extern void fill_buf(register int wid, register int *buf), clear_buf(int wid, register int *buf);
extern void squash(register int *buf0, register int *buf1, register int *buf2, register float *ret_buf, register int n);

STATIC void do_Char(int c, int xpos, int ypos, int odd);
void menu_char(int x_adjust, int menu_wid, int odd, register unsigned char *menu_border);

void
do_line(int xpos, int ypos, register char *line, RGBpixel (*menu_border))


                       /* Menu outline color, if NULL, do filtering. */
	{	register int    currx;
		register int    char_count, char_id;
		register int len = strlen( line );
#if DEBUG_STRINGS
	fb_log( "do_line: xpos=%d ypos=%d line=\"%s\" menu_border=0x%x\n",
		xpos, ypos, line, (int) menu_border );
#endif
	if( ffdes == NULL )
		{
		fb_log( "ERROR: must read font first.\n" );
		return;
		}
	currx = xpos;

	for( char_count = 0; char_count < len; char_count++ )
		{
		char_id = (int) line[char_count] & 0377;

		/* locate the bitmap for the character in the file */
		if( fseek( ffdes, (long)(SWABV(dir[char_id].addr)+offset), 0 )
			== EOF
			)
			{
			fb_log( "fseek() to %ld failed.\n",
				(long)(SWABV(dir[char_id].addr) + offset)
				);
			return;
			}

		/* Read in the dimensions for the character */
		width = SignedChar(dir[char_id].right) +
				SignedChar(dir[char_id].left);
		height = SignedChar(dir[char_id].up) +
				SignedChar(dir[char_id].down);

#if DEBUG_STRINGS
		fb_log( "do_line: right=%d left=%d up=%d down=%d\n",
			SignedChar(dir[char_id].right),
			SignedChar(dir[char_id].left),
			SignedChar(dir[char_id].up),
			SignedChar(dir[char_id].down)
			);
		fb_log( "do_line: width=%d height=%d\n", width, height );
#endif

		if( currx + width > fb_getwidth(fbp) - 1 )
			break;		/* won't fit on screen */

		if( menu_border == (RGBpixel *)RGBPIXEL_NULL )
			do_Char( char_id, currx, ypos,
				SignedChar(dir[char_id].down)%2 );
		else
			menu_char(	currx,
					ypos,
					SignedChar(dir[char_id].down) % 2,
					(unsigned char*)menu_border
					);
		currx += SWABV(dir[char_id].width) + 2;
    		}
	return;
	}

/* Shared by do_Char() and menu_char(). */
static int filterbuf[FONTBUFSZ][FONTBUFSZ];

STATIC void
do_Char(int c, int xpos, int ypos, int odd)
{	register int    i, j;
		int base;
		int     	totwid = width;
		int     	down;
		static float	resbuf[FONTBUFSZ];
		static RGBpixel fbline[FONTBUFSZ];
#if DEBUG_STRINGS
	fb_log( "do_Char: c='%c' xpos=%d ypos=%d odd=%d\n",
		c, xpos, ypos, odd );
#endif

	/* read in character bit map, with two blank lines on each end */
	for (i = 0; i < 2; i++)
		clear_buf (totwid, filterbuf[i]);
	for (i = height + 1; i >= 2; i--)
		fill_buf (width, filterbuf[i]);
	for (i = height + 2; i < height + 4; i++)
		clear_buf (totwid, filterbuf[i]);

	(void)SignedChar( dir[c].up );
	down = SignedChar( dir[c].down );

	/* Initial base line for filtering depends on odd flag. */
	base = (odd ? 1 : 2);


	/* Produce a RGBpixel buffer from a description of the character and
	 	the read back data from the frame buffer for anti-aliasing.
	 */
	for (i = height + base; i >= base; i--)
		{
		squash(	filterbuf[i - 1],	/* filter info */
			filterbuf[i],
			filterbuf[i + 1],
			resbuf,
			totwid + 4
			);
		fb_read( fbp, xpos, ypos - down + i, (unsigned char *)fbline, totwid+3);
		for (j = 0; j < (totwid + 3) - 1; j++)
			{	register int tmp;
			/* EDITOR'S NOTE : do not rearrange this code,
				the SUN compiler can't handle more
				complex expressions. */
			tmp = fbline[j][RED] & 0377;
			fbline[j][RED] =
				(int)(paint[RED]*resbuf[j]+(1-resbuf[j])*tmp);
			fbline[j][RED] &= 0377;
			tmp = fbline[j][GRN] & 0377;
			fbline[j][GRN] =
				(int)(paint[GRN]*resbuf[j]+(1-resbuf[j])*tmp);
			fbline[j][GRN] &= 0377;
			tmp = fbline[j][BLU] & 0377;
			fbline[j][BLU] =
				(int)(paint[BLU]*resbuf[j]+(1-resbuf[j])*tmp);
			fbline[j][BLU] &= 0377;
			}
		fb_write( fbp, xpos, ypos - down + i, (unsigned char *)fbline,  totwid+3 );
		}
	return;
	}

void
menu_char(int x_adjust, int menu_wid, int odd, register unsigned char *menu_border)
{	register int    i, j, k;
		int embold = 1;
		int base;
		int totwid = width;
	/* Read in the character bit map, with two blank lines on each end. */
	for (i = 0; i < 2; i++)
		clear_buf (totwid, filterbuf[i]);
	for (i = height + 1; i >= 2; i--)
		fill_buf (width, filterbuf[i]);
	for (i = height + 2; i < height + 4; i++)
		clear_buf (totwid, filterbuf[i]);

	for (k=0; k<embold; k++)
		for (i=2; i<height+2; i++)
			for (j=totwid+1; j>=2; j--)
		  		filterbuf[i][j+1] |= filterbuf[i][j];

	/* Initial base line for filtering depends on odd flag. */
	base = (odd ? 1 : 2);

	/* Change bits in menu that correspond to character bitmap. */
	for (i = height + base, k = 0; i >= base; i--, k++)
		{	register RGBpixel *menu;
		menu = menu_addr + k * menu_wid + x_adjust;
		for (j = 0; j < (totwid + 3) - 1; j++, menu++ )
			if( filterbuf[i][j] )
				{
				COPYRGB(*menu, menu_border);
				}
		}
	return;
	}

/*	b i t x ( )
	Extract a bit field from a bit string.
 */
int
bitx(register char *bitstring, register int posn)
{
#if 0 /* Was #ifdef vax , but doesn't work on 4.3BSD */
   	register field;

   	asm("extzv	r10,$1,(r11),r8");
	return field;
#else
	for( ; posn >= 8; posn -= 8, bitstring++ )
		;
#if defined( CANT_DO_ZERO_SHIFT )
	if( posn == 0 )
		return (int)(*bitstring) & 1;
	else
#endif
	return (int)(*bitstring) & (1<<posn);
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
