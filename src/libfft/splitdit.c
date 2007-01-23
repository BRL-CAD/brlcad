/*                      S P L I T D I T . C
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
/** @file splitdit.c
 *  Real valued, split-radix, decimation in time FFT.
 *
 *  Data comes out as:
 *
 *	[ Re(0), Re(1), ..., Re(N/2), Im(N/2-1), ..., Im(1) ]
 */
int	rfft_adds, rfft_mults;

#include <math.h>
#include "./complex.h"	/* for TWOPI */
#define	INVSQ2	0.70710678118654752440

void
rfft(double *X, int N)
{
	int	i0, i1, i2, i3;
	int	a0, a1, a2, a3, b0, b1, b2, b3;
	int	s, d;
	double	t0, t1, t2, r1, a, aa3, e, c2, c3, d2, d3;
	double	cc1, ss1, cc3, ss3, xt;
	int	i, j, k, ni;
	int	n2, n4;
rfft_adds = rfft_mults = 0;

	/* bit reverse counter */
	j = 1;
	ni = N - 1;
	for( i = 1; i <= ni; i++ ) {
		if( i < j ) {
			xt = X[j-1];
			X[j-1] = X[i-1];
			X[i-1] = xt;
		}
		k = N/2;
		while( k < j ) {
			j -= k;
			k /= 2;
		}
		j += k;
	}

	/* length two transforms */
	for( s = 1, d = 4; s < N; s = 2*d-1, d *= 4 ) {
		for( i0 = s; i0 <= N; i0 += d ) {
			i1 = i0 + 1;
			r1 = X[i0-1];
			X[i0-1] = r1 + X[i1-1];
			X[i1-1] = r1 - X[i1-1];
rfft_adds += 2;
		}
	}

	/* other butterflies */
	n2 = 2;
/*	for( k = 2; k <= M; k++ ) {*/
	for( k = 4; k <= N; k <<= 1 ) {
		n2 *= 2;
		n4 = n2/4;
		/* without mult */
		for( s = 1, d = 2*n2; s < N; s = 2*d-n2+1, d *= 4 ) {
			for( i0 = s; i0 < N; i0 += d ) {
				i1 = i0 + n4;
				i2 = i1 + n4;
				i3 = i2 + n4;
				t0 = X[i2-1] + X[i3-1];
				X[i3-1] = X[i2-1] - X[i3-1];
				X[i2-1] = X[i0-1] - t0;
				X[i0-1] += t0;
rfft_adds += 4;
			}
		}
		if( n4 < 2 ) continue;
		/* with 2 real mult */
		for( s = n4/2+1, d = 2*n2; s < N; s = 2*d-n2+n4/2+1, d *= 4 ) {
			for( i0 = s; i0 < N; i0 += d ) {
				i1 = i0 + n4;
				i2 = i1 + n4;
				i3 = i2 + n4;
				t1 = (X[i2-1]-X[i3-1])*INVSQ2;
				t2 = (X[i2-1]+X[i3-1])*INVSQ2;
				X[i2-1] = t2 - X[i1-1];
				X[i3-1] = t2 + X[i1-1];
				X[i1-1] = X[i0-1] - t1;
				X[i0-1] += t1;
rfft_mults += 2; rfft_adds += 6;
			}
		}
		e = TWOPI/n2;
		a = e;
		if( n4 < 4 ) continue;
		for( j = 2; j <= n4/2; j++ ) {
			aa3 = 3*a;
			cc1 = cos(a);
			ss1 = sin(a);
			cc3 = cos(aa3);
			ss3 = sin(aa3);
			a = j * e;
			/* with 6 real mult */
			for( s = j, d = 2*n2; s < N; s = 2*d-n2+j, d *= 4 ) {
				for( a0 = s; a0 < N; a0 += d ) {
					b1 = a0 + n4;
					a1 = b1-j-j+2;
					b0 = a1 + n4;
					a2 = b1 + n4;
					a3 = a2 + n4;
					b2 = b0 + n4;
					b3 = b2 + n4;
					c2 = X[a2-1]*cc1 - X[b2-1]*ss1;
					d2 = -(X[a2-1]*ss1 + X[b2-1]*cc1);
					c3 = X[a3-1]*cc3 - X[b3-1]*ss3;
					d3 = -(X[a3-1]*ss3 + X[b3-1]*cc3);
rfft_mults += 8; rfft_adds += 4;
					t1 = c2 + c3;
					c3 = c2 - c3;
					t2 = d2 - d3;
					d3 = d2 + d3;
					X[a2-1] = -X[b0-1] - d3;
					X[b2-1] = -X[b1-1] + c3;
					X[a3-1] = X[b1-1] + c3;
					X[b3-1] = X[b0-1] - d3;
					X[b1-1] = X[a1-1] + t2;
					X[b0-1] = X[a0-1] - t1;
					X[a0-1] += t1;
					X[a1-1] -= t2;
rfft_adds += 12;
				}
			}
		}
	}

	/*
	 * For some reason the Imag part is comming out with the wrong
	 * sign, so we reverse it here!  We need to figure this out!
	 */
	for( i = N/2+1; i < N; i++ )
		X[i] = -X[i];
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
