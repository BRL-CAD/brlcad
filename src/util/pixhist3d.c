/*                     P I X H I S T 3 D . C
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
/** @file pixhist3d.c
 *
 * Display R x G, R x B, B x G on a framebuffer, where the
 *  intensity is the relative frequency for that plane.
 *
 *	R		Three ortho views of this cube.
 *	| B		Front, Top, Right Side.
 *	|/
 *	+______G
 *
 *  Author -
 *	Phillip Dykstra
 *	20 June 1986
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "machine.h"
#include "fb.h"

/*
 * Smallest non-zero value we will plot.
 * i.e. all non-zero points less than this will be drawn at this
 * intensity so that they will be visible.
 */
#define	THRESH	20

FBIO	*fbp;
FILE	*fp;

long	rxb[256][256], rxg[256][256], bxg[256][256];

unsigned char ibuf[8*1024*3];

void	disp_array(long int (*v)[256], int xoff, int yoff);

static char *Usage = "usage: pixhist3d [file.pix]\n";

int
main(int argc, char **argv)
{
	int	n;

	if( argc > 1 ) {
		if( (fp = fopen(argv[1], "r")) == NULL ) {
			fprintf( stderr, "pixhist3d: can't open \"%s\"\n", argv[1] );
			fprintf( stderr, Usage );
			exit( 1 );
		}
	} else
		fp = stdin;

	if( isatty(fileno(fp)) ) {
		fprintf( stderr, Usage );
		exit( 2 );
	}

	if( (fbp = fb_open( NULL, 512, 512 )) == NULL )  {
		fprintf(stderr,"fb_open failed\n");
		exit(12);
	}

	while( (n = fread(&ibuf[0], sizeof(*ibuf), sizeof(ibuf), fp)) > 0 ) {
		register unsigned char *bp;
		register int i;

		bp = &ibuf[0];
		for( i = n/3; i > 0; i--, bp += 3 )  {
			rxb[ bp[RED] ][ bp[BLU] ]++;
			rxg[ bp[RED] ][ bp[GRN] ]++;
			bxg[ bp[BLU] ][ bp[GRN] ]++;
		}
	}

	disp_array( rxg, 0, 0 );
	disp_array( rxb, 256, 0 );
	disp_array( bxg, 0, 256 );

	fb_close( fbp );
	return 0;
}

/*
 * Display the array v[Y][X] at screen location xoff, yoff.
 */
void
disp_array(long int (*v)[256], int xoff, int yoff)
{
	register int	x, y;
	static long	max;
	static double scale;
	unsigned char	obuf[256*3];

	/* Find max value */
	max = 0;
	for( y = 0; y < 256; y++ ) {
		for( x = 0; x < 256; x++ ) {
			if( v[y][x] > max )
				max = v[y][x];
		}
	}
	scale = 255.0 / ((double)max);

	/* plot them */
	for( y = 0; y < 256; y++ ) {
		for( x = 0; x < 256; x++ ) {
			register int value;

			value = v[y][x] * scale;
			if( value < THRESH && v[y][x] != 0 )
				value = THRESH;
			obuf[x*3+RED] = obuf[x*3+GRN] = obuf[x*3+BLU] = value;
		}
		fb_write( fbp, xoff, yoff+y, obuf, 256 );
	}
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
