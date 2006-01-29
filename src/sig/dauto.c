/*                         D A U T O . C
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
/** @file dauto.c
 *
 *  Compute the autocorrelation function of doubles.
 *  Given L elements at a time, data[0..L-1], we estimate
 *  the autocorrelation at lag 0, trough lag L-1, r[0..L-1].
 *  The first value is based on L samples, the last on only one.
 *  Zeros are assumed outside of the range of an input record.
 */
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"

double	*data;			/* Input buffer */
double	*r;			/* autocor output */
double	*weight;		/* weights to unbias estimation */

static char usage[] = "\
Usage: dauto [window_size (512)] < doubles\n";

int main(int argc, char **argv)
{
	int	i, j, n, L;
	register double *dp1, *dp2;

	if( isatty(fileno(stdin)) || isatty(fileno(stdout)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	L = (argc > 1) ? atoi(argv[1]) : 512;
	data = (double *)bu_calloc( L, sizeof(double), "data" );
	r = (double *)bu_calloc( L, sizeof(double), "r" );
	weight = (double *)bu_calloc( L, sizeof(double), "weight" );

	for( i = 0; i < L; i++ ) {
		weight[i] = 1.0 / (double)(L-i);
	}

	while( !feof( stdin ) ) {
		n = fread( data, sizeof(*data), L, stdin );
		if( n <= 0 )
			break;
		if( n < L )
			bzero( (char *)&data[n], (L-n)*sizeof(*data) );

		for( i = 0; i < L; i++ ) {
			r[i] = 0;
			dp1 = &data[0];
			dp2 = &data[i];
			for( j = L-i; j > 0; j-- ) {
				r[i] += *dp1++ * *dp2++;
			}
		}

		/* unbias the estimation */
		for( i = 0; i < L; i++ ) {
			r[i] *= weight[i];
		}

		fwrite( r, sizeof(*r), L, stdout );
	}

	bu_free(data, "data");
	bu_free(r, "r");
	bu_free(weight, "weight");

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
