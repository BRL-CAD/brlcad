/*
 *		P I X - B W 3 . C
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
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

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
main( argc, argv )
int argc;
char **argv;
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
