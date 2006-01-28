/*                           F - I . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file f-i.c
 *
 *  Convert floats to 16bit ints
 *
 *	% f-i [-n || scale]
 *
 *	-n will normalize the data (scale -1.0 to +1.0
 *		between -32767 and +32767).
 *
 *  Phil Dykstra - 5 Nov 85.
 */
#include "common.h"

#include <stdlib.h> /* for atof() */
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"


float	ibuf[512];
short	obuf[512];

static char usage[] = "\
Usage: f-i [-n || scale] < floats > shorts\n";

int main(int argc, char **argv)
{
	int	i, num;
	double	scale;
	double	value;
	int	clip_high, clip_low;

	scale = 1.0;

	if( argc > 1 ) {
		if( strcmp( argv[1], "-n" ) == 0 )
			scale = 32767.0;
		else
			scale = atof( argv[1] );
		argc--;
	}

	if( argc > 1 || scale == 0 || isatty(fileno(stdin)) ) {
		fputs( usage, stderr );
		exit( 1 );
	}

	clip_high = clip_low = 0;

	while( (num = fread( &ibuf[0], sizeof( ibuf[0] ), 512, stdin)) > 0 ) {
		for( i = 0; i < num; i++ ) {
			value = ibuf[i] * scale;
			if( value > 32767.0 ) {
				obuf[i] = 32767;
				clip_high++;
			} else if( value < -32768.0 ) {
				obuf[i] = -32768;
				clip_low++;
			} else
				obuf[i] = (int)value;
		}

		fwrite( &obuf[0], sizeof( obuf[0] ), num, stdout );
	}

	if( clip_low != 0 || clip_high != 0 )
		fprintf( stderr, "Warning: Clipped %d high, %d low\n",
			clip_high, clip_low );

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
