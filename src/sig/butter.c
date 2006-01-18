/*                        B U T T E R . C
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
/** @file butter.c
 * Butterworth filter.
 *
 * 6-pole Butterworth Bandpass Filter (y = gamma, s = jw = relative freq)
 *
 * Hs(s) = y^3s^3 / [1 + 2ys + (3 + 2y^2)s^2 + (4 + y^2)ys^3
 *		     + (3 + 2y^2)s^4 + 2ys^5 + s^6]
 *
 * For 1/3 octave filters y = 2^(1/6) - 2^(-1/6).
 */
#include <math.h>
#include "./complex.h"

void cdiv();

/*
 *  Returns the magnitude of the transfer function Hs(s) for a
 *  1/3 octave 6-pole Butterworth bandpass filter of the given
 *  frequency.
 */
double
butter(double w)
      	  	/* relative frequency (1.0 = center freq) */
{
	COMPLEX	denom, num, H;
	double	gamma, k1, k2, k3, k4;

	/* 1/3 octave gamma */
	gamma = pow( 2.0, 1.0/6.0 ) - pow( 2.0, -1.0/6.0 );

	/* coefficients */
	k1 = pow( gamma, 3.0 );
	k2 = 2.0 * gamma;
	k3 = 3.0 + 2.0 * pow( gamma, 2.0 );
	k4 = gamma * (4.0 + pow( gamma, 2.0 ));

	num.re = 0.0;
	num.im = -k1 * pow( w, 3.0 );

	denom.re = 1.0 - k3 * pow( w, 2.0 )
		 + k3 * pow( w, 4.0 ) - pow( w, 6.0 );
	denom.im = k2 * w - k4 * pow( w, 3.0 )
		 + k2 * pow( w, 5.0 );

	cdiv( &H, &num, &denom );
/*	printf( "(%f, %f)\n", H.re, H.im );*/
	return( CMAG( H ) );
}

/*
 * Compute weights for a log point spaces critical band filter.
 */
void
cbweights(double *filter, int window, int points)

   	       		/* Length of FFT to compute relative freq for */
   	       		/* Length of filter kernel wanted */
{
	int	i, center;
	double	step, w;

	center = points/2;
	step = pow( (double)window, 1.0/(window-1.0) );

	filter[center] = butter( 1.0 );
	w = 1;
	for( i = 1; i <= points/2; i++ ) {
		w *= step;
		/* w = pow( step, (double)i ); */
		filter[center+i] = filter[center-i] = butter( w );
	}
}

#ifdef TEST
#define	N	512.0
int main()
{
	int	offset;
	double	wr, mag, step;

	step = pow( N, 1.0/(N-1) );

	for( offset = -15; offset <= 15; offset++ ) {
		wr = pow( step, (double)offset );
		mag = butter( wr );
		printf( "%4d: %f, %f, %f\n", offset, wr, mag, 20.0*log10( mag ) );
	}

	return 0;
}
#endif /* TEST */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
