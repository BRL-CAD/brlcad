/*                        B W - R L E . C
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
/** @file bw-rle.c
 *
 *  Encode a .bw file using the Utah Raster Toolkit RLE library
 *
 *  Author -
 *	Michael John Muuss
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

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <time.h>

#include "machine.h"
#include "bu.h"
#include "fb.h"
#include "rle.h"

static rle_hdr	outrle;
#define		outfp		outrle.rle_file
static char			comment[128];
#if HAVE_GETHOSTNAME
static char			host[128];
#endif
static rle_pixel		**rows;
static time_t			now;
static char			*who;

static FILE	*infp;
static char	*infile;

static int	background[3];

static int	file_width = 512;
static int	file_height = 512;

static char	usage[] = "\
Usage: pix-rle [-h] [-s squarefilesize]  [-C bg]\n\
	[-w file_width] [-n file_height] [file.pix] [file.rle]\n\
\n\
If omitted, the .pix file is taken from stdin\n\
and the .rle file is written to stdout\n";


/*
 *			G E T _ A R G S
 */
static int
get_args(int argc, register char **argv)
{
	register int	c;

	while( (c = bu_getopt( argc, argv, "hs:w:n:C:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			file_height = file_width = 1024;
			break;
		case 's':
			/* square file size */
			file_height = file_width = atoi(bu_optarg);
			break;
		case 'w':
			file_width = atoi(bu_optarg);
			break;
		case 'n':
			file_height = atoi(bu_optarg);
			break;
		case 'C':
			{
				register char *cp = bu_optarg;
				register int *conp = background;

				/* premature null => atoi gives zeros */
				for( c=0; c < 3; c++ )  {
					*conp++ = atoi(cp);
					while( *cp && *cp++ != '/' ) ;
				}
			}
			break;
		default:
		case '?':
			return	0;
		}
	}
	if( argv[bu_optind] != NULL )  {
		if( (infp = fopen( (infile=argv[bu_optind]), "r" )) == NULL )  {
			perror(infile);
			return	0;
		}
		bu_optind++;
	} else {
		infile = "-";
	}
	if( argv[bu_optind] != NULL )  {
		if( access( argv[bu_optind], 0 ) == 0 )  {
			(void) fprintf( stderr,
				"\"%s\" already exists.\n",
				argv[bu_optind] );
			exit( 1 );
		}
		if( (outfp = fopen( argv[bu_optind], "w" )) == NULL )  {
			perror(argv[bu_optind]);
			return	0;
		}
	}
	if( argc > ++bu_optind )
		(void) fprintf( stderr, "pix-rle: Excess arguments ignored\n" );

	if( isatty(fileno(infp)) || isatty(fileno(outfp)) )
		return 0;
	return	1;
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	register unsigned char *scan_buf;
	register int	y;

	infp = stdin;
	outfp = stdout;
	if( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}
	scan_buf = (unsigned char *)malloc( sizeof(unsigned char) * file_width );

	outrle.ncolors = 1;
	RLE_SET_BIT(outrle, RLE_RED);
	outrle.background = 2;		/* use background */
	outrle.bg_color = background;
	outrle.alpha = 0;		/* no alpha channel */
	outrle.ncmap = 0;		/* no color map */
	outrle.cmaplen = 0;
	outrle.cmap = (rle_map *)0;
	outrle.xmin = outrle.ymin = 0;
	outrle.xmax = file_width-1;
	outrle.ymax = file_height-1;
	outrle.comments = (const char **)0;

	/* Add comments to the header file, since we have one */
	sprintf( comment, "converted_from=%s", infile );
	rle_putcom( strdup(comment), &outrle );
	now = time(0);
	sprintf( comment, "converted_date=%24.24s", ctime(&now) );
	rle_putcom( strdup(comment), &outrle );
	if( (who = getenv("USER")) != (char *)0 ) {
		sprintf( comment, "converted_by=%s", who);
		rle_putcom( strdup(comment), &outrle );
	} else {
		if( (who = getenv("LOGNAME")) != (char *)0 ) {
			sprintf( comment, "converted_by=%s", who);
			rle_putcom( strdup(comment), &outrle );
		}
	}
#	if HAVE_GETHOSTNAME
	gethostname( host, sizeof(host) );
	sprintf( comment, "converted_host=%s", host);
	rle_putcom( strdup(comment), &outrle );
#	endif

	rle_put_setup( &outrle );
	rle_row_alloc( &outrle, &rows );

	/* Read image a scanline at a time, and encode it */
	for( y = 0; y < file_height; y++ )  {
		if( fread( (char *)scan_buf, sizeof(unsigned char), (size_t)file_width, infp ) != file_width)  {
			(void) fprintf(	stderr,
				"pix-rle: read of %d pixels on line %d failed!\n",
				file_width, y );
			exit(1);
		}

		/* Grumble, convert to Utah layout */
		{
			register unsigned char	*pp = scan_buf;
			register rle_pixel	*rp = rows[0];
			register int		i;

			for( i=0; i<file_width; i++ )  {
				*rp++ = *pp++;
			}
		}
		rle_putrow( rows, file_width, &outrle );
	}
	rle_puteof( &outrle );

	fclose( infp );
	fclose( outfp );
	exit(0);
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
