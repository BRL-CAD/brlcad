/*                       P L L I N E 2 . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file plline2.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <stdlib.h> /* for atof() */
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "plot3.h"


static char usage[] = "Usage: plline2 x1 y1 x2 y2 [r g b]\n";

int
main(int argc, char **argv)
{
	int	c;
	double	x1, y1, x2, y2;
	int	r = 0;
	int 	g = 0;
	int	b = 0;

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
