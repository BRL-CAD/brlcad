/*                         D A M D F . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file damdf.c
 *
 *  Average Magnitude Difference Function
 *  (Experimental: for pitch extraction)
 */
#include "common.h"



#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include "machine.h"

#define	BSIZE	2048		/* Must be AT LEAST 2*Points in spectrum */
double	data[BSIZE];		/* Input buffer */
double	r[BSIZE];

static char usage[] = "\
Usage: damdf [window_size (512)] < doubles\n";

int main(int argc, char **argv)
{
	int	i, j, n, L;
	register double *dp1, *dp2;
	register double	d;

	if( isatty(fileno(stdin)) || isatty(fileno(stdout)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	L = (argc > 1) ? atoi(argv[1]) : 512;

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
				d = *dp1++ - *dp2++;
				if( d > 0 )
					r[i] += d;
				else
					r[i] -= d;
			}
		}
		for( i = 0; i < L; i++ ) {
			r[i] = 1024 - r[i];
		}
		fwrite( r, sizeof(*r), L, stdout );
	}

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
