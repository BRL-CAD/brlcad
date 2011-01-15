/*                        I N T E R P . C
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
/** @file interp.c
 *  Do the interpolation necessary to change a set of points from
 *  linear to log scale.
 *
 *  Input, a set of linearly seperated samples.
 *  Output, the same number of samples on a log baseline.
 *
 *  This uses fourth order Lagrange Interpolation and may
 *  be too wigly for our purposes.
 *
 *  Sure this is ugly.... I'll be happy if it works at all.
 */

#include "common.h"

#include <stdio.h>
#include <math.h>


#define	MAXBUF	16*1024

void
LintoLog(double *in, double *out, int num)
{
    int	i;
    double	place, step;
    double	linpt[MAXBUF] = {0.0};
    double	x, y, x1, x2, x3, x4;

    /*
     * Compute the break points, i.e. the fractional input
     * sample number corresponding to each of our "num" output
     * samples.
     */
    step = pow( (double)num, 1.0/(double)(num-1) );
    place = 1.0;
    for ( i = 0; i < num; i++ ) {
	linpt[i] = place - 1.0;
	place *= step;
    }
#ifdef DEBUG
    for ( i = 0; i < num; i++ ) {
	printf("linpt[%d] = %f\n", i, linpt[i]);
    }
#endif /* DEBUG */

    for ( i = 0; i < num; i++ ) {
	/*
	 * Compute polynomial to interp with.
	 */
	x1 = (int)linpt[i] - 1;
	if ( x1 < 0 ) x1 = 0;
	if ( x1 > num - 4 ) x1 = num - 4;
	x2 = x1 + 1; x3 = x1 + 2; x4 = x1 + 3;

	x  = linpt[i];
	y  = in[(int)x1] * ((x-x2)*(x-x3)*(x-x4))/((x1-x2)*(x1-x3)*(x1-x4));
	y += in[(int)x2] * ((x-x1)*(x-x3)*(x-x4))/((x2-x1)*(x2-x3)*(x2-x4));
	y += in[(int)x3] * ((x-x1)*(x-x2)*(x-x4))/((x3-x1)*(x3-x2)*(x3-x4));
	y += in[(int)x4] * ((x-x1)*(x-x2)*(x-x3))/((x4-x1)*(x4-x2)*(x4-x3));
	out[i] = y;
    }
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
