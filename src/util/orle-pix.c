/*                      O R L E - P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
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
/** @file orle-pix.c
 *			R L E - P I X . C
 *
 *  Author -
 *	Gary S. Moss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Id$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <unistd.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "fb.h"		/* For Pixel typedef */
#include "orle.h"
#include "bu.h"

static char	*usage[] =
	{
"Usage: rle-pix [-dv] [-b (rgbBG)] [file.rle]",
"",
"If no rle file is specifed, rle-pix will read its standard input.",
"Pix(5B) format is written to the standard output.",
0
	};

static FILE	*fp;
static RGBpixel	bgpixel;
static int	bgflag = 0;
static int	pars_Argv(int argc, register char **argv);
static int	xpos, ypos;
static int	xlen = -1, ylen = -1;
static void	prnt_Cmap(ColorMap *cmap);
static void	prnt_Usage(void);
static int	non_linear_cmap = 0;

/*	m a i n ( )							*/
int
main(int argc, char **argv)
{
	register int	scan_ln;
	register int	fb_size = 512;
	static RGBpixel	scanbuf[1025];
	static RGBpixel	bg_scan[1025];
	static ColorMap	cmap;
	int		get_flags;
	int		scan_bytes;

	fp = stdin;
	if( ! pars_Argv( argc, argv ) || isatty(fileno(stdout)) )  {
		prnt_Usage();
		return	1;
	}
	if( rle_rhdr( fp, &get_flags, bgflag ? NULL : bgpixel ) == -1 )
		return	1;

	rle_rlen( &xlen, &ylen );
	rle_rpos( &xpos, &ypos );

	/* Automatic selection of high res. device.			*/
	if( xpos + xlen > 512 || ypos + ylen > 512 )
		fb_size = 1024;
	if( xpos + xlen > fb_size )
		xlen = fb_size - xpos;
	if( ypos + ylen > fb_size )
		ylen = fb_size - ypos;
	rle_wlen( xlen, ylen, 0 );

	scan_bytes = fb_size * sizeof(RGBpixel);

	if( rle_verbose )
		(void) fprintf( stderr,
		"Background is %d %d %d\n",
		bgpixel[RED], bgpixel[GRN], bgpixel[BLU]
		    );

	/* If color map provided, use it, else go with standard map. */
	if( ! (get_flags & NO_COLORMAP) )  {
		if( rle_verbose )
			(void) fprintf( stderr,
			"Reading color map from file\n"
			    );
		if( rle_rmap( fp, &cmap ) == -1 )
			return	1;
		if( rle_verbose )
			prnt_Cmap( &cmap );
		if( fb_is_linear_cmap( &cmap ) )
			non_linear_cmap = 0;
		else
			non_linear_cmap = 1;
	}  else  {
		/* Standard linear colormap */
		non_linear_cmap = 0;
	}
	if( rle_verbose )  (void)fprintf(stderr, "Using %s colormap\n",
		non_linear_cmap ? "stored" : "linear" );

	/* Fill buffer with background.	*/
	if( (get_flags & NO_BOX_SAVE) )  {
		register int	i;
		register RGBpixel	*to;

		to = bg_scan;
		for( i = 0; i < fb_size; i++,to++ )  {
			COPYRGB( *to, bgpixel );
		}
	}

	{
		for( scan_ln = fb_size-1; scan_ln >= 0; scan_ln-- )  {
			static int	touched = 1;
			register int	pix;
			if( touched && (get_flags & NO_BOX_SAVE) )  {
				bcopy( (char *)bg_scan, (char *)scanbuf, scan_bytes );
			}
			if( (touched = rle_decode_ln( fp, scanbuf )) == -1 )
				return	1;
			if( non_linear_cmap )  {
				for( pix = 0; pix < fb_size; pix++ )  {
					(void) putchar( cmap.cm_red[scanbuf[pix][RED]]>>8 );
					(void) putchar( cmap.cm_green[scanbuf[pix][GRN]]>>8 );
					(void) putchar( cmap.cm_blue[scanbuf[pix][BLU]]>>8 );
				}
			} else {
				/* .pix files are streams of RGBpixels */
				write( 1, scanbuf, fb_size*sizeof(RGBpixel) );
			}
		} /* end for */
	} /* end block */
	return	0;
}

/*	p a r s _ A r g v ( )						*/
static int
pars_Argv(int argc, register char **argv)
{	register int	c;
		extern int	bu_optind;
		extern char	*bu_optarg;
	/* Parse options.						*/
	while( (c = bu_getopt( argc, argv, "b:dv" )) != EOF )
		{
		switch( c )
			{
		case 'b' : /* User-specified background.		*/
			bgflag = bu_optarg[0];
			switch( bgflag )
				{
			case 'r':
				bgpixel[RED] = 255;
				break;
			case 'g':
				bgpixel[GRN] = 255;
				break;
			case 'b':
				bgpixel[BLU] = 255;
				break;
			case 'w':
				bgpixel[RED] =
				bgpixel[GRN] =
				bgpixel[BLU] = 255;
				break;
			case 'B':		/* Black */
				break;
			case 'G':		/* 18% grey, for alignments */
				bgpixel[RED] =
				bgpixel[GRN] =
				bgpixel[BLU] = 255.0 * 0.18;
				break;
			default:
				(void) fprintf( stderr,
						"Background '%c' unknown\n",
						bgflag
						);
				bgflag = 0;
				break;
				} /* End switch */
			break;
		case 'd' :
			rle_debug = 1;
			break;
		case 'v' :
			rle_verbose = 1;
			break;
		case '?' :
			return	0;
			} /* end switch */
		} /* end while */

	if( argv[bu_optind] != NULL )
		if( (fp = fopen( argv[bu_optind], "r" )) == NULL )
			{
			(void) fprintf( stderr,
					"Can't open %s for reading!\n",
					argv[bu_optind]
					);
			return	0;
			}
	if( argc > ++bu_optind )
		{
		(void) fprintf( stderr, "Too many arguments!\n" );
		return	0;
		}
	return	1;
	}

/*	p r n t _ U s a g e ( )
	Print usage message.
 */
static void
prnt_Usage(void)
{	register char	**p = usage;
	while( *p )
		(void) fprintf( stderr, "%s\n", *p++ );
	return;
	}

static void
prnt_Cmap(ColorMap *cmap)
{	register unsigned short	*cp;
		register int	i;
	(void) fprintf( stderr, "\t\t\t_________ Color map __________\n" );
	(void) fprintf( stderr, "Red segment :\n" );
	for( i = 0, cp = cmap->cm_red; i < 16; ++i, cp += 16 )
		{
		(void) fprintf( stderr,
	"%4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x\n",
	/* 1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16 */
				cp[0],
				cp[1],
				cp[2],
				cp[3],
				cp[4],
				cp[5],
				cp[6],
				cp[7],
				cp[8],
				cp[9],
				cp[10],
				cp[11],
				cp[12],
				cp[13],
				cp[14],
				cp[15]
				);
		}
	(void) fprintf( stderr, "Green segment :\n" );
	for( i = 0, cp = cmap->cm_green; i < 16; ++i, cp += 16 )
		{
		(void) fprintf( stderr,
	"%4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x\n",
	/* 1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16 */
				cp[0],
				cp[1],
				cp[2],
				cp[3],
				cp[4],
				cp[5],
				cp[6],
				cp[7],
				cp[8],
				cp[9],
				cp[10],
				cp[11],
				cp[12],
				cp[13],
				cp[14],
				cp[15]
				);
		}
	(void) fprintf( stderr, "Blue segment :\n" );
	for( i = 0, cp = cmap->cm_blue; i < 16; ++i, cp += 16 )
		{
		(void) fprintf( stderr,
	"%4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x\n",
	/* 1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16 */
				cp[0],
				cp[1],
				cp[2],
				cp[3],
				cp[4],
				cp[5],
				cp[6],
				cp[7],
				cp[8],
				cp[9],
				cp[10],
				cp[11],
				cp[12],
				cp[13],
				cp[14],
				cp[15]
				);
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
