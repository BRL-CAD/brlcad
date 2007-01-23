/*                       F B C B A R S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
 *
 */
/** @file fbcbars.c
 *
 *  Program to make vertical color scales arranged horizontally on screen.
 *  The wonderful thing about standards is that there are so many to
 *  choose from:
 *	FCC/EBU		Full screen is FCC pattern
 *	EIA		Uses lower and upper patterns
 *	SMPTE		Uses three patterns
 *
 *  The lower portion of the screen will contain:
 *	-I, 100% white, Q, black
 *
 *  In SMPTE mode, the middle portion of the screen will contain:
 *	blue, black, magenta, black, cyan, black, 75% grey
 *
 *  The upper portion of the screen will contain:
 *	75% gray, yellow, cyan, green, magenta, red, blue
 *
 *  In EBU/FCC mode, the whole screen will contain:
 *	100% white, yellow, cyan, green, magenta, red, blue, black
 *
 *
 *  The reason for this program running directly to the framebuffer, rather
 *  than generating a pix(5) format file (like pixbackgnd(1)) is twofold:
 *	1)  Some framebuffers may have irregular shapes (like the SGI
 *	    3030 is "hi-res" mode.
 *	2)  This program will most often be used to place an image on
 *	    a framebuffer;  other processing of this image is likely
 *	    to be a rare event.
 *  Thus, this bundled version is more efficient, easier to use,
 *  and more likely to have the desired effect.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Original inspiration -
 *	"ikcolorbars" by Mike Pique, University of North Carolina.
 *
 *  Details on SMPTE and FCC patterns -
 *	"bars" by Doris Kochanek, National Film Board of Canada
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "fb.h"

#define MAX_LINE	(8*1024)	/* Max pixels/line */
static unsigned char scanline[3*MAX_LINE];	/* 1 scanline pixel buffer */

static char	*framebuffer = NULL;

static int	scr_width = 0;
static int	scr_height = 0;

/*
 * "Since the relationship between RGB and the amplitude of YIQ in
 * the encoded signal depends on the encoder's setup value,
 * this must be defined below.  SETUP is normally 7.5 for NTSC.
 * The full luminance range is encoded into the range between
 * the setup level and 100 IRE, so Y values are effectively scaled
 * by (1-setup) when being converted to voltages.  I and Q are
 * scaled by the same amount to retain their relationship with Y.
 * To get -I and Q signals that are 40 IRE after encoding, we must
 * scale them by 1/(1-setup) before encoding."
 *
 *  "The -I and Q signals are not quite standard, since there is
 *  necessarily some luminance present when RGB is positive.
 *  They are designed to be 40 IRE P-P after encoding."
 *
 *	-- Doris Kochanek, National Film Board of Canada
 */

#define	SETUP		7.5			/* black setup; 7.5 IRE */
#define	COMP(x)		\
	(unsigned char)(((x) * 255.) / (1.0 - SETUP/100.0))	/* setup compensation */

static  RGBpixel fcc_all[8] = {
	/* 100% white */{255, 255, 255},
	/* yellow */	{191, 191, 0},
	/* cyan */	{0, 191, 191},
	/* green */	{0, 191, 0},
	/* magenta */	{191, 0, 191},
	/* red */	{191, 0, 0},
	/* blue */	{0, 0, 191},
	/* black */	{0, 0, 0}
};

/*
 *  SMPTE bars can be useful for aligning color demodulators
 */
static	RGBpixel smpte_middle[7] = {
	/* All bars at 75%, no blue, reversed side-to-side from eia_top */
	/* blue */	{0, 0, 191},
	/* black(red)*/	{0, 0, 0},
	/* magenta */	{191, 0, 191},
	/* black(green)*/{0, 0, 0},
	/* cyan */	{0, 191, 191},
	/* black(yellow)*/{0, 0, 0},
	/* 75% grey */ 	{191, 191, 191},
};

static  RGBpixel eia_top[7] = {
	/* All bars at 75% */
	/* grey */ 	{191, 191, 191},
	/* yel */	{191, 191, 0},
	/* cyan */	{0, 191, 191},
	/* green */	{0, 191, 0},
	/* magenta */	{191, 0, 191},
	/* red */	{191, 0, 0},
	/* blue */	{0, 0, 191}
};

static RGBpixel botpart[5] = {
#ifndef Floating_Initializers
	/* Most systems can't handle floating-point formulas as initializers */
	{ 0,		68,		114 },		/* 40 IRE -I */
	{ 255,		255,		255 },		/* 100% white */
	{ 69,		0,		129 },		/* 40 IRE Q */
	{ 0,		0,		0 },		/* black */
	{ 0,		0,		0 }		/* black */
#else
	{ 0,		COMP(0.2472),	COMP(0.4123) },	/* 40 IRE -I */
	{ 255,		255,		255 },		/* 100% white */
	{ COMP(0.2508),	0,		COMP(0.4670) },	/* 40 IRE Q */
	{ 0,		0,		0 },		/* black */
	{ 0,		0,		0 }		/* black */
#endif
};

static char usage[] = "\
Usage: fbcbars [-fs] [-h] [-F framebuffer]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height]\n\
	-f	FCC/EBU bars\n\
	-e	EIA bars\n\
	-s	SMPTE bars\n";

#define	M_EIA	0
#define M_FCC	1
#define M_SMPTE	2
int	mode = M_SMPTE;

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "efshF:S:W:N:" )) != EOF )  {
		switch( c )  {
		case 'e':
			mode = M_EIA;
			break;
		case 's':
			mode = M_SMPTE;
			break;
		case 'f':
			mode = M_FCC;
			break;
		case 'h':
			/* high-res */
			scr_height = scr_width = 1024;
			break;
		case 'F':
			framebuffer = bu_optarg;
			break;
		case 'S':
			scr_height = scr_width = atoi(bu_optarg);
			break;
		case 'W':
			scr_width = atoi(bu_optarg);
			break;
		case 'N':
			scr_height = atoi(bu_optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if ( argc > ++bu_optind )
		(void)fprintf( stderr, "fbcbars: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	register int x, y;
	register FBIO *fbp;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( (fbp = fb_open( framebuffer, scr_width, scr_height )) == NULL )
		exit(12);

	/* Get the screen size we were actually given */
	scr_width = fb_getwidth(fbp);
	scr_height = fb_getheight(fbp);

	/*
	 *  Operation is bottom-to-top.
	 */
	switch(mode)  {
	case M_FCC:
		for( x=0; x<scr_width; x++) {
			COPYRGB( &scanline[3*x], fcc_all[x*8/scr_width] );
		}
		for( y=0; y<scr_height; y++)
			fb_write( fbp, 0, y, scanline, scr_width );
		break;

	case M_EIA:
		/*
		 *  Build bottom line, and send it for 1/4th of the screen,
		 *  then build the top line, and fill the rest of the screen.
		 */
		for( x=0; x<scr_width; x++) {
			COPYRGB( &scanline[3*x], botpart[x*5/scr_width] );
		}
		for( y=0; y<(scr_height/4); y++)
			fb_write( fbp, 0, y, scanline, scr_width );

		for( x=0; x<scr_width; x++)  {
			COPYRGB( &scanline[3*x], eia_top[x*7/scr_width] );
		}
		for( ; y<scr_height; y++)
			fb_write( fbp, 0, y, scanline, scr_width );
		break;

	case M_SMPTE:
		/*
		 *  Build bottom line, and send it for 3/16th of the screen,
		 *  then send the SMPTE middle for 1/16th of the screen,
		 *  then build the top line, and fill the rest of the screen.
		 *  (Convert upper 1/4 of EIA -I white Q black to smpte)
		 */
		for( x=0; x<scr_width; x++) {
			COPYRGB( &scanline[3*x], botpart[x*5/scr_width] );
		}
		for( y=0; y<(scr_height*3/16); y++)
			fb_write( fbp, 0, y, scanline, scr_width );

		for( x=0; x<scr_width; x++) {
			COPYRGB( &scanline[3*x], smpte_middle[x*7/scr_width] );
		}
		for( ; y<(scr_height*4/16); y++)
			fb_write( fbp, 0, y, scanline, scr_width );

		for( x=0; x<scr_width; x++)  {
			COPYRGB( &scanline[3*x], eia_top[x*7/scr_width] );
		}
		for( ; y<scr_height; y++)
			fb_write( fbp, 0, y, scanline, scr_width );
		break;
	}
	fb_close(fbp);
	return(0);
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
