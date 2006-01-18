/*                       F B - O R L E . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2006 United States Government as represented by
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
 *
 */
/** @file fb-orle.c
 *
 *  Encode a frame buffer image using the old RLE library
 *
 *  Author -
 *	Gary S. Moss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Id$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "fb.h"
#include "orle.h"

static char	*usage[] = {
"Usage: fb-rle [-CScdhvw] [-l X Y] [-F Frame_buffer] [-p X Y] [file.rle]",
"",
"If no RLE file is specifed, fb-rle will write to its standard output.",
"If the environment variable FB_FILE is set, its value will be used",
"	to specify the framebuffer file or device to read from.",
0
};

static FBIO	*fbp;
static FILE	*fp;
static RGBpixel	bgpixel;
static int	bgflag = 1;
static int	ncolors = 3;
static int	cmflag = 1;
static int	crunch = 0;
static int	xpos = 0, ypos = 0, xlen = 0, ylen = 0;
static int	parsArgv(int argc, register char **argv);
static void	prntUsage(void);
static int	width = 512;
static char	*fb_file = (char *)NULL;

extern void	cmap_crunch(register RGBpixel (*scan_buf), register int pixel_ct, ColorMap *cmap);

/*	m a i n ( )							*/
int
main(int argc, char **argv)
{
	static RGBpixel	scan_buf[1024];
	static ColorMap	cmap;
	register int	y;
	register int	y_end;

	if( ! parsArgv( argc, argv ) )
	{
		prntUsage();
		return	1;
	}
	fp = stdout;
	setbuf( fp, malloc( BUFSIZ ) );

	rle_wlen( xlen, ylen, 1 );
	rle_wpos( xpos, ypos, 1 );

	if( (fbp = fb_open( fb_file, width, width )) == NULL )  {
		fprintf(stderr,"fb_open failed\n");
		exit(12);
	}

	/* Read color map, see if it's linear */
	cmflag = 1;		/* Need to save colormap */
	if( fb_rmap( fbp, &cmap ) == -1 )
		cmflag = 0;
	if( cmflag && fb_is_linear_cmap( &cmap ) )
		cmflag = 0;
	if( crunch && (cmflag == 0) )
		crunch = 0;

	/* Acquire "background" pixel from special location */
	if(	bgflag
	    &&	fb_read( fbp, 1, 1, bgpixel, 1 ) == -1
	    )
	{
		(void) fprintf( stderr, "Couldn't read background!\n" );
		return	1;
	}
	if( bgflag && rle_verbose )
		(void) fprintf( stderr,
		"Background saved as %d %d %d\n",
		bgpixel[RED], bgpixel[GRN], bgpixel[BLU]
		    );

	/* Write RLE header */
	if( rle_whdr( fp, ncolors, bgflag, cmflag, bgpixel ) == -1 )
		return	1;

	/* Follow RLE header with colormap */
	if( cmflag )  {
		if( rle_wmap( fp, &cmap ) == -1 )
			return	1;
		if( rle_debug )
			(void) fprintf( stderr,
			"Color map saved.\n"
			    );
	}

	if( ncolors == 0 )
		/* Only save colormap, so we are finished.		*/
		return	0;

	/* Get image from framebuffer and encode it */
	y_end = ypos + ylen;

	for( y = ypos; y < y_end; y++ )  {
		if(rle_debug)fprintf(stderr,"line %d\n", y);
		if( fb_read( fbp, xpos, y, (unsigned char *)scan_buf, xlen ) == -1)  {
			(void) fprintf(	stderr,
				"read of %d pixels from (%d,%d) failed!\n",
				xlen, xpos, y );
				return	1;
		}
		if( crunch )
			cmap_crunch( scan_buf, xlen, &cmap );

		if( rle_encode_ln( fp, scan_buf ) == -1 )
			return	1;
	}
	fb_close( fbp );
	return	0;
}

/*	p a r s A r g v ( )						*/
static int
parsArgv(int argc, register char **argv)
{
	register int	c;
	extern int	bu_optind;
	extern char	*bu_optarg;

	/* Parse options.						*/
	while( (c = bu_getopt( argc, argv, "CF:Scdhl:p:vw" )) != EOF )
	{
		switch( c )
		{
		case 'C' : /* Crunch color map.				*/
			crunch = 1;
			cmflag = 0;
			break;
		case 'S' : /* 'Box' save, store entire image.		*/
			bgflag = 0;
			break;
		case 'c' : /* Only save color map.			*/
			ncolors = 0;
			break;
		case 'd' : /* For debugging.				*/
			rle_debug = 1;
			break;
		case 'h' : /* High resolution.				*/
			width = 1024;
			break;
		case 'l' : /* Length in x and y.			*/
			if( argc - bu_optind < 1 )
			{
				(void) fprintf( stderr,
				"-l option requires an X and Y argument!\n"
				    );
				return	0;
			}
			xlen = atoi( bu_optarg );
			ylen = atoi( argv[bu_optind++] );
			break;
		case 'p' : /* Position of bottom-left corner.		*/
			if( argc - bu_optind < 1 )
			{
				(void) fprintf( stderr,
				"-p option requires an X and Y argument!\n"
				    );
				return	0;
			}
			xpos = atoi( bu_optarg );
			ypos = atoi( argv[bu_optind++] );
			break;
		case 'v' : /* Verbose on.				*/
			rle_verbose = 1;
			break;
		case 'w' : /* Monochrome (black & white) mode.		*/
			ncolors = 1;
			break;
		case 'F' : fb_file = bu_optarg;
			break;
		case '?' :
			return	0;
		}
	}
	if( argv[bu_optind] != NULL )
	{
		if( access( argv[bu_optind], 0 ) == 0 )
		{
			(void) fprintf( stderr,
			"\"%s\" already exists.\n",
			argv[bu_optind]
			    );
			exit( 1 );
		}
		if( (fp = fopen( argv[bu_optind], "w" )) == NULL )
		{
			perror(argv[bu_optind]);
			return	0;
		}
	}
	if( argc > ++bu_optind )
	{
		(void) fprintf( stderr, "Too many arguments!\n" );
		return	0;
	}
	if( isatty(fileno(fp)) )
		return 0;
	if( xlen == 0 )
		xlen = width;
	if( ylen == 0 )
		ylen = width;
	return	1;
}

/*	p r n t U s a g e ( )
	Print usage message.
 */
static void
prntUsage(void)
{
	register char	**p = usage;

	while( *p )
	{
		(void) fprintf( stderr, "%s\n", *p++ );
	}
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
