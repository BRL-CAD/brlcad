/*
 *		P I X H I S T . C
 *
 * Display a color histogram of a pix file.
 * 0 is top of screen, 255 bottom.
 *
 *  Author -
 *	Phillip Dykstra
 *	16 June 1986
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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "fb.h"

#define	IBUFSIZE 3*1024		/* Max read size in rgb pixels */
unsigned char ibuf[IBUFSIZE];	/* Input buffer */

long	bin_r[256];		/* Histogram bins */
long	bin_g[256];
long	bin_b[256];
int	verbose = 0;

static char *Usage = "usage: pixhist [-v] [file.pix]\n";

main( argc, argv )
int argc;
char **argv;
{
	int	n, i, j;
	int	max;
	int	level;
	long	num;
	Pixel	line[512];
	register unsigned char *bp;
	FILE *fp;

	/* check for verbose flag */
	if( argc > 1 && strcmp(argv[1], "-v") == 0 ) {
		verbose++;
		argv++;
		argc--;
	}

	/* look for optional input file */
	if( argc > 1 ) {
		if( (fp = fopen(argv[1],"r")) == 0 ) {
			fprintf( stderr, "pixhist: can't open \"%s\"\n", argv[1] );
			exit( 1 );
		}
		argv++;
		argc--;
	} else
		fp = stdin;

	/* check usage */
	if( argc > 1 || isatty(fileno(fp)) ) {
		fputs( Usage, stderr );
		exit( 1 );
	}

	while( (n = fread(&ibuf[0], sizeof(*ibuf), IBUFSIZE, fp)) > 0 ) {
		num = n/3;
		bp = &ibuf[0];
		for( j = 0; j < num; j++ ) {
			bin_r[ *bp++ ]++;
			bin_g[ *bp++ ]++;
			bin_b[ *bp++ ]++;
		}
	}

	/* find max */
	max = 1;
	for( i = 0; i < 256; i++ ) {
		if( bin_r[i] > max ) max = bin_r[i];
		if( bin_g[i] > max ) max = bin_g[i];
		if( bin_b[i] > max ) max = bin_b[i];
	}

	/* Display the max? */
	printf( "Full screen = %d pixels\n", max );

	fbopen( 0, 0 );

	/* Display them */
	for( i = 0; i < 256; i++ ) {
		for( j = 0; j < 512; j++ )
			line[j].red = line[j].green = line[j].blue = 0;
		
		level = (int)((float)bin_r[i]/(float)max * 511);
		for( j = 0; j < level; j++ )
			line[j].red = 255;

		level = (int)((float)bin_g[i]/(float)max * 511);
		for( j = 0; j < level; j++ )
			line[j].green = 255;

		level = (int)((float)bin_b[i]/(float)max * 511);
		for( j = 0; j < level; j++ )
			line[j].blue = 255;

		fbwrite( 0, 2*i, line, 512 );
		fbwrite( 0, 2*i+1, line, 512 );
		if( verbose )
			printf( "%3d: %10d %10d %10d\n",
				i, bin_r[i], bin_g[i], bin_b[i] );
	}
}
