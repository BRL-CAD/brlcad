/*
 *			B W H I S T . C
 *
 * Display, and optionally dump to tty, a histogram of a
 * black and white file.  Black is top of screen, white bottom.
 *
 *  Author -
 *	Phillip Dykstra
 *	18 June 1986
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
#include <ctype.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "externs.h"
#include "fb.h"

long	bin[256];
int	verbose = 0;
FBIO	*fbp;

static char *Usage = "usage: bwhist [-v] [file.bw]\n";

int
main(int argc, char **argv)
{
	register int i;
	int	n;
	register long	max;
	static double scale;
	unsigned char buf[512];
	register unsigned char *bp;
	unsigned char white[3*512];
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
			fprintf( stderr, "bwhist: can't open '%s'\n", argv[1] );
			exit(1);
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

	for( i = 0; i < 3*512; i++ )
		white[i] = 255;

	while( (n = fread(buf, sizeof(*buf), sizeof(buf), fp)) > 0 ) {
		bp = &buf[0];
		for( i = 0; i < n; i++ )
			bin[ *bp++ ]++;
	}

	/* find max */
	max = 1;
	for( i = 0; i < 256; i++ )
		if( bin[i] > max ) max = bin[i];
	scale = 511.0 / (double)max;

	/* Display the max? */
	printf( "Full screen = %ld pixels\n", max );

	if( (fbp = fb_open( NULL, 512, 512 )) == NULL )  {
		fprintf(stderr,"fb_open failed\n");
		exit(12);
	}

	/* Display them */
	for( i = 0; i < 256; i++ ) {
		register int	value;
		value = bin[i]*scale;
		if( value == 0 && bin[i] != 0 ) value = 1;
		fb_write( fbp, 0, 2*i, white, value );
		fb_write( fbp, 0, 2*i+1, white, value );
		if( verbose )
			printf( "%3d: %10ld (%10f)\n", i, bin[i], (float)bin[i]/(float)max );
	}
	fb_close( fbp );
	exit(0);
}
