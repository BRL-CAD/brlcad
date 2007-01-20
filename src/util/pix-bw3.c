/*                       P I X - B W 3 . C
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
/** @file pix-bw3.c
 *
 * Converts a RGB pix file into 3 8-bit BW files.
 *  (i.e. seperates the colors)
 *
 *  Author -
 *	Phillip Dykstra
 *	13 June 1986
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
#include <unistd.h>

unsigned char	ibuf[3*1024];
unsigned char	red[1024], green[1024], blue[1024];

char *Usage = "usage: pix-bw3 redout greenout blueout < file.pix\n";

int
main(int argc, char **argv)
{
	int	i, num;
	FILE	*rfp, *bfp, *gfp;
	register unsigned char *ibufp;

	if( argc != 4 || isatty(fileno(stdin)) ) {
		fputs( Usage, stderr );
		exit( 1 );
	}

	rfp = fopen( argv[1], "w" );
	gfp = fopen( argv[2], "w" );
	bfp = fopen( argv[3], "w" );

	if( rfp == NULL || gfp == NULL || bfp == NULL ) {
		fprintf( stderr, "pix-bw3: Can't open output files\n" );
		exit( 2 );
	}

	while( (num = fread( ibuf, sizeof( char ), 3*1024, stdin )) > 0 ) {
		ibufp = &ibuf[0];
		for( i = 0; i < num/3; i++ ) {
			red[i] = *ibufp++;
			green[i] = *ibufp++;
			blue[i] = *ibufp++;
		}
		fwrite( red, sizeof( *red ), num/3, rfp );
		fwrite( green, sizeof( *green ), num/3, gfp );
		fwrite( blue, sizeof( *blue ), num/3, bfp );
	}

	return 0;
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
