/*
 *		A P - P I X . C
 *
 * Applicon color ink jet printer to .pix file converter.
 *
 *  Author -
 *	Phillip Dykstra
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986-2004 by the United States Army.
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
/* Dots are least most signifigant bit first in increasing index */
struct	app_record {
	unsigned char	ml[432];
	unsigned char	yl[432];
	unsigned char	cl[432];
} magline, yelline, cyaline;

struct	{
	char	red, green, blue;
} out;

FILE	*magfp, *yelfp, *cyafp;
int	verbose = 0;

static char *Usage = "usage: ap-pix [-v] file.ap > file.pix (3456 x ?)\n";

int
main( argc, argv )
int argc; char **argv;
{
	int	i, bit;
	int	line;

	if( argc > 1 && strcmp(argv[1],"-v") == 0 ) {
		verbose++;
		argc--;
		argv++;
	}

	if( argc != 2 ) {
		fputs( Usage, stderr );
		exit( 1 );
	}

	magfp = fopen( argv[1], "r" );
	if( magfp == NULL ) {
		fprintf( stderr, "ap-pix: can't open \"%s\"\n", argv[1] );
		exit( 2 );
	}
	yelfp = fopen( argv[1], "r" );
	fseek( yelfp, (long)(50*sizeof(yelline)), 0 );
	cyafp = fopen( argv[1], "r" );
	fseek( cyafp, (long)(100*sizeof(cyaline)), 0 );

	line = 0;
	while( (int)fread( &cyaline, sizeof( cyaline ), 1, cyafp ) > 0 ) {
		fread( &magline, sizeof( magline ), 1, magfp );
		fread( &yelline, sizeof( yelline ), 1, yelfp );
		line++;

		for( i = 0; i < 432; i++ ) {
			for( bit = 7; bit >= 0; bit-- ) {
				out.red = ((cyaline.cl[i]>>bit)&1) ? 0 : 255;
				out.green = ((magline.ml[i]>>bit)&1) ? 0 : 255;
				out.blue = ((yelline.yl[i]>>bit)&1) ? 0 : 255;
				fwrite( &out, sizeof( out ), 1, stdout );
			}
		}
		if( verbose )
			fprintf( stderr, "wrote line %d\n", line );
	}
	return 0;
}
