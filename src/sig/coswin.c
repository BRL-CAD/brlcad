/*                        C O S W I N . C
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
/** @file coswin.c
 *
 *  Do Cosine windowing effecting p percent of the samples in
 *  the buffer.  Precomputes the window function.
 */
#include <stdlib.h>
#include <stdio.h>	/* for stderr */
#include <math.h>	/* for double sin(), cos() */
#include "./complex.h"

static int	_init_length = 0;	/* Internal: last initialized size */
static int	maxinitlen = 0;
static double	*coswintab = NULL;

int init_coswintab( int size );

void
coswin(double *data, int length, double percent)
{
	int	num, i;

	num = percent * length/2 + 0.5;

	/* Check for window table initialization */
	if( num != _init_length ) {
		if( init_coswintab( num ) == 0 ) {
			/* Can't do requested size */
			return;
		}
	}

	/* Do window - could use pointers here... */
	for( i = 0; i < num; i++ ) {
		data[i] *= coswintab[i];
		data[length-i-1] *= coswintab[i];
	}
}

/*
 * Complex Data Version.
 */
void
ccoswin(COMPLEX *data, int length, double percent)
{
	int	num, i;

	num = percent * length/2 + 0.5;

	/* Check for window table initialization */
	if( num != _init_length ) {
		if( init_coswintab( num ) == 0 ) {
			/* Can't do requested size */
			return;
		}
	}

	/* Do window - could use pointers here... */
	for( i = 0; i < num; i++ ) {
		data[i].re *= coswintab[i];
		data[length-i-1].re *= coswintab[i];
	}
}

/*
 *		I N I T _ C O S W I N T A B
 *
 * Internal routine to initialize the cosine window table for
 *  a given effect length (number of sample at each end effected).
 */
int
init_coswintab(int size)
{
	int	i;
	double	theta;

	if( size > maxinitlen ) {
		if( coswintab != NULL ) {
			free( coswintab );
			maxinitlen = 0;
		}
		if( (coswintab = (double *)malloc(size*sizeof(double))) == NULL ) {
			fprintf( stderr, "coswin: couldn't malloc space for %d elements\n", size );
			return( 0 );
		}
		maxinitlen = size;
	}

	/* Check for odd lengths? XXX */

	/*
	 * Size is okay.  Set up tables.
	 */
	for( i = 0; i < size; i++ ) {
		theta = PI * i / (double)(size);
		coswintab[ i ] = 0.5 - 0.5 * cos( theta );
	}

	/*
	 * Mark size and return success.
	 */
	_init_length = size;
	return( 1 );
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
