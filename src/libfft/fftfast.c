/*                       F F T F A S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file fftfast.c
 * Complex Number and FFT Library
 *
 * "Fast" Version - Function calls to complex math routines removed.
 *      Uses pre-computed sine/cosine tables.
 *
 *  The FFT is:
 *
 *	        N-1
 *	Xf(k) = Sum x(n)( cos(2PI(nk/N)) - isin(2PI(nk/N)) )
 *	        n=0
 *
 *  Author -
 *	Phil Dykstra, 12 Oct 84 and beyond.
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"


#include <stdlib.h>
#include <stdio.h>	/* for stderr */
#include <math.h>	/* for double sin(), cos() */

#define	MAXSIZE	65536	/* Needed for sin/cos tables */
int	_init_size = 0;	/* Internal: shows last initialized size */

#if !defined(PI)
#	define	PI	3.141592653589793238462643
#endif

/* The COMPLEX type used throughout */
typedef struct {
	double	re;	/* Real Part */
	double	im;	/* Imaginary Part */
} COMPLEX;

void	scramble(int numpoints, COMPLEX *dat);
void	butterflies(int numpoints, int inverse, COMPLEX *dat);
int	init_sintab( int size );

/*
 * Forward Complex Fourier Transform
 */
void
cfft(COMPLEX *dat, int num)
{
	/* Check for trig table initialization */
	if( num != _init_size ) {
		if( init_sintab( num ) == 0 ) {
			/* Can't do requested size */
			return;
		}
	}

	scramble( num, dat );
	butterflies( num, -1, dat );
}

/*
 * Inverse Complex Fourier Transform
 */
void
icfft(COMPLEX *dat, int num)
{
	/* Check for trig table initialization */
	if( num != _init_size ) {
		if( init_sintab( num ) == 0 ) {
			/* Can't do requested size */
			return;
		}
	}

	scramble(num, dat);
	butterflies(num, 1, dat);
}

/******************* INTERNAL FFT ROUTINES ********************/

/* The trig tables */
double	*sintab;
double	*costab;

/*
 *		I N I T _ S I N T A B
 *
 * Internal routine to initialize the sine/cosine table for
 *  transforms of a given size.  Checks size for power of two
 *  and within table limits.
 *
 * Note that once initialized for one size it ready for one
 *  smaller than that also, but it is convenient to do power of
 *  two checking here so we change the _init_size every time
 *  (We *could* pick up where ever we left off by keeping a
 *  _max_init_size but forget that for now).
 *
 * Note that we need sin and cos values for +/- (PI * (m / col))
 *  where col = 1, 2, 4, ..., N/2
 *          m = 0, 1, 2, ..., col-1
 *
 *  Thus we can subscript by: table[(m / col) * N/2]
 *   or with twice as many values by: table[m + col]
 *   We chose the later. (but N.B. this doesn't allow sub
 *   _init_size requests to use existing numbers!)
 */
int
init_sintab(int size)
{
	double	theta;
	int	col, m;

	/*
	 * Check whether the requested size is within our compiled
	 *  limit and make sure it's a power of two.
	 */
	if( size > MAXSIZE ) {
		fprintf( stderr, "fft: Only compiled for max size of %d\n", MAXSIZE );
		fprintf( stderr, "fft: Can't do the requested %d\n", size );
		return( 0 );
	}
	for( m = size; (m & 1) == 0; m >>= 1 )
		;
	if( m != 1 ) {
		fprintf( stderr, "fft: Can only do powers of two, not %d\n", size );
		fprintf( stderr, "fft: What do you think this is, a Winograd transform?\n" );
		return( 0 );
	}

	/* Get some buffer space */
	if( sintab != NULL ) free( sintab );
	if( costab != NULL ) free( costab );
	/* should not use bu_calloc() as libfft is not dependant upon libbu */
	sintab = (double *)calloc( sizeof(*sintab), size );
	costab = (double *)calloc( sizeof(*costab), size );

	/*
	 * Size is okay.  Set up tables.
	 */
	for( col = 1; col < size; col <<= 1 ) {
		for( m = 0; m < col; m++ ) {
			theta = PI * (double)m / (double)col;
			sintab[ m + col ] = sin( theta );
			costab[ m + col ] = cos( theta );
		}
	}

	/*
	 * Mark size and return success.
	 */
	_init_size = size;
/*	fprintf( stderr, "fft: table init, size = %d\n", size );*/
	return( 1 );
}

/*
 * This section implements the Cooley-Tukey Complex
 * Fourier transform.
 * Reference: Nov 84 Dr. Dobbs [#97], and
 *   "Musical Applications of Microprocessors", Hal Chamberlin.
 */

/*
 * SCRAMBLE - put data in bit reversed order
 *
 * Note: Could speed this up with pointers if necessary,
 *   but the butterflies take much longer.
 */
void
scramble(int numpoints, COMPLEX *dat)
{
	register int	i, j, m;
	COMPLEX	temp;

	j = 0;
	for( i = 0; i < numpoints; i++, j += m ) {
		if( i < j ) {
			/* Switch nodes i and j */
			temp.re = dat[j].re;
			temp.im = dat[j].im;
			dat[j].re = dat[i].re;
			dat[j].im = dat[i].im;
			dat[i].re = temp.re;
			dat[i].im = temp.im;
		}
		m = numpoints/2;
		while( m-1 < j ) {
			j -= m;
			m = (m + 1) / 2;
		}
	}
}

void
butterflies(int numpoints, int inverse, COMPLEX *dat)
{
	register COMPLEX *node1, *node2;
	register int step, column, m;
	COMPLEX	w, temp;

	/*
	 * For each column of the butterfly
	 */
	for( column = 1; column < numpoints; column = step ) {
		step = 2 * column;	/* step is size of "cross-hatch" */
		/*
		 * For each principle value of W  (roots on units).
		 */
		for( m = 0; m < column; m++ ) {
			/*
			 * Do these by table lookup:
			 *	theta = PI*(inverse*m)/column;
			 *	w.re = cos( theta );
			 *	w.im = sin( theta );
			 */
			w.re = costab[ column + m ];
			w.im = sintab[ column + m ] * inverse;
			/* Do all pairs of nodes */
			for( node1 = &dat[m]; node1 < &dat[numpoints]; node1 += step ) {
				node2 = node1 + column;
				/*
				 * Want to compute:
				 *  dat[node2] = dat[node1] - w * dat[node2];
				 *  dat[node1] = dat[node1] + w * dat[node2];
				 *
				 * We do all this with pointers now.
				 */

				/*cmult(&w, &dat[node2], &temp);*/
				temp.re = w.re*node2->re - w.im*node2->im;
				temp.im = w.im*node2->re + w.re*node2->im;

				/*csub(&dat[node1], &temp, &dat[node2]);*/
				node2->re = node1->re - temp.re;
				node2->im = node1->im - temp.im;

				/*cadd(&dat[node1], &temp, &dat[node1]);*/
				node1->re += temp.re;
				node1->im += temp.im;
			}
		}
	}

	/* Scale Data (on forward transform only) */
	/*
	 * Technically speaking this gives us the periodogram. XXX
	 * The canonical definition does the scaleing only
	 * after the inverse xform.  Our method may hurt certain
	 * other forms of analysis, e.g. cepstrum.
	 *   **** We Now Do It The Canonical Way! ****
	 */
	if( inverse > 0 ) {
		for( node1 = &dat[0]; node1 < &dat[numpoints]; node1++ ) {
			/* cdiv( &dat[i], &const, &dat[i] ); */
			node1->re /= (double)numpoints;
			node1->im /= (double)numpoints;
		}
	}
}

/**** COMPLEX ARITHMETIC ROUTINES ****/
/**** NO LONGER USED BY TRANSFORMS ***/
/*
 * CADD - Complex ADDition
 */
void
cadd(COMPLEX *result, COMPLEX *val1, COMPLEX *val2)
{
	result->re = val1->re + val2->re;
	result->im = val1->im + val2->im;
}

/*
 * CSUB - Complex SUBtraction
 */
void
csub(COMPLEX *result, COMPLEX *val1, COMPLEX *val2)
{
	result->re = val1->re - val2->re;
	result->im = val1->im - val2->im;
}

/*
 * CMULT - Complex MULTiply
 */
void
cmult(COMPLEX *result, COMPLEX *val1, COMPLEX *val2)
{
	result->re = val1->re*val2->re - val1->im*val2->im;
	result->im = val1->im*val2->re + val1->re*val2->im;
}

/*
 * CDIV - Complex DIVide
 */
void
cdiv(COMPLEX *result, COMPLEX *val1, COMPLEX *val2)
{
	double	denom;

	denom = val2->re*val2->re + val2->im*val2->im;
	result->re = (val1->re*val2->re + val1->im*val2->im)/denom;
	result->im = (val1->im*val2->re - val1->re*val2->im)/denom;
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
