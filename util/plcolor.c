/*
 *			P L C O L O R . C
 *
 *  Output a color in UNIX plot format, for inclusion in a plot file.
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
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

static char usage[] = "Usage: plcolor r g b\n";

main( argc, argv )
int argc; char **argv;
{
	int	c;
	int	r, g, b;

	if( argc != 4 || isatty(fileno(stdout)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	if( !isatty(fileno(stdin)) ) {
		/* Permit use in a pipeline -- copy input to output first */
		while( (c = getchar()) != EOF )
			putchar( c );
	}

	r = atoi( argv[1] );
	g = atoi( argv[2] );
	b = atoi( argv[3] );

	pl_color( stdout, r, g, b );
}
