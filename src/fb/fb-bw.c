/*                         F B - B W . C
 * BRL-CAD
 *
 * Copyright (C) 1986-2005 United States Government as represented by
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
/** @file fb-bw.c
 *
 * Read a Black and White image from the framebuffer and output
 *  it in 8-bit black and white form in pix order,
 *  i.e. Bottom UP, left to right.
 *
 *  Author -
 *	Phillip Dykstra
 *	15 Aug 1985
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

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "fb.h"


FBIO	*fbp;

#define LINELEN		8192

static unsigned char	inbuf[LINELEN*3];
static unsigned char	obuf[LINELEN];

int	height;
int	width;
int	inverse;
int	scr_xoff, scr_yoff;

char *framebuffer = NULL;
char *file_name;
FILE *outfp;

/* XXX -R -G -B */
char	usage[] = "\
Usage: fb-bw [-h -i] [-F framebuffer]\n\
	[-X scr_xoff] [-Y scr_yoff]\n\
	[-s squaresize] [-w width] [-n height] [file.bw]\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "hiF:X:Y:s:w:n:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			height = width = 1024;
			break;
		case 'i':
			inverse = 1;
			break;
		case 'F':
			framebuffer = bu_optarg;
			break;
		case 'X':
			scr_xoff = atoi(bu_optarg);
			break;
		case 'Y':
			scr_yoff = atoi(bu_optarg);
			break;
		case 's':
			/* square size */
			height = width = atoi(bu_optarg);
			break;
		case 'w':
			width = atoi(bu_optarg);
			break;
		case 'n':
			height = atoi(bu_optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( bu_optind >= argc ) {
		if( isatty(fileno(stdout)) )
			return(0);
		file_name = "-";
		outfp = stdout;
	} else {
		file_name = argv[bu_optind];
		if( (outfp = fopen(file_name, "w")) == NULL )  {
			(void)fprintf( stderr,
				"fb-bw: cannot open \"%s\" for writing\n",
				file_name );
			return(0);
		}
	}

	if ( argc > ++bu_optind )
		(void)fprintf( stderr, "fb-bw: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	int	x, y;
	int	xin, yin;		/* number of sceen output lines */

	height = width = 512;		/* Defaults */

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	/* Open Display Device */
	if ((fbp = fb_open(framebuffer, width, height )) == NULL ) {
		fprintf( stderr, "fb_open failed\n");
		exit( 1 );
	}

	/* determine "reasonable" behavior */
	xin = fb_getwidth(fbp) - scr_xoff;
	if( xin < 0 ) xin = 0;
	if( xin > width ) xin = width;
	yin = fb_getheight(fbp) - scr_yoff;
	if( yin < 0 ) yin = 0;
	if( yin > height ) yin = height;

	for( y = scr_yoff; y < scr_yoff + yin; y++ )  {
		if( inverse )
			fb_read( fbp, scr_xoff, fb_getheight(fbp)-1-y, inbuf, xin );
		else
			fb_read( fbp, scr_xoff, y, inbuf, xin );
		for( x = 0; x < xin; x++ ) {
			obuf[x] = (((int)inbuf[3*x+RED]) + ((int)inbuf[3*x+GRN])
				+ ((int)inbuf[3*x+BLU])) / 3;
		}
		fwrite( &obuf[0], sizeof( char ), xin, outfp );
	}

	fb_close( fbp );
	exit( 0 );
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
