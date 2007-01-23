/*                        H A M W I N . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file hamwin.c
 *
 *  Apply a Hamming Window to the given samples.
 *  Precomputes the window function.
 */
#include <stdlib.h>
#include <stdio.h>	/* for stderr */
#include <math.h>	/* for double sin(), cos() */
#include "./complex.h"

int init_hamwintab( int size );

static int	_init_length = 0;	/* Internal: last initialized size */
static int	maxinitlen = 0;
static double	*hamwintab = NULL;

void
hamwin(double *data, int length)
{
	int	i;

	/* Check for window table initialization */
	if( length != _init_length ) {
		if( init_hamwintab( length ) == 0 ) {
			/* Can't do requested size */
			return;
		}
	}

	/* Do window - could use pointers here... */
	for( i = 0; i < length; i++ ) {
		data[i] *= hamwintab[i];
	}
}

/*
 * Complex Data Version.
 */
void
chamwin(COMPLEX *data, int length)
{
	int	i;

	/* Check for window table initialization */
	if( length != _init_length ) {
		if( init_hamwintab( length ) == 0 ) {
			/* Can't do requested size */
			return;
		}
	}

	/* Do window - could use pointers here... */
	for( i = 0; i < length; i++ ) {
		data[i].re *= hamwintab[i];
	}
}

/*
 *		I N I T _ H A M W I N T A B
 *
 *  Internal routine to initialize the hamming window table
 *  of a given length.
 *  Returns zero on failure.
 */
int
init_hamwintab(int size)
{
	int	i;
	double	theta;

	if( size > maxinitlen ) {
		if( hamwintab != NULL ) {
			free( hamwintab );
			maxinitlen = 0;
		}
		if( (hamwintab = (double *)malloc(size*sizeof(double))) == NULL ) {
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
		theta = TWOPI * i / (double)(size);
		hamwintab[ i ] = 0.54 - 0.46 * cos( theta );
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
