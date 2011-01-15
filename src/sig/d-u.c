/*                           D - U . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file d-u.c
 *
 *  Convert doubles to 16bit unsigned ints
 *
 *	% d-i [-n || scale]
 *
 *	-n will normalize the data (scale -1.0 to +1.0
 *		between -32767 and +32767).
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h> /* for atof() */
#include <string.h>
#include "bio.h"

#include "bu.h"

double	ibuf[512];
unsigned short	obuf[512];


int main(int argc, char **argv)
{
    int	i, num;
    double	scale;
    double	value;
    int	clip_high, clip_low;

    scale = 1.0;

    if ( argc > 1 ) {
	if ( BU_STR_EQUAL( argv[1], "-n" ) )
	    scale = 65536.0;
	else
	    scale = atof( argv[1] );
	argc--;
    }

    if ( argc > 1 || scale == 0 || isatty(fileno(stdin)) ) {
	bu_exit(1, "Usage: d-i [-n || scale] < doubles > unsigned_shorts\n");
    }

    clip_high = clip_low = 0;

    while ( (num = fread( &ibuf[0], sizeof( ibuf[0] ), 512, stdin)) > 0 ) {
	for ( i = 0; i < num; i++ ) {
	    value = ibuf[i] * scale;
	    if ( value > 65535.0 ) {
		obuf[i] = 65535;
		clip_high++;
	    } else if ( value < 0.0 ) {
		obuf[i] = 0;
		clip_low++;
	    } else
		obuf[i] = (unsigned short)value;
	}

	fwrite( &obuf[0], sizeof( obuf[0] ), num, stdout );
    }

    if ( clip_low != 0 || clip_high != 0 )
	fprintf( stderr, "Warning: Clipped %d high, %d low\n",
		 clip_high, clip_low );
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
