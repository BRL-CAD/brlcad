/*
 *			P L L I N E 2 . C
 *
 *  Output a 2-D line with double coordinates in UNIX plot format.
 *
 *  Author -
 *	Phil Dykstra
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

#include "conf.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "externs.h"			/* For atof and atoi */


static char usage[] = "Usage: plline2 x1 y1 x2 y2 [r g b]\n";

main( argc, argv )
int argc; char **argv;
{
	int	c;
	double	x1, y1, x2, y2;
	int	r, g, b;

	if( argc < 5 || isatty(fileno(stdout)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	if( !isatty(fileno(stdin)) ) {
		/* Permit use in a pipeline -- copy input to output first */
		while( (c = getchar()) != EOF )
			putchar( c );
	}

	x1 = atof( argv[1] );
	y1 = atof( argv[2] );
	x2 = atof( argv[3] );
	y2 = atof( argv[4] );

	if( argc > 5 )
		r = atoi( argv[5] );
	if( argc > 6 )
		g = atoi( argv[6] );
	if( argc > 7 )
		b = atoi( argv[7] );

	if( argc > 5 )
		pl_color( stdout, r, g, b );

	pd_line( stdout, x1, y1, x2, y2 );
}
